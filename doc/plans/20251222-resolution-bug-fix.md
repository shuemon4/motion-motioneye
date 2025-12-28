# Libcamera Stream Resolution Bug - Fix Plan

**Date**: 2025-12-22
**Issue**: Stream outputs at 1/4 configured resolution
**Reference**: `doc/analysis/libcamera-stream-resolution-bug-20251222-1415.md`

---

## Root Cause Analysis

After thorough code analysis, I've identified the issue is **NOT** in Motion's codebase itself, but in **how libcamera is configuring the stream resolution on Pi 5**.

### Evidence Trail

1. **The Code Path is Correct**:
   - `libcam.cpp:start_config()` correctly requests the configured width/height (lines 895-896)
   - `cam->imgs.width/height` are set from libcamera's validated configuration (lines 938-939)
   - Stream JPEG encoding uses these dimensions correctly via `put_memory()` (webu_getimg.cpp:114-120)

2. **The Problem is Upstream**:
   Looking at `libcam.cpp:start_config()`:
   ```cpp
   config->at(0).size.width = (uint)cam->cfg->width;   // Requests 1920
   config->at(0).size.height = (uint)cam->cfg->height; // Requests 1080

   retcd = config->validate();  // <-- libcamera may adjust here!

   // Lines 930-936 log any adjustment, but...
   cam->imgs.width = (int)config->at(0).size.width;    // What libcamera returned
   cam->imgs.height = (int)config->at(0).size.height;
   ```

3. **Why 480x272 Specifically?**:
   - Pi Camera v3 (IMX708) native modes: 1536x864, 2304x1296, 4608x2592
   - Requested: 1920x1080 (not a native mode)
   - libcamera must scale from a native mode
   - 480x272 ≈ 1920/4 x 1080/4 - suggests libcamera may be returning a thumbnail/preview stream

4. **The `stream_preview_scale` Red Herring**:
   - Default is 25%
   - 480 ÷ (25/100) = 1920, 272 ÷ (25/100) = 1088 ≈ 1080
   - This suggests `stream_preview_scale` is being misapplied somewhere
   - However, I verified it's only used for:
     - HTML CSS width (display scaling, not resolution)
     - AllCam combined view (device_id=0 only)

### Hypothesis

The bug is caused by one of:

**A) libcamera Configuration Adjustment** (Most Likely):
- `config->validate()` is adjusting the resolution downward
- The adjustment log message (lines 930-936) may not be appearing if the adjustment happens differently
- Pi 5's PiSP may handle this differently than Pi 4's VideoCore

**B) StreamRole Mismatch** (Less Likely but Possible):
- We're using `StreamRole::VideoRecording` (correct for Pi 5)
- But there may be a secondary stream being configured that we're not aware of

**C) Buffer Size Mismatch**:
- The buffer plane size calculation in `start_req()` might be adjusting dimensions (lines 1098-1114)
- This would explain why the stream dimensions differ from configured

---

## Fix Plan

### Phase 1: Add Diagnostic Logging (Priority: Critical)

Add explicit logging to trace the exact resolution at each step:

1. **In `libcam.cpp:start_config()` after validate():**
   ```cpp
   MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
       , "After validate: config size=%dx%d, requested=%dx%d"
       , config->at(0).size.width, config->at(0).size.height
       , cam->cfg->width, cam->cfg->height);
   ```

2. **In `libcam.cpp:start_req()` after buffer allocation:**
   ```cpp
   MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
       , "Buffer allocated: plane0_length=%d, expected_size=%d, imgs.width=%d, imgs.height=%d"
       , buffer->planes()[0].length
       , cam->imgs.size_norm
       , cam->imgs.width, cam->imgs.height);
   ```

3. **In `webu_getimg.cpp:webu_getimg_norm()`:**
   ```cpp
   MOTION_LOG(DBG, TYPE_STREAM, NO_ERRNO
       , "Stream JPEG encode: %dx%d, buffer_size=%d"
       , cam->imgs.width, cam->imgs.height, cam->imgs.size_norm);
   ```

### Phase 2: Verify StreamConfiguration (Priority: High)

After diagnostics confirm the issue, fix the configuration:

1. **Ensure resolution is not auto-adjusted:**
   ```cpp
   // After setting size but before validate()
   config->at(0).size.width = (uint)cam->cfg->width;
   config->at(0).size.height = (uint)cam->cfg->height;

   // Log raw config before validate
   MOTION_LOG(NTC, TYPE_VIDEO, NO_ERRNO
       , "Pre-validate: stream config %dx%d, pixelFormat=%s"
       , config->at(0).size.width
       , config->at(0).size.height
       , config->at(0).pixelFormat.toString().c_str());
   ```

2. **Check if libcamera is selecting a lowres mode:**
   - libcamera may be selecting a sensor mode that doesn't match our request
   - May need to explicitly set sensor mode hints

### Phase 3: Potential Code Fixes

Based on diagnostic results, one of these fixes:

**Option A: Force Resolution After Adjustment**
```cpp
// If libcamera adjusts resolution, override imgs dimensions to use buffer size
// (This is already partially done in start_req but may need refinement)
```

**Option B: Use SensorConfiguration**
```cpp
// Explicitly request a sensor mode that supports our resolution
#include <libcamera/camera_manager.h>
// Use camera->properties() to query available sensor modes
// Select appropriate mode before generating configuration
```

**Option C: Use Multiple Streams**
```cpp
// Configure both a main stream (full res) and a viewfinder stream
// Pi 5 PiSP may require explicit stream role separation
StreamRoles roles = { StreamRole::VideoRecording, StreamRole::Viewfinder };
config = camera->generateConfiguration(roles);
// Use VideoRecording stream for capture, ignore Viewfinder
```

---

## Testing Steps

### Diagnostic Logging Added (2025-12-22)

The following diagnostic logging has been added to trace resolution through the pipeline:

**In `src/libcam.cpp`:**
1. Pre-validate: logs requested size, pixel format, buffer count
2. Post-validate: logs returned size, stride, frameSize, validation result
3. Final dimensions: logs imgs.width, imgs.height, size_norm, motionsize
4. Buffer allocation: logs total bytes, expected size, plane0 length
5. After buffer check: logs final dimensions after any adjustment

**In `src/webu_getimg.cpp`:**
1. Stream encode: logs width, height, size_norm, quality (once per session)

### Build and Deploy to Pi 5

```bash
# On the Pi 5:
cd ~/motion
git pull  # or copy updated files

autoreconf -fiv
./configure --with-libcam --with-sqlite3
make -j4
sudo make install
sudo systemctl restart motion
```

### Check Diagnostic Logs

```bash
# Watch logs for DIAG entries
sudo journalctl -u motion -f | grep "DIAG:"

# Or for MotionEye:
sudo journalctl -u motioneye -f | grep "DIAG:"
```

### Expected Log Output

With working 1920x1080 configuration, you should see:
```
DIAG: Pre-validate config: requested=1920x1080, pixelFormat=YUV420, bufferCount=4
DIAG: Post-validate config: size=1920x1080, stride=1920, frameSize=3110400, validate_result=0
DIAG: Final image dimensions: imgs.width=1920, imgs.height=1080, size_norm=3110400, motionsize=2073600
DIAG: Buffer allocation: total_bytes=3110400, expected_size_norm=3110400, plane0_length=2073600, num_planes=2
DIAG: After buffer check: imgs.width=1920, imgs.height=1080, size_norm=3110400
DIAG: Stream JPEG encode: width=1920, height=1080, size_norm=3110400, quality=53
```

If the bug is in libcamera, you'll see discrepancy in Post-validate vs Pre-validate.
If the bug is in buffer allocation, you'll see adjustment in "After buffer check".

### Verify Stream Resolution

```bash
# Capture a single JPEG from stream
curl -s 'http://localhost:7999/1/mjpg/stream' --max-time 2 > /tmp/test.mjpg
# Extract dimensions from first JPEG
file /tmp/test.mjpg  # May show MJPEG info

# Or use identify if available
identify /tmp/test.mjpg | head -1
```

### Compare with Direct libcamera

```bash
rpicam-hello --verbose --width 1920 --height 1080 -t 5000 2>&1 | grep -i "size\|resolution\|mode"
```

---

## Success Criteria

1. `width 1920` / `height 1080` config produces 1920x1080 stream output
2. Frame sizes are appropriate (~50-100KB for JPEG at quality 53)
3. No regression in framerate or CPU usage
4. Works across all supported libcamera resolutions

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/libcam.cpp` | Add diagnostic logging, potentially fix configuration |
| `src/webu_getimg.cpp` | Add diagnostic logging |
| `configure.ac` | (If needed) Check for libcamera version requirements |

---

## Risk Assessment

- **Low Risk**: Diagnostic logging - no functional changes
- **Medium Risk**: StreamConfiguration fixes - may affect other camera models
- **Mitigation**: Test on both Pi 4 and Pi 5, with multiple camera types

---

## Investigation Findings (2025-12-22 Evening Session)

### Confirmed Facts

1. **libcamera Configuration is Correct**:
   - DIAG logs show: `Pre-validate config: requested=1920x1080, pixelFormat=YUV420`
   - DIAG logs show: `Post-validate config: size=1920x1080, stride=1920, frameSize=3110400`
   - libcamera confirms: `configuring streams: (0) 1920x1080-YUV420/Rec709`
   - `cam->imgs.width=1920, height=1080, size_norm=3110400`
   - Buffer allocation correct: `total_bytes=3110400, plane0_length=2073600, num_planes=3`

2. **Stream Output is 480x272**:
   - Direct capture from `http://localhost:7999/1/mjpg/stream` shows JPEG dimensions 480x272
   - This is exactly 25% of 1920x1080 (1920/4=480, 1080/4≈272)
   - Frame size ~23KB (consistent with 480x272 JPEG)

3. **The Scaling Pattern**:
   - With `stream_preview_scale` default (25): Output 120x72
   - With `stream_preview_scale=100`: Output 480x272
   - This pattern shows: SOURCE is 480x272, and stream_preview_scale is applied correctly on top

### Contradiction

The diagnostic logs show `imgs.width=1920, imgs.height=1080` at libcamera initialization, but the actual stream JPEG is 480x272. This means either:

1. **Image Data Mismatch**: The YUV data in `current_image->image_norm` is 480x272 despite `imgs.width/height` being 1920x1080
2. **Dimensions Changed After Logging**: Something modifies `imgs.width/height` after the DIAG but before JPEG encoding
3. **Different Code Path**: Stream encoding uses different dimensions than what was logged

### Code Analysis Summary

| Location | What Happens | Status |
|----------|--------------|--------|
| `libcam.cpp:start_config()` | Sets config to 1920x1080 | ✓ Verified correct |
| `libcam.cpp:start_req()` | Allocates 3110400 byte buffers | ✓ Verified correct |
| `libcam.cpp:libcam_getimg()` | Copies frame to `image_norm` | Uses buffer size, not dimensions |
| `webu_getimg.cpp:webu_getimg_norm()` | Encodes JPEG with `imgs.width/height` | Passes dimensions to put_memory() |
| `picture.cpp:put_memory()` | JPEG encode | Uses passed width/height |
| `jpegutils.cpp:jpgutl_put_yuv420p()` | Sets `cinfo.image_width/height` | Uses passed parameters |

### Hypothesis Refinement

**Most Likely Cause**: The `stream_preview_scale` parameter is being applied to the JPEG encoding dimensions for individual camera streams, not just the allcam combined view.

Looking at the pattern:
- If SOURCE were 1920x1080 and scale=25 applied: 480x270 ✓
- If SOURCE were 1920x1080 and scale=100 applied: 1920x1080 ✗ (but we see 480x272)
- If SOURCE were 480x272 and scale=25 applied: 120x68 ≈ 120x72 ✓
- If SOURCE were 480x272 and scale=100 applied: 480x272 ✓

**Conclusion**: The SOURCE resolution is 480x272, not 1920x1080. Something is producing 1/4 resolution frames.

### Recommended Fix Approach

1. **Add Runtime DIAG**: Log `cam->imgs.width/height` at the exact moment of JPEG encoding in `webu_getimg_norm()` to verify values during stream

2. **Check for Hidden Scaling**: Search for any scaling operations between `libcam_getimg()` and `webu_getimg_norm()`

3. **Investigate camera.cpp Loop**: Check if the main camera processing loop modifies `imgs` dimensions

4. **Compare with Saved Images**: Capture a still image (not stream) and verify its resolution to isolate if issue is stream-specific

### Next Testing Session

1. Kill all Motion processes cleanly
2. Start Motion standalone (not via MotionEye)
3. Wait for camera to fully initialize
4. Connect to stream AND capture simultaneously
5. Verify DIAG logs show stream encoding dimensions
6. Compare with JPEG output dimensions

---

## Deep Code Analysis (2025-12-22 Follow-up Session)

### libcamera Controls Review

Reviewed the complete libcamera controls document (`doc/libcamera/libcamera-controls.html`). Key findings:

1. **ScalerCrop Control**: Only control related to image resizing is `ScalerCrop`, which is for digital zoom (cropping a portion that gets scaled UP to output). It does NOT cause downscaling.

2. **No Auto-Downscale Control**: There is no libcamera control that would automatically downscale the output stream.

3. **Motion's Control Usage**: Motion only sets `ScalerCrop` if explicitly configured via `libcam_control_item`. It's not set by default.

### Code Path Verification

Complete trace through the stream generation code:

| Step | File:Line | Operation | Status |
|------|-----------|-----------|--------|
| 1 | libcam.cpp:897 | `generateConfiguration(VideoRecording)` | ✓ Correct role |
| 2 | libcam.cpp:901-902 | Set config size to `cfg->width/height` | ✓ Uses config |
| 3 | libcam.cpp:960-961 | Set `imgs.width/height` from validated config | ✓ Verified |
| 4 | libcam.cpp:1627 | memcpy buffer to `image_norm` | ✓ Full buffer |
| 5 | webu_getimg.cpp:124-130 | `put_memory(imgs.width, imgs.height)` | ✓ Passes correct dims |
| 6 | jpegutils.cpp:881-882 | `cinfo.image_width/height = width/height` | ✓ Uses passed values |
| 7 | webu_stream.cpp:272-273 | Copy `jpg_data` to response | ✓ No modification |

### Key Finding: `stream_preview_scale` NOT Applied to Individual Streams

Verified that `stream_preview_scale` is ONLY used for:

1. **allcam.cpp:460** - Combined view (device_id=0) destination scaling
2. **allcam.cpp:506** - Per-camera scale within allcam grid
3. **webu_html.cpp:1226,1237** - HTML `width=XX%` CSS attribute (display only)

Individual camera streams (`/1/mjpg/stream`) bypass all of this and use `cam->imgs.width/height` directly.

### Refined Hypothesis

The contradiction between DIAG logs (1920x1080) and JPEG output (480x272) suggests:

**Hypothesis A: Stale Binary**
The Motion binary on Pi has outdated code that differs from this source. The DIAG logs may have been added but not deployed, or the deployed version has different behavior.

**Hypothesis B: MotionEye Configuration Override**
MotionEye may be writing a different `width`/`height` to the Motion config than what we think. It could be applying its own resolution calculation.

**Hypothesis C: Double Encoding Bug**
There may be a bug where the wrong stream buffer is being served. For example, the `sub` stream (which is half size) being mistakenly served as `norm`.

### Required Verification Steps

1. **Verify Binary Version**:
   ```bash
   ssh admin@192.168.1.176 "motion -h 2>&1 | head -5"
   ssh admin@192.168.1.176 "ls -la /usr/local/bin/motion"
   ssh admin@192.168.1.176 "md5sum /usr/local/bin/motion"
   ```

2. **Check Actual Config Values**:
   ```bash
   ssh admin@192.168.1.176 "grep -E 'width|height' /etc/motioneye/camera-1.conf"
   ```

3. **Add Per-Frame DIAG**: Modify DIAG to log on first 5 frames, not just once:
   ```cpp
   static int diag_count = 0;
   if (diag_count < 5) {
       MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
           , "DIAG: Stream encode #%d: width=%d, height=%d"
           , diag_count, cam->imgs.width, cam->imgs.height);
       diag_count++;
   }
   ```

4. **Compare sub vs norm Stream**:
   ```bash
   # Test norm stream
   curl -s --max-time 2 'http://localhost:7999/1/mjpg/stream' > /tmp/norm.mjpg
   # Test sub stream
   curl -s --max-time 2 'http://localhost:7999/1/mjpg/sub' > /tmp/sub.mjpg
   # Compare sizes
   ls -la /tmp/norm.mjpg /tmp/sub.mjpg
   ```

5. **Verify webu_getimg_norm() is Being Called**:
   Add logging to confirm the norm stream path, not sub stream path, is active.
