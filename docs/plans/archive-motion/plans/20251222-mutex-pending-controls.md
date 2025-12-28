# Mutex Protection for Pending Controls

**Date**: 2025-12-22
**Status**: Draft
**Problem**: Race condition in hot-reload controls causing intermittent update failures (especially AWB)

## Problem Summary

The `ctx_pending_controls` struct in `libcam.hpp` is accessed by two threads:
- **Web thread**: Writes control values via `set_*()` methods
- **Camera thread**: Reads values in `req_add()` at 30fps

Current implementation uses only an atomic `dirty` flag, but individual struct members are not protected. This causes race conditions where:
1. Partial updates are read (some new values, some stale)
2. Updates are lost when dirty flag is cleared mid-write
3. Interdependent controls (AWB mode/enable/locked) become inconsistent

## Solution

Add `std::mutex` to protect all reads/writes to `ctx_pending_controls`.

## Files to Modify

| File | Changes |
|------|---------|
| `src/libcam.hpp` | Add mutex to struct, remove atomic from dirty flag |
| `src/libcam.cpp` | Add lock_guard to all setters and req_add() |

## Implementation Steps

### Step 1: Update libcam.hpp

**Location**: `src/libcam.hpp:44-64`

```cpp
// BEFORE
struct ctx_pending_controls {
    float brightness = 0.0f;
    float contrast = 1.0f;
    float iso = 100.0f;
    bool awb_enable = true;
    int awb_mode = 0;
    bool awb_locked = false;
    int colour_temp = 0;
    float colour_gain_r = 0.0f;
    float colour_gain_b = 0.0f;
    int af_mode = 0;
    float lens_position = 0.0f;
    int af_range = 0;
    int af_speed = 0;
    bool af_trigger = false;
    bool af_cancel = false;
    std::atomic<bool> dirty{false};
};

// AFTER
struct ctx_pending_controls {
    mutable std::mutex mtx;           // Protects all members below
    float brightness = 0.0f;
    float contrast = 1.0f;
    float iso = 100.0f;
    bool awb_enable = true;
    int awb_mode = 0;
    bool awb_locked = false;
    int colour_temp = 0;
    float colour_gain_r = 0.0f;
    float colour_gain_b = 0.0f;
    int af_mode = 0;
    float lens_position = 0.0f;
    int af_range = 0;
    int af_speed = 0;
    bool af_trigger = false;
    bool af_cancel = false;
    bool dirty = false;               // No longer atomic
};
```

**Note**: `mutable` allows locking in const contexts if needed.

**Include required**: Verify `<mutex>` is included (already have `<atomic>` on line 26).

### Step 2: Update Setter Methods

All setters in `libcam.cpp:1290-1478` need lock_guard added.

**Pattern for each setter**:

```cpp
void cls_libcam::set_brightness(float value)
{
    #ifdef HAVE_LIBCAM
        std::lock_guard<std::mutex> lock(pending_ctrls.mtx);
        pending_ctrls.brightness = value;
        pending_ctrls.dirty = true;
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , "Hot-reload: brightness set to %.2f", value);
    #else
        (void)value;
    #endif
}
```

**Setters to update** (11 total):
- [ ] `set_brightness()` - line 1290
- [ ] `set_contrast()` - line 1302
- [ ] `set_iso()` - line 1314
- [ ] `set_awb_enable()` - line 1326
- [ ] `set_awb_mode()` - line 1338
- [ ] `set_awb_locked()` - line 1356
- [ ] `set_colour_temp()` - line 1368
- [ ] `set_colour_gains()` - line 1385
- [ ] `set_af_mode()` - line 1402
- [ ] `set_lens_position()` - line 1416
- [ ] `set_af_range()` - line 1429
- [ ] `set_af_speed()` - line 1443
- [ ] `trigger_af_scan()` - line 1456
- [ ] `cancel_af_scan()` - line 1468

### Step 3: Update req_add() Reader

**Location**: `src/libcam.cpp:957-1039`

Wrap the dirty check and all control reads in a single lock scope:

```cpp
int cls_libcam::req_add(Request *request)
{
    int retcd;

    /* Apply pending controls under lock */
    {
        std::lock_guard<std::mutex> lock(pending_ctrls.mtx);
        if (pending_ctrls.dirty) {
            ControlList &req_controls = request->controls();

            req_controls.set(controls::Brightness, pending_ctrls.brightness);
            req_controls.set(controls::Contrast, pending_ctrls.contrast);
            req_controls.set(controls::AnalogueGain, iso_to_gain(pending_ctrls.iso));

            req_controls.set(controls::AwbEnable, pending_ctrls.awb_enable);
            req_controls.set(controls::AwbMode, pending_ctrls.awb_mode);
            req_controls.set(controls::AwbLocked, pending_ctrls.awb_locked);

            if (!pending_ctrls.awb_enable) {
                if (pending_ctrls.colour_temp > 0) {
                    req_controls.set(controls::ColourTemperature, pending_ctrls.colour_temp);
                }
                if (pending_ctrls.colour_gain_r > 0.0f || pending_ctrls.colour_gain_b > 0.0f) {
                    float cg[2] = {pending_ctrls.colour_gain_r, pending_ctrls.colour_gain_b};
                    req_controls.set(controls::ColourGains, cg);
                }
            }

            if (is_control_supported(&controls::AfMode)) {
                req_controls.set(controls::AfMode, pending_ctrls.af_mode);
            }
            if (is_control_supported(&controls::AfRange)) {
                req_controls.set(controls::AfRange, pending_ctrls.af_range);
            }
            if (is_control_supported(&controls::AfSpeed)) {
                req_controls.set(controls::AfSpeed, pending_ctrls.af_speed);
            }

            if (pending_ctrls.af_mode == 0 && is_control_supported(&controls::LensPosition)) {
                req_controls.set(controls::LensPosition, pending_ctrls.lens_position);
            }

            if (pending_ctrls.af_trigger && is_control_supported(&controls::AfTrigger)) {
                req_controls.set(controls::AfTrigger, controls::AfTriggerStart);
                pending_ctrls.af_trigger = false;
            }

            if (pending_ctrls.af_cancel && is_control_supported(&controls::AfTrigger)) {
                req_controls.set(controls::AfTrigger, controls::AfTriggerCancel);
                pending_ctrls.af_cancel = false;
            }

            pending_ctrls.dirty = false;

            MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
                , "Applied controls: brightness=%.2f, contrast=%.2f, iso=%.0f, "
                  "awb_enable=%s, awb_mode=%d, af_mode=%d, lens_pos=%.2f"
                , pending_ctrls.brightness, pending_ctrls.contrast
                , pending_ctrls.iso
                , pending_ctrls.awb_enable ? "true" : "false"
                , pending_ctrls.awb_mode
                , pending_ctrls.af_mode
                , pending_ctrls.lens_position);
        }
    }  // lock released here

    retcd = camera->queueRequest(request);
    return retcd;
}
```

### Step 4: Update apply_pending_controls()

**Location**: `src/libcam.cpp:1480-1494`

This function just logs pending state. Add lock:

```cpp
void cls_libcam::apply_pending_controls()
{
    #ifdef HAVE_LIBCAM
        std::lock_guard<std::mutex> lock(pending_ctrls.mtx);
        if (pending_ctrls.dirty) {
            MOTION_LOG(INF, TYPE_VIDEO, NO_ERRNO
                , "Brightness/Contrast/ISO update pending: brightness=%.2f, contrast=%.2f, iso=%.0f"
                , pending_ctrls.brightness, pending_ctrls.contrast, pending_ctrls.iso);
        }
    #endif
}
```

### Step 5: Constructor Initialization

**Location**: `src/libcam.cpp:1599-1615`

The constructor initializes pending_ctrls. This runs before any other thread accesses it, so no lock needed. However, verify the struct is fully initialized:

```cpp
// In constructor - no lock needed (single-threaded at this point)
pending_ctrls.brightness = cam->cfg->parm_cam.libcam_brightness;
pending_ctrls.contrast = cam->cfg->parm_cam.libcam_contrast;
pending_ctrls.iso = cam->cfg->parm_cam.libcam_iso;
pending_ctrls.awb_enable = cam->cfg->parm_cam.libcam_awb_enable;
pending_ctrls.awb_mode = cam->cfg->parm_cam.libcam_awb_mode;
pending_ctrls.awb_locked = cam->cfg->parm_cam.libcam_awb_locked;
pending_ctrls.colour_temp = cam->cfg->parm_cam.libcam_colour_temp;
pending_ctrls.colour_gain_r = cam->cfg->parm_cam.libcam_colour_gain_r;
pending_ctrls.colour_gain_b = cam->cfg->parm_cam.libcam_colour_gain_b;
pending_ctrls.af_mode = cam->cfg->parm_cam.libcam_af_mode;
pending_ctrls.lens_position = cam->cfg->parm_cam.libcam_lens_position;
pending_ctrls.af_range = cam->cfg->parm_cam.libcam_af_range;
pending_ctrls.af_speed = cam->cfg->parm_cam.libcam_af_speed;
pending_ctrls.af_trigger = false;
pending_ctrls.af_cancel = false;
pending_ctrls.dirty = false;  // Changed from .store(false)
```

## Testing Plan

### Unit Test Scenarios

1. **Basic hot-reload**: Change brightness via web API, verify applied within 2 frames
2. **Rapid changes**: Change brightness 10 times in 100ms, verify final value applied
3. **AWB mode switch**: Change awb_mode from 0→5, verify colour_temp/gains cleared
4. **Interdependent**: Disable AWB then set colour_temp, verify both applied together
5. **Concurrent stress**: Hammer setter from web thread while camera runs at 30fps

### Manual Test on Pi 5

1. Start motion with camera streaming
2. Open web interface, adjust brightness slider
3. Verify stream brightness changes within ~100ms
4. Adjust AWB mode dropdown
5. Verify white balance changes consistently
6. Rapidly toggle AWB enable on/off
7. Verify no missed updates or stuck states

## Rollback Plan

If issues arise, revert to atomic dirty flag:
1. Remove mutex from struct
2. Restore `std::atomic<bool> dirty{false}`
3. Restore `.load()` and `.store()` calls

## Performance Impact

| Metric | Before | After | Notes |
|--------|--------|-------|-------|
| Lock overhead | 0 | ~25ns/frame | Uncontended lock |
| Memory | 1 byte (atomic bool) | ~40 bytes (mutex) | Platform-dependent |
| Contention | N/A | ~0.001% | User updates rare vs 30fps |

**Expected CPU impact**: Negligible (<0.01% increase)

## Checklist

- [ ] Add `#include <mutex>` if not present
- [ ] Add `mutable std::mutex mtx` to struct
- [ ] Change `std::atomic<bool> dirty` to `bool dirty`
- [ ] Update all 14 setter methods with lock_guard
- [ ] Update `req_add()` with lock_guard scope
- [ ] Update `apply_pending_controls()` with lock_guard
- [ ] Update constructor (remove .store() calls)
- [ ] Build and verify no compile errors
- [ ] Test on Pi 5 with actual camera
- [ ] Verify AWB changes apply consistently
