# Stream Resolution Diagnostic Logging
**Date:** 2025-12-22 17:35
**Purpose:** Track all diagnostic logging added to identify 480x272 stream resolution bug

## Problem Statement
- Configuration: 1920x1080
- Internal Motion dimensions: 1920x1080 (verified via DIAG logs)
- libcamera output: 1920x1080 (verified)
- **MJPEG stream output: 480x272** ← THE BUG

## Key Mystery
The diagnostic log "DIAG: Stream JPEG encode" never appeared, meaning the normal stream encoding path in `webu_getimg_norm()` didn't execute, yet we received a 480x272 MJPEG stream.

## Diagnostic Logging Changes

### Phase 1: Entry Point Tracking

#### File: src/webu_getimg.cpp
**Location:** `webu_getimg_norm()` - line ~100

**ADDED:** Entry point logging (ALWAYS logs, no conditions)
```c
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_ENTRY: webu_getimg_norm() called - jpg_cnct=%d, ts_cnct=%d, all_cnct=%d"
    , cam->stream.norm.jpg_cnct, cam->stream.norm.ts_cnct, cam->stream.norm.all_cnct);
```

**ADDED:** Condition tracking
```c
if (cam->stream.norm.jpg_cnct > 0) {
    MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
        , "DIAG_PATH: jpg_cnct > 0, jpg_data=%p, image_norm=%p, consumed=%d"
        , cam->stream.norm.jpg_data, cam->current_image->image_norm
        , cam->stream.norm.consumed);
```

**ADDED:** JPEG encoding result
```c
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_ENCODE: Encoded JPEG - width=%d, height=%d, jpg_sz=%d"
    , cam->imgs.width, cam->imgs.height, cam->stream.norm.jpg_sz);
```

---

### Phase 2: Stream Delivery Tracking

#### File: src/webu_stream.cpp
**Location:** `mjpeg_response()` - line ~282

**ADDED:** Route selection logging
```c
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_ROUTE: device_id=%d, routing to %s"
    , webua->device_id
    , (webua->device_id == 0) ? "mjpeg_all_img" : "mjpeg_one_img");
```

**Location:** `mjpeg_one_img()` - line ~229

**ADDED:** Entry and stream type
```c
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_SERVE: mjpeg_one_img() - cnct_type=%d, cam=%p"
    , webua->cnct_type, webua->cam);
```

**ADDED:** JPEG size being served
```c
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_SERVE: Serving JPEG - jpg_sz=%d bytes"
    , strm->jpg_sz);
```

---

### Phase 3: AllCam Scaling Check

#### File: src/allcam.cpp
**Location:** `getsizes_img()` - line ~35

**ADDED:** Scaling calculation logging
```c
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_SCALE: Device %d - scale=%d, src=%dx%d, calculating dst..."
    , p_cam->cfg->device_id, p_cam->all_loc.scale
    , p_cam->all_sizes.src_w, p_cam->all_sizes.src_h);
```

**ADDED:** Final scaled dimensions
```c
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_SCALE: Device %d - dst=%dx%d (from scale=%d%%)"
    , p_cam->cfg->device_id, dst_w, dst_h, p_cam->all_loc.scale);
```

---

### Phase 4: Connection Tracking

#### File: src/webu_getimg.cpp
**Location:** `webu_getimg_main()` - line ~324

**ADDED:** Stream processing entry
```c
MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
    , "DIAG_MAIN: webu_getimg_main() processing camera %d"
    , cam->cfg->device_id);
```

---

## Diagnostic Search Patterns

After running Motion with these diagnostics, search logs for:

1. **Entry tracking:** `grep DIAG_ENTRY /tmp/motion_test.log`
2. **Encoding path:** `grep DIAG_PATH /tmp/motion_test.log`
3. **Actual encoding:** `grep DIAG_ENCODE /tmp/motion_test.log`
4. **Routing:** `grep DIAG_ROUTE /tmp/motion_test.log`
5. **Serving:** `grep DIAG_SERVE /tmp/motion_test.log`
6. **Scaling:** `grep DIAG_SCALE /tmp/motion_test.log`
7. **All diagnostics:** `grep 'DIAG_' /tmp/motion_test.log`

## Expected Results

### If bug is in webu_getimg_norm():
- Will see DIAG_ENTRY logs
- Will see DIAG_ENCODE with wrong dimensions

### If bug is in routing:
- Will see DIAG_ROUTE showing device_id=0 when it should be 1
- May see mjpeg_all_img being called instead of mjpeg_one_img

### If bug is in allcam scaling:
- Will see DIAG_SCALE showing scale=25 instead of 100
- Will see dst dimensions as 480x272

### If stream not from webu_getimg_norm():
- Will NOT see DIAG_ENTRY or DIAG_ENCODE
- Suggests alternate stream source (static snapshot, cache, etc.)

## Cleanup Plan

After identifying the bug, remove all lines marked with `DIAG_` prefix:
```bash
# Search for all diagnostic lines
grep -rn "DIAG_" src/

# Remove them systematically
# Keep only the final fix, remove all temporary diagnostics
```

## Notes
- All DIAG logs use NTC level to ensure visibility
- DIAG_MAIN uses DBG level to avoid spam
- Each log includes unique DIAG_ prefix for easy filtering
- Logs include relevant context (pointers, sizes, flags)

## CRITICAL FINDING - 2025-12-22 18:07

**THE BUG IS A ROUTING ISSUE!**

```
[NTC][STR][wc00] mjpeg_response: DIAG_ROUTE: device_id=0, routing to mjpeg_all_img
```

When accessing `/1/mjpg/stream`, Motion is routing to `mjpeg_all_img` (allcam combined view) with `device_id=0` instead of `mjpeg_one_img` (individual camera) with `device_id=1`.

### Evidence:
1. `DIAG_ENTRY` logs show `jpg_cnct=0` - webu_getimg_norm() never gets a connection
2. `DIAG_ROUTE` logs show `device_id=0` - requests are routed to allcam handler
3. `DIAG_SCALE` shows correct individual camera scale: `scale=100, dst=1920x1080`
4. But stream outputs 480x272 instead

### Root Cause:
The URL routing logic is mapping `/1/mjpg/stream` to `device_id=0` (allcam) instead of `device_id=1` (camera 1).

### Resolution - FIXED! 2025-12-22 18:40

**Root Cause:** Off-by-one error in URL parsing (`src/webu_ans.cpp:156-160`)

**The Bug:**
```cpp
// BEFORE (wrong):
pos_slash1 = url.find("/", baselen+1);
uri_cmd0 = url.substr(baselen+1, pos_slash1-baselen-1);

// For URL "/1/mjpg/stream" with baselen=1:
// - url.find("/", 2) finds position 2
// - url.substr(2, 0) = "" (empty string!)
// - Empty uri_cmd0 caused device_id=0
```

**The Fix:**
```cpp
// AFTER (correct):
pos_slash1 = url.find("/", baselen);
uri_cmd0 = url.substr(baselen, pos_slash1-baselen);

// For URL "/1/mjpg/stream" with baselen=1:
// - url.find("/", 1) finds position 2
// - url.substr(1, 1) = "1" ✓
// - Correctly sets device_id=1
```

**Verification:**
- DIAG logs: `Set device_id=1 from uri_cmd0='1'`
- Routing: `device_id=1, routing to mjpeg_one_img` ✓
- Stream output: **1920x1080** (verified via Python PIL)
- File size: 78,990 bytes per JPEG frame (consistent with full resolution)

---

### Phase 5: URL Parsing Diagnostics

#### File: src/webu_ans.cpp
**Location:** `parseurl()` - line ~123

**ADDED:** Base path and URL logging
```c
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_PARSE: url='%s', base_path='%s', baselen=%d"
    , url.c_str(), app->cfg->webcontrol_base_path.c_str(), (int)baselen);
```

**Location:** `parseurl()` - line ~164

**ADDED:** uri_cmd0 extraction logging
```c
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_PARSE: Extracted uri_cmd0='%s' from position %d to %d"
    , uri_cmd0.c_str(), (int)(baselen+1), (int)pos_slash1);
```

**Location:** `parms_edit()` - line ~222

**ADDED:** device_id assignment tracking
```c
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_PARSE: parms_edit() - uri_cmd0='%s', length=%d"
    , uri_cmd0.c_str(), (int)uri_cmd0.length());

/* Inside numeric check */
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_PARSE: Set device_id=%d from uri_cmd0='%s'"
    , device_id, uri_cmd0.c_str());

/* If not numeric */
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_PARSE: uri_cmd0='%s' is NOT numeric, device_id unchanged"
    , uri_cmd0.c_str());

/* If empty */
MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
    , "DIAG_PARSE: uri_cmd0 is empty, set device_id=0");
```

**Purpose:** Track complete URL parsing flow to identify where `/1/mjpg/stream` fails to set `device_id=1`
