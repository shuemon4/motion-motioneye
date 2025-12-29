# HANDOFF: Implement CSRF Protection for React UI

**Date**: 2025-12-28
**Priority**: High
**Estimated Effort**: Small (2 backend files, 4 frontend files)

---

## Context

The React UI frontend needs CSRF protection to make POST requests to Motion's C++ backend. Motion already has CSRF validation implemented - we just need to:
1. Expose the token to the React UI
2. Accept the token via HTTP header (in addition to existing form data)

## Plan Document

Read the full plan first:
```
docs/plans/react-ui/02-csrf-protection-20251228.md
```

---

## Task: Implement CSRF Protection

### Phase 1: Backend - Add Token to Config Response

**File**: `src/webu_json.cpp`

Find the `api_config()` function (around line 1055) and modify it to include the CSRF token at the start of the JSON response:

```cpp
void cls_webu_json::api_config()
{
    webua->resp_page = "{\"csrf_token\":\"" + webu->csrf_token + "\",";
    // Remove the opening brace from config() output since we added it above
    config();
}
```

**Note**: The `config()` function starts with `{` - you need to handle this so the JSON is valid. Check the `config()` function to understand its output format.

### Phase 2: Backend - Accept X-CSRF-Token Header

**File**: `src/webu_post.cpp`

Find the CSRF validation section (around line 898-905) and add header check before form data check:

```cpp
/* Validate CSRF token before processing any state-changing operations */
std::string csrf_token_received = "";

/* Check X-CSRF-Token header first (React UI) */
const char* header_token = MHD_lookup_connection_value(
    webua->connection, MHD_HEADER_KIND, "X-CSRF-Token");
if (header_token != nullptr) {
    csrf_token_received = std::string(header_token);
} else {
    /* Fall back to form data (legacy HTML UI) */
    for (int indx = 0; indx < post_sz; indx++) {
        if (mystreq(post_info[indx].key_nm, "csrf_token")) {
            csrf_token_received = std::string(post_info[indx].key_val);
            break;
        }
    }
}
```

### Phase 3: Frontend - Create CSRF Module

**New File**: `frontend/src/api/csrf.ts`

```typescript
/**
 * CSRF Token Management
 * Token is obtained from config response and cached in memory.
 * Automatically refreshed on 403 errors.
 */

let csrfToken: string | null = null;

export function setCsrfToken(token: string): void {
  csrfToken = token;
}

export function getCsrfToken(): string | null {
  return csrfToken;
}

export function invalidateCsrfToken(): void {
  csrfToken = null;
}
```

### Phase 4: Frontend - Update Types

**File**: `frontend/src/api/types.ts`

Add `csrf_token` to the `MotionConfig` interface:

```typescript
export interface MotionConfig {
  csrf_token?: string;  // Add this line
  version: string;
  cameras: CamerasResponse;
  // ... rest unchanged
}
```

### Phase 5: Frontend - Extract Token from Config

**File**: `frontend/src/api/queries.ts`

Import the CSRF module and extract token when config is fetched:

```typescript
import { setCsrfToken } from './csrf';

// Modify useMotionConfig:
export function useMotionConfig() {
  return useQuery({
    queryKey: queryKeys.config(0),
    queryFn: async () => {
      const config = await apiGet<MotionConfig>('/0/api/config');
      if (config.csrf_token) {
        setCsrfToken(config.csrf_token);
      }
      return config;
    },
    staleTime: 60000,
  });
}
```

### Phase 6: Frontend - Add Token to POST Requests

**File**: `frontend/src/api/client.ts`

Import CSRF functions and modify `apiPost`:

```typescript
import { getCsrfToken, setCsrfToken, invalidateCsrfToken } from './csrf';

export async function apiPost<T>(
  endpoint: string,
  data: Record<string, unknown>
): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  const token = getCsrfToken();

  try {
    const response = await fetch(endpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
        ...(token && { 'X-CSRF-Token': token }),
      },
      credentials: 'same-origin',
      body: JSON.stringify(data),
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    // Handle 403 - token may be stale (Motion restarted)
    if (response.status === 403) {
      invalidateCsrfToken();

      // Refetch config to get new token
      const configResponse = await fetch('/0/api/config');
      if (configResponse.ok) {
        const config = await configResponse.json();
        if (config.csrf_token) {
          setCsrfToken(config.csrf_token);

          // Retry with new token
          const retryResponse = await fetch(endpoint, {
            method: 'POST',
            headers: {
              'Content-Type': 'application/json',
              'Accept': 'application/json',
              'X-CSRF-Token': config.csrf_token,
            },
            credentials: 'same-origin',
            body: JSON.stringify(data),
          });

          if (!retryResponse.ok) {
            throw new ApiClientError('CSRF validation failed after retry', 403);
          }

          const text = await retryResponse.text();
          return text ? JSON.parse(text) : ({} as T);
        }
      }
      throw new ApiClientError('CSRF validation failed', 403);
    }

    if (!response.ok) {
      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    const text = await response.text();
    return text ? JSON.parse(text) : ({} as T);
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}
```

---

## Testing

After implementation:

1. **Build and deploy to Pi**:
   ```bash
   cd frontend && npm run build
   # Sync to Pi and restart Motion
   ```

2. **Test config fetch includes token**:
   ```bash
   ssh admin@192.168.1.176 "curl -s http://localhost:7999/0/api/config | head -c 100"
   # Should see: {"csrf_token":"abc123...","version":...
   ```

3. **Test POST with header works**:
   ```bash
   # Get token first
   TOKEN=$(ssh admin@192.168.1.176 "curl -s http://localhost:7999/0/api/config" | jq -r '.csrf_token')

   # Test config set with header
   ssh admin@192.168.1.176 "curl -X POST -H 'X-CSRF-Token: $TOKEN' 'http://localhost:7999/0/config/set?log_level=6'"
   ```

4. **Test legacy form data still works** (for HTML UI):
   ```bash
   ssh admin@192.168.1.176 "curl -X POST -d 'csrf_token=$TOKEN&command=config&camid=0' http://localhost:7999/"
   ```

5. **Test React UI** in browser:
   - Open Settings page
   - Change a setting
   - Click Save
   - Verify no 403 errors in console

---

## Files Summary

| File | Action |
|------|--------|
| `src/webu_json.cpp` | Modify `api_config()` to include `csrf_token` |
| `src/webu_post.cpp` | Add `X-CSRF-Token` header check |
| `frontend/src/api/csrf.ts` | Create new file |
| `frontend/src/api/types.ts` | Add `csrf_token` to interface |
| `frontend/src/api/queries.ts` | Extract token from config response |
| `frontend/src/api/client.ts` | Add token to POST headers + 403 retry |

---

## Important Notes

- The CSRF token is generated once when Motion starts (`webu.cpp:522`)
- Token is 64 hex characters from `/dev/urandom`
- Token validation uses constant-time comparison (timing attack safe)
- Legacy HTML UI uses form data - must keep that path working
- React UI uses `X-CSRF-Token` header - cleaner for JSON APIs

---

## Success Criteria

- [ ] `/0/api/config` response includes `csrf_token` field
- [ ] POST requests with `X-CSRF-Token` header are accepted
- [ ] POST requests with form data `csrf_token` still work (legacy)
- [ ] React UI can save config changes without 403 errors
- [ ] 403 triggers automatic token refresh and retry (once)
