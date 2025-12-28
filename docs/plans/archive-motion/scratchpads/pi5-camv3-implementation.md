# Motion Pi 5 + Camera v3 Native Implementation Plan

**Status**: Draft
**Created**: 2025-12-06
**Target**: Raspberry Pi 5 with Camera Module 3 (IMX708)
**Scope**: Native libcamera integration, preserving Motion's motion detection algorithm

---

## Executive Summary

Update Motion's `cls_libcam` class to work natively with Pi 5 and Camera v3. The existing motion detection algorithm (`cls_alg`) is proven and will be preserved. All other components may be modified, removed, or enhanced as needed.

### Problem Statement

Motion's current libcamera implementation (`cls_libcam`) fails to detect cameras on Pi 5 because:
1. Uses `StreamRole::Viewfinder` instead of `VideoRecording`
2. Single buffer allocation causes pipeline issues on Pi 5's ISP (PiSP)
3. No filtering of USB cameras from CSI cameras
4. Hardcoded camera selection logic

### Solution Approach

Apply learnings from MediaMTX's working `rpiCamera` implementation to fix Motion's `cls_libcam`.

---

## Architecture Overview

### Components to KEEP (Unchanged)

| Component | File | Reason |
|-----------|------|--------|
| `cls_alg` | `alg.cpp/hpp` | Core motion detection algorithm - proven and solid |
| `cls_algsec` | `alg_sec.cpp/hpp` | Secondary detection methods |

### Components to MODIFY

| Component | File | Changes |
|-----------|------|---------|
| `cls_libcam` | `libcam.cpp/hpp` | Major rewrite for Pi 5 compatibility |
| `cls_config` | `conf.cpp/hpp` | Add new Pi Camera parameters |
| `cls_movie` | `movie.cpp/hpp` | Ensure h264_v4l2m2m works with new capture |
| `cls_camera` | `camera.cpp/hpp` | Minor adjustments for new frame delivery |

### Components: Scope Decisions

| Component | File | Decision | Notes |
|-----------|------|----------|-------|
| `cls_v4l2cam` | `video_v4l2.cpp/hpp` | **KEEP DISABLED** | Keep code, don't compile by default |
| `cls_netcam` | `netcam.cpp/hpp` | **KEEP OPTIONAL** | Available if user enables |
| `cls_sound` | `sound.cpp/hpp` | **KEEP** | Maintain audio detection |
| `cls_dbse` (SQLite) | `dbse.cpp/hpp` | **KEEP SQLite ONLY** | Already in project, no new DB options |
| MySQL/MariaDB/PostgreSQL | `dbse.cpp/hpp` | **REMOVE** | Focus on SQLite only |

---

## Phase 1: Critical libcamera Fixes

**Priority**: HIGH
**Effort**: 1-2 days
**Goal**: Get camera detection and basic frame capture working

### 1.1 Stream Role Change

**File**: `src/libcam.cpp`
**Location**: `start_config()` function, line ~528

```cpp
// CURRENT (broken):
config = camera->generateConfiguration({ StreamRole::Viewfinder });

// FIXED:
config = camera->generateConfiguration({ StreamRole::VideoRecording });
```

**Rationale**: `VideoRecording` is the correct role for video capture. Pi 5's PiSP requires this for proper ISP pipeline initialization.

### 1.2 Buffer Count Increase

**File**: `src/libcam.cpp`
**Location**: `start_config()` function, line ~534

```cpp
// CURRENT:
config->at(0).bufferCount = 1;

// FIXED:
config->at(0).bufferCount = 4;  // Configurable via libcam_buffer_count
```

**Rationale**: Multiple buffers allow the ISP pipeline to queue frames, preventing drops.

### 1.3 Request Buffer Pool

**File**: `src/libcam.cpp`
**Location**: `start_req()` function

Current implementation creates only 1 request. Need to create a pool matching buffer count:

```cpp
// Create multiple requests for the buffer pool
for (unsigned int i = 0; i < config->at(0).bufferCount; i++) {
    std::unique_ptr<Request> request = camera->createRequest();
    if (!request) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "Create request error.");
        return -1;
    }
    // Add buffer for this request...
    requests.push_back(std::move(request));
}
```

### 1.4 Camera Filtering

**File**: `src/libcam.cpp`
**Location**: New function + `start_mgr()` modification

```cpp
// New function to filter cameras
std::vector<std::shared_ptr<libcamera::Camera>> cls_libcam::get_pi_cameras()
{
    std::vector<std::shared_ptr<libcamera::Camera>> pi_cams;

    for (const auto& cam : cam_mgr->cameras()) {
        std::string id = cam->id();
        // Filter out USB cameras (keep CSI/built-in only)
        if (id.find("usb") == std::string::npos) {
            pi_cams.push_back(cam);
        }
    }

    // Sort by ID for consistent ordering
    std::sort(pi_cams.begin(), pi_cams.end(),
        [](const auto& a, const auto& b) {
            return a->id() < b->id();
        });

    return pi_cams;
}
```

### 1.5 Enhanced Camera Selection

**File**: `src/libcam.cpp`
**Location**: `start_mgr()` function, lines ~235-247

Support multiple selection methods:
- `camera0`, `camera1` - by index
- `imx708`, `imx219` - by sensor model
- Auto-detect first available

```cpp
if (cam->cfg->libcam_device == "auto") {
    auto pi_cams = get_pi_cameras();
    if (pi_cams.empty()) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "No Pi cameras found");
        return -1;
    }
    camera = pi_cams[0];
} else if (cam->cfg->libcam_device.substr(0, 6) == "camera") {
    int idx = std::stoi(cam->cfg->libcam_device.substr(6));
    auto pi_cams = get_pi_cameras();
    if (idx >= (int)pi_cams.size()) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "Camera index %d not found", idx);
        return -1;
    }
    camera = pi_cams[idx];
} else {
    // Try matching by sensor model (e.g., "imx708")
    camera = find_camera_by_model(cam->cfg->libcam_device);
}
```

---

## Phase 2: Configuration Enhancements

**Priority**: HIGH
**Effort**: 1 day
**Goal**: Add configurable parameters for Pi Camera

### 2.1 New Configuration Parameters

**File**: `src/conf.hpp`

Add to config structure:
```cpp
// Pi Camera specific
int libcam_buffer_count;        // Buffer pool size (default: 4)
std::string libcam_role;        // "video" or "viewfinder"
bool libcam_hdr;                // Enable HDR mode
int libcam_af_mode;             // 0=manual, 1=auto, 2=continuous
int libcam_af_range;            // 0=normal, 1=macro, 2=full
float libcam_lens_position;     // Manual focus position
int libcam_noise_reduction;     // 0=off, 1=fast, 2=high_quality
```

**File**: `src/conf.cpp`

Add parameter definitions and defaults.

### 2.2 Web UI Integration

**Files**: `src/webu_html.cpp`, `src/webu_json.cpp`

Add Pi Camera section to web configuration interface.

---

## Phase 3: Frame Delivery Optimization

**Priority**: MEDIUM
**Effort**: 2 days
**Goal**: Efficient frame handoff to motion detection

### 3.1 DMA Buffer Optimization

Study MediaMTX approach of using DMA heap allocation:
```cpp
// MediaMTX approach (reference):
int dma_heap_fd = open("/dev/dma_heap/system", O_RDWR);
struct dma_heap_allocation_data alloc = {
    .len = buffer_size,
    .fd_flags = O_RDWR,
};
ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
```

Evaluate if this provides performance benefits over current mmap approach.

### 3.2 Frame Queue Management

Implement proper multi-buffer request cycling:
```cpp
void cls_libcam::req_complete(Request *request) {
    if (request->status() == Request::RequestCancelled) {
        return;
    }

    // Extract frame data
    // ...

    // Queue to processing
    req_queue.push(request);

    // Immediately requeue if not stopping
    if (running) {
        request->reuse(Request::ReuseBuffers);
        camera->queueRequest(request);
    }
}
```

---

## Phase 4: Camera v3 Feature Support

**Priority**: MEDIUM
**Effort**: 2-3 days
**Goal**: Expose Camera v3 specific features

### 4.1 Autofocus Controls

Most AF controls already exist in `config_control_item()`. Verify they work:
- `AfMode` (0=manual, 1=auto, 2=continuous)
- `AfRange` (0=normal, 1=macro, 2=full)
- `AfSpeed` (0=normal, 1=fast)
- `LensPosition` (manual focus)

### 4.2 HDR Support

Add HDR mode control:
```cpp
if (cam->cfg->libcam_hdr) {
    // Set HDR mode via V4L2 control (if available)
    // or libcamera control
}
```

### 4.3 Wide Angle Lens Handling

Camera v3 Wide has different FOV. Ensure:
- Proper aspect ratio handling
- No distortion in motion detection mask

---

## Phase 5: Encoder Integration Verification

**Priority**: MEDIUM
**Effort**: 1 day
**Goal**: Verify H.264 hardware encoding works

### 5.1 h264_v4l2m2m Compatibility

Verify `cls_movie` properly encodes frames from new capture path:
- YUV420 format compatibility
- Timestamp handling
- Bitrate control

### 5.2 Frame Format Verification

Ensure captured frames match encoder expectations:
```cpp
// Capture provides YUV420P (planar)
// Encoder expects YUV420P
// Verify no conversion needed
```

---

## Phase 6: Build System Updates

**Priority**: HIGH
**Effort**: 0.5 days
**Goal**: Ensure proper compilation on Pi 5

### 6.1 configure.ac Updates

Verify libcamera detection works with Bookworm's packages:
```bash
# Required packages on Pi 5:
sudo apt install libcamera-dev libcamera-tools
```

### 6.2 Compiler Flags

Ensure C++17 compatibility and Pi 5 optimizations:
```makefile
CXXFLAGS += -std=c++17 -mcpu=cortex-a76  # Pi 5 CPU
```

---

## Phase 7: Testing & Validation

**Priority**: HIGH
**Effort**: 2-3 days
**Goal**: Comprehensive testing on actual hardware

### 7.1 Unit Tests

Create test cases for:
- Camera detection
- Frame capture
- Motion detection accuracy
- Recording functionality

### 7.2 Integration Tests

- Full pipeline: capture → detect → record
- Web interface functionality
- Long-running stability (24+ hours)

### 7.3 Performance Benchmarks

Measure and document:
- Frame rate achieved (target: 30fps @ 1080p)
- Motion detection latency
- CPU/memory usage
- Encoder performance

---

## Implementation Order

| Step | Phase | Description | Dependency |
|------|-------|-------------|------------|
| 1 | 1.1 | Stream role fix | None |
| 2 | 1.2 | Buffer count increase | Step 1 |
| 3 | 1.3 | Request buffer pool | Step 2 |
| 4 | 1.4 | Camera filtering | Step 3 |
| 5 | 1.5 | Enhanced selection | Step 4 |
| 6 | 6.1 | Build system verify | Step 5 |
| 7 | 7.1 | Basic testing | Step 6 |
| 8 | 2.x | Configuration params | Step 7 |
| 9 | 3.x | Frame optimization | Step 8 |
| 10 | 4.x | Camera v3 features | Step 9 |
| 11 | 5.x | Encoder verification | Step 10 |
| 12 | 7.2-3 | Full testing | Step 11 |

---

## Files Summary

### Must Modify

| File | Priority | Changes |
|------|----------|---------|
| `src/libcam.cpp` | Critical | Stream role, buffers, camera selection |
| `src/libcam.hpp` | Critical | New member functions |
| `src/conf.cpp` | High | New parameters |
| `src/conf.hpp` | High | New config struct members |

### May Modify

| File | Priority | Changes |
|------|----------|---------|
| `src/camera.cpp` | Medium | Frame handling adjustments |
| `src/movie.cpp` | Low | Verify encoder compatibility |
| `src/webu_*.cpp` | Low | Pi Camera UI section |
| `configure.ac` | Medium | Build system updates |

### Unchanged (Core Motion Detection)

| File | Reason |
|------|--------|
| `src/alg.cpp` | Core algorithm - proven |
| `src/alg.hpp` | Core algorithm interface |
| `src/alg_sec.cpp` | Secondary detection |
| `src/alg_sec.hpp` | Secondary detection interface |

---

## Success Criteria

1. **Camera Detection**: `motion` finds Camera v3 on startup
2. **Frame Capture**: Consistent 30fps @ 1920x1080
3. **Motion Detection**: Algorithm performs identically to working V4L2 path
4. **Recording**: H.264 files playable and properly timestamped
5. **Stability**: 24+ hour operation without crashes or memory leaks
6. **Web Interface**: Live view and configuration functional

---

## References

- [MediaMTX rpiCamera implementation](https://github.com/bluenviron/mediamtx-rpicamera)
- [Raspberry Pi Camera Documentation](https://www.raspberrypi.com/documentation/computers/camera_software.html)
- [libcamera API Documentation](https://libcamera.org/api-html/)
- [Pi 5 Camera Issues Forum](https://forums.raspberrypi.com/viewtopic.php?t=371329)

---

## Scratchpad Notes

### Decisions Made (2025-12-06)

| Question | Decision |
|----------|----------|
| V4L2 support | **Keep disabled** - code stays, not compiled by default |
| Netcam support | **Keep optional** - available if user needs it |
| Database support | **SQLite only** - already in project, remove MySQL/MariaDB/PostgreSQL |
| Audio/Sound support | **Keep** - maintain audio detection capability |

### Open Questions

1. What minimum libcamera version should we require? (Bookworm default?)
2. Should we target 32-bit or 64-bit OS? (64-bit recommended for Pi 5)

### Research Needed

1. Exact DMA heap allocation benefits on Pi 5
2. PiSP-specific tuning file requirements
3. HDR mode availability on Camera v3
4. libcamera version differences between Bullseye and Bookworm

### Test Environment

- Hardware: Raspberry Pi 5 (4GB or 8GB)
- Camera: Camera Module 3 (IMX708) or Wide variant
- OS: Raspberry Pi OS Bookworm (64-bit recommended)
- libcamera: System package from apt

### Build Configuration (Target)

```bash
./configure \
  --with-libcam \
  --without-v4l2 \
  --with-sqlite3 \
  --without-mysql \
  --without-mariadb \
  --without-pgsql
```

---

## Comparison: Original Plan vs Revised Plan

| Aspect | Original Plan (MotionUpdate-Pi5-CamV3.md) | This Plan |
|--------|-------------------------------------------|-----------|
| Approach | Build new app from scratch | Modify existing Motion fork |
| New code | ~35K lines estimated | ~500-1000 lines changes |
| Motion detection | Rewrite algorithm | **Preserve `cls_alg` exactly** |
| HTTP server | New embedded server | Keep existing `cls_webu` |
| Encoding | New FFmpeg integration | Verify existing `cls_movie` |
| Build system | New CMake | Keep existing autotools |
| Camera capture | New libcamera wrapper | **Fix existing `cls_libcam`** |
| Config system | New .ini/.toml parser | Keep existing `cls_config` |

### Why This Plan is Better

1. **Lower risk** - modifying working code vs building from scratch
2. **Faster** - weeks vs months of development
3. **Proven components** - motion detection, web UI, encoding already work
4. **Easier maintenance** - smaller diff to upstream Motion

---

*Last Updated: 2025-12-06*
