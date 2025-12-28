# Motion - Architecture Reference

Detailed architecture documentation for AI agents modifying this codebase.

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        cls_motapp                                │
│                    (Main Application)                            │
├─────────┬─────────┬─────────┬─────────┬─────────┬──────────────┤
│         │         │         │         │         │              │
▼         ▼         ▼         ▼         ▼         ▼              │
cls_camera cls_webu cls_dbse cls_sound cls_schedule cls_allcam   │
(per cam)  (web UI)  (database) (audio)  (timing)   (multi-view) │
│                                                                 │
├── cls_alg ◄────────────────────────────────────────────────────┤
├── cls_movie                                                     │
├── cls_picture                                                   │
├── cls_netcam / cls_video_v4l2 / cls_libcam                     │
└── cls_draw                                                      │
```

## Core Classes Detailed

### cls_motapp (motion.hpp/cpp)
**Purpose**: Main application controller, lifecycle management

**Key Members**:
```cpp
std::vector<cls_camera*> cam_list;  // Active cameras
std::vector<cls_sound*> snd_list;   // Sound devices
cls_webu *webu;                      // Web server
cls_dbse *dbse;                      // Database handler
cls_schedule *schedule;              // Scheduling
cls_allcam *allcam;                  // Multi-camera aggregation
ctx_params *conf_src;                // Source config
cls_config *cfg;                     // Parsed configuration
```

**Key Methods**:
| Method | Purpose |
|--------|---------|
| `init()` | Application startup, load config, start services |
| `deinit()` | Graceful shutdown, cleanup resources |
| `signal_process()` | Handle Unix signals |
| `check_devices()` | Monitor device availability |
| `camera_add()` | Add camera at runtime |
| `camera_delete()` | Remove camera at runtime |

**Lifecycle**:
```
main()
  → cls_motapp::init()
    → Load configuration
    → Initialize cameras (cls_camera for each)
    → Start web server (cls_webu)
    → Connect database (cls_dbse)
    → Start sound if enabled (cls_sound)
  → Main loop (signal handling, device monitoring)
  → cls_motapp::deinit()
```

---

### cls_camera (camera.hpp/cpp)
**Purpose**: Individual camera management, motion detection, recording

**Key Members**:
```cpp
// Identity
int camera_id;
std::string camera_name;
enum CAMERA_TYPE camera_type;  // V4L2, LIBCAM, NETCAM

// Image handling
ctx_images imgs;               // Ring buffer of frames
ctx_image_data *current_image; // Current processing frame

// Detection state
bool detecting_motion;
int threshold;
int noise;

// Components
cls_alg *alg;           // Motion detection
cls_movie *movie;       // Video recording
cls_picture *picture;   // Snapshot handling
cls_netcam *netcam;     // Network camera (if applicable)
cls_video_v4l2 *v4l2;   // V4L2 camera (if applicable)
cls_libcam *libcam;     // libcamera (if applicable)
cls_draw *draw;         // Overlays
```

**Processing Pipeline**:
```cpp
handler() {
    while (running) {
        capture();      // Get frame from source
        detection();    // Run motion detection
        ring_process(); // Manage ring buffer
        // Trigger picture/movie if motion detected
    }
}
```

**Image Ring Buffer**:
```cpp
struct ctx_images {
    ctx_image_data *image_ring;  // Circular buffer
    int ring_size;               // Total capacity
    int ring_in;                 // Write position
    int ring_out;                // Read position

    // Special indices
    int image_preview;           // Preview frame index
    int image_motion;            // Motion detected frame
};
```

---

### cls_config (conf.hpp/cpp)
**Purpose**: Configuration parameter management

**Parameter Categories** (PARM_CAT_00 through PARM_CAT_18):
| Category | Description | Examples |
|----------|-------------|----------|
| 00 | System | daemon, pid_file, log_level |
| 01 | Camera | camera_name, camera_id |
| 02 | Source | device, video_device, libcam_device, libcam_buffer_count |
| 03 | Image | width, height, framerate |
| 04 | Overlay | text_left, text_right |
| 05 | Method | detection method options |
| 06 | Masks | mask_file, smart_mask |
| 07 | Detection | threshold, noise_level |
| 08 | Scripts | on_motion_detected, on_event_start |
| 09 | Picture | snapshot settings |
| 10 | Movies | movie_filename, codec |
| 11 | Timelapse | timelapse settings |
| 12 | Pipes | pipe output settings |
| 13 | WebControl | webcontrol_port, webcontrol_localhost |
| 14 | Streams | stream settings |
| 15 | Database | database_type, database_dbname |
| 16 | SQL | sql_query settings |
| 17 | Tracking | tracking settings |
| 18 | Sound | sound settings |

**Parameter Definition Structure**:
```cpp
struct ctx_parm {
    std::string parm_name;      // Config key
    enum PARM_TYP parm_type;    // STRING, INT, BOOL, LIST, ARRAY
    enum PARM_CAT parm_cat;     // Category (00-18)
    enum WEBUI_LEVEL webui_level; // Web UI visibility
};
```

**Type System**:
| Type | Enum | Usage |
|------|------|-------|
| PARM_TYP_STRING | String values | paths, names |
| PARM_TYP_INT | Integer values | ports, counts |
| PARM_TYP_BOOL | Boolean (on/off) | feature flags |
| PARM_TYP_LIST | Predefined choices | codecs, methods |
| PARM_TYP_ARRAY | Multiple values | camera lists |

---

### cls_alg (alg.hpp/cpp)
**Purpose**: Motion detection algorithms

**Key Algorithms**:
```cpp
// Frame difference detection
int diff(cls_camera *cam);

// Standard deviation analysis
void stddev(cls_camera *cam);

// Motion location calculation
void location(cls_camera *cam);

// Reference frame management
void ref_frame_update(cls_camera *cam, bool full_update);

// Auto-tuning
void threshold_tune(cls_camera *cam, int diff_count);
void noise_tune(cls_camera *cam);
```

**Detection Flow**:
```
1. Compare current frame to reference frame
2. Count changed pixels (above threshold)
3. If changes > threshold → motion detected
4. Calculate center of motion (location)
5. Update smart mask if enabled
6. Tune threshold/noise based on results
```

---

### cls_webu & Related (webu*.hpp/cpp)
**Purpose**: HTTP server and web interface

**Component Hierarchy**:
```
cls_webu (main server)
├── cls_webu_ans (request routing)
│   ├── cls_webu_html (HTML responses)
│   ├── cls_webu_json (JSON API)
│   ├── cls_webu_text (Plain text)
│   ├── cls_webu_post (POST handling)
│   ├── cls_webu_stream (MJPEG)
│   ├── cls_webu_mpegts (MPEG-TS)
│   ├── cls_webu_getimg (Image retrieval)
│   └── cls_webu_file (Static files)
```

**Request Types**:
| URL Pattern | Handler | Response |
|-------------|---------|----------|
| `/` | webu_html | HTML dashboard |
| `/config/*` | webu_json | JSON config API |
| `/camera/*/stream` | webu_stream | MJPEG video |
| `/camera/*/ts` | webu_mpegts | MPEG-TS video |
| `/action/*` | webu_post | Control actions |

---

### cls_libcam (libcam.hpp/cpp)
**Purpose**: libcamera support for Raspberry Pi cameras

**Multi-Buffer Pool Pattern** (Pi 5 optimized):
```cpp
// Request with associated buffer index for multi-buffer support
struct ctx_reqinfo {
    libcamera::Request *request;
    int buffer_idx;
};

// Buffer tracking
std::queue<ctx_reqinfo>            req_queue;      // Completed requests with buffer index
std::vector<ctx_imgmap>            membuf_pool;    // Memory-mapped buffer pool
```

**Camera Selection**:
```cpp
// Three selection modes supported
"auto"     // Auto-detect first Pi camera (filters out USB/UVC)
"camera0"  // Specific camera by index (0, 1, etc.)
"imx708"   // By sensor model name
```

**Key Configuration**:
| Parameter | Default | Range | Purpose |
|-----------|---------|-------|---------|
| `libcam_device` | auto | auto/cameraX/model | Camera selection |
| `libcam_buffer_count` | 4 | 2-8 | Buffer pool size |

**Pi 5 Specific**:
- Uses `StreamRole::VideoRecording` (required for PiSP pipeline)
- Multi-buffer pool prevents pipeline stalls
- USB/UVC cameras filtered from camera list

---

### cls_netcam (netcam.hpp/cpp)
**Purpose**: Network camera (RTSP/HTTP) handling

**Triple Buffer Pattern**:
```cpp
// Three buffers for lock-minimal operation
netcam_buff *latest;      // Last complete frame (consumer reads)
netcam_buff *receiving;   // Currently being written (producer)
netcam_buff *processing;  // Being decoded/processed

// Swap pattern
void swap_buffers() {
    // Rotate: latest → processing → receiving → latest
}
```

**Connection Types**:
```cpp
enum NETCAM_TYPE {
    NETCAM_V4L2,     // Video4Linux (used for some paths)
    NETCAM_RTSP,     // RTSP protocol
    NETCAM_HTTP,     // HTTP/MJPEG
    NETCAM_FILE      // File input
};
```

---

### cls_movie (movie.hpp/cpp)
**Purpose**: Video recording via FFmpeg

**Key Operations**:
```cpp
void start();           // Initialize encoder, open output file
void stop();            // Finalize and close file
void put_image();       // Encode and write frame
void reset_start_time(); // Handle timelapse continuation
```

**FFmpeg Integration**:
```cpp
AVFormatContext *oc;      // Output format context
AVCodecContext *ctx;      // Encoder context
AVStream *strm;           // Video stream
AVFrame *picture;         // Current frame
AVPacket *pkt;            // Encoded packet
```

---

### cls_dbse (dbse.hpp/cpp)
**Purpose**: Database abstraction layer

**Supported Backends**:
| Backend | Compile Flag | Library |
|---------|-------------|---------|
| SQLite3 | `--with-sqlite3` | libsqlite3 |
| MariaDB | `--with-mariadb` | libmariadb |
| MySQL | `--with-mysql` | libmysqlclient |
| PostgreSQL | `--with-pgsql` | libpq |

**Key Tables**:
```sql
-- Motion events
CREATE TABLE motion_events (
    id, camera_id, event_time, event_type, ...
);

-- Video files
CREATE TABLE video_files (
    id, camera_id, filename, start_time, duration, ...
);
```

---

## Data Flow Diagrams

### Frame Capture to Motion Detection
```
[Camera Source]
      │
      ▼
┌──────────────┐    ┌──────────────┐
│ cls_camera   │───▶│ Ring Buffer  │
│ capture()    │    │ (ctx_images) │
└──────────────┘    └──────────────┘
      │                    │
      ▼                    ▼
┌──────────────┐    ┌──────────────┐
│ cls_alg      │◀───│ Current      │
│ diff()       │    │ Frame        │
└──────────────┘    └──────────────┘
      │
      ▼
┌──────────────────────────────────┐
│ Motion Detected?                 │
├──────────────┬───────────────────┤
│ YES          │ NO                │
▼              ▼                   │
┌─────────┐ ┌─────────┐           │
│ Picture │ │ Movie   │           │
│ Capture │ │ Record  │           │
└─────────┘ └─────────┘           │
```

### Web Request Processing
```
[HTTP Request]
      │
      ▼
┌──────────────┐
│ libmicrohttpd│
└──────────────┘
      │
      ▼
┌──────────────┐
│ cls_webu_ans │
│ route request│
└──────────────┘
      │
      ├──────────┬──────────┬──────────┐
      ▼          ▼          ▼          ▼
┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐
│ HTML    │ │ JSON    │ │ Stream  │ │ Static  │
│ Handler │ │ Handler │ │ Handler │ │ Files   │
└─────────┘ └─────────┘ └─────────┘ └─────────┘
```

---

## Thread Safety

### Mutex Usage Patterns
```cpp
// Camera image access
pthread_mutex_lock(&cam->mutex_img);
// ... access image data ...
pthread_mutex_unlock(&cam->mutex_img);

// Ring buffer operations
pthread_mutex_lock(&cam->mutex_ring);
// ... ring buffer operations ...
pthread_mutex_unlock(&cam->mutex_ring);

// Web streaming
pthread_mutex_lock(&stream->mutex);
// ... stream data access ...
pthread_mutex_unlock(&stream->mutex);
```

### Thread Responsibilities
| Thread | Class | Purpose |
|--------|-------|---------|
| Main | cls_motapp | Signal handling, lifecycle |
| Camera N | cls_camera | Capture, detect, record |
| Web pool | cls_webu | HTTP request handling |
| Movie | cls_movie | Video encoding |
| Sound | cls_sound | Audio processing |

---

## Configuration Flow

```
┌──────────────────────────┐
│ Config Files             │
│ - motion.conf            │
│ - camera1.conf           │
└──────────────────────────┘
           │
           ▼
┌──────────────────────────┐
│ cls_config               │
│ - Parse files            │
│ - Validate parameters    │
│ - Apply defaults         │
└──────────────────────────┘
           │
           ▼
┌──────────────────────────┐
│ Runtime Objects          │
│ - cls_camera.cfg         │
│ - cls_sound.cfg          │
└──────────────────────────┘
           │
           ▼
┌──────────────────────────┐
│ Web UI (optional)        │
│ - View/modify at runtime │
│ - Changes apply live     │
└──────────────────────────┘
```

---

## Memory Management

### Image Buffers
- Pre-allocated ring buffer based on `pre_capture` + `post_capture` settings
- Each `ctx_image_data` contains fixed-size image buffer
- Buffers reused cyclically, never reallocated during operation

### Network Buffers
- Triple-buffer pattern prevents allocation during streaming
- Buffers sized to maximum expected frame size
- Grow-only strategy for unexpected large frames

### String Handling
- C++ `std::string` used throughout modern code
- Legacy C strings (`char*`) in some FFmpeg interfaces

---

## Error Handling

### Logging Levels
```cpp
enum LOG_LEVEL {
    LOG_EMERG,   // System unusable
    LOG_ALERT,   // Immediate action required
    LOG_CRIT,    // Critical conditions
    LOG_ERR,     // Error conditions
    LOG_WARNING, // Warning conditions
    LOG_NOTICE,  // Normal but significant
    LOG_INFO,    // Informational
    LOG_DEBUG    // Debug messages
};
```

### Common Error Patterns
```cpp
// Device error recovery
if (capture_failed) {
    log(LOG_ERR, "Capture failed, attempting reconnect");
    close_device();
    sleep(retry_interval);
    open_device();
}

// Configuration validation
if (param_value < min_valid) {
    log(LOG_WARNING, "Value out of range, using default");
    param_value = default_value;
}
```
