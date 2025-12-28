# AWB Hot-Reload Implementation Notes

## Implementation Plan
Following: `doc/plans/20251220-AWB-HotReload-Implementation.md`

## Parameters Being Added
1. `libcam_awb_enable` (bool) - default: true
2. `libcam_awb_mode` (int 0-7) - default: 0 (Auto)
3. `libcam_awb_locked` (bool) - default: false
4. `libcam_colour_temp` (int 0-10000) - default: 0 (disabled)
5. `libcam_colour_gain_r` (float 0.0-8.0) - default: 0.0 (auto)
6. `libcam_colour_gain_b` (float 0.0-8.0) - default: 0.0 (auto)

## Progress Tracking

### Step 1: parm_structs.hpp ✅
- [x] Add 6 struct members after libcam_iso (lines 143-148)

### Step 2: conf.cpp ✅
- [x] Add 6 parameter definitions to config_parms[] (lines 82-87)
- [x] Add 6 edit handlers to dispatch_edit() (lines 695-705)

### Step 3: libcam.hpp ✅
- [x] Extend ctx_pending_controls struct with 6 AWB fields (lines 48-54)
- [x] Add 5 setter method declarations (lines 67-71)

### Step 4: libcam.cpp ✅
- [x] Implement 5 setter methods (lines 1024-1083)
- [x] Extend req_add() to apply AWB controls (lines 729-743)
- [x] Initialize pending controls in constructor (lines 1225-1230)

### Step 5: camera.hpp + camera.cpp ✅
- [x] Add 5 wrapper method declarations (camera.hpp lines 210-214)
- [x] Implement 5 wrapper methods (camera.cpp lines 2108-2141)

### Step 6: webu_json.cpp ✅
- [x] Add hot-reload handlers for all-cameras path (lines 598-614)
- [x] Add hot-reload handlers for single-camera path (lines 627-643)

### Step 7: Testing 🔄
- [ ] Build verification (requires Raspberry Pi environment)
- [ ] Config file test
- [ ] Hot-reload test via web API
- [ ] Visual verification

## Implementation Complete

All code changes implemented successfully. Testing requires Raspberry Pi 5 with libcamera dependencies.

## Notes
- Using libcamera native AWB mode ordering (0-7)
- No mode conversion needed
- ColourGains sent together when either changes
- Value 0 for colour_temp/gains means "don't apply"
