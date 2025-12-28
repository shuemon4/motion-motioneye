# AWB Mode Hot-Reload Issue Analysis

**Date:** 2025-12-21
**Status:** Root cause identified, fix required in Motion
**Severity:** Medium - AWB Mode control silently fails on IMX708 sensor

## Executive Summary

The `libcam_awb_mode` hot-reload parameter appears to succeed (returns success to MotionEye) but has no effect on the video stream. Investigation revealed that the IMX708 sensor (Pi Camera v3) does not expose the `controls::AwbMode` control through libcamera's per-request ControlList interface.

## Symptoms

1. User changes AWB Mode in MotionEye UI
2. MotionEye sends hot-reload request to Motion
3. Motion returns success (green checkmark in UI)
4. **Video stream color balance does NOT change**
5. Other AWB controls (AwbEnable, ColourGains, ColourTemperature) DO work

## Root Cause

### Error in Motion Logs

```
/var/log/motioneye/motion.log:
[ERROR] Controls controls.cpp:1157 Control 0x00000013 is not valid for /base/axi/pcie@1000120000/rp1/i2c@88000/imx708@1a
```

- `0x00000013` (decimal 19) = `controls::AwbMode`
- The IMX708 sensor's libcamera driver does not include AwbMode in its ControlInfoMap
- libcamera silently rejects the control setting

### Code Location

**File:** `src/libcam.cpp`
**Function:** `cls_libcam::req_add()` (lines 718-755)

```cpp
int cls_libcam::req_add(Request *request)
{
    int retcd;

    /* Apply pending brightness/contrast/ISO/AWB controls if changed */
    if (pending_ctrls.dirty.load()) {
        ControlList &req_controls = request->controls();
        req_controls.set(controls::Brightness, pending_ctrls.brightness);
        req_controls.set(controls::Contrast, pending_ctrls.contrast);
        req_controls.set(controls::AnalogueGain, iso_to_gain(pending_ctrls.iso));

        // Apply AWB controls
        req_controls.set(controls::AwbEnable, pending_ctrls.awb_enable);
        req_controls.set(controls::AwbMode, pending_ctrls.awb_mode);  // <-- FAILS SILENTLY
        req_controls.set(controls::AwbLocked, pending_ctrls.awb_locked);
        // ...
    }
    // ...
}
```

The code unconditionally calls `req_controls.set(controls::AwbMode, ...)` without checking if the control is supported by the camera.

## Technical Details

### libcamera Control Validation

libcamera validates controls against the camera's `ControlInfoMap`. When a control is not in the map:
1. libcamera logs an error: "Control 0x00000013 is not valid"
2. The control setting is **silently ignored**
3. The request still succeeds
4. Motion returns success to MotionEye

### Why Other AWB Controls Work

| Control | ID | Works on IMX708 | Notes |
|---------|-----|-----------------|-------|
| AwbEnable | 0x0011 | Yes | Enables/disables auto white balance |
| AwbMode | 0x0013 | **No** | Not in IMX708's ControlInfoMap |
| AwbLocked | 0x0012 | Unknown | Not tested |
| ColourGains | 0x0017 | Yes | Manual red/blue gains |
| ColourTemperature | 0x0018 | Partial | "AWB uncalibrated" warning |

### rpicam-apps Comparison

The official Raspberry Pi camera tools (`rpicam-still`, `rpicam-vid`) DO support AWB mode:

```bash
rpicam-still --help 2>&1 | grep awb
  --awb arg (=auto)    Set the AWB mode (auto, incandescent, tungsten, fluorescent, indoor, daylight, cloudy, custom)
```

This suggests AWB mode IS supported by the IMX708, but through a different mechanism than `controls::AwbMode` on per-request controls.

## Proposed Fix

### Option 1: Check Control Support Before Setting (Recommended)

Add control support checking to Motion:

1. Store supported controls during camera initialization
2. Check before setting each control
3. Log warning once if control not supported

```cpp
// In cls_libcam class (libcam.hpp)
private:
    bool supports_awb_mode = false;
    bool supports_awb_locked = false;
    // etc.

// During camera start (libcam.cpp)
void cls_libcam::check_control_support()
{
    const ControlInfoMap &ctrl_info = camera->controls();
    supports_awb_mode = ctrl_info.find(&controls::AwbMode) != ctrl_info.end();
    if (!supports_awb_mode) {
        MOTION_LOG(WRN, TYPE_VIDEO, NO_ERRNO,
            "Camera does not support AwbMode control - AWB mode changes will be ignored");
    }
}

// In req_add()
if (supports_awb_mode) {
    req_controls.set(controls::AwbMode, pending_ctrls.awb_mode);
}
```

### Option 2: Set AWB Mode at Configuration Time

Some controls need to be set during camera configuration, not per-request. Investigate if AwbMode should be set via:
- `config->at(0).controls` during configuration
- Or through the initial ControlList before `camera->start()`

### Option 3: Use libcamera IPA Properties

The RPi IPA (Image Processing Algorithm) may handle AWB mode internally. Research if there's an IPA-specific way to set AWB mode that rpicam-apps uses.

## Additional Issue Found

The motion log also shows:
```
[WARN] RPiAwb awb.cpp:298 AWB uncalibrated - cannot set colour temperature
```

This suggests the IMX708 sensor's AWB calibration data may be incomplete, affecting ColourTemperature control as well.

## Files to Modify

1. **`src/libcam.hpp`** - Add member variables for control support flags
2. **`src/libcam.cpp`** - Add control support checking and conditional setting

## Testing Checklist

After implementing fix:

- [ ] Motion logs warning about unsupported AwbMode on startup (not every frame)
- [ ] Hot-reload of AwbMode returns appropriate response (success=false or warning)
- [ ] Other AWB controls (AwbEnable, ColourGains) continue to work
- [ ] No error spam in motion.log
- [ ] Test on both IMX708 (Pi Camera v3) and older sensors if available

## References

- libcamera controls: https://libcamera.org/api-html/controls_8h.html
- RPi camera documentation: https://www.raspberrypi.com/documentation/computers/camera_software.html
- Motion libcam source: `src/libcam.cpp`, `src/libcam.hpp`

## Related MotionEye Changes

A separate fix was applied to MotionEye to stop generating `libcam_control_item` for AWB settings, ensuring AWB uses only the dedicated `libcam_awb_*` parameters which are hot-reloadable:

- **File:** `motioneye/config/camera/converters.py`
- **Change:** Removed AWB entries from `libcam_control_item` generation
- **Result:** AWB changes no longer force Motion restart
