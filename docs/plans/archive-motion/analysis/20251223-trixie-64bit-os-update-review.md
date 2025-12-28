# Motion Analysis: Raspberry Pi OS Lite 64-bit (Debian 13 "Trixie") Update Review

**Date:** 2025-12-23
**Target OS:** Raspberry Pi OS Lite (64-bit) based on Debian 13 "Trixie"
**Target Hardware:** Raspberry Pi 5 + Pi Camera v3, Raspberry Pi 4 (minimum supported)
**Scope:** Motion fork optimized for Pi 5 + Camera v3 on Bookworm; assess Trixie + 64-bit risks and optimizations

---

## Summary

The codebase is close to Trixie-ready, but there are a few high-risk compatibility edges around libcamera API changes, pixel format validation, and 64-bit safety. There are also operational gaps in scripts and docs that will surface during OS migration (rpicam tool naming, Pi 4 vs Pi 5 build profile differences).

**Risk Level:** Medium
**Primary Concerns:** libcamera API drift, pixel format enforcement, 64-bit mmap/logging correctness, rpicam tool detection

---

## Key Findings (Ordered by Risk)

### 1) libcamera draft controls are used without version guards (HIGH)
**Location:** `src/libcam.cpp` uses `controls::draft::*` controls.

**Why this matters:** libcamera 0.4/0.5 has evolving control namespaces. Draft controls can be promoted, renamed, or moved, leading to build failures or missing runtime control application on Trixie.

**Recommendation:** add version checks (via `LIBCAMVER`) or feature-detect control presence before compiling/using draft controls.

---

### 2) Hard fail on pixel format adjustment (HIGH)
**Location:** `src/libcam.cpp` rejects configuration if libcamera adjusts away from `YUV420`.

**Why this matters:** Trixie libcamera defaults can adjust to `NV12` or other YUV formats. Current code treats this as fatal, which can prevent camera startup on newer stacks.

**Recommendation:** allow safe alternatives (e.g., accept `NV12` or map to an internal converter) and log adjustment rather than failing.

---

### 3) 64-bit mmap sizes and pointer logging (MEDIUM)
**Locations:**
- `src/libcam.hpp` uses `int` for buffer size.
- `src/libcam.cpp` casts mmap lengths to `uint`.
- `src/video_v4l2.cpp` logs pointer with `%x`.

**Why this matters:** 64-bit aarch64 uses 64-bit sizes and pointers. Truncation can cause incorrect logging and possible runtime size errors for larger buffers.

**Recommendation:** use `size_t` for buffer sizes, pass `size_t` to `mmap`, and log pointers with `%p`.

---

### 4) Trixie uses rpicam tools; setup script checks libcamera-hello only (MEDIUM)
**Location:** `scripts/pi5-setup.sh` checks `libcamera-hello` only.

**Why this matters:** Raspberry Pi has renamed tooling to `rpicam-*` (e.g., `rpicam-hello`). The script can misreport that the camera tools are missing on Trixie.

**Recommendation:** check `rpicam-hello` first, then fall back to `libcamera-hello` for older OSes.

---

### 5) Pi 4 vs Pi 5 build profile mismatch (MEDIUM)
**Location:** `scripts/pi5-build.sh` disables V4L2 globally.

**Why this matters:** Pi 4 users may rely on USB cameras or V4L2 capture. Disabling V4L2 may unnecessarily reduce support on Pi 4 in a unified build pipeline.

**Recommendation:** provide a Pi 4 build script or a configurable build path that preserves V4L2 on Pi 4.

---

### 6) Potential FFmpeg 7 deprecations (MEDIUM)
**Location:** `src/motion.cpp` uses `avdevice_register_all()` unguarded.

**Why this matters:** FFmpeg 7 is expected in Debian 13; legacy init functions may be removed. This could cause compile failures.

**Recommendation:** guard the call with version checks or remove if not required for this project.

---

## OS Migration Considerations

### Pi 5 (PiSP + Camera v3)
- `StreamRole::VideoRecording` is correct and required.
- Multi-buffer pool approach is correct for PiSP.
- Ensure libcamera tuning profile uses `target: pisp` on Pi 5.

### Pi 4 (VideoCore ISP)
- Pi 4 should still work with libcamera, but V4L2 is often used for USB cameras.
- Ensure camera selection logic still behaves as expected if both CSI and USB cameras exist.

---

## Recommended Validation Plan

1. **Build on Trixie (64-bit)** with libcamera 0.4 and 0.5 (backports).
2. **Camera startup tests** on Pi 5 (Camera v3) and Pi 4 (CSI + optional USB).
3. **Recording tests**
   - Pi 5: software H.264 (libx264)
   - Pi 4: h264_v4l2m2m
4. **Pixel format tests**
   - Verify `YUV420` vs `NV12` adjustments
   - Confirm motion pipeline correctness if format is adjusted
5. **Stress test** multi-camera pipeline on Pi 5 to confirm no stalls.

---

## Suggested Next Actions (No Code Changes Yet)

- Decide on libcamera version target for Trixie (0.4 vs 0.5).
- Decide on Pi 4 support scope (libcamera-only vs V4L2 supported).
- After decisions, implement targeted fixes (mmap sizes, draft control gating, pixel format acceptance, rpicam tool detection).

