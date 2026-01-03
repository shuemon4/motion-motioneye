# Configuration Profiles Research
## Date: 2026-01-03

## Objective
Implement ability to save configurations for quick switching of settings, including:
- All libcamera Controls
- All Motion Detection settings
- Device Settings - Framerate

## Current Configuration Architecture

### Configuration Storage (Backend)
- **Primary Storage**: `cls_config` class in `src/conf.hpp` and `src/conf.cpp`
- **Scoped Structures**:
  - `parm_app`: Application-level parameters
  - `parm_cam`: Camera device parameters (includes detection, capture, output)
  - `parm_snd`: Sound device parameters
- **Parameter Metadata**: `config_parms[]` array with ~600 parameters
  - Each parameter has: name, type, category, webui_level, hot_reload flag

### Libcamera Controls
**Location**: `src/libcam.hpp:45-66`, `src/libcam.cpp`

**Structure**: `ctx_pending_controls`
```cpp
struct ctx_pending_controls {
    std::mutex mtx;
    float brightness = 0.0f;        // -1 to 1
    float contrast = 1.0f;          // 0-32
    float iso = 100.0f;             // 100-6400 (converted to AnalogueGain)

    // AWB controls
    bool awb_enable = true;
    int awb_mode = 0;               // 0=Auto
    bool awb_locked = false;
    int colour_temp = 0;            // 0-10000 K
    float colour_gain_r = 0.0f;     // 0-8
    float colour_gain_b = 0.0f;     // 0-8

    // Autofocus controls
    int af_mode = 0;                // 0=Manual, 1=Auto, 2=Continuous
    float lens_position = 0.0f;     // Dioptres (0=infinity, 0-15)
    int af_range = 0;               // 0=Normal, 1=Macro, 2=Full
    int af_speed = 0;               // 0=Normal, 1=Fast
    bool af_trigger = false;
    bool af_cancel = false;
    bool dirty = false;
};
```

**Additional**: `libcam_params` string in `conf.cpp:77` (PARM_TYP_PARAMS)
- Contains arbitrary libcamera control key=value pairs

### Motion Detection Settings
**Category**: PARM_CAT_07 (detect)

**Key Parameters** (from `conf.cpp`):
```
threshold                   // Frame change threshold (hot_reload: true)
threshold_maximum
threshold_sdevx
threshold_sdevy
threshold_sdevxy
threshold_ratio
threshold_ratio_change
threshold_tune
noise_level                 // Noise level (hot_reload: true)
noise_tune
despeckle_filter           // Smart mask settings
area_detect
lightswitch_percent        // Light switch detection (hot_reload: true)
lightswitch_frames         // (hot_reload: true)
minimum_motion_frames      // (hot_reload: true)
static_object_time         // (hot_reload: true)
event_gap                  // (hot_reload: true)
pre_capture                // (hot_reload: true)
post_capture               // (hot_reload: true)
```

### Device Settings - Framerate
**Parameter**: `framerate` in `conf.cpp:97`
- Type: PARM_TYP_INT
- Category: PARM_CAT_03 (image)
- Level: PARM_LEVEL_LIMITED
- Hot reload: false (requires camera restart)
- Range: 2-100 (default: 15) - see `conf.cpp:661`

## Current API Endpoints

### GET /0/api/config
**Source**: `webu_json.cpp:1315`
- Returns: All configuration parameters + csrf_token + version + cameras list
- Structure: `parms_all()` serializes all config_parms[]

### PATCH /0/api/config
**Source**: `webu_json.cpp:1345`
- Batch update of multiple parameters
- CSRF token validation required
- Returns: Status + applied/failed results per parameter
- **Security**: Blocks SQL parameter modifications
- **Validation**: Checks hot_reload flag for each parameter
- **Process**: Uses `cfg->edit_set()` to apply changes

## Existing Frontend (React)

### Settings Sliders (from settings-sliders.md)
**Total**: 22 sliders across 6 sections

**Relevant to Profile System**:
1. **Device Settings**:
   - framerateSlider (2-30 fps)

2. **Libcamera Controls** (Device Section):
   - brightnessSlider (-1 to 1)
   - contrastSlider (0-32)
   - isoSlider (1-64, logarithmic)
   - lensPositionSlider (0-15 dioptres)
   - colourTempSlider (0-10000 K)
   - colourGainRSlider (0-8)
   - colourGainBSlider (0-8)

3. **Motion Detection**:
   - frameChangeThresholdSlider (0-20%)
   - noiseLevelSlider (0-25%)
   - lightSwitchDetectSlider (0-100%)
   - smartMaskSluggishnessSlider (1-10)

## Database Availability
**SQLite3**: Available (`--with-sqlite3` configure flag)
- Currently used for: (need to research usage)
- Located: libsqlite3-dev installed on Pi

## Profile System Requirements

### Parameters to Include in Profile
1. **Libcamera Controls** (13 parameters):
   - brightness, contrast, iso
   - awb_enable, awb_mode, awb_locked, colour_temp, colour_gain_r, colour_gain_b
   - af_mode, lens_position, af_range, af_speed
   - Plus: libcam_params string

2. **Motion Detection** (17 parameters):
   - threshold, threshold_maximum, threshold_sdevx/y/xy, threshold_ratio, threshold_ratio_change, threshold_tune
   - noise_level, noise_tune
   - despeckle_filter, area_detect
   - lightswitch_percent, lightswitch_frames
   - minimum_motion_frames, static_object_time
   - event_gap, pre_capture, post_capture

3. **Device Settings** (1 parameter):
   - framerate

**Total**: ~31 parameters per profile

### Hot Reload Considerations
- Most motion detection params support hot_reload: true
- Framerate does NOT support hot_reload (requires camera restart)
- Libcamera controls use runtime hot-reload via `apply_pending_controls()`

### Profile Metadata
- profile_name (string, unique)
- profile_description (string, optional)
- camera_id (int, 0 for global?)
- created_at (timestamp)
- updated_at (timestamp)
- is_default (bool)

## Design Considerations

### Storage Options
1. **SQLite Database** (Recommended)
   - Pro: Already available, structured, queryable
   - Pro: Transactional, ACID compliant
   - Pro: Easy backup/export
   - Con: Requires schema management

2. **JSON Files**
   - Pro: Human-readable, easy to edit
   - Pro: Simple backup (copy files)
   - Con: No concurrent access control
   - Con: Manual file management

3. **Motion Config Files**
   - Pro: Uses existing format
   - Con: Not designed for multiple profiles
   - Con: Harder to parse/manage

### UI Considerations
- Profile selector dropdown (top of settings page?)
- Save current settings as profile
- Load profile
- Delete profile
- Export/import profiles (future)
- Visual indication of active profile
- Unsaved changes warning

### CPU Efficiency
- **Critical**: Profile switching should be lightweight
- Batch API calls: Use existing PATCH /0/api/config
- Avoid camera restarts when possible
- Cache loaded profiles in memory
- Minimize UI re-renders

### Edge Cases
- Conflicting parameters between profiles
- Profile with framerate change (requires restart)
- Profile for camera that doesn't exist
- Default profile fallback
- Profile version compatibility

## Next Steps
1. Design database schema for profile storage
2. Design API endpoints for profile management
3. Plan backend implementation
4. Plan frontend UI components
5. Consider migration/upgrade path
