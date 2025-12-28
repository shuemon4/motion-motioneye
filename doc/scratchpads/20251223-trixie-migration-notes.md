# Trixie 64-bit Migration - Working Notes

**Date:** 2025-12-23
**Status:** In Progress

## Session Context

Implementing the Trixie 64-bit migration per plan in `doc/plans/20251223-trixie-64bit-migration.md`.

## Key Constraints Reminder

1. **Do NOT modify** `src/alg.cpp` or `src/alg.hpp` - proven algorithm
2. **Minimize CPU usage** - Pi is resource-constrained
3. **Test builds before committing** - must compile

## Progress Tracking

### Phase 1: 64-bit Type Safety ✅ COMPLETE
- [x] 1.1: Fix buffer size type in ctx_imgmap (`src/libcam.hpp:36`)
- [x] 1.2: Fix mmap size casts in libcam.cpp (lines ~1128-1134, ~1167, ~1177, ~1182, ~1629, ~1632)
- [x] 1.3: Fix pointer logging in video_v4l2.cpp (line ~795)

### Phase 2: libcamera Compatibility ✅ COMPLETE
- [x] 2.1: Add version guards for 9 draft controls (lines 718-742)
- [x] 2.2: Accept NV12 pixel format (lines 934-938)
- [x] 2.3: NV12 conversion (DEFERRED - only if testing shows need)

### Phase 3: Script Updates ✅ COMPLETE
- [x] 3.1: Update camera tool detection in scripts/pi5-setup.sh (lines 80-88)
- [x] 3.2: Create Pi 4 build profile (scripts/pi4-build.sh)
- [x] 3.3: Update pi5-build.sh comments

### Phase 4: FFmpeg Compatibility ✅ COMPLETE
- [x] 4.1: Guard avdevice_register_all (src/motion.cpp:280)

### Build Validation ✅ COMPLETE
- [x] Build succeeds on Pi 5 with Bookworm (backward compatible)
- No new warnings introduced
- Only pre-existing float conversion warnings remain

## Notes

### Decisions Already Made
- libcamera: Support both 0.4.0 and 0.5.x
- Pi 4: Both Pi 4 and Pi 5 supported with separate build profiles
- Pixel format: Accept NV12, defer conversion until testing proves needed
- Draft controls: Silent skip if unavailable (no warning logs)

### Files to Modify
- `src/libcam.hpp`
- `src/libcam.cpp`
- `src/video_v4l2.cpp`
- `src/motion.cpp`
- `scripts/pi5-setup.sh`
- `scripts/pi5-build.sh`
- `scripts/pi4-build.sh` (new)

## Build Testing

Will need to sync to Pi 5 and test:
```bash
rsync -avz --exclude='.git' /Users/tshuey/Documents/GitHub/motion/ admin@192.168.1.176:~/motion/
ssh admin@192.168.1.176 "cd ~/motion && autoreconf -fiv && ./configure --with-libcam --with-sqlite3 && make -j4"
```
