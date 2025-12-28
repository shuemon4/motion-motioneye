# AWB Hot-Reload Implementation Plan

## Overview

Add hot-reload support for all AWB (Auto White Balance) controls, matching the existing pattern for brightness/contrast/ISO. This enables runtime changes via web API without camera restart.

## AWB Controls to Implement

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `libcam_awb_enable` | bool | true | true/false | Enable/disable auto white balance |
| `libcam_awb_mode` | int | 0 | 0-7 | AWB mode (libcamera native ordering) |
| `libcam_awb_locked` | bool | false | true/false | Lock current white balance values |
| `libcam_colour_temp` | int | 0 | 0-10000 | Manual colour temperature in Kelvin (0=disabled) |
| `libcam_colour_gain_r` | float | 0.0 | 0.0-8.0 | Manual red channel gain (0=auto) |
| `libcam_colour_gain_b` | float | 0.0 | 0.0-8.0 | Manual blue channel gain (0=auto) |

### AWB Mode Values (libcamera native)

| Value | Name | Description |
|-------|------|-------------|
| 0 | Auto | Automatic white balance |
| 1 | Incandescent | Warm/tungsten lighting (~2700K) |
| 2 | Tungsten | Similar to incandescent |
| 3 | Fluorescent | Cool white fluorescent (~4000K) |
| 4 | Indoor | General indoor lighting |
| 5 | Daylight | Outdoor daylight (~5500K) |
| 6 | Cloudy | Overcast/cloudy (~6500K) |
| 7 | Custom | Use manual ColourGains/ColourTemperature |

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/parm_structs.hpp` | Add AWB struct members (~6 lines) |
| `src/conf.cpp` | Add parameter definitions + edit handlers (~20 lines) |
| `src/libcam.hpp` | Extend `ctx_pending_controls` struct + add setter declarations (~12 lines) |
| `src/libcam.cpp` | Add setter methods + apply logic (~60 lines) |
| `src/camera.hpp` | Add wrapper method declarations (~6 lines) |
| `src/camera.cpp` | Add wrapper method implementations (~30 lines) |
| `src/webu_json.cpp` | Add hot-reload handlers (~18 lines) |

---

## Implementation Steps

### Step 1: Add struct members to `parm_structs.hpp`

Add after line 142 (after `libcam_iso`):

```cpp
bool            libcam_awb_enable;
int             libcam_awb_mode;
bool            libcam_awb_locked;
int             libcam_colour_temp;
float           libcam_colour_gain_r;
float           libcam_colour_gain_b;
```

### Step 2: Add parameter definitions to `src/conf.cpp`

Add to `config_parms[]` array (after line 81, after `libcam_iso`):

```cpp
{"libcam_awb_enable",         PARM_TYP_BOOL,   PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
{"libcam_awb_mode",           PARM_TYP_INT,    PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
{"libcam_awb_locked",         PARM_TYP_BOOL,   PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
{"libcam_colour_temp",        PARM_TYP_INT,    PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
{"libcam_colour_gain_r",      PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
{"libcam_colour_gain_b",      PARM_TYP_STRING, PARM_CAT_02, PARM_LEVEL_ADVANCED, true},
```

Add edit handlers in `dispatch_edit()` (after line 691, after `libcam_iso`):

```cpp
if (name == "libcam_awb_enable") return edit_generic_bool(parm_cam.libcam_awb_enable, parm, pact, true);
if (name == "libcam_awb_mode") return edit_generic_int(parm_cam.libcam_awb_mode, parm, pact, 0, 0, 7);
if (name == "libcam_awb_locked") return edit_generic_bool(parm_cam.libcam_awb_locked, parm, pact, false);
if (name == "libcam_colour_temp") return edit_generic_int(parm_cam.libcam_colour_temp, parm, pact, 0, 0, 10000);
if (name == "libcam_colour_gain_r") return edit_generic_float(parm_cam.libcam_colour_gain_r, parm, pact, 0.0f, 0.0f, 8.0f);
if (name == "libcam_colour_gain_b") return edit_generic_float(parm_cam.libcam_colour_gain_b, parm, pact, 0.0f, 0.0f, 8.0f);
```

### Step 3: Extend `ctx_pending_controls` in `src/libcam.hpp`

Modify struct (lines 43-49):

```cpp
struct ctx_pending_controls {
    float brightness = 0.0f;
    float contrast = 1.0f;
    float iso = 100.0f;
    // AWB controls
    bool awb_enable = true;
    int awb_mode = 0;           // libcamera native: 0=Auto
    bool awb_locked = false;
    int colour_temp = 0;        // 0 = disabled
    float colour_gain_r = 0.0f; // 0 = auto
    float colour_gain_b = 0.0f; // 0 = auto
    std::atomic<bool> dirty{false};
};
```

Add setter declarations to `cls_libcam` public section (after line 59):

```cpp
void set_awb_enable(bool value);
void set_awb_mode(int value);
void set_awb_locked(bool value);
void set_colour_temp(int value);
void set_colour_gains(float red, float blue);
```

### Step 4: Implement setters and apply logic in `src/libcam.cpp`

Add setter methods (after `set_iso`, around line 1022):

```cpp
void cls_libcam::set_awb_enable(bool value)
{
    #ifdef HAVE_LIBCAM
        pending_ctrls.awb_enable = value;
        pending_ctrls.dirty.store(true);
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , "Hot-reload: AWB enable set to %s", value ? "true" : "false");
    #else
        (void)value;
    #endif
}

void cls_libcam::set_awb_mode(int value)
{
    #ifdef HAVE_LIBCAM
        pending_ctrls.awb_mode = value;
        pending_ctrls.dirty.store(true);
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , "Hot-reload: AWB mode set to %d", value);
    #else
        (void)value;
    #endif
}

void cls_libcam::set_awb_locked(bool value)
{
    #ifdef HAVE_LIBCAM
        pending_ctrls.awb_locked = value;
        pending_ctrls.dirty.store(true);
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , "Hot-reload: AWB locked set to %s", value ? "true" : "false");
    #else
        (void)value;
    #endif
}

void cls_libcam::set_colour_temp(int value)
{
    #ifdef HAVE_LIBCAM
        pending_ctrls.colour_temp = value;
        pending_ctrls.dirty.store(true);
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , "Hot-reload: Colour temperature set to %dK", value);
    #else
        (void)value;
    #endif
}

void cls_libcam::set_colour_gains(float red, float blue)
{
    #ifdef HAVE_LIBCAM
        pending_ctrls.colour_gain_r = red;
        pending_ctrls.colour_gain_b = blue;
        pending_ctrls.dirty.store(true);
        MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
            , "Hot-reload: Colour gains set to R=%.2f, B=%.2f", red, blue);
    #else
        (void)red; (void)blue;
    #endif
}
```

Extend `req_add()` to apply AWB controls (after existing controls, around line 729):

```cpp
// Apply AWB controls
req_controls.set(controls::AwbEnable, pending_ctrls.awb_enable);
req_controls.set(controls::AwbMode, pending_ctrls.awb_mode);
req_controls.set(controls::AwbLocked, pending_ctrls.awb_locked);

// Apply manual colour temperature if set (non-zero)
if (pending_ctrls.colour_temp > 0) {
    req_controls.set(controls::ColourTemperature, pending_ctrls.colour_temp);
}

// Apply manual colour gains if set (non-zero)
if (pending_ctrls.colour_gain_r > 0.0f || pending_ctrls.colour_gain_b > 0.0f) {
    float cg[2] = {pending_ctrls.colour_gain_r, pending_ctrls.colour_gain_b};
    req_controls.set(controls::ColourGains, cg);
}
```

Update initialization in constructor (around line 1146):

```cpp
pending_ctrls.awb_enable = cam->cfg->parm_cam.libcam_awb_enable;
pending_ctrls.awb_mode = cam->cfg->parm_cam.libcam_awb_mode;
pending_ctrls.awb_locked = cam->cfg->parm_cam.libcam_awb_locked;
pending_ctrls.colour_temp = cam->cfg->parm_cam.libcam_colour_temp;
pending_ctrls.colour_gain_r = cam->cfg->parm_cam.libcam_colour_gain_r;
pending_ctrls.colour_gain_b = cam->cfg->parm_cam.libcam_colour_gain_b;
```

### Step 5: Add wrapper methods to `src/camera.hpp`

Add declarations (after line 209):

```cpp
void set_libcam_awb_enable(bool value);
void set_libcam_awb_mode(int value);
void set_libcam_awb_locked(bool value);
void set_libcam_colour_temp(int value);
void set_libcam_colour_gains(float red, float blue);
```

### Step 6: Implement wrappers in `src/camera.cpp`

Add after existing libcam setters (around line 2106):

```cpp
void cls_camera::set_libcam_awb_enable(bool value)
{
    if (libcam != nullptr) {
        libcam->set_awb_enable(value);
    }
}

void cls_camera::set_libcam_awb_mode(int value)
{
    if (libcam != nullptr) {
        libcam->set_awb_mode(value);
    }
}

void cls_camera::set_libcam_awb_locked(bool value)
{
    if (libcam != nullptr) {
        libcam->set_awb_locked(value);
    }
}

void cls_camera::set_libcam_colour_temp(int value)
{
    if (libcam != nullptr) {
        libcam->set_colour_temp(value);
    }
}

void cls_camera::set_libcam_colour_gains(float red, float blue)
{
    if (libcam != nullptr) {
        libcam->set_colour_gains(red, blue);
    }
}
```

### Step 7: Add web API handlers to `src/webu_json.cpp`

Extend hot-reload section (after line 597/610):

```cpp
} else if (parm_name == "libcam_awb_enable") {
    app->cam_list[indx]->set_libcam_awb_enable(parm_val == "true" || parm_val == "1");
} else if (parm_name == "libcam_awb_mode") {
    app->cam_list[indx]->set_libcam_awb_mode(atoi(parm_val.c_str()));
} else if (parm_name == "libcam_awb_locked") {
    app->cam_list[indx]->set_libcam_awb_locked(parm_val == "true" || parm_val == "1");
} else if (parm_name == "libcam_colour_temp") {
    app->cam_list[indx]->set_libcam_colour_temp(atoi(parm_val.c_str()));
} else if (parm_name == "libcam_colour_gain_r") {
    float r = atof(parm_val.c_str());
    float b = app->cam_list[indx]->cfg->parm_cam.libcam_colour_gain_b;
    app->cam_list[indx]->set_libcam_colour_gains(r, b);
} else if (parm_name == "libcam_colour_gain_b") {
    float r = app->cam_list[indx]->cfg->parm_cam.libcam_colour_gain_r;
    float b = atof(parm_val.c_str());
    app->cam_list[indx]->set_libcam_colour_gains(r, b);
}
```

(Duplicate for the single-camera path with `webua->cam`)

---

## Testing

1. **Build verification**: `make clean && make -j4`
2. **Config file test**: Add parameters to motion.conf, verify parsing
3. **Hot-reload test**: Use web API to change AWB mode while streaming
4. **Logging verification**: Check debug logs show AWB control application
5. **Visual verification**: Observe white balance changes in live stream

### Test Commands

```bash
# Set AWB to Daylight
curl "http://pi:8080/0/config/set?libcam_awb_mode=5"

# Set AWB to Tungsten (warm)
curl "http://pi:8080/0/config/set?libcam_awb_mode=2"

# Disable AWB for manual control
curl "http://pi:8080/0/config/set?libcam_awb_enable=false"

# Set manual colour temperature (6500K daylight)
curl "http://pi:8080/0/config/set?libcam_colour_temp=6500"

# Set manual gains
curl "http://pi:8080/0/config/set?libcam_colour_gain_r=1.5"
curl "http://pi:8080/0/config/set?libcam_colour_gain_b=1.2"
```

---

## Notes

- Uses libcamera's native AWB mode ordering (0=Auto through 7=Custom)
- ColourGains and ColourTemperature typically require `AwbEnable=false` to take effect
- Value of 0 for colour_temp/colour_gains means "don't set" (use automatic)
- No mode conversion needed - values pass directly to libcamera

---

## Implementation Summary (2025-12-21)

### Bug Fix
- **File**: `src/webu_json.cpp:637`
- **Issue**: Single-camera path fetched wrong gain variable when setting red gain
- **Fix**: Changed `libcam_colour_gain_r` to `libcam_colour_gain_b`

### Mutual Exclusivity
Temperature and gains are now mutually exclusive:
- Setting `libcam_colour_temp` to non-zero clears both gain values to 0
- Setting either `libcam_colour_gain_r` or `libcam_colour_gain_b` to non-zero clears temperature to 0
- **Files modified**: `src/libcam.cpp` (set_colour_temp, set_colour_gains)

### Enhanced Logging
Added conditional debug logging in `req_add()`:
- Logs colour temperature when applied (non-zero)
- Logs colour gains when applied (either non-zero)
- Follows existing `MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO, ...)` pattern

### API Behavior After Update
```bash
# Method 1: Temperature-based (clears any existing gains)
curl "http://pi:8080/0/config/set?libcam_colour_temp=6500"

# Method 2: Gains-based (clears any existing temperature)
curl "http://pi:8080/0/config/set?libcam_colour_gain_r=1.5"
curl "http://pi:8080/0/config/set?libcam_colour_gain_b=1.2"

# Reset to auto (set all to 0)
curl "http://pi:8080/0/config/set?libcam_colour_temp=0"
```
