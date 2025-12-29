# JSON API Architecture - Implementation Handoff

**For**: Fresh Claude Code session
**Date**: 2025-12-29
**Reference**: `docs/plans/20251229-0115-JSON-API-Architecture.md`

## Objective

Replace the legacy query-string config API with a proper JSON REST API. Remove all legacy code - no backwards compatibility layer.

## Current Problem

- React frontend sends `Content-Type: application/json`
- MHD (libmicrohttpd) POST processor only accepts `application/x-www-form-urlencoded`
- Current workaround in `webu_post.cpp` is fragile
- Settings page makes N HTTP requests to save N parameters (inefficient)

## Target Architecture

### New API Endpoint

```
PATCH /0/api/config
Content-Type: application/json
X-CSRF-Token: <token>

{
  "threshold": 4000,
  "noise_level": 32,
  "framerate": 15
}

Response:
{
  "status": "ok",
  "applied": [
    {"param": "threshold", "old": "1500", "new": "4000", "hot_reload": true},
    {"param": "noise_level", "old": "32", "new": "32", "unchanged": true},
    {"param": "framerate", "old": "30", "new": "15", "hot_reload": true}
  ]
}
```

## Implementation Tasks

### Phase 1: Backend - JSON Parser

Create minimal JSON parser (no external deps):

1. Create `src/json_parse.hpp`:
```cpp
#ifndef _INCLUDE_JSON_PARSE_HPP_
#define _INCLUDE_JSON_PARSE_HPP_

#include <string>
#include <map>
#include <variant>

class JsonParser {
public:
    using JsonValue = std::variant<std::string, double, bool, std::nullptr_t>;

    bool parse(const std::string& json);
    bool has(const std::string& key) const;
    JsonValue get(const std::string& key) const;
    const std::map<std::string, JsonValue>& getAll() const;
    std::string getString(const std::string& key, const std::string& def = "") const;

private:
    std::map<std::string, JsonValue> values_;
    size_t pos_ = 0;
    std::string json_;

    void skipWhitespace();
    bool parseObject();
    bool parseKeyValue();
    std::string parseString();
    JsonValue parseValue();
    double parseNumber();
};

#endif
```

2. Create `src/json_parse.cpp` - implement parser (~150-200 lines)

3. Update `src/Makefile.am` - add `json_parse.cpp` to sources

### Phase 2: Backend - Raw Body Reading

Modify `src/webu_ans.hpp`:
```cpp
// Add member to cls_webu_ans
std::string raw_body;  // Accumulated POST body for JSON endpoints
```

Modify `src/webu_ans.cpp` in `answer_main()`:
- For `PATCH` requests to `/api/*`, accumulate raw body instead of using MHD POST processor
- Call JSON handler when body is complete

### Phase 3: Backend - PATCH /api/config Endpoint

Modify `src/webu_json.hpp`:
```cpp
void api_config_patch();  // New batch config update
```

Modify `src/webu_json.cpp`:
- Add `api_config_patch()` that:
  1. Parses JSON body using `JsonParser`
  2. Validates all parameters exist
  3. Applies all valid parameters
  4. Returns batch response with results

### Phase 4: Backend - Remove Legacy Code

In `src/webu_post.cpp`:
- Remove the workaround in `processor_init()` (lines 884-901)
- Remove the special case in `processor_start()` for `/config/set` (lines 950-967)
- Remove `config_set()` method if no longer used

In `src/webu_json.cpp`:
- Remove `config_set()` (the single-param version)
- Remove related helpers if unused

### Phase 5: Frontend - Batch API

Modify `frontend/src/api/client.ts`:
```typescript
export async function apiPatch<T>(endpoint: string, body: unknown): Promise<T> {
  const response = await fetch(`${API_BASE}${endpoint}`, {
    method: 'PATCH',
    headers: {
      'Content-Type': 'application/json',
      'X-CSRF-Token': await getCsrfToken(),
    },
    body: JSON.stringify(body),
  });
  // ... error handling
  return response.json();
}
```

Modify `frontend/src/api/queries.ts`:
```typescript
export function useUpdateConfig() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async ({ camId, changes }: { camId: number; changes: Record<string, string | number | boolean> }) => {
      return apiPatch(`/${camId}/api/config`, changes);
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['config'] });
    },
  });
}
```

Modify `frontend/src/pages/Settings.tsx`:
- Update `handleSave()` to send all changes in one request
- Update mutation call signature

### Phase 6: Rebuild and Test

```bash
# Sync to Pi
rsync -avz --exclude='.git' --exclude='node_modules' \
  /Users/tshuey/Documents/GitHub/motion-motioneye/ admin@192.168.1.176:~/motion-motioneye/

# Build on Pi (touch modified files first)
ssh admin@192.168.1.176 "cd ~/motion-motioneye && touch src/json_parse.cpp src/webu_ans.cpp src/webu_json.cpp src/webu_post.cpp && make -j4"

# Install and restart
ssh admin@192.168.1.176 "sudo pkill -9 motion; sleep 2; sudo make install && sudo /usr/local/bin/motion -c /etc/motion/motion.conf -n &"

# Test new API
ssh admin@192.168.1.176 'TOKEN=$(curl -s "http://localhost:7999/0/api/config" | python3 -c "import json,sys; print(json.load(sys.stdin)[\"csrf_token\"])") && curl -X PATCH -H "X-CSRF-Token: $TOKEN" -H "Content-Type: application/json" -d "{\"threshold\": 3000, \"noise_level\": 40}" "http://localhost:7999/0/api/config"'
```

## Files to Modify

| File | Action |
|------|--------|
| `src/json_parse.hpp` | CREATE - JSON parser header |
| `src/json_parse.cpp` | CREATE - JSON parser implementation |
| `src/Makefile.am` | MODIFY - add json_parse to build |
| `src/webu_ans.hpp` | MODIFY - add raw_body member |
| `src/webu_ans.cpp` | MODIFY - accumulate body for PATCH |
| `src/webu_json.hpp` | MODIFY - add api_config_patch() |
| `src/webu_json.cpp` | MODIFY - implement batch update, remove legacy |
| `src/webu_post.cpp` | MODIFY - remove legacy workarounds |
| `frontend/src/api/client.ts` | MODIFY - add apiPatch() |
| `frontend/src/api/queries.ts` | MODIFY - batch mutation |
| `frontend/src/pages/Settings.tsx` | MODIFY - use batch save |

## Key Constraints

1. **No external JSON library** - custom minimal parser only
2. **Remove all legacy code** - no backwards compatibility
3. **CPU efficiency** - batch operations reduce overhead
4. **Hot reload** - parameters that support hot reload should still work
5. **CSRF protection** - must validate token on PATCH requests

## Reference Files

Read these to understand current implementation:
- `src/webu_post.cpp` - current POST handling and workarounds
- `src/webu_json.cpp` - current JSON response generation
- `src/webu_ans.cpp` - MHD request routing
- `frontend/src/api/client.ts` - current API client
- `frontend/src/pages/Settings.tsx` - current settings UI

## Success Criteria

1. `PATCH /0/api/config` accepts JSON body with multiple parameters
2. Single HTTP request saves all settings changes
3. Legacy `/config/set?param=value` endpoint removed
4. No MHD POST processor workarounds remain
5. Frontend Settings page works with new API
