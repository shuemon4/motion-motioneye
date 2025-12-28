# Hot Reload API - Research Notes

**Date**: 2025-12-13
**Purpose**: Research notes for implementing MotionEye hot reload API request

---

## Current Architecture Analysis

### Configuration System Overview

**Files**:
- `src/conf.hpp` - Configuration class definition (~575 lines)
- `src/conf.cpp` - Configuration implementation
- `src/parm_structs.hpp` - Scoped parameter structures

**Key Architecture Points**:

1. **Scoped Parameter Structures** (Modern design):
   - `ctx_parm_app` - Application-level params (daemon, webcontrol, database)
   - `ctx_parm_cam` - Camera params (detection, capture, output)
   - `ctx_parm_snd` - Sound params

2. **Backward Compatibility References**:
   - Reference aliases redirect flat member access (`->cfg->threshold`) to scoped structs
   - This preserves binary compatibility with 469 direct access sites in 28 files

3. **Parameter Types** (`PARM_TYP`):
   - `PARM_TYP_STRING` - String values
   - `PARM_TYP_INT` - Integer values
   - `PARM_TYP_BOOL` - Boolean (on/off)
   - `PARM_TYP_LIST` - Predefined choices
   - `PARM_TYP_PARAMS` - Key-value params

4. **Parameter Categories** (`PARM_CAT_00` through `PARM_CAT_18`):
   - 00: System (daemon, pid_file, log_level)
   - 01: Camera (device_name, device_id)
   - 02: Source (v4l2_device, netcam_url, libcam_device)
   - 03: Image (width, height, framerate)
   - 04: Overlay (text_left, text_right, locate_motion_mode)
   - 05: Method (threshold, threshold_tune, noise_level, noise_tune)
   - 06: Masks (mask_file, mask_privacy, smart_mask_speed)
   - 07: Detect (lightswitch_percent, minimum_motion_frames, event_gap)
   - 08: Scripts (on_event_start, on_event_end, on_motion_detected)
   - 09: Picture (picture_output, picture_quality)
   - 10: Movies (movie_output, movie_container)
   - 11: Timelapse
   - 12: Pipes
   - 13: WebControl
   - 14: Streams
   - 15: Database
   - 16: SQL
   - 17: Tracking/PTZ
   - 18: Sound

---

### Web Control System Analysis

**Files**:
- `src/webu.cpp/hpp` - Main web server (libmicrohttpd)
- `src/webu_ans.cpp/hpp` - Request routing/handling
- `src/webu_json.cpp/hpp` - JSON API responses
- `src/webu_post.cpp/hpp` - POST request handling (config updates)
- `src/webu_text.cpp/hpp` - Text responses (detection/action commands)

**Current API Endpoints**:

| Endpoint | Handler | Purpose |
|----------|---------|---------|
| `/config.json` | webu_json | Get all config params |
| `/status.json` | webu_json | Get camera status |
| `/detection/status` | webu_text | Detection status |
| `/detection/pause` | webu_text→webu_post | Pause detection |
| `/detection/start` | webu_text→webu_post | Resume detection |
| `/action/eventstart` | webu_text→webu_post | Trigger event |
| `/action/eventend` | webu_text→webu_post | End event |
| `/action/snapshot` | webu_text→webu_post | Take snapshot |
| `POST /` | webu_post | Config updates (triggers restart) |

**Key Observation**:
- `webu_text.cpp` already routes GET requests like `/detection/start` to `webu_post` action handlers
- These actions modify runtime state (e.g., `cam->user_pause`) without restart
- This pattern can be extended for config hot reload

---

### Current Config Update Flow (webu_post.cpp)

**`cls_webu_post::config()`** (lines 602-684):
1. Calls `config_restart_reset()` to track which components need restart
2. Iterates through POST params, calls `config_set()` for each
3. `config_set()` updates `conf_src` (source config) and schedules restart
4. After processing, sets `restart=true` on affected components

**Critical Point**: Currently ALL config changes trigger component restart.

**`config_restart_set()` Logic**:
- Tracks restart needs per component type: "log", "webu", "dbse", "cam", "snd"
- Camera restart: `app->cam_list[indx]->restart = true`

---

### Thread Safety Analysis

**Existing Mutexes**:
```cpp
// motion.hpp:202-203
pthread_mutex_t mutex_camlst;  // Lock camera list for add/remove
pthread_mutex_t mutex_post;    // Process POST actions

// Additional per-component mutexes:
pthread_mutex_t stream.mutex;      // Per-camera stream access
pthread_mutex_t netcam.mutex;      // Network camera
pthread_mutex_t logger.mutex_log;  // Logging
pthread_mutex_t dbse.mutex_dbse;   // Database
pthread_mutex_t algsec.mutex;      // Secondary algorithm
```

**Current Thread Model**:
- Main thread: Signal handling, lifecycle management
- Per-camera thread: `cls_camera::handler()` - capture, detect, record
- Web server: libmicrohttpd thread pool
- Movie thread: Video encoding

**Config Access Pattern**:
- Camera thread reads `cam->cfg->threshold`, `cam->cfg->noise_level` etc. every frame
- Web thread can update config via POST
- No mutex currently protects config reads in camera thread

---

## Hot Reload Classification

### Tier 1: Safe for Hot Reload (No Buffer Changes)

These parameters are read per-frame and can be atomically updated:

**Detection Parameters (Category 05)**:
- `threshold` (int)
- `threshold_maximum` (int)
- `threshold_tune` (bool)
- `noise_level` (int)
- `noise_tune` (bool)
- `despeckle_filter` (string)
- `minimum_motion_frames` (int)
- `event_gap` (int)
- `lightswitch_percent` (int)
- `lightswitch_frames` (int)

**Overlay Parameters (Category 04)**:
- `text_left` (string)
- `text_right` (string)
- `text_scale` (int)
- `locate_motion_mode` (list)
- `locate_motion_style` (list)

**Script Parameters (Category 08)**:
- `on_event_start` (string)
- `on_event_end` (string)
- `on_motion_detected` (string)
- `on_movie_start` (string)
- `on_movie_end` (string)
- `on_picture_save` (string)

**Mask Parameters (partial)**:
- `smart_mask_speed` (int)

### Tier 2: Requires Special Handling

**Mask File Updates**:
- `mask_file` - Requires loading new PGM file
- `mask_privacy` - Requires loading new PGM file and updating UV buffers

### Tier 3: Requires Restart (Buffer/Device Changes)

- `width`, `height`, `framerate` - Buffer reallocation
- `rotate` - Buffer reallocation
- `v4l2_device`, `netcam_url`, `libcam_device` - Device reconnection
- `webcontrol_port` - Server rebind
- `movie_container`, `movie_codec` - Encoder reconfiguration

---

## Implementation Strategy

### Option A: Direct Variable Update (Recommended for Integers/Bools)

For atomic types (int, bool), direct assignment is thread-safe on modern CPUs:

```cpp
// webu_json.cpp - new endpoint
void cls_webu_json::config_set_single(std::string parm_nm, std::string parm_val) {
    // Validate parameter is hot-reloadable
    // Update cam->cfg directly (atomic for int/bool)
    // Log change
}
```

**Pros**: Zero overhead, immediate effect
**Cons**: String parameters need careful handling

### Option B: Mutex-Protected Update (Recommended for Strings)

Add config mutex to cls_camera:
```cpp
pthread_mutex_t mutex_cfg;  // Protect config reads/writes
```

**Pros**: Thread-safe for all types
**Cons**: Slight overhead on frame loop

### Option C: Double Buffering (Overkill)

Maintain two config instances, swap pointer atomically.

**Pros**: Zero-copy reads
**Cons**: Memory overhead, complexity

---

## Proposed API Design

### Endpoint

```
GET /{camera_id}/config/set?{param}={value}
```

or

```
POST /{camera_id}/config/set
Content-Type: application/x-www-form-urlencoded
{param}={value}
```

### Response Format

```json
{
  "status": "ok|error",
  "parameter": "threshold",
  "old_value": "1500",
  "new_value": "2000",
  "hot_reload": true
}
```

### Error Response

```json
{
  "status": "error",
  "parameter": "width",
  "message": "Parameter requires restart",
  "hot_reload": false
}
```

---

## Key Files to Modify

1. **src/webu_json.cpp** - Add `config_set()` handler
2. **src/webu_json.hpp** - Declare new method
3. **src/webu_ans.cpp** - Route new endpoint
4. **src/conf.hpp** - Add hot-reload classification
5. **src/conf.cpp** - Add `edit_set_hot()` method for direct update

---

## Testing Considerations

1. **Thread Safety**: Run concurrent config updates while streaming
2. **Boundary Values**: Test min/max values for int parameters
3. **String Safety**: Ensure no buffer overflows on string params
4. **Functional**: Verify detection behavior changes immediately
5. **Performance**: Measure frame loop impact with/without mutex

---

## References

- MotionEye Request: `doc/plans/MotionEye-Plans/motion-hot-reload-api-request-20251213-0830.md`
- Architecture: `doc/project/ARCHITECTURE.md`
- Patterns: `doc/project/PATTERNS.md`
