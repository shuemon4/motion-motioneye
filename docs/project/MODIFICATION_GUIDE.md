# Motion - Modification Guide

Step-by-step guides for common modifications. Designed for AI agents to follow.

## Quick Reference: File Locations

| Task | Primary Files | Secondary Files |
|------|--------------|-----------------|
| Add config parameter | `src/conf.cpp`, `src/conf.hpp` | `data/*.conf.in` |
| Add web API endpoint | `src/webu_ans.cpp`, `src/webu_json.cpp` | `src/webu.hpp` |
| Modify motion detection | `src/alg.cpp`, `src/alg_sec.cpp` | `src/camera.cpp` |
| Add camera source type | `src/camera.cpp`, new source file | `src/camera.hpp` |
| Modify video recording | `src/movie.cpp` | `src/movie.hpp` |
| Add database query | `src/dbse.cpp` | `src/dbse.hpp` |
| Modify web UI | `data/webcontrol/` | `src/webu_html.cpp` |

---

## 1. Adding a New Configuration Parameter

### Step 1: Define the parameter (conf.cpp)

**Location**: `src/conf.cpp` - find the `config_parms[]` array

```cpp
// Add entry to config_parms[] array
// Format: {"name", TYPE, CATEGORY, WEBUI_LEVEL}

{"my_new_param",
    PARM_TYP_INT,           // STRING, INT, BOOL, LIST, ARRAY
    PARM_CAT_07,            // See category list below
    WEBUI_LEVEL_LIMITED     // ALWAYS, LIMITED, ADVANCED, RESTRICTED, NEVER
},
```

**Categories**:
- `PARM_CAT_00`: System
- `PARM_CAT_01`: Camera
- `PARM_CAT_07`: Detection
- `PARM_CAT_13`: WebControl
- (See ARCHITECTURE.md for full list)

### Step 2: Add class member (conf.hpp)

**Location**: `src/conf.hpp` - in `cls_config` class

```cpp
class cls_config {
public:
    // ... existing members ...

    int my_new_param;  // Add near related parameters
};
```

### Step 3: Add edit handler (conf.cpp)

**Location**: `src/conf.cpp` - add handler function

```cpp
void cls_config::edit_my_new_param(ctx_parm_item &parm)
{
    int parm_in;

    parm_in = atoi(parm.parm_value.c_str());

    // Add validation
    if (parm_in < 0) {
        parm_in = 0;
    }
    if (parm_in > 100) {
        parm_in = 100;
    }

    my_new_param = parm_in;
}
```

**Location**: `src/conf.cpp` - register in edit dispatcher

```cpp
// Find the parameter dispatch switch/if block
if (parm.parm_name == "my_new_param") {
    edit_my_new_param(parm);
}
```

### Step 4: Set default value (conf.cpp)

**Location**: `src/conf.cpp` - in constructor or defaults function

```cpp
my_new_param = 50;  // Default value
```

### Step 5: Update config template

**Location**: `data/motion-dist.conf.in`

```ini
############################################################
# My New Parameter
############################################################

# Description of what this parameter does.
# Valid values: 0-100
# Default: 50
; my_new_param 50
```

### Step 6: Use the parameter

**Location**: Wherever needed (e.g., `src/camera.cpp`)

```cpp
if (cam->cfg->my_new_param > threshold) {
    // Use the parameter
}
```

---

## 2. Adding a New Web API Endpoint

### Step 1: Add handler method

**Location**: `src/webu_json.cpp` (for JSON) or appropriate handler

```cpp
void cls_webu_json::my_endpoint()
{
    // Access camera/app data
    int camera_id = webuc->cam->camera_id;

    // Build JSON response
    resp_body = "{";
    resp_body += "\"camera_id\": " + std::to_string(camera_id);
    resp_body += ", \"status\": \"ok\"";
    resp_body += "}";

    // Set response type
    resp_type = "application/json";
}
```

### Step 2: Declare method (header)

**Location**: `src/webu_json.hpp`

```cpp
class cls_webu_json {
public:
    // ... existing ...
    void my_endpoint();
};
```

### Step 3: Add routing

**Location**: `src/webu_ans.cpp` - in `process_answer()` or routing function

```cpp
// Find URL routing section
if (uri.find("/api/myendpoint") != std::string::npos) {
    json->my_endpoint();
    return;
}
```

### Step 4: Test the endpoint

```bash
curl http://localhost:8080/api/myendpoint
```

---

## 3. Adding a New Motion Detection Feature

### Step 1: Add algorithm function

**Location**: `src/alg.cpp`

```cpp
void cls_alg::my_detection_feature(cls_camera *cam)
{
    unsigned char *image = cam->imgs.image_ring[idx].image_norm;
    int width = cam->imgs.width;
    int height = cam->imgs.height;

    // Implement detection logic
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int pixel = image[y * width + x];
            // Process pixel
        }
    }

    // Store results
    cam->detection_result = calculated_value;
}
```

### Step 2: Declare function

**Location**: `src/alg.hpp`

```cpp
class cls_alg {
public:
    void my_detection_feature(cls_camera *cam);
};
```

### Step 3: Integrate into detection pipeline

**Location**: `src/camera.cpp` - in detection() or handler()

```cpp
void cls_camera::detection()
{
    // Existing detection code...

    // Add new feature
    if (cfg->enable_my_feature) {
        alg->my_detection_feature(this);
    }
}
```

### Step 4: Add configuration (see Section 1)

Add parameter to enable/configure the feature.

---

## 4. Modifying Video Recording

### Adding a new output format

**Location**: `src/movie.cpp`

### Step 1: Add format detection

```cpp
// In start() or format initialization
if (cam->cfg->movie_container == "myformat") {
    avformat_alloc_output_context2(&oc, NULL, "myformat", filename.c_str());
    // Configure format-specific options
}
```

### Step 2: Add codec configuration

```cpp
// Find codec selection
if (cam->cfg->movie_codec == "mycodec") {
    codec = avcodec_find_encoder(AV_CODEC_ID_MYCODEC);
    // Set codec parameters
    ctx->profile = FF_PROFILE_MYCODEC_MAIN;
}
```

### Step 3: Add config parameters

See Section 1 for adding:
- `movie_container` list value
- `movie_codec` list value

---

## 5. Adding Database Functionality

### Step 1: Add SQL query

**Location**: `src/dbse.cpp`

```cpp
void cls_dbse::my_query(int camera_id, const std::string &data)
{
    std::string sql;

    sql = "INSERT INTO my_table (camera_id, data, timestamp) VALUES (";
    sql += std::to_string(camera_id) + ", ";
    sql += "'" + escape_string(data) + "', ";
    sql += "NOW())";

    exec_sql(sql);
}
```

### Step 2: Add escape helper (if not exists)

```cpp
std::string cls_dbse::escape_string(const std::string &input)
{
    std::string output = input;
    // Escape single quotes
    size_t pos = 0;
    while ((pos = output.find("'", pos)) != std::string::npos) {
        output.replace(pos, 1, "''");
        pos += 2;
    }
    return output;
}
```

### Step 3: Declare method

**Location**: `src/dbse.hpp`

```cpp
class cls_dbse {
public:
    void my_query(int camera_id, const std::string &data);
};
```

### Step 4: Create table (if new table)

**Location**: `src/dbse.cpp` - in table creation function

```cpp
void cls_dbse::create_tables()
{
    // Existing tables...

    std::string sql = "CREATE TABLE IF NOT EXISTS my_table ("
                      "id INTEGER PRIMARY KEY, "
                      "camera_id INTEGER, "
                      "data TEXT, "
                      "timestamp DATETIME)";
    exec_sql(sql);
}
```

---

## 6. Modifying libcamera Support (Pi 5)

### Camera Selection Enhancement

**Location**: `src/libcam.cpp`

The libcamera implementation supports three camera selection modes:

```cpp
// In start_mgr() function
std::string device = cam->cfg->libcam_device;

if (device == "auto" || device.empty()) {
    // Auto-detect: filter USB cameras, select first Pi camera
    auto pi_cams = get_pi_cameras();
    camera = pi_cams[0];
} else if (device.find("camera") == 0) {
    // Index mode: "camera0", "camera1", etc.
    int idx = std::stoi(device.substr(6));
    auto pi_cams = get_pi_cameras();
    camera = pi_cams[idx];
} else {
    // Model name mode: "imx708", "imx219", etc.
    camera = find_camera_by_model(device);
}
```

### USB Camera Filtering

**Location**: `src/libcam.cpp` - `get_pi_cameras()` function

```cpp
std::vector<std::shared_ptr<Camera>> cls_libcam::get_pi_cameras()
{
    std::vector<std::shared_ptr<Camera>> pi_cams;
    for (const auto& cam_item : cam_mgr->cameras()) {
        std::string id = cam_item->id();
        std::string id_lower = id;
        std::transform(id_lower.begin(), id_lower.end(),
                       id_lower.begin(), ::tolower);
        // Exclude USB/UVC cameras
        if (id_lower.find("usb") == std::string::npos &&
            id_lower.find("uvc") == std::string::npos) {
            pi_cams.push_back(cam_item);
        }
    }
    return pi_cams;
}
```

### Multi-Buffer Configuration

**Location**: `src/libcam.cpp` - `start_config()` function

```cpp
// Pi 5 requires VideoRecording role for proper PiSP pipeline
config = camera->generateConfiguration({ StreamRole::VideoRecording });

// Multi-buffer support: configurable 2-8 buffers (default 4)
int buffer_count = cam->cfg->libcam_buffer_count;
if (buffer_count < 2) buffer_count = 2;
if (buffer_count > 8) buffer_count = 8;
config->at(0).bufferCount = (uint)buffer_count;
```

### Adding libcam_buffer_count Parameter

Already added to `src/conf.cpp` and `src/conf.hpp`:
- Parameter name: `libcam_buffer_count`
- Type: `PARM_TYP_INT`
- Category: `PARM_CAT_02` (Source)
- Default: 4, Range: 2-8

---

## 7. Adding a New Camera Source Type (Generic)

### Step 1: Create source class

**Location**: Create `src/mysource.hpp`

```cpp
#ifndef MYSOURCE_HPP
#define MYSOURCE_HPP

#include "motion.hpp"

class cls_mysource {
public:
    cls_mysource(cls_camera *p_cam);
    ~cls_mysource();

    int start();
    int stop();
    int capture_image(unsigned char *image);

private:
    cls_camera *cam;
    // Source-specific members
};

#endif
```

### Step 2: Implement source class

**Location**: Create `src/mysource.cpp`

```cpp
#include "mysource.hpp"
#include "logger.hpp"

cls_mysource::cls_mysource(cls_camera *p_cam)
{
    cam = p_cam;
}

cls_mysource::~cls_mysource()
{
    stop();
}

int cls_mysource::start()
{
    // Initialize source
    motlog(LOG_INFO, cam->camera_id, "Starting mysource");
    return 0;
}

int cls_mysource::stop()
{
    // Cleanup source
    return 0;
}

int cls_mysource::capture_image(unsigned char *image)
{
    // Capture frame into image buffer
    // Return 0 on success, -1 on error
    return 0;
}
```

### Step 3: Add to camera type enum

**Location**: `src/camera.hpp`

```cpp
enum CAMERA_TYPE {
    CAMERA_TYPE_UNKNOWN,
    CAMERA_TYPE_V4L2,
    CAMERA_TYPE_LIBCAM,
    CAMERA_TYPE_NETCAM,
    CAMERA_TYPE_MYSOURCE  // Add new type
};
```

### Step 4: Integrate into camera

**Location**: `src/camera.cpp`

```cpp
#include "mysource.hpp"

// In capture() function
int cls_camera::capture()
{
    switch (camera_type) {
    // Existing cases...

    case CAMERA_TYPE_MYSOURCE:
        return mysource->capture_image(current_image);
    }
}

// In initialization
void cls_camera::init()
{
    if (cfg->source_type == "mysource") {
        camera_type = CAMERA_TYPE_MYSOURCE;
        mysource = new cls_mysource(this);
        mysource->start();
    }
}
```

### Step 5: Update build system

**Location**: `src/Makefile.am`

```makefile
motion_SOURCES = \
    # ... existing sources ...
    mysource.cpp \
    mysource.hpp
```

---

## 7. Common Modifications

### Changing Detection Sensitivity

**Files**: `src/alg.cpp`
**Functions**: `threshold_tune()`, `noise_tune()`

```cpp
// Modify threshold adjustment rate
void cls_alg::threshold_tune(cls_camera *cam, int diff_count)
{
    // Change increment/decrement values
    int adjust_rate = 2;  // Was 1

    if (diff_count > cam->cfg->threshold) {
        cam->threshold += adjust_rate;
    } else {
        cam->threshold -= adjust_rate;
    }
}
```

### Adding Overlay Text

**Files**: `src/draw.cpp`
**Functions**: `text()`, `draw_text()`

```cpp
// Add new overlay element
void cls_draw::my_overlay(cls_camera *cam)
{
    std::string text = "Custom: " + std::to_string(value);
    int x = 10;
    int y = cam->imgs.height - 20;

    draw_text(cam->imgs.image_ring[idx].image_norm,
              cam->imgs.width, cam->imgs.height,
              x, y, text.c_str());
}
```

### Modifying Stream Quality

**Files**: `src/webu_stream.cpp`

```cpp
// Change JPEG quality
int quality = cam->cfg->stream_quality;
if (quality < 1) quality = 1;
if (quality > 100) quality = 100;

// Apply to encoding
jpeg_set_quality(&cinfo, quality, TRUE);
```

---

## 8. Build and Test

### Compile Changes

```bash
# Clean build
make clean

# Rebuild
make -j6

# Install (optional)
sudo make install
```

### Test Configuration

```bash
# Validate config
./motion -c /path/to/motion.conf -n

# Run in foreground for debugging
./motion -c /path/to/motion.conf -n -d
```

### Check Logs

```bash
# Follow log output
tail -f /var/log/motion/motion.log

# Check for errors
grep -i error /var/log/motion/motion.log
```

---

## 9. Verification Checklist

Before completing a modification:

- [ ] Code compiles without warnings (`make -j6`)
- [ ] Header guards in place for new headers
- [ ] Memory allocated is freed in destructors
- [ ] Thread safety considered for shared data
- [ ] Configuration parameter has default value
- [ ] Config template updated if parameter added
- [ ] Logging added for significant operations
- [ ] Error conditions handled appropriately

---

## 10. Common Pitfalls

### Thread Safety
- Always lock mutexes when accessing shared image data
- Use `pthread_mutex_lock(&cam->mutex_img)` / `pthread_mutex_unlock()`

### Memory Leaks
- Match every `new` with `delete` in destructor
- Check for early returns that skip cleanup

### Configuration
- Validate all user-provided values
- Set reasonable defaults
- Document valid ranges in config template

### FFmpeg
- Check return values from all av* functions
- Properly unref packets and frames
- Close contexts in correct order

### Build System
- Add new files to `src/Makefile.am`
- Run `autoreconf -i` if changing `configure.ac`
