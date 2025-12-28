# Libcamera Stream Resolution Bug Analysis

**Date**: 2025-12-22
**Motion Version**: 5.0.0-gitUNKNOWN
**Platform**: Raspberry Pi 5, Debian Bookworm
**Camera**: Pi Camera v3 (IMX708 Wide NoIR)
**Severity**: High - Stream outputs at 1/4 configured resolution

---

## Executive Summary

Motion's libcamera integration is outputting video streams at approximately 1/4 of the configured resolution. When configured for 1920x1080, the actual stream output is 480x272. Direct libcamera testing confirms the camera hardware works correctly at full resolution, isolating the bug to Motion's libcamera stream handling code.

---

## Problem Description

### Symptoms
- Stream resolution is significantly lower than configured
- Frame sizes are ~11KB instead of expected ~50-100KB for 1080p JPEG
- Video appears pixelated/degraded in MotionEye web interface
- Issue persists across service restarts

### Expected vs Actual Behavior

| Configuration | Expected | Actual (without stream_preview_scale) | Actual (with stream_preview_scale 100) |
|--------------|----------|---------------------------------------|----------------------------------------|
| 1920x1080 | 1920x1080 | 120x72 | 480x272 |
| 1280x720 | 1280x720 | 80x64 | ~320x180 (estimated) |

The `stream_preview_scale` parameter affects output but doesn't fix the underlying capture resolution issue.

---

## Environment Details

### Hardware
```
Raspberry Pi 5 Model B Rev 1.0
8GB RAM
Camera: imx708_wide_noir [4608x2592 10-bit]
```

### Software Versions
```
Motion: 5.0.0-gitUNKNOWN (compiled from source)
libcamera: v0.5.2+99-bfd68f78
libpisp: v1.2.1 981977ff21f3 29-04-2025
Kernel: Linux 6.x (Debian Bookworm)
```

### Camera Detection
```
Available cameras
-----------------
0 : imx708_wide_noir [4608x2592 10-bit] (/base/axi/pcie@1000120000/rp1/i2c@88000/imx708@1a)
    Modes: 'SBGGR10_CSI2P' : 1536x864 [30.00 fps]
                             2304x1296 [30.00 fps]
                             4608x2592 [30.00 fps]
```

---

## Diagnostic Evidence

### Test 1: Direct libcamera Capture (PASSED)
```bash
rpicam-jpeg -o /tmp/test.jpg --width 1920 --height 1080 -t 2000
```
**Result**: Successfully captured 1920x1080 JPEG (491,629 bytes)

libcamera output showed correct mode selection:
```
Mode selection for 1920:1080:12:P
    SRGGB10_CSI2P,2304x1296/0 - Score: 1150  # Selected
configuring streams: (0) 1920x1080-YUV420/sYCC
```

### Test 2: Motion Stream Capture (FAILED)
```bash
curl -s --max-time 3 'http://localhost:7999/1/mjpg/stream' > /tmp/stream.bin
```
**Result**: Frame resolution 480x272 (~11KB per frame)

Python analysis:
```python
# Extracted JPEG dimensions from stream
Resolution: 480x272  # Should be 1920x1080
Frame size: 11187 bytes  # Should be ~50-100KB
```

### Test 3: Resolution Scaling Behavior
| stream_preview_scale | Output Resolution | Notes |
|---------------------|-------------------|-------|
| Not set (default) | 120x72 | 1/16 of configured |
| 100 | 480x272 | 1/4 of configured |
| 400 | Invalid (reverts to default) | Range is 1-100 |

---

## Technical Analysis

### Suspected Root Cause

The bug appears to be in how Motion initializes libcamera streams. Based on the evidence:

1. **Motion captures at 1/4 resolution internally** - The 480x272 output (with scale=100) suggests Motion is requesting/receiving 480x272 frames from libcamera, not 1920x1080

2. **stream_preview_scale only affects output scaling** - It scales from the internal capture resolution, not the configured resolution

3. **The internal resolution appears to be width/4 x height/4** - This is consistent across different configured resolutions

### Likely Code Locations

Based on Motion's architecture, investigate these areas:

1. **`src/libcam.cpp`** - libcamera initialization and stream configuration
   - Look for where StreamConfiguration is set up
   - Check if width/height from config are being properly passed
   - Look for any hardcoded scaling or thumbnail modes

2. **`src/libcam.hpp`** - libcamera class definitions
   - Check stream buffer configuration
   - Verify resolution parameters are stored correctly

3. **`src/video_common.cpp`** - Common video handling
   - Check if there's post-capture scaling happening
   - Look for resolution override logic

4. **`src/conf.cpp`** - Configuration parsing
   - Verify width/height are being read correctly for libcamera devices

### Key Questions to Investigate

1. Is Motion requesting the correct resolution from libcamera?
2. Is there a "preview" vs "main" stream confusion?
3. Is there post-capture downscaling happening?
4. Are there Pi 5 / PiSP specific code paths that differ from Pi 4?

---

## Relevant Configuration

### Working Camera Config (`/etc/motioneye/camera-1.conf`)
```ini
libcam_device auto
libcam_buffer_count 4
width 1920
height 1080
framerate 15
stream_maxrate 30
stream_quality 53
stream_preview_scale 100
```

### Motion Main Config
```ini
webcontrol_port 7999
webcontrol_interface default
webcontrol_localhost on
webcontrol_parms 2
```

---

## Reproduction Steps

1. Configure Motion with libcamera device on Pi 5
2. Set `width 1920` and `height 1080` in camera config
3. Start Motion service
4. Capture stream output: `curl -s --max-time 3 'http://localhost:7999/1/mjpg/stream' > stream.bin`
5. Extract first JPEG and check dimensions:
```python
# Find SOF marker (0xFF 0xC0 or 0xFF 0xC2)
# Read height at offset +5,+6 and width at +7,+8
```

---

## Suggested Investigation Approach

### Phase 1: Trace Resolution Through Code
1. Add debug logging at libcamera stream configuration
2. Log what resolution Motion requests from libcamera
3. Log what resolution libcamera reports back
4. Log frame dimensions at capture time

### Phase 2: Compare with Working Scenarios
1. Test with V4L2 device (USB webcam) - does it have same issue?
2. Test on Pi 4 with libcamera - same issue?
3. Compare libcam.cpp initialization between Pi 4 and Pi 5 modes

### Phase 3: Fix Implementation
1. Identify where resolution gets reduced
2. Implement fix to use full configured resolution
3. Test with multiple resolution settings
4. Verify no performance regression

---

## Additional Observations

### Port 8081 Error (Non-blocking)
```
[ERR][STR][mo00] start_daemon_port2: Unable to start secondary webserver on port 8081
```
This error appears at every startup but doesn't affect primary stream functionality. It may be related to a secondary stream configuration issue.

### Config Option Warnings
```
[ALR][ALL][mo00] edit_set: Unknown config option "libcam_control_item"
```
This custom MotionEye option is not recognized by Motion. Not related to resolution bug but should be addressed separately.

---

## Test Environment SSH Access

```bash
# Pi 5 connection
ssh admin@192.168.1.176

# Check Motion logs
sudo journalctl -u motioneye -f

# Test stream
curl -s --max-time 3 'http://localhost:7999/1/mjpg/stream' | wc -c

# Direct libcamera test (stop motioneye first)
sudo systemctl stop motioneye
rpicam-jpeg -o /tmp/test.jpg --width 1920 --height 1080 -t 2000
file /tmp/test.jpg
sudo systemctl start motioneye
```

---

## References

- Motion libcamera source: `src/libcam.cpp`, `src/libcam.hpp`
- libcamera documentation: https://libcamera.org/api-html/
- Pi Camera v3 sensor modes: IMX708 supports 1536x864, 2304x1296, 4608x2592
- PiSP (Pi 5 ISP): Uses different pipeline than Pi 4's VideoCore

---

## Success Criteria

The fix is successful when:
1. `width 1920` / `height 1080` config produces 1920x1080 stream output
2. Frame sizes are appropriate (~50-100KB for JPEG at quality 53)
3. No regression in framerate or CPU usage
4. Works across all supported libcamera resolutions
