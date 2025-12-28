# Motion - AI Agent Navigation Guide

Quick-reference documentation for AI agents to navigate, understand, and modify this codebase.

## Project Identity

| Attribute | Value |
|-----------|-------|
| **Type** | Video motion detection & surveillance system |
| **Language** | C++17 |
| **Build** | GNU Autotools (autoconf/automake) |
| **Size** | ~35,000 lines of code |
| **License** | GPL v3+ |

## Directory Map

```
motion/
├── src/                    # All source code (*.hpp, *.cpp)
├── data/                   # Config templates, systemd, web UI resources
│   └── webcontrol/        # Static web UI files
├── doc/                   # Documentation (HTML guides, plans)
├── man/                   # Manual pages
├── po/                    # Translations (15 languages)
├── scripts/               # Build helper scripts
├── configure.ac           # Autoconf main config
└── Makefile.am           # Automake main config
```

## Class Naming Convention

All classes use `cls_` prefix. Key classes:

| Class | File | Purpose | LOC |
|-------|------|---------|-----|
| `cls_motapp` | motion.cpp | Main application controller | ~700 |
| `cls_camera` | camera.cpp | Camera & motion detection | ~2,100 |
| `cls_config` | conf.cpp | Configuration management | ~4,500 |
| `cls_alg` | alg.cpp | Motion detection algorithms | ~1,400 |
| `cls_webu` | webu.cpp | Web server main | ~600 |
| `cls_netcam` | netcam.cpp | Network camera handling | ~2,400 |
| `cls_movie` | movie.cpp | Video recording | ~1,800 |
| `cls_dbse` | dbse.cpp | Database abstraction | ~1,400 |

## Quick File Lookup by Feature

### Motion Detection
- `src/alg.cpp` - Core detection algorithms (diff, stddev, threshold)
- `src/alg_sec.cpp` - Secondary/advanced algorithms
- `src/camera.cpp` - Camera processing loop

### Video Input
- `src/video_v4l2.cpp` - V4L2 (local cameras)
- `src/libcam.cpp` - libcamera support (Pi 5 optimized with multi-buffer pool)
- `src/netcam.cpp` - Network cameras (RTSP/HTTP)

### Video Output
- `src/movie.cpp` - Recording to file (FFmpeg)
- `src/picture.cpp` - JPEG snapshots
- `src/draw.cpp` - Overlays & annotations

### Web Interface
- `src/webu.cpp` - HTTP server core
- `src/webu_ans.cpp` - Response routing
- `src/webu_html.cpp` - HTML pages
- `src/webu_json.cpp` - JSON API
- `src/webu_stream.cpp` - MJPEG streaming
- `src/webu_mpegts.cpp` - MPEG-TS streaming

### Configuration
- `src/conf.cpp` - Parameter handling (~600 params)
- `data/motion-dist.conf.in` - Default config template

### Database
- `src/dbse.cpp` - SQL abstraction (SQLite/MySQL/MariaDB/PostgreSQL)

### Audio
- `src/sound.cpp` - Audio capture & analysis

## Data Structure Prefixes

| Prefix | Type | Example |
|--------|------|---------|
| `cls_` | Class | `cls_camera`, `cls_config`, `cls_libcam` |
| `ctx_` | Context struct | `ctx_image_data`, `ctx_coord`, `ctx_imgmap`, `ctx_reqinfo` |
| `PARM_` | Parameter enum | `PARM_CAT_00`, `PARM_TYP_STRING` |

## Entry Points

### Application Start
```
main() [motion.cpp]
  └── cls_motapp::init()
        ├── Configuration loading
        ├── Camera initialization
        ├── Web server startup
        └── Database connection
```

### Camera Processing Loop
```
cls_camera::handler()
  ├── capture() - Get frame from source
  ├── detection() - Run motion algorithms
  ├── ring_process() - Handle buffers
  ├── picture handling - Snapshots
  └── movie handling - Recording
```

### Web Request Flow
```
HTTP Request → libmicrohttpd
  └── cls_webu_ans::process_answer()
        ├── cls_webu_html - HTML responses
        ├── cls_webu_json - JSON API
        ├── cls_webu_stream - Video streams
        └── cls_webu_file - Static files
```

## Key Patterns

### Ring Buffer (Images)
Camera uses circular buffer for pre/post capture:
- `ring_first` - Oldest available frame
- `ring_last` - Newest frame
- `ring_count` - Total buffer size

### Triple Buffer (Network)
Network cameras use three buffers:
- `latest` - Most recent complete frame
- `receiving` - Currently being written
- `processing` - Being read by consumer

### Multi-Buffer Pool (libcamera/Pi 5)
libcamera uses configurable buffer pool (2-8 buffers, default 4):
- `ctx_reqinfo` - Tracks request with associated buffer index
- `membuf_pool` - Vector of memory-mapped buffers
- `req_queue` - Queue of completed requests with buffer indices
- Prevents pipeline stalls on Pi 5 PiSP

### Configuration Parameter
Each parameter defined in `config_parms[]` array:
```cpp
{
  "parameter_name",     // Config file key
  PARM_TYP_INT,        // Type (STRING/INT/BOOL/LIST)
  PARM_CAT_03,         // Category (19 total)
  WEBUI_LEVEL_LIMITED  // Web UI visibility
}
```

## Search Strategies

### Find where a feature is implemented
```bash
# Search for function/method
grep -rn "function_name" src/

# Search class definition
grep -rn "class cls_" src/

# Search enum/constant
grep -rn "ENUM_NAME" src/
```

### Find configuration handling
```bash
# Parameter defined in
grep -rn '"param_name"' src/conf.cpp

# Parameter used in
grep -rn "param_name" src/*.cpp
```

### Find web API endpoint
```bash
# Endpoint handler
grep -rn '"endpoint"' src/webu*.cpp
```

## Common Modification Patterns

### Add new configuration parameter
1. Add to `config_parms[]` in `src/conf.cpp`
2. Add member variable to appropriate class
3. Add edit function in `conf.cpp`
4. Update config template in `data/motion-dist.conf.in`

### Add new web API endpoint
1. Add handler in `src/webu_ans.cpp` or specific handler file
2. Add routing logic in `process_answer()`
3. Implement response formatting

### Modify motion detection
1. Core algorithms in `src/alg.cpp`
2. Key functions: `diff()`, `stddev()`, `threshold_tune()`
3. Reference frame logic in `ref_frame_update()`

### Add new camera source type
1. Create new class inheriting video capture pattern
2. Add to camera type enum in `camera.hpp`
3. Integrate in `cls_camera::capture()`

### Modify libcamera (Pi 5)
1. Camera selection in `src/libcam.cpp` - `start_mgr()`
2. Buffer configuration in `start_config()` - uses `libcam_buffer_count`
3. USB filtering in `get_pi_cameras()`
4. Model matching in `find_camera_by_model()`

## Build Commands

```bash
# Standard build
./configure && make -j6

# With specific features
./configure --with-opencv --with-sqlite3

# Without optional features
./configure --without-mariadb --without-mysql

# Full test matrix (maintainer)
make maintainer-check
```

## Signal Handling

| Signal | Enum | Action |
|--------|------|--------|
| SIGTERM/SIGINT | MOTION_SIGNAL_SIGTERM | Graceful shutdown |
| SIGHUP | MOTION_SIGNAL_SIGHUP | Reload configuration |
| SIGUSR1 | MOTION_SIGNAL_USR1 | Force motion event |
| SIGALRM | MOTION_SIGNAL_ALARM | Timer operations |

## Threading Model

- Main thread: Application control
- Per-camera thread: Capture & detection
- Web server: libmicrohttpd thread pool
- Movie encoding: Separate thread
- Sound processing: Dedicated thread

## Related Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) - Detailed system design
- [PATTERNS.md](PATTERNS.md) - Code patterns & idioms
- [MODIFICATION_GUIDE.md](MODIFICATION_GUIDE.md) - Step-by-step modification guides
- [motion_config.html](motion_config.html) - Configuration reference
- [motion_guide.html](motion_guide.html) - User guide
