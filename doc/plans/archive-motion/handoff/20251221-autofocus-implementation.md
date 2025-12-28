# Handoff Prompt: Autofocus Implementation

**Date:** 2025-12-21
**Plan:** `doc/plans/20251221-autofocus-implementation-plan.md`
**Analysis:** `doc/analysis/20251221-autofocus-analysis.md`

---

## Task

Implement the autofocus feature improvements for Motion's libcamera integration as specified in the implementation plan. This involves fixing bugs, adding configuration options, and implementing hot-reload support for AF controls.

---

## Context

Motion is a C++ video motion detection daemon. The libcamera integration (`src/libcam.cpp`) provides camera capture for Raspberry Pi cameras. The autofocus feature was partially implemented but has critical bugs and missing functionality.

**Key files:**
- `src/libcam.cpp` - libcamera capture implementation
- `src/libcam.hpp` - libcamera class interface
- `src/conf.cpp` - Configuration parameter definitions
- `src/parm_structs.hpp` - Parameter storage structures
- `src/camera.cpp` / `src/camera.hpp` - Camera class (wraps libcam)
- `src/webu_json.cpp` - Web API handlers for config changes

---

## Execution Instructions

Read the full implementation plan first:
```
Read doc/plans/20251221-autofocus-implementation-plan.md
```

Then execute each issue in order:

### Issue 1: Bug Fix - Read-Only Controls (CRITICAL)

1. Open `src/libcam.cpp`
2. Find lines 506-511 (AfState and AfPauseState setters)
3. Replace the setter code with warning logs as specified in the plan
4. Update `log_controls()` (lines 152-161) to mark these as READ-ONLY

**Verify:** The code should warn users who try to set AfState or AfPauseState via libcam_params.

### Issue 2: AF Initialization Defaults

1. Add AF fields to `ctx_parm_cam` in `src/parm_structs.hpp` (after line 148):
   - `libcam_af_mode` (int, default 0)
   - `libcam_lens_position` (float, default 0.0)
   - `libcam_af_range` (int, default 0)
   - `libcam_af_speed` (int, default 0)

2. Add config_parms entries in `src/conf.cpp` (after line 85) with `hot=true`

3. Add edit handlers in `src/conf.cpp` (after line 701)

4. Apply defaults in `config_controls()` in `src/libcam.cpp` (after line 562)

**Verify:** New config options are recognized and applied at startup.

### Issue 3: AfWindows Multi-Window Support

1. Replace the AfWindows parsing in `src/libcam.cpp` (lines 489-496)
2. Implement semicolon-separated window parsing (max 4 windows)
3. Update the log_controls() documentation for AfWindows format

**Verify:** Both single window and multiple windows work correctly.

### Issue 4: Documentation Fix

1. Already handled in Issue 3 (log_controls update)
2. Update `doc/motion_config.html` if AfWindows documentation exists there

**Verify:** Help text shows correct format: `x|y|width|height`

### Issue 5: Hot-Reload for AF Controls

This is the largest change. Follow the existing pattern used by brightness/contrast/AWB:

1. **libcam.hpp:** Add AF fields to `ctx_pending_controls` struct
2. **libcam.hpp:** Declare `set_af_mode()`, `set_lens_position()`, etc.
3. **libcam.cpp:** Implement the setter methods
4. **libcam.cpp:** Update `req_add()` to apply AF controls from pending_ctrls
5. **libcam.cpp:** Initialize pending AF controls in constructor
6. **camera.hpp:** Declare wrapper methods
7. **camera.cpp:** Implement wrapper methods
8. **webu_json.cpp:** Add handlers for AF parameters in `apply_config_change()`

**Verify:** AF mode and lens position can be changed via web API:
```bash
curl "http://localhost:8080/1/config/set?libcam_af_mode=2"
curl "http://localhost:8080/1/config/set?libcam_lens_position=0.5"
```

---

## Build Verification

After each issue (or all issues), verify the build compiles:

```bash
# On development machine (may need cross-compile setup)
autoreconf -fiv
./configure --with-libcam
make -j4

# Or on Pi 5 directly
ssh admin@192.168.1.176 "cd ~/motion && make clean && make -j4"
```

---

## Testing Checklist

- [ ] Build compiles without errors or warnings
- [ ] AfState/AfPauseState in libcam_params produces warning log
- [ ] Default AF mode is Manual (0) when no config specified
- [ ] `libcam_af_mode 2` enables continuous autofocus
- [ ] `libcam_lens_position 0.5` sets focus to 2m in manual mode
- [ ] Multiple AfWindows parse correctly
- [ ] Web API hot-reload works for AF controls
- [ ] Existing brightness/contrast/AWB hot-reload still works
- [ ] Backward compatibility: old libcam_params syntax still works

---

## Completion Requirements

When all issues are implemented and tested:

1. **Append a summary section to the plan** (`doc/plans/20251221-autofocus-implementation-plan.md`):

```markdown
---

## Implementation Summary

**Completed:** YYYY-MM-DD
**Implementer:** [Claude Code]

### Changes Made

[List files modified and key changes]

### Testing Results

[Document what was tested and results]

### Notes

[Any deviations from plan, issues encountered, or follow-up items]
```

2. **Commit the changes** (if requested by user):
```
git add -A
git commit -m "Implement autofocus improvements for libcamera

- Fix bug: AfState/AfPauseState are read-only controls
- Add default AF mode (Manual) and lens position config
- Support multiple AfWindows (up to 4)
- Add hot-reload for AF controls via web API
- Update documentation for correct AfWindows format

See doc/plans/20251221-autofocus-implementation-plan.md"
```

---

## Reference: Existing Hot-Reload Pattern

Study these existing implementations as reference:

**Config definition (src/conf.cpp:79-85):**
```cpp
{"libcam_brightness",         PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
```

**Pending controls struct (src/libcam.hpp:44-56):**
```cpp
struct ctx_pending_controls {
    float brightness = 0.0f;
    // ... etc
    std::atomic<bool> dirty{false};
};
```

**Setter method (src/libcam.cpp:1020-1030):**
```cpp
void cls_libcam::set_brightness(float value)
{
    #ifdef HAVE_LIBCAM
        pending_ctrls.brightness = value;
        pending_ctrls.dirty.store(true);
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , "Hot-reload: brightness set to %.2f", value);
    #else
        (void)value;
    #endif
}
```

**Web handler (src/webu_json.cpp:592-593):**
```cpp
if (parm_name == "libcam_brightness") {
    app->cam_list[indx]->set_libcam_brightness(atof(parm_val.c_str()));
}
```

---

## Important Notes

- **Do not modify `src/alg.cpp`** - core motion detection algorithm is proven
- Use `#ifdef HAVE_LIBCAM` guards for all libcamera-specific code
- Follow existing code patterns and naming conventions
- The `mtoi()`, `mtof()`, `mtok()` functions are Motion's parsing utilities
- libcamera uses `controls::AfMode`, `controls::LensPosition`, etc.
