# Brightness/Contrast Hot-Reload Implementation Scratchpad

## Implementation Date
2025-12-14

## Goal
Add libcam_brightness and libcam_contrast as hot-reloadable parameters for runtime adjustment without camera restart.

## Design Summary
- Option A: Dedicated float parameters (libcam_brightness, libcam_contrast)
- Runtime path: Dirty flag check in next() → setControls() only when changed
- Startup path: Apply initial values alongside existing libcam_params controls

## Default Values
- libcam_brightness: 0.0 (neutral, range -1.0 to 1.0)
- libcam_contrast: 1.0 (neutral, range ~0.0 to 32.0)

## Implementation Progress

### Step 1: Configuration Surface ✓ COMPLETE
- [x] Add to config_parms[] in conf.cpp (PARM_CAT_02, hot_reload=true)
- [x] Add to ctx_parm_cam in parm_structs.hpp (float fields)
- [x] Add edit_libcam_brightness() and edit_libcam_contrast() handlers
- [x] Add declarations in conf.hpp
- [x] Wire into edit_cat02() dispatch in conf.cpp

### Step 2: Runtime Control Path ✓ COMPLETE
- [x] Add ctx_pending_controls struct to libcam.hpp (brightness, contrast, std::atomic<bool> dirty)
- [x] Add pending_ctrls member to cls_libcam private section
- [x] Add apply_pending_controls() private method to cls_libcam
- [x] Add set_brightness() and set_contrast() public methods
- [x] Implement dirty flag check in next() - calls apply_pending_controls()

### Step 3: Startup Integration ✓ COMPLETE
- [x] Initialize pending_ctrls in constructor with config values
- [x] Seed initial brightness/contrast in config_controls() via controls.set()
- [x] Defaults: brightness=0.0, contrast=1.0 (libcamera neutral values)

### Step 4: Web API Integration ✓ COMPLETE
- [x] Modified apply_hot_reload() in webu_json.cpp
- [x] Calls libcam->set_brightness()/set_contrast() when parameters updated
- [x] Handles both device_id=0 (all cameras) and specific camera cases

### Step 5: Configuration Template ✓ COMPLETE
- [x] Add libcam_brightness to motion-dist.conf.in (commented out, default 0.0)
- [x] Add libcam_contrast to motion-dist.conf.in (commented out, default 1.0)

## Code Locations
- Config params: src/conf.cpp:44-240 (config_parms array)
- Param storage: src/parm_structs.hpp:116-261 (ctx_parm_cam)
- Edit handlers: src/conf.cpp:964-987 (libcam edit functions)
- Edit dispatch: src/conf.cpp:3292-3293 (edit_set/edit_get switch)
- libcam class: src/libcam.hpp:42-89
- Control application: src/libcam.cpp:534-556 (config_controls)
- Frame loop: src/libcam.cpp:995-1049 (next function)
- Config template: data/motion-dist.conf.in

## Threading Notes
- Web thread: Sets brightness/contrast via setter methods
- Camera thread: Reads dirty flag in next(), calls setControls()
- Synchronization: std::atomic<bool> for dirty flag (lock-free)

## CPU Impact Analysis
- Per-frame cost: 1 branch check (~3ns)
- On change cost: ~10-50μs for setControls() IPC
- Comparison: Frame copy ~1-2ms, motion detect ~5-15ms
- Verdict: Negligible (<0.01% of frame time)

## Questions/Issues
- None currently

## Testing Plan
1. Build with changes
2. Start motion daemon with defaults
3. Issue /config/set?libcam_brightness=0.2
4. Issue /config/set?libcam_contrast=1.4
5. Verify JSON response shows hot_reload=true
6. Verify image brightness/contrast changes without restart
7. Monitor CPU usage via top/htop (should be stable)

## Notes
- Following K&R style from doc/code_standard
- Using 4-space indentation, no tabs
- Braces on all if blocks
- Comments for non-obvious logic
