# Edit Handler Consolidation - Handoff Prompt

**Created**: 2025-12-17
**Plan Document**: `doc/plans/EditHandler-Consolidation-Plan-20251217.md`
**Target**: Reduce `src/conf.cpp` from ~4095 lines to ~2000 lines

---

## Context

You are continuing a configuration system refactoring for the Motion video surveillance project. The previous session:

1. Removed 36 deprecated parameters and their handlers (329 lines)
2. Created a detailed plan for consolidating edit handlers
3. Verified the build on Pi 5

Your task is to implement the edit handler consolidation plan.

---

## Objective

Consolidate ~175 individual `edit_XXX()` handler functions into 6 generic type-based handlers. Each individual handler follows one of ~5 patterns that can be replaced with generic handlers + inline metadata.

---

## Files to Modify

| File | Action |
|------|--------|
| `src/conf.cpp` | Add generic handlers, create dispatch function, remove individual handlers |
| `src/conf.hpp` | Remove ~150 private method declarations |

---

## Implementation Steps

### Step 1: Add Generic Handler Functions

Add these 6 generic handlers to `src/conf.cpp` (after `edit_get_bool`):

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
        bool valid = parm.empty();
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

Add declarations to `src/conf.hpp` private section.

### Step 2: Create Dispatch Function

Create a single `dispatch_edit()` function that routes all parameters to the appropriate generic handler. This replaces the 19 `edit_catXX()` functions.

**IMPORTANT**: To extract the correct default values, min/max ranges, and valid list values, you MUST read each existing `edit_XXX()` handler before deleting it.

Example extraction pattern:
```cpp
// From edit_threshold():
//   DFLT: threshold = 1500
//   Range check: (parm_in < 1) || (parm_in > 2147483647)
// Becomes:
if (name == "threshold") return edit_generic_int(threshold, parm, pact, 1500, 1, INT_MAX);
```

### Step 3: Identify Custom Handlers to Keep

These handlers have non-standard logic and CANNOT be replaced with generic handlers:

| Handler | Reason |
|---------|--------|
| `edit_log_file` | Uses strftime() for filename expansion |
| `edit_target_dir` | Trailing slash removal, % character validation |
| `edit_text_changes` | Complex validation |
| `edit_picture_filename` | Conversion specifier validation |
| `edit_movie_filename` | Conversion specifier validation |
| `edit_snapshot_filename` | Conversion specifier validation |
| `edit_timelapse_filename` | Conversion specifier validation |

Review each handler - if it has code beyond the standard DFLT/SET/GET pattern, it's custom.

### Step 4: Update Category Dispatch

Modify `edit_cat()` to use the new dispatch function instead of the 19 `edit_catXX()` functions.

### Step 5: Remove Individual Handlers

After verifying the new dispatch works, delete:
- All individual `edit_XXX()` handlers (except custom ones)
- All `edit_catXX()` functions
- Corresponding declarations in `conf.hpp`

### Step 6: Build and Test

```bash
# Sync to Pi 5
rsync -avz --exclude='.git' /Users/tshuey/Documents/GitHub/motion/ admin@192.168.1.176:~/motion/

# Build
ssh admin@192.168.1.176 "cd ~/motion && make -j4"

# Test run
ssh admin@192.168.1.176 "cd ~/motion/src && ./motion -h"
```

---

## Pattern Reference

### Simple String Pattern (~45 handlers)
```cpp
// Original:
void cls_config::edit_v4l2_device(std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) { v4l2_device = ""; }
    else if (pact == PARM_ACT_SET) { v4l2_device = parm; }
    else if (pact == PARM_ACT_GET) { parm = v4l2_device; }
}
// Becomes:
if (name == "v4l2_device") return edit_generic_string(v4l2_device, parm, pact, "");
```

### Bool Pattern (~25 handlers)
```cpp
// Original:
void cls_config::edit_daemon(std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) { daemon = false; }
    else if (pact == PARM_ACT_SET) { edit_set_bool(daemon, parm); }
    else if (pact == PARM_ACT_GET) { edit_get_bool(parm, daemon); }
}
// Becomes:
if (name == "daemon") return edit_generic_bool(daemon, parm, pact, false);
```

### Int with Range Pattern (~55 handlers)
```cpp
// Original:
void cls_config::edit_framerate(std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) { framerate = 15; }
    else if (pact == PARM_ACT_SET) {
        int val = atoi(parm.c_str());
        if ((val < 2) || (val > 100)) { MOTION_LOG(...); }
        else { framerate = val; }
    }
    else if (pact == PARM_ACT_GET) { parm = std::to_string(framerate); }
}
// Becomes:
if (name == "framerate") return edit_generic_int(framerate, parm, pact, 15, 2, 100);
```

### List/Enum Pattern (~20 handlers)
```cpp
// Original:
void cls_config::edit_log_type(std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) { log_type_str = "ALL"; }
    else if (pact == PARM_ACT_SET) {
        if ((parm == "ALL") || (parm == "COR") || ...) { log_type_str = parm; }
        else { MOTION_LOG(...); }
    }
    else if (pact == PARM_ACT_GET) { parm = log_type_str; }
    else if (pact == PARM_ACT_LIST) { parm = "[\"ALL\",\"COR\",..."; }
}
// Becomes:
static const std::vector<std::string> log_types = {"ALL","COR","STR","ENC","NET","DBL","EVT","TRK","VID"};
if (name == "log_type") return edit_generic_list(log_type_str, parm, pact, "ALL", log_types);
```

### Float Pattern (~5 handlers)
```cpp
// Original:
void cls_config::edit_libcam_brightness(std::string &parm, enum PARM_ACT pact) {
    if (pact == PARM_ACT_DFLT) { parm_cam.libcam_brightness = 0.0; }
    else if (pact == PARM_ACT_SET) {
        float val = atof(parm.c_str());
        if ((val < -1.0) || (val > 1.0)) { MOTION_LOG(...); }
        else { parm_cam.libcam_brightness = val; }
    }
    else if (pact == PARM_ACT_GET) { parm = std::to_string(parm_cam.libcam_brightness); }
}
// Becomes:
if (name == "libcam_brightness") return edit_generic_float(parm_cam.libcam_brightness, parm, pact, 0.0f, -1.0f, 1.0f);
```

---

## Validation Checklist

Before marking complete:

- [ ] Generic handlers added and compile
- [ ] Dispatch function routes all 102 parameters
- [ ] Custom handlers preserved for non-standard logic
- [ ] Individual handlers deleted
- [ ] `edit_catXX()` functions deleted
- [ ] `conf.hpp` declarations cleaned up
- [ ] Build succeeds on Pi 5
- [ ] `conf.cpp` < 2200 lines
- [ ] Motion runs and accepts config file

---

## Reference Files

- Plan: `doc/plans/EditHandler-Consolidation-Plan-20251217.md`
- Scratchpad: `doc/scratchpads/conf-cpp-refactor-strategy-20251217.md`
- Previous refactor plan: `doc/plans/ConfigParam-Refactor-20251211-1730.md`

---

## Notes

- The O(1) parameter lookup via `ctx_parm_registry` is already implemented
- Scoped parameter structs (`parm_app`, `parm_cam`, `parm_snd`) are already in place
- Member reference aliases ensure backward compatibility with `->cfg->member` access
- This is a code reduction effort, not a performance optimization
