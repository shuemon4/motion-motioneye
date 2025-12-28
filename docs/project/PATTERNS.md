# Motion - Code Patterns Reference

Common code patterns, idioms, and conventions used throughout this codebase.

## Naming Conventions

### Classes
```cpp
// All classes use cls_ prefix
class cls_camera { ... };
class cls_config { ... };
class cls_webu { ... };
```

### Structs (Context/Data)
```cpp
// Data structures use ctx_ prefix
struct ctx_image_data { ... };
struct ctx_coord { ... };
struct ctx_params { ... };
struct ctx_imgmap { ... };     // libcamera memory-mapped buffer
struct ctx_reqinfo { ... };    // libcamera request with buffer index
```

### Enums
```cpp
// UPPERCASE with underscores
enum CAMERA_TYPE { ... };
enum PARM_CAT { ... };
enum LOG_LEVEL { ... };
enum MOTION_SIGNAL { ... };
```

### Variables
```cpp
// lowercase with underscores
int frame_count;
std::string camera_name;
bool detecting_motion;
```

### Methods
```cpp
// lowercase with underscores
void process_frame();
bool is_motion_detected();
int get_threshold();
```

---

## Common Idioms

### Class Constructor Pattern
```cpp
cls_example::cls_example(cls_motapp *p_app)
{
    app = p_app;
    // Initialize members
    member_var = 0;
    ptr_member = nullptr;
}

cls_example::~cls_example()
{
    // Cleanup in reverse order
    if (ptr_member != nullptr) {
        delete ptr_member;
    }
}
```

### Configuration Parameter Access
```cpp
// From camera object
int threshold = cam->cfg->threshold;
std::string filename = cam->cfg->movie_filename;
bool enabled = cam->cfg->snapshot_enabled;

// From application object
int port = app->cfg->webcontrol_port;
```

### Thread-Safe Image Access
```cpp
pthread_mutex_lock(&cam->mutex_img);
{
    // Access image data safely
    memcpy(dest, cam->imgs.image_ring[idx].image_norm, size);
}
pthread_mutex_unlock(&cam->mutex_img);
```

### Ring Buffer Access
```cpp
// Get current write position
int write_idx = cam->imgs.ring_in;

// Get current read position
int read_idx = cam->imgs.ring_out;

// Advance write position (circular)
cam->imgs.ring_in = (cam->imgs.ring_in + 1) % cam->imgs.ring_size;

// Check if buffer has data
bool has_data = (cam->imgs.ring_in != cam->imgs.ring_out);
```

### Logging Pattern
```cpp
// Standard logging call
motlog(LOG_INFO, cam->camera_id, "Processing frame %d", frame_num);

// Error with context
motlog(LOG_ERR, cam->camera_id, "Failed to open device: %s", strerror(errno));

// Debug (may be compiled out)
motlog(LOG_DEBUG, cam->camera_id, "Threshold value: %d", threshold);
```

---

## File Organization Pattern

### Header Files (.hpp)
```cpp
#ifndef EXAMPLE_HPP
#define EXAMPLE_HPP

// Required includes
#include "config.h"
#include "motion.hpp"

// Forward declarations
class cls_motapp;
class cls_camera;

// Class declaration
class cls_example {
public:
    cls_example(cls_motapp *p_app);
    ~cls_example();

    void public_method();

private:
    cls_motapp *app;
    int private_member;

    void private_method();
};

#endif // EXAMPLE_HPP
```

### Implementation Files (.cpp)
```cpp
#include "example.hpp"
#include "logger.hpp"
#include "util.hpp"

// Constructor
cls_example::cls_example(cls_motapp *p_app)
{
    app = p_app;
}

// Destructor
cls_example::~cls_example()
{
    // Cleanup
}

// Method implementations
void cls_example::public_method()
{
    // Implementation
}
```

---

## Configuration Patterns

### Adding a New Parameter

**Step 1: Add to parameter list (conf.cpp)**
```cpp
// In config_parms[] array
{
    "new_parameter",           // Parameter name
    PARM_TYP_INT,             // Type (STRING, INT, BOOL, LIST)
    PARM_CAT_07,              // Category
    WEBUI_LEVEL_LIMITED       // Web UI access level
},
```

**Step 2: Add member variable (conf.hpp)**
```cpp
// In cls_config class
int new_parameter;
```

**Step 3: Add edit handler (conf.cpp)**
```cpp
void cls_config::edit_new_parameter(ctx_parm_item &parm)
{
    int parm_in;

    parm_in = atoi(parm.parm_value.c_str());

    // Validation
    if (parm_in < MIN_VALUE) {
        parm_in = MIN_VALUE;
    }
    if (parm_in > MAX_VALUE) {
        parm_in = MAX_VALUE;
    }

    new_parameter = parm_in;
}
```

**Step 4: Add to config template (data/motion-dist.conf.in)**
```
# Description of new parameter
# Valid values: min-max
; new_parameter value
```

### Parameter Type Handlers

```cpp
// String parameter
void cls_config::edit_string_param(ctx_parm_item &parm)
{
    param_value = parm.parm_value;
}

// Integer parameter
void cls_config::edit_int_param(ctx_parm_item &parm)
{
    param_value = atoi(parm.parm_value.c_str());
}

// Boolean parameter
void cls_config::edit_bool_param(ctx_parm_item &parm)
{
    param_value = mycheck_torf(parm.parm_value);
}

// List parameter (predefined values)
void cls_config::edit_list_param(ctx_parm_item &parm)
{
    if (parm.parm_value == "option1") {
        param_value = OPTION_1;
    } else if (parm.parm_value == "option2") {
        param_value = OPTION_2;
    }
}
```

---

## Web API Patterns

### JSON Response Structure
```cpp
// Standard response
{
    "status": "success",
    "data": { ... }
}

// Error response
{
    "status": "error",
    "message": "Error description"
}

// Camera config response
{
    "camera_id": 1,
    "camera_name": "Front Door",
    "parameters": { ... }
}
```

### Adding New Endpoint

**Step 1: Add handler method (webu_*.cpp)**
```cpp
void cls_webu_json::endpoint_name()
{
    // Build response
    resp_body = "{";
    resp_body += "\"key\": \"value\"";
    resp_body += "}";

    // Set content type
    resp_type = "application/json";
}
```

**Step 2: Add routing (webu_ans.cpp)**
```cpp
if (uri == "/api/endpoint") {
    json->endpoint_name();
    return;
}
```

### Stream Handling
```cpp
// MJPEG boundary
#define MJPEG_BOUNDARY "--BoundaryString\r\n"

// Frame output
void send_frame(unsigned char *jpg, size_t jpg_size)
{
    // Boundary
    resp.append(MJPEG_BOUNDARY);

    // Headers
    resp.append("Content-Type: image/jpeg\r\n");
    resp.append("Content-Length: ");
    resp.append(std::to_string(jpg_size));
    resp.append("\r\n\r\n");

    // Data
    resp.append((char*)jpg, jpg_size);
    resp.append("\r\n");
}
```

---

## Motion Detection Patterns

### Frame Comparison
```cpp
int cls_alg::diff(cls_camera *cam)
{
    int diff_count = 0;
    unsigned char *ref = cam->imgs.ref;
    unsigned char *cur = cam->imgs.image_ring[idx].image_norm;

    for (int i = 0; i < cam->imgs.size_norm; i++) {
        int delta = abs(cur[i] - ref[i]);
        if (delta > cam->noise) {
            diff_count++;
        }
    }

    return diff_count;
}
```

### Threshold Tuning
```cpp
void cls_alg::threshold_tune(cls_camera *cam, int diff_count)
{
    // Adaptive threshold based on motion history
    if (diff_count > cam->cfg->threshold) {
        // Increase threshold to reduce false positives
        if (cam->threshold < cam->cfg->threshold_maximum) {
            cam->threshold++;
        }
    } else {
        // Decrease threshold for sensitivity
        if (cam->threshold > cam->cfg->threshold_minimum) {
            cam->threshold--;
        }
    }
}
```

### Smart Mask Update
```cpp
// Pixels that are frequently changing get masked out
void update_smart_mask(cls_camera *cam)
{
    for (int i = 0; i < cam->imgs.size_norm; i++) {
        if (changed[i]) {
            mask_count[i]++;
            if (mask_count[i] > MASK_THRESHOLD) {
                smart_mask[i] = 0;  // Mask this pixel
            }
        }
    }
}
```

---

## Video Recording Patterns

### FFmpeg Initialization
```cpp
void cls_movie::start()
{
    // Allocate format context
    avformat_alloc_output_context2(&oc, NULL, NULL, filename.c_str());

    // Find encoder
    codec = avcodec_find_encoder(AV_CODEC_ID_H264);

    // Create stream
    strm = avformat_new_stream(oc, codec);

    // Configure encoder
    ctx = avcodec_alloc_context3(codec);
    ctx->width = width;
    ctx->height = height;
    ctx->time_base = {1, framerate};
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    // Open encoder
    avcodec_open2(ctx, codec, NULL);

    // Open output file
    avio_open(&oc->pb, filename.c_str(), AVIO_FLAG_WRITE);

    // Write header
    avformat_write_header(oc, NULL);
}
```

### Frame Encoding
```cpp
void cls_movie::put_image()
{
    // Convert to encoder format if needed
    sws_scale(swsctx,
        (const uint8_t * const*)src_data, src_linesize,
        0, height,
        picture->data, picture->linesize);

    // Set timestamp
    picture->pts = frame_count++;

    // Encode
    avcodec_send_frame(ctx, picture);

    // Get encoded packet
    while (avcodec_receive_packet(ctx, pkt) == 0) {
        // Write to file
        av_interleaved_write_frame(oc, pkt);
        av_packet_unref(pkt);
    }
}
```

---

## Database Patterns

### Query Execution
```cpp
void cls_dbse::exec_sql(const std::string &sql)
{
    switch (dbtype) {
    case DBTYPE_SQLITE3:
        sqlite3_exec(sqlite_db, sql.c_str(), NULL, NULL, NULL);
        break;
    case DBTYPE_MARIADB:
    case DBTYPE_MYSQL:
        mysql_query(mysql_db, sql.c_str());
        break;
    case DBTYPE_PGSQL:
        PQexec(pgsql_db, sql.c_str());
        break;
    }
}
```

### Record Insertion
```cpp
void cls_dbse::insert_event(ctx_event &event)
{
    std::string sql = "INSERT INTO motion_events "
                      "(camera_id, event_time, event_type) "
                      "VALUES (" +
                      std::to_string(event.camera_id) + ", " +
                      "'" + event.time_str + "', " +
                      "'" + event.type + "')";

    exec_sql(sql);
}
```

---

## Error Handling Patterns

### Return Code Pattern
```cpp
enum CAPTURE_RESULT {
    CAPTURE_SUCCESS,
    CAPTURE_ERROR,
    CAPTURE_AGAIN,    // Retry needed
    CAPTURE_FATAL     // Unrecoverable
};

CAPTURE_RESULT cls_camera::capture()
{
    int ret = read_frame();

    if (ret == 0) {
        return CAPTURE_SUCCESS;
    } else if (ret == EAGAIN) {
        return CAPTURE_AGAIN;
    } else if (is_recoverable(ret)) {
        return CAPTURE_ERROR;
    } else {
        return CAPTURE_FATAL;
    }
}
```

### Retry Pattern
```cpp
void capture_with_retry(cls_camera *cam)
{
    int retry_count = 0;
    const int MAX_RETRIES = 3;

    while (retry_count < MAX_RETRIES) {
        CAPTURE_RESULT result = cam->capture();

        if (result == CAPTURE_SUCCESS) {
            break;
        } else if (result == CAPTURE_AGAIN) {
            // Brief wait, try again
            usleep(1000);
            continue;
        } else if (result == CAPTURE_ERROR) {
            retry_count++;
            motlog(LOG_WARNING, cam->camera_id,
                   "Capture error, retry %d/%d",
                   retry_count, MAX_RETRIES);
            sleep(1);
        } else {
            // Fatal, reinitialize
            cam->reinitialize();
            break;
        }
    }
}
```

---

## Memory Patterns

### RAII-style Management
```cpp
class cls_buffer {
public:
    cls_buffer(size_t size) {
        data = new unsigned char[size];
        buffer_size = size;
    }

    ~cls_buffer() {
        delete[] data;
    }

    unsigned char *data;
    size_t buffer_size;
};
```

### Pool Allocation
```cpp
// Pre-allocate ring buffer once
void allocate_ring_buffer(ctx_images *imgs, int count)
{
    imgs->image_ring = new ctx_image_data[count];
    imgs->ring_size = count;

    for (int i = 0; i < count; i++) {
        imgs->image_ring[i].image_norm = new unsigned char[frame_size];
    }
}
```

### libcamera Multi-Buffer Pool (Pi 5)
```cpp
// Memory-mapped buffer structure
struct ctx_imgmap {
    uint8_t *buf;
    int     bufsz;
};

// Request with buffer index tracking
struct ctx_reqinfo {
    libcamera::Request *request;
    int buffer_idx;
};

// Pool initialization (in start_req)
for (int i = 0; i < buffer_count; i++) {
    // Create request for each buffer
    std::unique_ptr<Request> request = camera->createRequest((uint64_t)i);
    request->addBuffer(stream, buffers[i].get());

    // Map memory for each buffer
    const FrameBuffer::Plane &plane = buffers[i]->planes()[0];
    membuf_pool[i].buf = (uint8_t*)mmap(NULL, plane.length,
        PROT_READ, MAP_SHARED, plane.fd.get(), plane.offset);
    membuf_pool[i].bufsz = plane.length;
}

// Frame retrieval with correct buffer
ctx_reqinfo req_info = req_queue.front();
req_queue.pop();
memcpy(dest, membuf_pool[req_info.buffer_idx].buf, size);
```

---

## String Formatting

### Format Substitution
```cpp
// Time-based filename formatting
std::string format_filename(const std::string &pattern,
                            time_t timestamp,
                            int camera_id)
{
    std::string result = pattern;
    struct tm *tm_info = localtime(&timestamp);

    // Replace %Y with year
    myreplace(result, "%Y", std::to_string(tm_info->tm_year + 1900));

    // Replace %m with month
    char buf[3];
    snprintf(buf, sizeof(buf), "%02d", tm_info->tm_mon + 1);
    myreplace(result, "%m", buf);

    // Replace %t with camera ID
    myreplace(result, "%t", std::to_string(camera_id));

    return result;
}
```

---

## Testing Patterns

### Unit Test Structure
```cpp
// Tests typically in separate test files
void test_threshold_calculation()
{
    cls_camera cam;

    // Setup
    cam.threshold = 50;
    cam.cfg->threshold_minimum = 10;
    cam.cfg->threshold_maximum = 100;

    // Execute
    cam.alg->threshold_tune(&cam, 100);  // High motion

    // Verify
    assert(cam.threshold > 50);  // Should increase
}
```

### Debug Build Features
```cpp
#ifdef DEBUG
    // Additional validation in debug builds
    assert(buffer != nullptr);
    assert(size > 0);
    motlog(LOG_DEBUG, 0, "Debug: buffer=%p, size=%zu", buffer, size);
#endif
```
