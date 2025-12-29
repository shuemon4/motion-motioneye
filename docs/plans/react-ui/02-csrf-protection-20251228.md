# CSRF Protection Plan for React UI

**Created**: 2025-12-28
**Status**: APPROVED
**Goal**: Enable CSRF protection for React UI state-changing API calls

## Decisions Made

- [x] **Hybrid approach approved**: Token in config response + header-based validation
- [x] **Token in config response**: APPROVED - No security impact (same-origin policy, already exposed in HTML)
- [x] **Retry strategy**: 1 retry on 403 is sufficient

---

## Current State Analysis

### Motion Backend CSRF Implementation

Motion already has a complete CSRF protection system implemented:

**Token Generation** (`webu.cpp:531-568`):
- 64-character hex token generated from `/dev/urandom`
- Generated once at daemon startup
- Stored in `cls_webu::csrf_token`
- Uses constant-time comparison to prevent timing attacks

**Token Validation** (`webu_post.cpp:897-916`):
- POST requests are processed through `webu_post->processor_start()`
- CSRF token is validated from form data field `csrf_token`
- Returns 403 Forbidden if validation fails
- After validation, routes `/config/set` requests to `config_set()`

**Token Exposure** (`webu_html.cpp:933-934`):
- Currently only embedded in legacy HTML UI:
  ```javascript
  pCsrfToken = 'abc123...64hexchars...';
  ```
- **Not exposed via a dedicated API endpoint**

### Original MotionEye Approach

MotionEye Python extracted the token by:
1. Fetching Motion's homepage HTML
2. Parsing out `pCsrfToken = '...'` with regex
3. Caching the token
4. Including it in POST requests as form data

**Code Reference** (`motioneye/motionctl.py:637-700`):
```python
async def _get_csrf_token(force_refresh: bool = False) -> str:
    # Fetch Motion homepage
    url = f'http://127.0.0.1:{settings.MOTION_CONTROL_PORT}/'
    resp = await AsyncHTTPClient().fetch(request)

    # Extract token from JavaScript variable
    html = resp.body.decode('utf-8')
    match = re.search(r"pCsrfToken\s*=\s*'([0-9a-f]{64})'", html)
```

### React UI Current State

**No CSRF handling implemented**:
- `frontend/src/api/client.ts` sends JSON body (not form data)
- No token fetching mechanism
- Config updates will fail with 403 when Motion validates

---

## Critical Discovery: Two POST Paths

There are **two different POST processing paths** in Motion:

### Path 1: Form-based POST (Legacy UI)
```
POST / with Content-Type: application/x-www-form-urlencoded
  → webu_ans.cpp:1108-1109 → webu_post->processor_start()
  → CSRF validation in webu_post.cpp:897-916
  → Routes to config_set() if valid
```

### Path 2: Direct POST to `/config/set` (React UI path)
```
POST /{camId}/config/set?param=value with Content-Type: application/json
  → webu_ans.cpp:1108-1109 → webu_post->processor_start()
  → CSRF validation in webu_post.cpp:918-927
  → Routes to config_set() if valid
```

**Key Finding**: Both paths go through `webu_post->processor_start()` which validates CSRF from POST body.

---

## Problem Statement

The React UI currently:
1. Has no way to obtain the CSRF token
2. Sends POST with JSON body, but Motion expects `csrf_token` in form data
3. Will receive 403 Forbidden for all config updates

---

## Solution Options

### Option A: Add CSRF Token API Endpoint (Recommended)

**Backend Change** - Add new endpoint to `webu_json.cpp`:
```cpp
void cls_webu_json::api_csrf_token()
{
    webua->resp_type = WEBUI_RESP_JSON;
    webua->resp_page = "{\"csrf_token\":\"" + webu->csrf_token + "\"}";
}
```

**Route in `webu_ans.cpp`**:
```cpp
} else if (uri_cmd2 == "csrf") {
    webu_json->api_csrf_token();
    mhd_send();
}
```

**Frontend Change** - Fetch token and include in requests:
```typescript
// frontend/src/api/csrf.ts
let csrfToken: string | null = null;

export async function getCsrfToken(): Promise<string> {
  if (csrfToken) return csrfToken;

  const response = await fetch('/0/api/csrf');
  const data = await response.json();
  csrfToken = data.csrf_token;
  return csrfToken;
}

export function invalidateCsrfToken() {
  csrfToken = null;
}
```

**Modify POST to include token**:
- Must use form data format (or modify backend to accept header/JSON)

### Option B: Include CSRF in Config Response

**Backend Change** - Add token to `/0/api/config` response:
```cpp
void cls_webu_json::api_config()
{
    webua->resp_page = "{\"csrf_token\":\"" + webu->csrf_token + "\",";
    config();  // Adds version, cameras, configuration, categories
}
```

**Pros**: No additional request needed
**Cons**: Exposes token on every config fetch (less secure if config is cached)

### Option C: HTTP Header-Based CSRF (Most Modern)

**Backend Change** - Accept token from `X-CSRF-Token` header in `webu_post.cpp`:
```cpp
// Check header first, then form data
std::string csrf_token_received = "";
const char* header_value = MHD_lookup_connection_value(
    webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
if (header_value != nullptr) {
    csrf_token_received = std::string(header_value);
} else {
    // Fall back to form data check (existing code)
}
```

**Frontend Change**:
```typescript
const token = await getCsrfToken();
const response = await fetch(endpoint, {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
    'X-CSRF-Token': token,
  },
  body: JSON.stringify(data),
});
```

**Pros**:
- Standard web security pattern
- Works with JSON body
- Cleaner than form data

**Cons**:
- Requires backend modification
- Breaks legacy form-based clients if header-only

---

## Approved Solution: Token in Config + Header Validation

### Backend Changes

1. **Include CSRF token in config response** (`webu_json.cpp`):
   - Modify `api_config()` to include `csrf_token` field
   - No separate endpoint needed - more efficient

2. **Accept header-based token** (`webu_post.cpp`):
   - Check `X-CSRF-Token` header first
   - Fall back to form data field `csrf_token`
   - Maintains backward compatibility with legacy HTML UI

### Frontend Changes

1. **Create CSRF module** (`frontend/src/api/csrf.ts`):
   ```typescript
   let csrfToken: string | null = null;

   export function setCsrfToken(token: string) {
     csrfToken = token;
   }

   export function getCsrfToken(): string | null {
     return csrfToken;
   }

   export function invalidateCsrfToken() {
     csrfToken = null;
   }
   ```

2. **Extract token from config response** (`frontend/src/api/queries.ts`):
   ```typescript
   export function useMotionConfig() {
     return useQuery({
       queryKey: queryKeys.config(0),
       queryFn: async () => {
         const config = await apiGet<MotionConfig>('/0/api/config');
         // Store CSRF token from config response
         if (config.csrf_token) {
           setCsrfToken(config.csrf_token);
         }
         return config;
       },
       staleTime: 60000,
     });
   }
   ```

3. **Modify API client** (`frontend/src/api/client.ts`):
   ```typescript
   export async function apiPost<T>(endpoint: string, data: Record<string, unknown>): Promise<T> {
     const token = getCsrfToken();

     const response = await fetch(endpoint, {
       method: 'POST',
       headers: {
         'Content-Type': 'application/json',
         'Accept': 'application/json',
         ...(token && { 'X-CSRF-Token': token }),
       },
       credentials: 'same-origin',
       body: JSON.stringify(data),
     });

     // Handle 403 - token may be stale (Motion restarted)
     if (response.status === 403) {
       invalidateCsrfToken();
       // Refetch config to get new token, then retry
       const configResponse = await fetch('/0/api/config');
       const config = await configResponse.json();
       if (config.csrf_token) {
         setCsrfToken(config.csrf_token);
         const retryResponse = await fetch(endpoint, {
           method: 'POST',
           headers: {
             'Content-Type': 'application/json',
             'X-CSRF-Token': config.csrf_token,
           },
           body: JSON.stringify(data),
         });
         if (!retryResponse.ok) {
           throw new ApiClientError('CSRF validation failed', 403);
         }
         return retryResponse.json();
       }
       throw new ApiClientError('CSRF validation failed', 403);
     }

     if (!response.ok) {
       throw new ApiClientError(`HTTP ${response.status}`, response.status);
     }

     const text = await response.text();
     return text ? JSON.parse(text) : ({} as T);
   }
   ```

---

## Implementation Plan

### Phase 1: Backend - Token in Config Response
1. Modify `api_config()` in `webu_json.cpp` to include `csrf_token` field
2. Test config response includes token

### Phase 2: Backend - Header Support
1. Modify `webu_post.cpp` to check `X-CSRF-Token` header first
2. Keep form data fallback for legacy HTML UI compatibility
3. Test both header and form data paths work

### Phase 3: Frontend - CSRF Integration
1. Create `frontend/src/api/csrf.ts` module
2. Modify `frontend/src/api/queries.ts` to extract token from config
3. Modify `frontend/src/api/client.ts` to include token in headers
4. Add 403 retry logic with token refresh
5. Update types in `frontend/src/api/types.ts`

### Phase 4: Testing
1. Test config fetch includes CSRF token
2. Test config save works with header token
3. Test Motion restart triggers token refresh on 403
4. Test legacy HTML UI still works (form data path)

---

## Files to Modify

### Backend (C++)
| File | Change |
|------|--------|
| `src/webu_json.cpp` | Modify `api_config()` to include `csrf_token` |
| `src/webu_post.cpp` | Add `X-CSRF-Token` header check before form data check |

### Frontend (TypeScript)
| File | Change |
|------|--------|
| `frontend/src/api/csrf.ts` | New file - CSRF token management |
| `frontend/src/api/client.ts` | Add token to POST headers, add 403 retry |
| `frontend/src/api/queries.ts` | Extract token from config response |
| `frontend/src/api/types.ts` | Add `csrf_token` to MotionConfig interface |

---

## Security Considerations

1. **Token in JSON response**: The `/0/api/csrf` endpoint exposes the token, but this is acceptable because:
   - Same-origin policy prevents cross-site access
   - Token is session-specific (changes on Motion restart)
   - Attack requires authenticated access first

2. **Token caching**: Frontend caches token to reduce requests, but:
   - Invalidates on 403 response
   - Re-fetches automatically on retry
   - No persistence across page reloads

3. **Backward compatibility**:
   - Form data path (`csrf_token` field) kept for legacy HTML UI
   - Header path added for modern React UI
   - Both paths validate identically

---

## Security Analysis: Token in Config Response

**Why including the CSRF token in `/0/api/config` is safe:**

1. **Same-origin policy**: Only JavaScript from the same origin can read the response body. Cross-site scripts cannot access it.

2. **Already exposed**: The token is already embedded in Motion's HTML page (line 934 of `webu_html.cpp`). Including it in JSON is equivalent exposure.

3. **Authentication required**: Both the HTML page and the config API require authentication. An attacker cannot fetch either without valid credentials.

4. **Session-bound**: The token changes when Motion restarts, limiting the window of any theoretical exposure.

**Conclusion**: No additional security risk vs. current HTML embedding approach.

---

**End of Plan**
