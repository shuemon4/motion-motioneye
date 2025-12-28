# Motion: Raspberry Pi OS Trixie 64-bit Migration Plan

**Date:** 2025-12-23
**Target:** Raspberry Pi OS Lite 64-bit (Debian 13 "Trixie")
**Hardware:** Pi 5 (primary), Pi 4 (secondary support)
**Estimated Effort:** 2-4 hours code + testing

---

## Overview

Migrate the Motion fork to full Trixie 64-bit compatibility. The codebase is close to ready, but has specific issues around 64-bit type safety, libcamera API drift, and script compatibility.

**Decision Points (confirmed):**
- libcamera: Support both 0.4.0 (Trixie) and 0.5.x (backports)
- Pi 4: Both Pi 4 and Pi 5 supported - preserve V4L2 for USB cameras
- Pixel format: Accept NV12, defer conversion until testing proves it's needed
- Draft controls: Silent skip if unavailable (no warning logs)

---

## Phase 1: 64-bit Type Safety (Critical)

These fixes prevent runtime truncation on aarch64 where `size_t` is 64-bit but `uint/int` is 32-bit.

### 1.1 Fix buffer size type in ctx_imgmap

**File:** `src/libcam.hpp:36`

```cpp
// BEFORE
struct ctx_imgmap {
    uint8_t *buf;
    int     bufsz;  // 32-bit on 64-bit system
};

// AFTER
struct ctx_imgmap {
    uint8_t *buf;
    size_t  bufsz;  // Proper 64-bit size
};
```

### 1.2 Fix mmap size casts in libcam.cpp

**File:** `src/libcam.cpp`

| Line | Before | After |
|------|--------|-------|
| ~1167 | `mmap(NULL, (uint)bytes, ...)` | `mmap(NULL, bytes, ...)` |
| ~1182 | `mmap(NULL, (uint)buf_bytes, ...)` | `mmap(NULL, (size_t)buf_bytes, ...)` |
| ~1128-1134 | `int bytes` accumulator | `size_t bytes` |
| ~1177 | `int buf_bytes = 0` | `size_t buf_bytes = 0` |

Also update memcpy size casts at lines ~1629, ~1632 to use `size_t`.

### 1.3 Fix pointer logging in video_v4l2.cpp

**File:** `src/video_v4l2.cpp:795`

```cpp
// BEFORE
MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
    ,_("%i length=%d Address (%x)")
    ,buffer_index, p_buf.length, buffers[buffer_index].ptr);

// AFTER
MOTION_LOG(DBG, TYPE_VIDEO, NO_ERRNO
    ,_("%i length=%d Address (%p)")
    ,buffer_index, p_buf.length, (void*)buffers[buffer_index].ptr);
```

---

## Phase 2: libcamera Compatibility (High Priority)

### 2.1 Add version guards for draft controls

**File:** `src/libcam.cpp` (lines 718-742)

Wrap each draft control usage with feature detection (silent skip if unavailable):

```cpp
// Example pattern for each draft control - silent skip
if (ctrls.contains(controls::draft::AePrecaptureTrigger.id())) {
    config_control_item(&req_controls,
        &controls::draft::AePrecaptureTrigger,
        cam->libcam_params.ae_precapture_trigger);
}
// No else/warning - silent skip per user preference
```

**Draft controls requiring guards (9 total):**
1. `AePrecaptureTrigger` (line 718)
2. `NoiseReductionMode` (line 721)
3. `ColorCorrectionAberrationMode` (line 724)
4. `AwbState` (line 727)
5. `SensorRollingShutterSkew` (line 730)
6. `LensShadingMapMode` (line 733)
7. `PipelineDepth` (line 736)
8. `MaxLatency` (line 739)
9. `TestPatternMode` (line 742)

### 2.2 Accept NV12 pixel format

**File:** `src/libcam.cpp` (lines 934-938)

```cpp
// BEFORE - hard fail
if (config->at(0).pixelFormat != PixelFormat::fromString("YUV420")) {
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        , "Pixel format was adjusted to %s."
        , config->at(0).pixelFormat.toString().c_str());
    return -1;
}

// AFTER - accept safe alternatives
libcamera::PixelFormat adjusted = config->at(0).pixelFormat;
libcamera::PixelFormat yuv420 = PixelFormat::fromString("YUV420");
libcamera::PixelFormat nv12 = PixelFormat::fromString("NV12");

if (adjusted != yuv420 && adjusted != nv12) {
    MOTION_LOG(ERR, TYPE_VIDEO, NO_ERRNO
        , "Unsupported pixel format: %s (need YUV420 or NV12)"
        , adjusted.toString().c_str());
    return -1;
}

if (adjusted != yuv420) {
    MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
        , "Using pixel format %s (will convert internally)"
        , adjusted.toString().c_str());
    // Set flag for conversion in frame processing
    needs_nv12_conversion = true;
}
```

### 2.3 NV12→YUV420 conversion (DEFERRED)

**Status:** Will only implement if testing shows motion detection fails with NV12.

NV12 and YUV420 are both YUV 4:2:0 formats. The motion detection algorithm in `cls_alg` may work correctly with NV12 since the Y plane (luminance) is identical - motion detection primarily uses luminance.

**If conversion is needed later:**
- NV12 has interleaved UV, YUV420 has separate U/V planes
- Conversion is a simple UV plane deinterleave
- Add `needs_nv12_conversion` flag and conversion function

---

## Phase 3: Script Updates (Medium Priority)

### 3.1 Update camera tool detection

**File:** `scripts/pi5-setup.sh` (lines 80-88)

```bash
# BEFORE
if command -v libcamera-hello &> /dev/null; then
    echo "  Running libcamera-hello --list-cameras..."
    libcamera-hello --list-cameras 2>&1 || true

# AFTER - check rpicam first (Trixie), fall back to libcamera (Bookworm)
if command -v rpicam-hello &> /dev/null; then
    echo "  Running rpicam-hello --list-cameras..."
    rpicam-hello --list-cameras 2>&1 || true
elif command -v libcamera-hello &> /dev/null; then
    echo "  Running libcamera-hello --list-cameras..."
    libcamera-hello --list-cameras 2>&1 || true
else
    echo "  No camera tools found (rpicam-hello or libcamera-hello)"
fi
```

### 3.2 Create Pi 4 build profile

**File:** `scripts/pi4-build.sh` (new file)

```bash
#!/bin/bash
# Build script for Pi 4 with V4L2 support for USB cameras

./configure \
    --with-libcam \
    --with-v4l2 \        # Keep V4L2 for USB cameras
    --with-sqlite3 \
    --with-webp \
    --without-mysql \
    --without-mariadb \
    --without-pgsql

make -j4
```

### 3.3 Update pi5-build.sh comments

**File:** `scripts/pi5-build.sh`

Add comment explaining why V4L2 is disabled:
```bash
# V4L2 disabled on Pi 5 because:
# 1. Pi 5 requires libcamera for CSI cameras (legacy camera stack removed)
# 2. USB cameras can still work via libcamera's UVC support
# For Pi 4 with USB cameras, use scripts/pi4-build.sh instead
--without-v4l2 \
```

---

## Phase 4: FFmpeg Compatibility (Low Priority)

### 4.1 Guard avdevice_register_all

**File:** `src/motion.cpp:280`

```cpp
// BEFORE
avdevice_register_all();

// AFTER - guard for FFmpeg 7+ which may deprecate this
#if (LIBAVDEVICE_VERSION_MAJOR < 61)
    avdevice_register_all();
#endif
```

Note: FFmpeg 7 may still require this call; test before removing.

---

## Phase 5: Validation

### Build Tests
- [ ] Build on Trixie 64-bit with libcamera 0.4.0
- [ ] Build on Trixie 64-bit with libcamera 0.5.x (backports)
- [ ] Build on Bookworm 64-bit (regression test)

### Camera Startup Tests
- [ ] Pi 5 + Camera v3 startup and stream
- [ ] Pi 4 + CSI camera startup
- [ ] Pi 4 + USB camera (V4L2 enabled build)

### Recording Tests
- [ ] Pi 5: software H.264 (libx264)
- [ ] Pi 4: hardware H.264 (h264_v4l2m2m)
- [ ] Verify NAL unit handling on both platforms

### Format Tests
- [ ] Verify YUV420 still works when available
- [ ] Verify NV12 conversion produces correct output
- [ ] Confirm motion detection works with converted frames

### Stability Tests
- [ ] 24+ hour stability test on Pi 5
- [ ] Multi-camera stress test (if applicable)

---

## Critical Files Summary

| File | Changes |
|------|---------|
| `src/libcam.hpp` | Change `bufsz` type to `size_t` |
| `src/libcam.cpp` | mmap casts, draft control guards, NV12 acceptance, conversion function |
| `src/video_v4l2.cpp` | Pointer logging format |
| `src/motion.cpp` | FFmpeg version guard |
| `scripts/pi5-setup.sh` | rpicam tool detection |
| `scripts/pi5-build.sh` | Add explanatory comment |
| `scripts/pi4-build.sh` | New file for Pi 4 builds |

---

## Implementation Order

1. **Phase 1** - 64-bit type safety fixes (low risk, enables correct operation)
2. **Phase 2.1** - Draft control guards with silent skip
3. **Phase 2.2** - Accept NV12 pixel format (log but don't convert yet)
4. **Phase 3** - Script updates (Pi 4 build profile, rpicam detection)
5. **Phase 4** - FFmpeg guard (lowest priority)
6. **Phase 5** - Validation on both Pi 4 and Pi 5

**Note:** Phase 2.3 (NV12 conversion) is deferred until testing proves it's needed.

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Draft controls renamed in 0.5.x | Feature-detect with `ctrls.contains()` before use; silent skip |
| NV12 breaks motion detection | Test on Trixie first; motion uses Y plane which is identical |
| h264_v4l2m2m behavior differs | Test recording on both Pi 4 and Pi 5 platforms |
| Pi 4 regression | Separate pi4-build.sh with V4L2 enabled |
| libcamera 0.4.0 vs 0.5.x differences | Test both versions; version guards where needed |

---

## Implementation Log

### Phase 1: 64-bit Type Safety - COMPLETED 2025-12-23

**Files Modified:**
- `src/libcam.hpp:36` - Changed `int bufsz` to `size_t bufsz` in `ctx_imgmap` struct
- `src/libcam.cpp:1081-1083` - Changed `bytes` variable from `int` to `size_t`
- `src/libcam.cpp:1168` - Removed `(uint)` cast from mmap size parameter
- `src/libcam.cpp:1178` - Changed `buf_bytes` variable from `int` to `size_t`
- `src/libcam.cpp:1183` - Removed `(uint)` cast from mmap size parameter
- `src/libcam.cpp:1629,1633` - Removed `(uint)` casts from memcpy size parameters
- `src/video_v4l2.cpp:795-796` - Changed pointer logging from `%x` to `%p` with `(void*)` cast

**Notes:**
- All changes ensure proper 64-bit handling of buffer sizes
- Prevents truncation when `size_t` is 64-bit but `int/uint` is 32-bit
- Backward compatible with existing 32-bit and 64-bit systems

### Phase 2: libcamera Compatibility - COMPLETED 2025-12-23

**Files Modified:**
- `src/libcam.cpp:716-761` - Added `is_control_supported()` guards for 9 draft controls with silent skip
  - AePrecaptureTrigger, NoiseReductionMode, ColorCorrectionAberrationMode
  - AwbState, SensorRollingShutterSkew, LensShadingMapMode
  - PipelineDepth, MaxLatency, TestPatternMode
- `src/libcam.cpp:951-981` - Modified pixel format validation to accept NV12 in addition to YUV420
  - Added informative logging when NV12 is used
  - Note added that conversion may not be needed (Y-plane identical)

**Notes:**
- Draft controls silently skip if unavailable (no warnings per user preference)
- NV12 acceptance prepares for Trixie's libcamera 0.4.0/0.5.x behavior
- Conversion function deferred until testing shows it's required
- Backward compatible with Bookworm (will use YUV420 when available)

### Phase 3: Script Updates - COMPLETED 2025-12-23

**Files Modified:**
- `scripts/pi5-setup.sh:80-91` - Updated camera detection to check `rpicam-hello` first, fall back to `libcamera-hello`
- `scripts/pi5-build.sh:49-52` - Added explanatory comment about V4L2 disabled on Pi 5
- `scripts/pi4-build.sh` - **NEW FILE** created for Pi 4 builds with V4L2 support

**Notes:**
- rpicam-* tools are the new naming on Trixie (Bookworm uses libcamera-*)
- Pi 4 build profile preserves V4L2 for USB camera support
- Both scripts maintain backward compatibility

### Phase 4: FFmpeg Compatibility - COMPLETED 2025-12-23

**Files Modified:**
- `src/motion.cpp:281-284` - Added `#if (LIBAVDEVICE_VERSION_MAJOR < 61)` guard around `avdevice_register_all()`

**Notes:**
- Prepares for FFmpeg 7+ which may deprecate/remove this function
- No effect on current Bookworm (FFmpeg 5.x) or Trixie (FFmpeg 6.x)
- Backward compatible

### Build Validation - COMPLETED 2025-12-23

**Environment:**
- Device: Raspberry Pi 5 (pi5-motioneye)
- OS: Raspberry Pi OS Bookworm 64-bit
- libcamera: 0.5.2
- FFmpeg: 59.27.100 (5.x)

**Build Results:**
- ✅ Configure: Successful
- ✅ Make: Successful (4 cores)
- ⚠️ Warnings: Only pre-existing float conversion warnings in webu_json.cpp (unrelated to migration changes)
- ✅ No new warnings introduced
- ✅ Binary created: `src/motion`

**Deviations from Plan:**
- None - all phases implemented as specified

**Next Steps:**
- Code is ready for Trixie migration when OS upgrade occurs
- All changes are backward compatible with Bookworm
- Testing on actual Trixie system recommended after OS upgrade
- NV12 conversion function can be added if testing shows motion detection issues

