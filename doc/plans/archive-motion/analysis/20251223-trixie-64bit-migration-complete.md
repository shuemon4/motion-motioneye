# Motion Analysis: Raspberry Pi OS Trixie (Debian 13) + 64-bit Migration

**Date:** 2025-12-23
**Target OS:** Raspberry Pi OS Lite (64-bit) based on Debian 13 "Trixie"
**Target Hardware:** Raspberry Pi 5 + Camera Module v3, Raspberry Pi 4 (minimum supported)
**Current State:** Motion fork optimized for Pi 5 + Camera v3 on Bookworm

---

## Executive Summary

The Motion fork is close to Trixie-ready with modern C++17 practices and libcamera abstractions. However, there are specific high-risk compatibility edges around libcamera API changes, pixel format validation, and 64-bit pointer/size handling. Additional operational gaps exist in scripts and documentation (rpicam tool naming, Pi 4 vs Pi 5 build profiles).

| Aspect | Assessment |
|--------|------------|
| **Overall Risk** | Medium |
| **Estimated Effort** | 2-4 hours code changes + testing |
| **Primary Concerns** | libcamera API drift, pixel format enforcement, 64-bit mmap/logging, rpicam tool detection |

---

## Issues by Priority

### HIGH PRIORITY

#### 1. libcamera ABI Breaking Changes & Draft Controls

**Location:** `src/libcam.cpp`

**Issues:**
- Trixie ships libcamera 0.4.0 (0.5.x in backports); only 91.2% binary compatible with prior versions
- Code uses `controls::draft::*` controls without version guards
- Draft controls can be promoted, renamed, or moved between versions

**Current Code:** Version detection exists at line 31:
```cpp
#define LIBCAMVER (LIBCAMERA_VERSION_MAJOR * 1000000)+(LIBCAMERA_VERSION_MINOR* 1000) + LIBCAMERA_VERSION_PATCH
```

**Recommendations:**
- Add version checks around control usage (`LIBCAMVER`)
- Feature-detect control presence before using draft controls
- Test explicitly with both 0.4.0 (Trixie default) and 0.5.x (backports)
- Watch for `undefined symbol: libpisp` errors on Pi 5

**Reference:** [libcamera-devel mailing list](https://lists.libcamera.org/pipermail/libcamera-devel/2024-December/047898.html)

---

#### 2. Hard Fail on Pixel Format Adjustment

**Location:** `src/libcam.cpp`

**Issue:** Code rejects configuration if libcamera adjusts away from `YUV420`. Trixie's libcamera defaults can adjust to `NV12` or other YUV formats, causing camera startup failures.

**Recommendations:**
- Accept safe alternatives (e.g., `NV12`)
- Add internal format conversion if needed
- Log adjustment as warning rather than failing

---

#### 3. Pi 5 PiSP vs Pi 4 VideoCore ISP Architecture

**Location:** `src/libcam.cpp`

**Context:** Pi 5 uses a completely different ISP architecture (PiSP split between RP1 southbridge and BCM2712) vs Pi 4's VideoCore VI (on-chip).

**Current State (Good):**
- `StreamRole::VideoRecording` at line 897 (correct for Pi 5)
- Multi-buffer support (correct for PiSP pipeline)
- USB camera filtering implemented

**Recommendations:**
- Tuning files differ: Pi 4 uses `"target": "vc4"`, Pi 5 needs `"target": "pisp"`
- Test autofocus/exposure explicitly on both platforms
- Ensure camera selection logic handles mixed CSI + USB scenarios

**References:**
- [PiSP Driver - Phoronix](https://www.phoronix.com/news/Linux-6.11-Media-Raspberry-PiSP)
- [PiSP Kernel Docs](https://docs.kernel.org/admin-guide/media/raspberrypi-pisp-be.html)

---

### MEDIUM PRIORITY

#### 4. 64-bit mmap Sizes and Pointer Logging

**Locations:**
- `src/libcam.hpp:36` - `int bufsz` should be `size_t`
- `src/libcam.cpp:1167` - casts `bytes` to `uint` for mmap
- `src/libcam.cpp:1182` - casts `buf_bytes` to `uint`
- `src/video_v4l2.cpp:782` - mmap size cast
- `src/video_v4l2.cpp` - logs pointers with `%x` instead of `%p`

**Issue:** On 64-bit aarch64, `size_t` is 64-bit while `uint` is 32-bit. Truncation causes incorrect logging and possible runtime size errors for larger buffers.

**Current Code:**
```cpp
membuf.buf = (uint8_t *)mmap(NULL, (uint)bytes, PROT_READ, MAP_SHARED, plane0.fd.get(), 0);
```

**Fix:**
```cpp
membuf.buf = (uint8_t *)mmap(NULL, (size_t)bytes, PROT_READ, MAP_SHARED, plane0.fd.get(), 0);
```

---

#### 5. Hardware Encoder Changes (h264_v4l2m2m)

**Location:** `src/movie.cpp:218`

**Issue:** Special NAL unit handling for `h264_v4l2m2m`:
```cpp
if (preferred_codec == "h264_v4l2m2m") {
    encode_nal();
}
```

Pi 5 has VideoCore VII vs Pi 4's VideoCore VI. V4L2 M2M encoder behavior may differ on Trixie's kernel 6.12 LTS.

**Recommendations:**
- Test video encoding on both Pi 4 and Pi 5 with Trixie
- Monitor for NAL unit issues in recorded videos
- Consider explicit codec detection for platform differences

---

#### 6. rpicam Tool Naming Change

**Location:** `scripts/pi5-setup.sh`

**Issue:** Script checks for `libcamera-hello` only, but Raspberry Pi renamed tools to `rpicam-*` (e.g., `rpicam-hello`) on Trixie. Script misreports camera tools as missing.

**Fix:** Check `rpicam-hello` first, fall back to `libcamera-hello` for Bookworm compatibility.

---

#### 7. Pi 4 vs Pi 5 Build Profile Mismatch

**Location:** `scripts/pi5-build.sh`

**Issue:** Disables V4L2 globally, which may unnecessarily reduce USB camera support on Pi 4.

**Recommendation:** Provide separate build profile or configurable build path that preserves V4L2 on Pi 4.

---

#### 8. Potential FFmpeg 7 Deprecations

**Location:** `src/motion.cpp`

**Issue:** Uses `avdevice_register_all()` unguarded. FFmpeg 7 (expected in Debian 13) may remove legacy init functions.

**Recommendation:** Guard call with FFmpeg version checks or remove if not required.

---

### LOW PRIORITY

#### 9. Buffer Size Type in ctx_imgmap

**Location:** `src/libcam.hpp:36`
```cpp
struct ctx_imgmap {
    uint8_t *buf;
    int     bufsz;  // Should be size_t for 64-bit
};
```

Change `bufsz` to `size_t` for proper 64-bit handling of large buffers.

---

## Hardware Differences: Pi 4 vs Pi 5

| Feature | Pi 4 (Bookworm/Trixie) | Pi 5 (Trixie) |
|---------|------------------------|---------------|
| ISP | VideoCore VI (on-chip) | PiSP (split: RP1 + BCM2712) |
| CSI Receiver | Unicam (main processor) | RP1 southbridge |
| GPU | VideoCore VI @ 800MHz | VideoCore VII @ 1.1GHz |
| MIPI Interface | 2-lane | 4-lane |
| libcamera tuning | `target: vc4` | `target: pisp` |
| H.264 Encoder | h264_v4l2m2m | h264_v4l2m2m (different behavior) |
| Vulkan Support | 1.0 | 1.2 |

---

## Build System Compatibility

### Recommended Build Configuration

```bash
./configure \
  --with-libcam \
  --with-sqlite3 \
  --without-v4l2 \
  --without-mysql \
  --without-mariadb \
  --without-pgsql
```

**Note:** `--without-v4l2` recommended because libcamera is required for 64-bit systems—the legacy camera stack doesn't work in 64-bit mode.

### Package Notes
- Python 3.13 is default on Trixie (not relevant for C++ Motion)
- Kernel 6.12 LTS may have different V4L2 behavior
- `configure.ac` properly detects libcamera via pkgconf

---

## 64-bit Optimization Opportunities

| Optimization | Benefit |
|--------------|---------|
| **Memory Alignment** | 64-bit ARM benefits from 8-byte aligned allocations; image buffers would benefit |
| **NEON SIMD** | aarch64 has improved NEON intrinsics; `alg.cpp` could benefit (requires explicit request) |
| **Address Space** | Larger ring buffers for pre/post capture without memory fragmentation risk |

---

## Validation Plan

### Build Tests
1. Build on Trixie 64-bit with libcamera 0.4.0
2. Build on Trixie 64-bit with libcamera 0.5.x (backports)

### Camera Startup Tests
3. Pi 5 + Camera v3 startup and stream
4. Pi 4 + CSI camera startup
5. Pi 4 + USB camera (if V4L2 enabled)

### Recording Tests
6. Pi 5: software H.264 (libx264)
7. Pi 4: hardware H.264 (h264_v4l2m2m)
8. Verify NAL unit handling on both platforms

### Format Tests
9. Verify `YUV420` vs `NV12` format adjustments
10. Confirm motion pipeline correctness if format is adjusted

### Stability Tests
11. 24+ hour stability test on Pi 5
12. Multi-camera stress test on Pi 5

---

## Code Changes Checklist

### Required Before Migration
- [ ] `src/libcam.hpp:36` — Change `int bufsz` to `size_t bufsz`
- [ ] `src/libcam.cpp:1167` — Change `(uint)bytes` to `(size_t)bytes`
- [ ] `src/libcam.cpp:1182` — Change `(uint)buf_bytes` to `(size_t)buf_bytes`
- [ ] `src/video_v4l2.cpp:782` — Change mmap size cast to `size_t`
- [ ] `src/video_v4l2.cpp` — Change pointer logging from `%x` to `%p`
- [ ] `src/libcam.cpp` — Add version guards for draft controls
- [ ] `src/libcam.cpp` — Accept `NV12` as valid pixel format alternative

### Script Updates
- [ ] `scripts/pi5-setup.sh` — Check `rpicam-hello` first, fall back to `libcamera-hello`
- [ ] Consider Pi 4 build profile with V4L2 support

### Testing
- [ ] Test with libcamera 0.4.0
- [ ] Test with libcamera 0.5.x
- [ ] Test on Pi 4 with Trixie 64-bit
- [ ] Test on Pi 5 with Trixie 64-bit
- [ ] Verify h264_v4l2m2m encoding on both platforms
- [ ] Verify autofocus works on Camera v3

---

## Decision Points

Before implementing fixes, decide:

1. **libcamera version target** — Support 0.4.0 only, or 0.4.0 + 0.5.x compatibility?
  **answer** - 0.4.0 + 0.5.x compatibility
2. **Pi 4 support scope** — libcamera-only, or preserve V4L2 for USB cameras?
  **answer** - USB cameras are still common, so V4L2 should be preserved
3. **Pixel format policy** — Strict `YUV420`, or accept `NV12` with conversion?
  **answer** - accept `NV12` with conversion

---

## Sources

- [Raspberry Pi OS Trixie Release - Phoronix](https://www.phoronix.com/news/Raspberry-Pi-OS-Debian-13)
- [Trixie Camera Issues - Raspberry Pi Forums](https://forums.raspberrypi.com/viewtopic.php?t=392639)
- [libcamera v0.4.0 Release Notes](https://lists.libcamera.org/pipermail/libcamera-devel/2024-December/047898.html)
- [libcamera 64-bit Requirement - MediaMTX GitHub](https://github.com/bluenviron/mediamtx/issues/2647)
- [Motion-Project libcamera Discussion](https://github.com/Motion-Project/motion/discussions/1636)
- [PiSP Driver Landing - Phoronix](https://www.phoronix.com/news/Linux-6.11-Media-Raspberry-PiSP)
- [PiSP Kernel Documentation](https://docs.kernel.org/admin-guide/media/raspberrypi-pisp-be.html)
- [Collabora's Debian Trixie Contributions](https://www.collabora.com/news-and-blog/news-and-events/debian-trixie-the-2025-flavor-release.html)
- [CNX Software - Raspberry Pi OS Debian 13](https://www.cnx-software.com/2025/10/06/raspberry-pi-os-debian-13-trixie/)
