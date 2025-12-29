# JSON API Architecture Refactor

**Date**: 2025-12-29
**Status**: Planning
**Priority**: High - Foundational change

## Current State

The codebase has an architectural mismatch:

1. **Frontend (React)**: Modern stack expecting JSON APIs with proper REST semantics
2. **Backend (Motion C++)**: Legacy web API designed for HTML forms

### Current Problems

1. **Content-Type mismatch**: MHD's `MHD_create_post_processor()` only accepts `application/x-www-form-urlencoded` or `multipart/form-data`. React sends `application/json`.

2. **Query string abuse**: Config changes use `POST /config/set?param=value` with empty body - the data is in URL, not body.

3. **No batching**: Settings page makes N sequential HTTP requests to save N parameters.

4. **No JSON parsing**: Backend manually constructs JSON via string concatenation but cannot parse incoming JSON.

## Proposed Architecture

### API Design

```
# Current (legacy, keep for backwards compatibility)
POST /0/config/set?threshold=4000
Content-Type: application/x-www-form-urlencoded

# New REST API (proper design)
PATCH /0/api/config
Content-Type: application/json
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

### Implementation Components

#### 1. Minimal JSON Parser (No External Dependency)

For CPU efficiency and minimal footprint, implement a lightweight JSON parser that handles only what we need:
- Parse flat object: `{"key": value, ...}`
- Value types: string, number, boolean
- No nested objects, no arrays (not needed for config)

```cpp
// src/json_parse.hpp
struct JsonValue {
    enum Type { STRING, NUMBER, BOOL, NUL } type;
    std::string str_val;
    double num_val;
    bool bool_val;
};

class JsonParser {
public:
    bool parse(const std::string& json);
    bool has(const std::string& key) const;
    JsonValue get(const std::string& key) const;
    std::map<std::string, JsonValue> getAll() const;
private:
    std::map<std::string, JsonValue> values;
};
```

Estimated size: ~200 lines of C++. No external dependencies.

#### 2. Raw Body Reading in MHD

Instead of using `MHD_create_post_processor()`, read raw POST body directly:

```cpp
// In webu_ans.cpp - accumulate raw body
mhdrslt cls_webu_ans::answer_main(const char *upload_data, size_t *upload_data_size)
{
    if (*upload_data_size > 0) {
        raw_body.append(upload_data, *upload_data_size);
        *upload_data_size = 0;
        return MHD_YES;
    }
    // Body complete, now route
}
```

#### 3. New Endpoint: `PATCH /api/config`

```cpp
// src/webu_json.cpp
void cls_webu_json::api_config_patch()
{
    JsonParser parser;
    if (!parser.parse(webua->raw_body)) {
        error_response(400, "Invalid JSON");
        return;
    }

    std::vector<ConfigResult> results;

    for (const auto& [key, value] : parser.getAll()) {
        ConfigResult result = apply_config(key, value);
        results.push_back(result);
    }

    // Single atomic response
    build_batch_response(results);
}
```

#### 4. Frontend Changes

```typescript
// frontend/src/api/queries.ts
export function useUpdateConfig() {
  return useMutation({
    mutationFn: async (changes: Record<string, string | number | boolean>) => {
      return apiPatch<ConfigResponse>(`/${camId}/api/config`, changes);
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['config'] });
    },
  });
}

// frontend/src/pages/Settings.tsx
const handleSave = async () => {
  // Single request with all changes
  await updateConfigMutation.mutateAsync(changes);
};
```

### Migration Strategy

1. **Phase 1**: Add JSON parser and raw body reading (no breaking changes)
2. **Phase 2**: Add `PATCH /api/config` endpoint (new functionality)
3. **Phase 3**: Update frontend to use batch API
4. **Phase 4**: Deprecate legacy query string API

### CPU Efficiency Gains

| Operation | Current | Proposed |
|-----------|---------|----------|
| Save 5 settings | 5 HTTP requests | 1 HTTP request |
| CSRF validation | 5x | 1x |
| Response generation | 5x | 1x |
| Network round-trips | 5 | 1 |

### Files to Create/Modify

**New Files**:
- `src/json_parse.hpp` - Minimal JSON parser header
- `src/json_parse.cpp` - JSON parser implementation

**Modified Files**:
- `src/webu_ans.hpp` - Add `raw_body` member
- `src/webu_ans.cpp` - Accumulate raw body for JSON endpoints
- `src/webu_json.hpp` - Add `api_config_patch()` declaration
- `src/webu_json.cpp` - Implement batch config update
- `src/Makefile.am` - Add json_parse to build
- `frontend/src/api/client.ts` - Add `apiPatch()` function
- `frontend/src/api/queries.ts` - Update `useUpdateConfig` to batch
- `frontend/src/pages/Settings.tsx` - Single save for all changes (already batches locally)

### Rollback Plan

The legacy `/config/set?param=value` endpoint remains functional. If issues arise with the new API, frontend can revert to sequential calls.

### Testing Checklist

- [ ] JSON parser handles valid JSON
- [ ] JSON parser rejects invalid JSON
- [ ] JSON parser handles Unicode strings
- [ ] PATCH /api/config applies multiple params atomically
- [ ] PATCH /api/config validates all params before applying
- [ ] Frontend batches all changes in single request
- [ ] Hot reload still works for applicable params
- [ ] Legacy API still works for backwards compatibility

## Decision

Implement Option B: **Backend reads JSON body** with:
- Minimal custom JSON parser (no external deps)
- New `PATCH /api/config` endpoint for batch updates
- Keep legacy API for backwards compatibility
- Frontend updated to use batch API

## Estimated Scope

- Backend: ~400 lines of new C++ code
- Frontend: ~50 lines of TypeScript changes
- Testing: ~2 hours on actual hardware
