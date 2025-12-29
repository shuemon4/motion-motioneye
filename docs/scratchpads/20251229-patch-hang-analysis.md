# PATCH Endpoint Hang - Root Cause Analysis

## Issue
PATCH /0/api/config endpoint hangs - curl sends data but never receives response.

## Root Cause
`api_config_patch()` in `src/webu_json.cpp` (line 1266-1375) has a **mutex lock leak**:

1. Line 1305: `pthread_mutex_lock(&app->mutex_post);` acquires lock
2. Lines 1306-1366: Process each parameter
3. Line 1367: `pthread_mutex_unlock(&app->mutex_post);` releases lock
4. Lines 1369-1374: Build response summary **AFTER** unlock

**The Problem**: Lock is released at line 1367, but function continues to modify `resp_page` after unlock. This is correct behavior - the lock is actually being released.

Wait, let me re-read... Looking again at the code flow:

```cpp
1305:    pthread_mutex_lock(&app->mutex_post);
1306:    for (const auto& kv : parser.getAll()) {
         // ... process parameters ...
1366:    }
1367:    pthread_mutex_unlock(&app->mutex_post);
1368:
1369:    webua->resp_page += "]";
1370:    webua->resp_page += ",\"summary\":{";
         // ... rest of response ...
1374:    webua->resp_page += "}}";
1375: }
```

The mutex is properly unlocked at 1367. So that's not the issue.

## Re-analysis: The Real Issue

Looking at the flow in `webu_ans.cpp:1174-1191`:

```cpp
1174:    } else if (mystreq(method,"PATCH")) {
1175:        /* Accumulate raw body for JSON endpoints */
1176:        if (*upload_data_size > 0) {
1177:            raw_body.append(upload_data, *upload_data_size);
1178:            *upload_data_size = 0;
1179:            return MHD_YES;
1180:        }
1181:        /* Body complete, process request */
1182:        if (uri_cmd1 == "api" && uri_cmd2 == "config") {
1183:            if (webu_json == nullptr) {
1184:                webu_json = new cls_webu_json(this);
1185:            }
1186:            webu_json->api_config_patch();
1187:            mhd_send();
1188:        } else {
1189:            bad_request();
1190:        }
1191:        retcd = MHD_YES;
```

**The response IS being sent via `mhd_send()` at line 1187.**

Let me check if `mhd_send()` is working correctly...

## Hypothesis 2: Response Not Built

Maybe `api_config_patch()` is failing early and not building a response?

Let me check for early returns in `api_config_patch()`:
- Line 1277: Early return on CSRF validation failure - **sets resp_page**
- Line 1287: Early return on JSON parse error - **sets resp_page**
- No other early returns

Both early returns set `resp_page` before returning, so `mhd_send()` should work.

## Hypothesis 3: mhd_send() Issue

The `mhd_send()` function in `webu_ans.cpp:826-900` should work. Let me verify it builds and queues the response properly...

Actually, I notice that `mhd_send()` calls `MHD_queue_response()` at line 894 which should work.

## Hypothesis 4: Missing parms_edit() call

Wait! Looking at the request flow:

1. First call (mhd_first=true): Sets cnct_method = WEBUI_METHOD_PATCH, returns MHD_YES (line 1159)
2. Second call (mhd_first=false): Should process PATCH

But at line 1182, we check `if (uri_cmd1 == "api" && uri_cmd2 == "config")` - these variables were set by `parms_edit()` called in constructor at line 1350.

The URL is `/0/api/config`, so:
- uri_cmd0 = "0"
- uri_cmd1 = "api"
- uri_cmd2 = "config"

This should match correctly.

## ROOT CAUSE IDENTIFIED

After careful analysis, I found the issue! In `webu_ans.cpp`, the PATCH handler has a subtle but critical bug in lines 1176-1180:

```cpp
if (*upload_data_size > 0) {
    raw_body.append(upload_data, *upload_data_size);
    *upload_data_size = 0;
    return MHD_YES;
}
```

**The Problem**: This returns `MHD_YES` to signal "keep calling me", but **libmicrohttpd (MHD) expects the callback to continue being called until upload is complete**. HOWEVER, when `*upload_data_size == 0` on subsequent calls (which happens after we set it to 0), MHD thinks we're done accumulating and expects us to process the request.

**But here's the subtle issue**: When the body is empty (test with `{"threshold":"2000"}`), MHD might call us with `upload_data_size = 0` IMMEDIATELY, meaning we skip the accumulation block and go straight to processing - but `raw_body` is empty!

**Actually, wait...** Let me re-read the MHD documentation logic:

MHD calling pattern for PATCH/POST with body:
1. First call: `mhd_first=true`, `*upload_data_size=0` - we return after setting method
2. Second call: `mhd_first=false`, `*upload_data_size > 0` - we accumulate, set to 0, return YES
3. Third call: `mhd_first=false`, `*upload_data_size=0` - body complete, we process

So the logic SHOULD work. Let me think about what else could cause a hang...

## ACTUAL ROOT CAUSE

I just realized - **the call to `mhd_send()` at line 1187 calls `MHD_queue_response()` at line 894, which queues the response but DOESN'T send it yet**.

Looking at `mhd_send()` in `webu_ans.cpp:826-900`, line 894 calls `MHD_queue_response()` which QUEUES the response to be sent by MHD's internal machinery.

The hang must be caused by **the function not returning to MHD after queueing the response**. But wait, line 1191 does `retcd = MHD_YES;` and line 1199 returns it.

## BREAKTHROUGH FINDING

With debug logging enabled, I found:

**First PATCH request (invalid CSRF):**
```
Dec 29 03:14:12 [DBG][STR][wc00] answer_main: PATCH: Accumulating 20 bytes, total now 20
Dec 29 03:14:12 [DBG][STR][wc00] answer_main: PATCH: Body complete (20 bytes), processing api/config
Dec 29 03:14:12 [ERR][STR][wc00] api_config_patch: CSRF token validation failed for PATCH from 192.168.1.184
Dec 29 03:14:12 [DBG][STR][wc00] answer_main: PATCH: api_config_patch() completed, sending response (53 bytes)
Dec 29 03:14:12 [DBG][STR][wc00] answer_main: PATCH: mhd_send() completed
```

**This request worked perfectly!** Response was sent. The client received `{"status":"error","message":"CSRF validation failed"}` immediately.

**Subsequent PATCH requests (valid CSRF):**
- No log entries at all
- Requests hang/timeout
- Never reach our PATCH handler

**CONCLUSION**: The hang happens on **SECOND and subsequent PATCH requests to the same endpoint**. This suggests:
1. **Connection state issue** - first request works, second hangs
2. **MHD connection pooling** - persistent connection not being handled correctly
3. **Resource leak** - first request doesn't clean up properly

The issue is NOT in `api_config_patch()` - that works fine. The issue is **BEFORE our handler is called**, likely in MHD's request dispatch or our connection setup.

## Next Investigation

Need to check:
1. How `raw_body` is initialized/cleared between requests
2. Whether `mhd_first` flag is being reset properly
3. MHD connection keep-alive handling
