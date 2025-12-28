# Motion Pi 5 + Camera v3 Implementation Design

**Status**: DRAFT - Awaiting Approval
**Version**: 1.0
**Created**: 2025-12-06
**Target**: Raspberry Pi 5 with Camera Module 3 (IMX708)

---

## 1. Executive Summary

This design document details the technical approach to update Motion's `cls_libcam` class for native Pi 5 and Camera v3 support. The design preserves Motion's proven `cls_alg` motion detection algorithm while addressing critical libcamera integration issues.

### Key Decisions

| Decision | Rationale |
|----------|-----------|
| Preserve `cls_alg` exactly | Proven, working algorithm - no changes needed |
| Fix `cls_libcam` vs rewrite | Lower risk, faster delivery, smaller diff |
| SQLite only for database | Already in project, simplify maintenance |
| Software H.264 encoding | Pi 5 lacks H.264 hardware encoder |
| Consider HEVC for output | Pi 5 has HEVC hardware decoder for playback |

---

## 2. Requirements Analysis

### 2.1 Stated Requirements (from scratchpad)

| REQ-ID | Requirement | Priority |
|--------|-------------|----------|
| REQ-1 | Camera detection on Pi 5 with Camera v3 | Critical |
| REQ-2 | Consistent 30fps @ 1920x1080 capture | High |
| REQ-3 | Preserve existing motion detection algorithm | Critical |
| REQ-4 | H.264 recording with proper timestamps | High |
| REQ-5 | 24+ hour operation stability | High |
| REQ-6 | Web interface live view functional | Medium |
| REQ-7 | Utilize Pi 5 GPU where beneficial | Medium |
| REQ-8 | Filter USB cameras from CSI cameras | Medium |

### 2.2 Clarifying Questions

**Q1: What libcamera version should we target?**
- **Answer**: Bookworm default (0.2.x+). The scratchpad mentions this as an open question.

**Q2: 32-bit or 64-bit OS?**
- **Answer**: 64-bit recommended for Pi 5 (better performance, more memory).

**Q3: GPU utilization scope?**
- Research shows Pi 5 GPU (VideoCore VII) has **no H.264 hardware encoding** but does have HEVC decode. GPU utilization will be limited to potential HEVC output and OpenGL-accelerated processing if beneficial.

### 2.3 Existing Architecture Context (Triptych Query)

**Related Components Identified:**
- `cls_libcam` (src/libcam.cpp/hpp) - Primary modification target
- `cls_camera` (src/camera.cpp/hpp) - Frame consumer
- `cls_movie` (src/movie.cpp/hpp) - H.264 encoding via h264_v4l2m2m/FFmpeg
- `cls_alg` (src/alg.cpp/hpp) - Motion detection (PRESERVE)
- `cls_config` (src/conf.cpp/hpp) - Configuration parameters

**Existing Patterns:**
- libcamera ControlList for camera settings
- mmap for frame buffer access
- Request queue for async frame delivery
- YUV420 pixel format throughout pipeline

**Integration Points:**
- `libcam.cpp:528` - StreamRole configuration
- `libcam.cpp:534` - Buffer count allocation
- `libcam.cpp:591-663` - Request/buffer setup
- `movie.cpp:296-326` - h264_v4l2m2m codec handling

---

## 3. Architecture Design

### 3.1 Component Overview

```
+------------------+     +----------------+     +-------------+
|   libcamera      |---->|  cls_libcam    |---->| cls_camera  |
|   (Pi 5 PiSP)    |     |  (MODIFIED)    |     | (minor adj) |
+------------------+     +----------------+     +-------------+
                                                      |
                                                      v
+------------------+     +----------------+     +-------------+
|   cls_movie      |<----|   cls_alg      |<----| Frame Queue |
|   (verify only)  |     |   (UNCHANGED)  |     |             |
+------------------+     +----------------+     +-------------+
```

### 3.2 Components to Modify

#### 3.2.1 cls_libcam (Critical - Major Changes)

**File**: `src/libcam.cpp`, `src/libcam.hpp`

**Changes Required:**

1. **Stream Role Change** (Line ~528)
```cpp
// BEFORE:
config = camera->generateConfiguration({ StreamRole::Viewfinder });

// AFTER:
config = camera->generateConfiguration({ StreamRole::VideoRecording });
```
**Rationale**: Pi 5's PiSP (Pi Image Signal Processor) requires VideoRecording role for proper ISP pipeline initialization. Viewfinder role fails to configure correctly.

2. **Buffer Count Increase** (Line ~534)
```cpp
// BEFORE:
config->at(0).bufferCount = 1;

// AFTER:
config->at(0).bufferCount = cam->cfg->libcam_buffer_count;  // default: 4
```
**Rationale**: Single buffer causes pipeline stalls on Pi 5. Multiple buffers allow queuing.

3. **Request Buffer Pool** (Function `start_req()`)
```cpp
// BEFORE: Creates single request
std::unique_ptr<Request> request = camera->createRequest();

// AFTER: Create pool matching buffer count
int cls_libcam::start_req()
{
    // ... existing setup ...

    Stream *stream = config->at(0).stream();
    const std::vector<std::unique_ptr<FrameBuffer>> &buffers =
        frmbuf->buffers(stream);

    // Create request for each buffer
    for (unsigned int i = 0; i < buffers.size(); i++) {
        std::unique_ptr<Request> request = camera->createRequest();
        if (!request) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "Create request error.");
            return -1;
        }

        int retcd = request->addBuffer(stream, buffers[i].get());
        if (retcd < 0) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                "Add buffer for request error.");
            return -1;
        }

        requests.push_back(std::move(request));
    }

    // Map all buffer planes for memory access
    membuf_pool.clear();
    for (unsigned int i = 0; i < buffers.size(); i++) {
        ctx_imgmap map;
        const FrameBuffer::Plane &plane0 = buffers[i]->planes()[0];
        int bytes = 0;
        for (const auto &plane : buffers[i]->planes()) {
            bytes += plane.length;
        }
        map.buf = (uint8_t *)mmap(NULL, (uint)bytes, PROT_READ,
            MAP_SHARED, plane0.fd.get(), 0);
        map.bufsz = bytes;
        membuf_pool.push_back(map);
    }

    started_req = true;
    return 0;
}
```

4. **Camera Filtering** (New function)
```cpp
std::vector<std::shared_ptr<libcamera::Camera>> cls_libcam::get_pi_cameras()
{
    std::vector<std::shared_ptr<libcamera::Camera>> pi_cams;

    for (const auto& cam : cam_mgr->cameras()) {
        std::string id = cam->id();
        // Filter out USB cameras (UVC devices)
        // Pi cameras have IDs like: /base/axi/pcie@120000/rp1/i2c@88000/imx708@1a
        if (id.find("usb") == std::string::npos &&
            id.find("uvc") == std::string::npos) {
            pi_cams.push_back(cam);
            MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
                "Found Pi camera: %s", id.c_str());
        }
    }

    // Sort for consistent ordering
    std::sort(pi_cams.begin(), pi_cams.end(),
        [](const auto& a, const auto& b) {
            return a->id() < b->id();
        });

    return pi_cams;
}
```

5. **Enhanced Camera Selection** (Function `start_mgr()`)
```cpp
int cls_libcam::start_mgr()
{
    // ... existing manager startup ...

    std::string device = cam->cfg->libcam_device;
    auto pi_cams = get_pi_cameras();

    if (pi_cams.empty()) {
        MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO, "No Pi cameras found");
        return -1;
    }

    if (device == "auto" || device == "camera0") {
        camera = pi_cams[0];
    } else if (device.substr(0, 6) == "camera") {
        int idx = std::stoi(device.substr(6));
        if (idx >= (int)pi_cams.size()) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                "Camera index %d not found (have %d cameras)",
                idx, (int)pi_cams.size());
            return -1;
        }
        camera = pi_cams[idx];
    } else {
        // Try matching by sensor model (e.g., "imx708")
        camera = find_camera_by_model(device);
        if (!camera) {
            MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO,
                "Camera '%s' not found", device.c_str());
            return -1;
        }
    }

    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO,
        "Selected camera: %s", camera->id().c_str());

    camera->acquire();
    started_aqr = true;
    return 0;
}
```

6. **Request Completion Handler** (Function `req_complete()`)
```cpp
void cls_libcam::req_complete(Request *request)
{
    if (request->status() == Request::RequestCancelled) {
        return;
    }

    // Find which buffer this request used
    Stream *stream = config->at(0).stream();
    FrameBuffer *buffer = request->findBuffer(stream);

    // Get buffer index for membuf_pool lookup
    const auto &buffers = frmbuf->buffers(stream);
    int buf_idx = -1;
    for (size_t i = 0; i < buffers.size(); i++) {
        if (buffers[i].get() == buffer) {
            buf_idx = (int)i;
            break;
        }
    }

    if (buf_idx >= 0) {
        req_queue.push({request, buf_idx});
    }
}
```

7. **Frame Retrieval** (Function `next()`)
```cpp
int cls_libcam::next(ctx_image_data *img_data)
{
    if (started_cam == false) {
        return CAPTURE_FAILURE;
    }

    cam->watchdog = cam->cfg->watchdog_tmo;

    // Wait for frame with timeout
    int indx = 0;
    while (req_queue.empty() && indx < 50) {
        SLEEP(0, 2000);
        indx++;
    }

    cam->watchdog = cam->cfg->watchdog_tmo;

    if (!req_queue.empty()) {
        auto [request, buf_idx] = req_queue.front();
        req_queue.pop();

        // Copy frame data from correct buffer
        memcpy(img_data->image_norm,
               membuf_pool[buf_idx].buf,
               (uint)membuf_pool[buf_idx].bufsz);

        // Requeue request
        request->reuse(Request::ReuseBuffers);
        camera->queueRequest(request);

        cam->rotate->process(img_data);
        reconnect_count = 0;

        return CAPTURE_SUCCESS;
    }

    return CAPTURE_FAILURE;
}
```

#### 3.2.2 cls_libcam Header Changes

**File**: `src/libcam.hpp`

```cpp
#ifdef HAVE_LIBCAM
    #include <queue>
    #include <tuple>
    #include <vector>
    #include <sys/mman.h>
    #include <libcamera/libcamera.h>

    struct ctx_imgmap {
        uint8_t *buf;
        int     bufsz;
    };

    // Request with associated buffer index
    struct ctx_req_info {
        libcamera::Request *request;
        int buffer_idx;
    };

    class cls_libcam {
        public:
            // ... existing public interface unchanged ...

        private:
            // ... existing members ...

            // NEW: Buffer pool for multi-buffer support
            std::vector<ctx_imgmap> membuf_pool;

            // CHANGED: Queue now includes buffer index
            std::queue<ctx_req_info> req_queue;

            // NEW: Camera filtering
            std::vector<std::shared_ptr<libcamera::Camera>> get_pi_cameras();
            std::shared_ptr<libcamera::Camera> find_camera_by_model(
                const std::string &model);
    };
#endif
```

#### 3.2.3 cls_config (Configuration Parameters)

**File**: `src/conf.hpp`

Add new parameters:
```cpp
// Pi Camera specific (libcam section)
int         libcam_buffer_count;    // Buffer pool size (default: 4)
```

**File**: `src/conf.cpp`

Add parameter definition:
```cpp
// In defaults:
libcam_buffer_count = 4;

// In parameter handlers:
void cls_config::edit_libcam_buffer_count(std::string &parm, enum PARM_ACT pact)
{
    if (pact == PARM_ACT_DFLT) {
        libcam_buffer_count = 4;
    } else if (pact == PARM_ACT_SET) {
        libcam_buffer_count = mtoi(parm);
        if (libcam_buffer_count < 2) libcam_buffer_count = 2;
        if (libcam_buffer_count > 8) libcam_buffer_count = 8;
    } else if (pact == PARM_ACT_GET) {
        parm = std::to_string(libcam_buffer_count);
    }
}
```

### 3.3 Components Unchanged

| Component | File | Reason |
|-----------|------|--------|
| `cls_alg` | alg.cpp/hpp | Core motion detection - proven and solid |
| `cls_algsec` | alg_sec.cpp/hpp | Secondary detection methods |
| `cls_movie` | movie.cpp/hpp | H.264 encoding works, verify only |

---

## 4. GPU Utilization Analysis

### 4.1 Pi 5 GPU Capabilities

Based on research:

| Capability | Status | Notes |
|------------|--------|-------|
| H.264 Hardware Encode | **NOT AVAILABLE** | Removed from Pi 5 |
| H.264 Hardware Decode | **NOT AVAILABLE** | Removed from Pi 5 |
| HEVC (H.265) Decode | Available | 4Kp60 capable |
| OpenGL ES 3.1 | Available | Via Mesa V3D driver |
| Vulkan 1.2 | Available | Via Mesa V3D driver |
| General GPU Compute | Limited | No CUDA-like environment |

### 4.2 GPU Utilization Opportunities

**Opportunity 1: HEVC Output Container** (Low Priority)
- Motion currently uses H.264 for mp4/mkv output
- Could add HEVC output option for playback on Pi 5
- Encoding would still be CPU (FFmpeg libx265)
- Benefit: Hardware-accelerated playback on Pi 5

**Opportunity 2: OpenGL-Accelerated YUV Conversion** (Not Recommended)
- Could use OpenGL for color space conversion
- Current CPU-based approach is efficient for 1080p
- Added complexity outweighs marginal benefit

**Opportunity 3: Motion Detection Acceleration** (Future - Not In Scope)
- V3DLib exists for VideoCore compute
- Would require significant algorithm rewrite
- Not recommended for this project

### 4.3 GPU Recommendation

**For this implementation: Focus on CPU-based processing.**

Rationale:
1. Pi 5 CPU (Cortex-A76 @ 2.4GHz) is powerful enough for 1080p30 motion detection
2. No hardware H.264 encoding available regardless
3. GPU acceleration would add complexity without significant benefit
4. `cls_alg` is already optimized and proven

**Future Enhancement**: Consider adding `movie_container = "hevc"` option for users who want hardware-accelerated playback.

---

## 5. Encoder Integration

### 5.1 Current Encoder Path

Motion uses FFmpeg with `h264_v4l2m2m` codec preference:
- `movie.cpp:296`: `if (preferred_codec == "h264_v4l2m2m")`
- V4L2 M2M encoder available on Pi 4, **not available on Pi 5**

### 5.2 Pi 5 Encoder Approach

On Pi 5, FFmpeg will fall back to software encoding:
- `libx264` for H.264
- `libx265` for HEVC (if configured)

**No code changes required** - existing fallback in `set_codec_preferred()` handles this:
```cpp
codec = avcodec_find_encoder_by_name(preferred_codec.c_str());
if (codec == nullptr) {
    // Falls back to default encoder for container
    codec = avcodec_find_encoder(oc->video_codec_id);
}
```

### 5.3 Performance Considerations

| Resolution | Framerate | CPU Usage (est.) | Notes |
|------------|-----------|------------------|-------|
| 1920x1080 | 30fps | 30-50% | Single core |
| 1280x720 | 30fps | 15-25% | Recommended for multi-camera |
| 640x480 | 30fps | 5-10% | Low resource usage |

**Recommendation**: Document that Pi 5 users should consider 720p for multi-camera setups to manage CPU load.

---

## 6. Build System Updates

### 6.1 configure.ac Changes

The existing libcamera detection works. Add Pi 5 optimization flags:

```m4
# After line 3 (CXXFLAGS)
# Add Pi 5 CPU optimization (optional, detected at runtime would be better)
AC_ARG_ENABLE([pi5-optimize],
  AS_HELP_STRING([--enable-pi5-optimize],[Enable Pi 5 CPU optimizations]),
  [PI5_OPT="$enableval"],
  [PI5_OPT="no"]
)
AS_IF([test "${PI5_OPT}" = "yes"], [
    CXXFLAGS="$CXXFLAGS -mcpu=cortex-a76"
    AC_MSG_NOTICE([Enabled Pi 5 (Cortex-A76) optimizations])
  ]
)
```

### 6.2 Build Configuration (Recommended)

```bash
# On Pi 5:
./configure \
  --with-libcam \
  --without-v4l2 \
  --with-sqlite3 \
  --without-mysql \
  --without-mariadb \
  --without-pgsql \
  --enable-pi5-optimize
```

---

## 7. Requirements Traceability

| Requirement | Design Element | Coverage |
|-------------|----------------|----------|
| REQ-1 Camera detection | `get_pi_cameras()`, `find_camera_by_model()`, `start_mgr()` | Full |
| REQ-2 30fps @ 1080p | `VideoRecording` role, multi-buffer pool | Full |
| REQ-3 Preserve algorithm | `cls_alg` unchanged | Full |
| REQ-4 H.264 recording | Existing `cls_movie`, FFmpeg fallback | Full |
| REQ-5 24+ hour stability | Multi-buffer, proper resource cleanup | Full (needs testing) |
| REQ-6 Web interface | Unchanged, depends on capture | Full |
| REQ-7 GPU utilization | Analysis complete - CPU approach recommended | Full |
| REQ-8 USB camera filter | `get_pi_cameras()` USB/UVC filtering | Full |

---

## 8. Implementation Phases

### Phase 1: Critical libcamera Fixes (1-2 days)
- Stream role change
- Buffer count increase
- Request buffer pool
- Basic testing

### Phase 2: Camera Selection Enhancement (0.5 days)
- Camera filtering
- Model-based selection
- Multi-camera support

### Phase 3: Configuration Parameters (0.5 days)
- `libcam_buffer_count` parameter
- Web UI integration

### Phase 4: Testing & Validation (2-3 days)
- Unit tests
- Integration tests
- 24-hour stability test
- Performance benchmarks

---

## 9. Risks and Mitigations

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| libcamera API changes | High | Low | Pin to Bookworm version |
| Software encoding CPU load | Medium | Medium | Document 720p for multi-cam |
| Memory pressure with buffers | Low | Low | Configurable buffer count |
| Pi 5 variants (4GB vs 8GB) | Low | Low | Test on both |

---

## 10. Assumptions

1. Target OS is Raspberry Pi OS Bookworm 64-bit
2. libcamera version from Bookworm apt packages
3. Camera Module 3 (IMX708) as primary test target
4. Single camera per Motion instance (multi-camera via multiple instances)
5. No hardware H.264 encoding available

---

## 11. Success Criteria

1. `motion` finds Camera v3 on Pi 5 startup
2. Consistent 30fps @ 1920x1080 capture
3. Motion detection accuracy matches V4L2 reference
4. H.264 files playable with correct timestamps
5. 24+ hour operation without crashes/memory leaks
6. Web interface live view functional
7. CPU usage under 60% for single 1080p30 camera

---

## 12. References

- [MediaMTX rpiCamera implementation](https://github.com/bluenviron/mediamtx-rpicamera)
- [Raspberry Pi Camera Documentation](https://www.raspberrypi.com/documentation/computers/camera_software.html)
- [libcamera API Documentation](https://libcamera.org/api-html/)
- [Pi 5 Camera Issues Forum](https://forums.raspberrypi.com/viewtopic.php?t=371329)
- [Pi 5 Hardware Encoding Discussion](https://raspberrypi.stackexchange.com/questions/144475/did-the-raspberry-pi-5-drop-hardware-encoding-support)
- [VideoCore VII Overview](https://www.phoronix.com/review/raspberry-pi-5-graphics)

---

## Appendix A: File Change Summary

### Must Modify

| File | Changes | Lines Est. |
|------|---------|------------|
| `src/libcam.cpp` | StreamRole, buffers, camera selection, request pool | +200, -50 |
| `src/libcam.hpp` | New structs, member variables | +30 |
| `src/conf.cpp` | `libcam_buffer_count` parameter | +20 |
| `src/conf.hpp` | Parameter declaration | +2 |

### Verify Only (No Changes Expected)

| File | Verification |
|------|--------------|
| `src/movie.cpp` | FFmpeg fallback for encoding |
| `src/camera.cpp` | Frame handling compatibility |
| `configure.ac` | libcamera detection |

### Unchanged

| File | Reason |
|------|--------|
| `src/alg.cpp` | Core algorithm - proven |
| `src/alg.hpp` | Core algorithm interface |
| `src/alg_sec.cpp` | Secondary detection |

---

*Document Version: 1.0*
*Last Updated: 2025-12-06*
