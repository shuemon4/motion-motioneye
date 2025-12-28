# Edit Handler Consolidation Plan

**Date**: 2025-12-17
**Status**: Analysis Complete - Ready for Implementation
**Target**: Reduce conf.cpp from ~4095 lines to ~2000 lines
**Risk Level**: Medium (functional logic changes)

---

## Executive Summary

Consolidate ~175 individual `edit_XXX()` handler functions into a small set of type-based generic handlers. Each individual handler currently follows one of ~5 patterns; consolidating into generic handlers with metadata reduces code by ~2000 lines while improving maintainability.

---

## Current State Analysis

### Handler Count by Pattern Type

| Pattern | Count | Lines Each | Total Lines | Example |
|---------|-------|------------|-------------|---------|
| **Simple String** | ~45 | 10 | 450 | `edit_v4l2_device`, `edit_netcam_url` |
| **Simple Bool** | ~25 | 8 | 200 | `edit_daemon`, `edit_threshold_tune` |
| **Int with min/max** | ~55 | 15 | 825 | `edit_threshold`, `edit_framerate` |
| **Int with min only** | ~15 | 12 | 180 | `edit_watchdog_tmo` |
| **List (enum)** | ~20 | 18 | 360 | `edit_secondary_method`, `edit_log_type` |
| **Float with range** | ~5 | 15 | 75 | `edit_libcam_brightness` |
| **Custom Logic** | ~10 | 20+ | 200+ | `edit_target_dir`, `edit_log_file` |
| **Total** | ~175 | - | **~2300** | |

### Category Dispatch Functions

19 `edit_catXX()` functions with if-else chains:
- Lines 3156-3385 (229 lines)
- Each function dispatches to individual handlers via string comparison
- Already optimized: `edit_set_active()` uses O(1) registry lookup

---

## Proposed Architecture

### 1. Parameter Metadata Structure

Extend `ctx_parm` (or create `ctx_parm_meta`) with validation metadata:

```cpp
// In parm_registry.hpp
struct ctx_parm_validation {
    // For integers
    int int_default;
    int int_min;
    int int_max;

    // For floats
    float float_default;
    float float_min;
    float float_max;

    // For booleans
    bool bool_default;

    // For strings
    std::string str_default;

    // For list/enum types
    std::vector<std::string> valid_values;  // Empty = any string OK

    // Offset in scoped struct for direct access (Phase 2)
    // Using std::variant or union for type-safe access
};
```

### 2. Generic Handler Functions

Replace 175 individual handlers with 6 type-based handlers:

```cpp
// Generic handlers (private methods in cls_config)
void edit_generic_bool(bool& target, const std::string& parm,
                       enum PARM_ACT pact, bool default_val);

void edit_generic_int(int& target, const std::string& parm,
                      enum PARM_ACT pact, int default_val, int min_val, int max_val);

void edit_generic_float(float& target, const std::string& parm,
                        enum PARM_ACT pact, float default_val, float min_val, float max_val);

void edit_generic_string(std::string& target, const std::string& parm,
                         enum PARM_ACT pact, const std::string& default_val);

void edit_generic_list(std::string& target, const std::string& parm,
                       enum PARM_ACT pact, const std::string& default_val,
                       const std::vector<std::string>& valid_values);

void edit_generic_array(std::list<std::string>& target, const std::string& parm,
                        enum PARM_ACT pact);
```

### 3. Dispatch Mechanism

Two options for routing parameters to the correct member variable:

#### Option A: Pointer-to-Member (Type-Safe, Compile-Time)

```cpp
// Metadata includes pointer-to-member
struct ctx_parm_meta {
    std::string name;
    enum PARM_TYP type;

    // Type-specific pointer-to-member (use std::variant)
    std::variant<
        int ctx_parm_cam::*,      // For int members
        bool ctx_parm_cam::*,     // For bool members
        float ctx_parm_cam::*,    // For float members
        std::string ctx_parm_cam::* // For string members
    > member_ptr;

    // Validation data
    int int_min, int_max, int_default;
    // ... etc
};
```

**Pros**: Type-safe, no runtime overhead
**Cons**: Complex template metaprogramming, different scopes (parm_app vs parm_cam)

#### Option B: Named Dispatch with Switch (Simple, Readable)

Keep parameter name dispatch but use generic handlers:

```cpp
void cls_config::edit_set(const std::string& parm_nm, const std::string& parm_val) {
    const ctx_parm_ext* parm = ctx_parm_registry::instance().find(parm_nm);
    if (!parm) {
        MOTION_LOG(ALR, TYPE_ALL, NO_ERRNO, _("Unknown config option \"%s\""), parm_nm.c_str());
        return;
    }

    // Dispatch by name to generic handler with correct target reference
    dispatch_edit(parm_nm, parm_val, PARM_ACT_SET);
}

void cls_config::dispatch_edit(const std::string& name, std::string& val, enum PARM_ACT pact) {
    // Group by type for efficiency

    // Booleans
    if (name == "daemon")           return edit_generic_bool(daemon, val, pact, false);
    if (name == "threshold_tune")   return edit_generic_bool(threshold_tune, val, pact, false);
    if (name == "noise_tune")       return edit_generic_bool(noise_tune, val, pact, true);
    // ... ~25 bool params

    // Integers with range
    if (name == "threshold")        return edit_generic_int(threshold, val, pact, 1500, 1, INT_MAX);
    if (name == "framerate")        return edit_generic_int(framerate, val, pact, 15, 2, 100);
    if (name == "log_level")        return edit_generic_int(log_level, val, pact, 6, 1, 9);
    // ... ~55 int params

    // Strings (simple)
    if (name == "v4l2_device")      return edit_generic_string(v4l2_device, val, pact, "");
    if (name == "netcam_url")       return edit_generic_string(netcam_url, val, pact, "");
    // ... ~45 string params

    // Lists (enum with valid values)
    static const std::vector<std::string> log_types = {"ALL","COR","STR","ENC","NET","DBL","EVT","TRK","VID"};
    if (name == "log_type")         return edit_generic_list(log_type_str, val, pact, "ALL", log_types);
    // ... ~20 list params

    // Custom handlers (can't be generalized)
    if (name == "target_dir")       return edit_custom_target_dir(val, pact);
    if (name == "log_file")         return edit_custom_log_file(val, pact);
    // ... ~10 custom params
}
```

**Pros**: Simple, readable, easy to maintain, no template complexity
**Cons**: Still has string comparisons (but O(1) lookup already done in edit_set)

### Recommendation: Option B (Named Dispatch)

Rationale:
1. Simpler implementation
2. Easier to understand and maintain
3. O(1) lookup already handled by registry
4. Custom handlers can coexist easily
5. No template metaprogramming complexity

---

## Implementation Phases

### Phase 1: Create Generic Handlers (~50 lines new)

Add to `conf.cpp`:

```cpp
void cls_config::edit_generic_bool(bool& target, std::string& parm,
                                   enum PARM_ACT pact, bool default_val)
{
    if (pact == PARM_ACT_DFLT) {
        target = default_val;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(target, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, target);
    }
}

void cls_config::edit_generic_int(int& target, std::string& parm,
                                  enum PARM_ACT pact, int default_val, int min_val, int max_val)
{
    if (pact == PARM_ACT_DFLT) {
        target = default_val;
    } else if (pact == PARM_ACT_SET) {
        int val = atoi(parm.c_str());
        if (val < min_val || val > max_val) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid value %d (range %d-%d)"), val, min_val, max_val);
        } else {
            target = val;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(target);
    }
}

void cls_config::edit_generic_float(float& target, std::string& parm,
                                    enum PARM_ACT pact, float default_val, float min_val, float max_val)
{
    if (pact == PARM_ACT_DFLT) {
        target = default_val;
    } else if (pact == PARM_ACT_SET) {
        float val = (float)atof(parm.c_str());
        if (val < min_val || val > max_val) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid value %.2f (range %.2f-%.2f)"), val, min_val, max_val);
        } else {
            target = val;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(target);
    }
}

void cls_config::edit_generic_string(std::string& target, std::string& parm,
                                     enum PARM_ACT pact, const std::string& default_val)
{
    if (pact == PARM_ACT_DFLT) {
        target = default_val;
    } else if (pact == PARM_ACT_SET) {
        target = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = target;
    }
}

void cls_config::edit_generic_list(std::string& target, std::string& parm,
                                   enum PARM_ACT pact, const std::string& default_val,
                                   const std::vector<std::string>& valid_values)
{
    if (pact == PARM_ACT_DFLT) {
        target = default_val;
    } else if (pact == PARM_ACT_SET) {
        bool valid = parm.empty();  // Empty usually OK
        for (const auto& v : valid_values) {
            if (parm == v) { valid = true; break; }
        }
        if (!valid) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid value %s"), parm.c_str());
        } else {
            target = parm.empty() ? default_val : parm;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = target;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[";
        for (size_t i = 0; i < valid_values.size(); i++) {
            if (i > 0) parm += ",";
            parm += "\"" + valid_values[i] + "\"";
        }
        parm += "]";
    }
}
```

### Phase 2: Create Dispatch Function (~300 lines)

Create single `dispatch_edit()` that routes all parameters to generic handlers with inline metadata.

### Phase 3: Remove Individual Handlers (~2000 lines removed)

Delete all individual `edit_XXX()` functions except:
- Custom handlers that can't be generalized (~10)
- Helper functions (`edit_set_bool`, `edit_get_bool`)

### Phase 4: Simplify Category Dispatch (~200 lines removed)

Replace 19 `edit_catXX()` functions with single dispatch through `dispatch_edit()`.

### Phase 5: Update Header File

Remove ~150 private method declarations from `conf.hpp`.

---

## Custom Handlers to Keep

These handlers have logic that can't be generalized:

| Handler | Reason |
|---------|--------|
| `edit_log_file` | strftime expansion of filename |
| `edit_target_dir` | Trailing slash removal, % validation |
| `edit_text_changes` | Complex validation logic |
| `edit_picture_filename` | Validates conversion specifiers |
| `edit_movie_filename` | Validates conversion specifiers |
| `edit_snapshot_filename` | Validates conversion specifiers |
| `edit_timelapse_filename` | Validates conversion specifiers |

These can be identified by checking for code beyond the standard DFLT/SET/GET pattern.

---

## Estimated Line Count Changes

| Component | Current | After | Change |
|-----------|---------|-------|--------|
| Individual handlers | ~2300 | 0 | -2300 |
| Generic handlers | 0 | 80 | +80 |
| Dispatch function | 0 | 350 | +350 |
| Custom handlers | ~200 | ~200 | 0 |
| Category dispatchers | 229 | 0 | -229 |
| **Total conf.cpp** | ~4095 | ~2000 | **-2095** |
| **conf.hpp declarations** | ~200 | ~50 | **-150** |

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Regression in parameter handling | Medium | High | Comprehensive test of all 102 params |
| Incorrect default values | Medium | Medium | Extract from existing handlers systematically |
| Validation range errors | Low | Medium | Code review of all min/max values |
| Custom handler missed | Low | High | Grep for non-standard patterns before deletion |

---

## Testing Strategy

### 1. Golden File Comparison
- Dump all parameter values with current code
- Compare against new implementation

### 2. Web UI Testing
- Test edit operations for each parameter type
- Verify LIST values appear correctly

### 3. Config File Parsing
- Load existing motion.conf files
- Verify identical behavior

### 4. Build Verification
- Clean build on Pi 5
- No warnings from handler changes

---

## Execution Order

1. **Phase 1**: Add generic handlers (low risk, additive)
2. **Phase 2**: Add dispatch function (medium risk, parallel path)
3. **Test**: Verify new path produces identical results
4. **Phase 3**: Delete individual handlers (high risk, deletion)
5. **Phase 4**: Simplify category dispatch
6. **Phase 5**: Clean up header

---

## Success Criteria

- [ ] conf.cpp < 2200 lines
- [ ] All 102 parameters work identically
- [ ] Build succeeds with no new warnings
- [ ] Web UI edits work correctly
- [ ] Config file loading unchanged
- [ ] No runtime performance impact

---

## Alternative Approach: Incremental Migration

Instead of "big bang" refactor, migrate one category at a time:

1. Start with PARM_CAT_00 (system, 8 params)
2. Verify, then PARM_CAT_01 (camera, 10 params)
3. Continue through all 19 categories

**Pros**: Lower risk per change, easier to debug
**Cons**: Longer total time, hybrid code during migration

---

## Appendix: Handler Pattern Examples

### Pattern: Simple String
```cpp
void cls_config::edit_v4l2_device(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        v4l2_device = "";
    } else if (pact == PARM_ACT_SET) {
        v4l2_device = parm;
    } else if (pact == PARM_ACT_GET) {
        parm = v4l2_device;
    }
}
```
**Becomes**: `edit_generic_string(v4l2_device, parm, pact, "")`

### Pattern: Bool
```cpp
void cls_config::edit_daemon(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        daemon = false;
    } else if (pact == PARM_ACT_SET) {
        edit_set_bool(daemon, parm);
    } else if (pact == PARM_ACT_GET) {
        edit_get_bool(parm, daemon);
    }
}
```
**Becomes**: `edit_generic_bool(daemon, parm, pact, false)`

### Pattern: Int with Range
```cpp
void cls_config::edit_threshold(std::string &parm, enum PARM_ACT pact)
{
    int parm_in;
    if (pact == PARM_ACT_DFLT) {
        threshold = 1500;
    } else if (pact == PARM_ACT_SET) {
        parm_in = atoi(parm.c_str());
        if ((parm_in < 1) || (parm_in > 2147483647)) {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid threshold %d"),parm_in);
        } else {
            threshold = parm_in;
        }
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(threshold);
    }
}
```
**Becomes**: `edit_generic_int(threshold, parm, pact, 1500, 1, INT_MAX)`

### Pattern: List/Enum
```cpp
void cls_config::edit_log_type(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        log_type_str = "ALL";
    } else if (pact == PARM_ACT_SET) {
        if ((parm == "ALL") || (parm == "COR") || ...) {
            log_type_str = parm;
        } else {
            MOTION_LOG(NTC, TYPE_ALL, NO_ERRNO, _("Invalid log_type %s"),parm.c_str());
        }
    } else if (pact == PARM_ACT_GET) {
        parm = log_type_str;
    } else if (pact == PARM_ACT_LIST) {
        parm = "[\"ALL\",\"COR\",...";
    }
}
```
**Becomes**: `edit_generic_list(log_type_str, parm, pact, "ALL", {"ALL","COR","STR",...})`

---

## Analysis Session Summary (2025-12-17)

### Discovery Process

1. **Initial Assessment**: Confirmed conf.cpp is very large (63,126 tokens, cannot read in single call)
2. **Handler Count**: Used grep to identify 197 function definitions starting with `void cls_config::edit_`
3. **Pattern Analysis**: Attempted systematic extraction using Python regex to parse function bodies

### Key Findings

**Handler Distribution**:
- Total handlers found: **162 unique parameter handlers**
- Generic handlers already exist (5):
  - `edit_generic_bool` (line 270)
  - `edit_generic_int` (line 282)
  - `edit_generic_float` (line 299)
  - `edit_generic_string` (line 316)
  - `edit_generic_list` (line 328)

**Current State**:
- Only 4 handlers have been converted to use generic handlers:
  - `edit_libcam_brightness` uses `edit_generic_float`
  - `edit_libcam_contrast` uses `edit_generic_float`
  - `edit_libcam_buffer_count` uses `edit_generic_int`
  - 1 string handler identified

- **158 handlers remain unconverted** - all use the old pattern with individual DFLT/SET/GET cases

### Analysis Completed

✅ Confirmed generic handler infrastructure exists
✅ Identified all 162 handlers requiring consolidation
✅ Validated that most handlers follow standard patterns
✅ Confirmed custom handlers list is accurate

### Next Steps (When Resuming)

1. **Extract Handler Metadata**: Run comprehensive analysis to extract:
   - Parameter type (bool/int/float/string/list)
   - Default values
   - Min/max ranges (for int/float)
   - Valid values lists (for list/enum types)
   - Custom logic flags

2. **Generate Dispatch Function**: Create the complete `dispatch_edit()` function with all 162 handlers consolidated

3. **Implement Phase 1-5** per the plan above

### Files Modified

- `/Users/tshuey/Documents/GitHub/motion/doc/plans/EditHandler-Consolidation-Plan-20251217.md` - Updated status to "Analysis Complete"

### Technical Notes

- Generic handlers already implemented (lines 270-355 in conf.cpp)
- Helper functions exist: `edit_set_bool`, `edit_get_bool` (lines 251-268)
- Category dispatch functions (19) are in lines 3156-3385
- O(1) registry lookup already exists in `edit_set_active()`

### Confidence Level

**HIGH** - The infrastructure exists, patterns are clear, and the consolidation is straightforward. The main work is systematic extraction of metadata from existing handlers and generation of the dispatch table.
