# Motion - Movie Formats and Encoding Reference

## Overview

Motion uses FFmpeg (libavcodec/libavformat) for video encoding and supports multiple container formats and codecs. This document catalogs all supported formats, encoding options, and their use cases.

---

## Container Formats

Motion supports the following container formats via the `movie_container` configuration parameter:

| Container | Extension | Default Codec | Best For | Notes |
|-----------|-----------|---------------|----------|-------|
| **MKV** (Matroska) | `.mkv` | H.264 | General use, default | Open format, excellent compatibility, supports metadata |
| **MP4** | `.mp4` | H.264 | Web playback, mobile devices | Widely compatible, preferred for streaming |
| **MOV** (QuickTime) | `.mov` | H.264 | Apple ecosystem | Native macOS/iOS format |
| **WEBM** | `.webm` | VP8 | Web browsers, HTML5 | Open format, designed for web |
| **MPG** (MPEG-2) | `.mpg` | MPEG-2 Video | Timelapse append mode only | Legacy format, used for appending frames |

### Special Container Configurations

#### HEVC Container Mode
Setting `movie_container = hevc` produces:
- **Container**: MP4 (`.mp4` extension)
- **Codec**: H.265/HEVC (high efficiency)
- **Use case**: Smaller file sizes at same quality vs H.264

#### Timelapse Containers
Via `timelapse_container` parameter:
- **MKV**: Creates new file per timelapse period (default)
- **MPG**: Appends frames to existing file (legacy mode)

---

## Video Codecs

Motion automatically selects codecs based on container format:

### H.264 (AV_CODEC_ID_H264)
**Used by**: MKV, MP4, MOV containers
**Default Profile**: High (main for other encoders)
**Platforms**: Software (libx264), hardware (h264_v4l2m2m on Pi)

**Quality Control**:
```
# CRF mode (constant rate factor) - software encoders
quality_value = ((100 - movie_quality) * 51) / 100
# Lower CRF = higher quality (0 = lossless, 51 = worst)

# Bitrate mode - h264_v4l2m2m hardware encoder
bit_rate = width * height * fps * movie_quality / 128
# Minimum 4000 bps enforced
```

**Encoder Settings** (libx264):
- Profile: `high` (better compression than baseline/main)
- Tune: `zerolatency` (low latency encoding)
- Preset: User-configurable (see Encoder Presets section)

**Hardware Encoder** (h264_v4l2m2m - Raspberry Pi):
- Profile: `MY_PROFILE_H264_HIGH`
- Bitrate-based quality control
- Preset: `ultrafast`, Tune: `zerolatency`
- Special NAL unit handling for compatibility

### H.265/HEVC (AV_CODEC_ID_HEVC)
**Used by**: HEVC container mode (outputs MP4)
**Advantage**: ~50% smaller files than H.264 at same quality
**Platform**: Software encoders (libx265)

**Quality Control**:
```
# Same CRF calculation as H.264
quality_value = ((100 - movie_quality) * 51) / 100
```

**Encoder Settings**:
- Tune: `zerolatency`
- Preset: User-configurable (see Encoder Presets section)

### VP8 (AV_CODEC_ID_VP8)
**Used by**: WEBM container
**Platform**: Software (libvpx)

**Quality Control**:
```
# VBR (variable bitrate) mode
if movie_quality > 0:
    quality = ((100 - movie_quality)³ * 8000) / 1000000 + 1
    # Higher values = lower quality
```

### MPEG-2 Video (AV_CODEC_ID_MPEG2VIDEO)
**Used by**: MPG container (timelapse append mode only)
**Quality**: Fixed encoding parameters
**Notes**: Legacy format for frame-by-frame appending

---

## Encoding Quality Parameters

### movie_quality
**Range**: 1-100 (default: 60)
**Interpretation**:
- **1-30**: Low quality, small files
- **31-60**: Medium quality, balanced (default: 60)
- **61-85**: High quality, larger files
- **86-100**: Very high quality, very large files

**Codec-Specific Behavior**:
- **H.264/H.265**: Maps to CRF 0-51 (inverted scale)
  - `movie_quality = 100` → CRF 0 (nearly lossless)
  - `movie_quality = 45` → CRF 28 (default level)
  - `movie_quality = 1` → CRF 51 (maximum compression)
- **VP8**: Maps to VBR quality factor
- **Hardware encoders**: Maps to bitrate calculation

### movie_bps
**Default**: 400000 (400 Kbps)
**Range**: 0 - INT_MAX
**Usage**: Overrides quality-based bitrate calculation
**Note**: Ignored for CRF-based encoders unless codec requires it

### movie_encoder_preset
**Options**: `ultrafast`, `superfast`, `veryfast`, `faster`, `fast`, `medium`, `slow`, `slower`, `veryslow`
**Default**: `medium` (config can override to `superfast`)

**Trade-offs**:

| Preset | CPU Usage | Compression | Encoding Speed | File Size | Profile |
|--------|-----------|-------------|----------------|-----------|---------|
| ultrafast | Lowest | Poor | Fastest | Largest (+30%) | Baseline |
| superfast | Very Low | Good | Very Fast | Large | High |
| veryfast | Low | Better | Fast | Moderate | High |
| fast | Moderate | Good | Moderate | Moderate | High |
| medium | Moderate | Better | Moderate | Smaller | High |
| slow | High | Best | Slow | Smallest | High |
| slower | Very High | Excellent | Very Slow | Very Small | High |
| veryslow | Extreme | Maximum | Extremely Slow | Minimal | High |

**Raspberry Pi 5 Recommendations**:
- **ultrafast**: Use if thermal throttling occurs or real-time is critical
  - Forces baseline profile (~30% larger files)
- **superfast**: Best balance for Pi 5 (recommended default)
  - Allows high profile, minimal CPU overhead
- **veryfast** or slower: Only if CPU headroom available

**Note**: Hardware encoders (h264_v4l2m2m) ignore preset and always use optimized settings.

---

## Pixel Format

All encoding uses **YUV420P** (AV_PIX_FMT_YUV420P):
- Y (luma): Full resolution
- U/V (chroma): 1/4 resolution (2x2 subsampling)
- Standard for H.264, HEVC, VP8, MPEG-2

**Memory Layout** (src/movie.cpp:1138-1152):
```cpp
picture->data[0] = image;                                // Y plane
picture->data[1] = image + (width * height);             // U plane
picture->data[2] = picture->data[1] + ((width * height) / 4);  // V plane

picture->linesize[0] = width;      // Y stride
picture->linesize[1] = width / 2;  // U stride
picture->linesize[2] = width / 2;  // V stride
```

---

## GOP (Group of Pictures) Structure

**GOP Size** (src/movie.cpp:377-388):
```cpp
if (timelapse) {
    gop_size = 1;  // All I-frames for timelapse
} else if (fps <= 5) {
    gop_size = 1;  // Low FPS = all keyframes
} else if (fps > 30) {
    gop_size = 15;  // High FPS = larger GOP
} else {
    gop_size = fps / 2;  // Normal: keyframe every 0.5 seconds
}
```

**Frame Types**:
- **I-frames** (keyframes): Self-contained, larger size
- **P-frames** (predicted): Reference previous frames, smaller size
- **B-frames**: Disabled (`max_b_frames = 0`) for low latency

---

## Passthrough Mode

For network cameras (RTSP/HTTP streams), Motion can copy streams without re-encoding.

**Configuration**:
```
movie_passthrough = on
```

**Supported Containers**: MP4, MOV, MKV
**Behavior**:
- Copies video and audio streams directly from source
- No transcoding = minimal CPU usage
- Preserves original codec and quality
- Timestamp correction for proper playback

**Limitations**:
- Source codec must be compatible with container
- MP4 requires compatible audio codecs (AAC, MP3, AC3)
- Not available for V4L2/libcamera sources (no encoded stream)

**Implementation** (src/movie.cpp:970-1136):
- Clones stream parameters from source
- Adjusts PTS/DTS timestamps to output timebase
- Writes packets with `av_interleaved_write_frame()`

---

## Timelapse Recording

Motion supports scheduled timelapse recording separate from motion events.

### Timelapse Modes (timelapse_mode)
- **off**: No timelapse (default)
- **hourly**: New file each hour
- **daily**: New file each day
- **weekly**: New file each week
- **monthly**: New file each month

### Timelapse Parameters
```
timelapse_interval = 0      # Seconds between frames (0 = every frame)
timelapse_fps = 30          # Playback FPS (1-100)
timelapse_container = mkv   # Container: mkv or mpg
timelapse_filename = %Y%m%d-timelapse
```

### Container Behavior

**MKV Mode** (`timelapse_container = mkv`):
- **Mode**: TIMELAPSE_NEW
- **Behavior**: Creates new file per timelapse period
- **Encoding**: Standard H.264 with GOP size = 1 (all I-frames)
- **Extension**: `.mkv`

**MPG Mode** (`timelapse_container = mpg`):
- **Mode**: TIMELAPSE_APPEND
- **Behavior**: Appends frames to existing file
- **Encoding**: MPEG-2 Video
- **Extension**: `.mpg`
- **Note**: Legacy mode, less efficient than MKV

### Quality Settings
- All I-frame encoding (GOP = 1) for frame-accurate seeking
- Uses same `movie_quality` parameter as regular movies
- Fixed FPS for consistent playback speed

---

## External Pipe (movie_extpipe)

Motion can pipe raw YUV420 frames to external programs for custom processing.

**Configuration**:
```
movie_extpipe_use = on
movie_extpipe = ffmpeg -f rawvideo -pix_fmt yuv420p -s %wx%h -r %fps \
                -i - -vcodec libx264 -preset ultrafast -f mp4 %f.mp4
```

**Behavior**:
- Writes raw YUV420 frames via `popen()` to command
- No encoding by Motion (handled by external program)
- Uses high-resolution images if available
- Separate from `movie_output` (both can run simultaneously)

**Use Cases**:
- Custom encoding pipelines
- Real-time streaming to external services
- Multi-format encoding
- Advanced filtering/processing

---

## Frame Rate Handling

### Low FPS Workaround (src/movie.cpp:390-402)
Some containers have issues with very low frame rates:

```cpp
if (fps <= 5 && (container == "mp4" || container == "hevc")) {
    // Encode at 10 fps, use PTS to display frames correctly
    fps = 10;
}
```

**Reason**: MP4 container has poor quality playback at <5 fps
**Solution**: Encode at 10 fps, PTS controls actual frame timing

### Dynamic FPS Fallback (src/movie.cpp:420-440)
If `avcodec_open2()` fails with requested FPS:
```cpp
// Try FPS values 1-36 until codec accepts one
chkrate = 1;
while (chkrate < 36 && retcd != 0) {
    ctx_codec->time_base.den = chkrate;
    retcd = avcodec_open2(ctx_codec, codec, &opts);
    chkrate++;
}
```

**Reason**: Some codecs have restrictions on supported frame rates
**Solution**: Automatically finds compatible FPS

---

## PTS (Presentation Timestamp) Calculation

### Normal Recording (src/movie.cpp:233-262)
```cpp
// Calculate microseconds since start
pts_interval = (1000000L * (ts->tv_sec - start_time.tv_sec)) +
               (ts->tv_nsec/1000) - (start_time.tv_nsec/1000);

// Convert to stream timebase
picture->pts = base_pts + av_rescale_q(
    pts_interval,
    {1, 1000000},           // Source: microseconds
    stream->time_base       // Target: stream timebase
);
```

**Monotonicity Check**:
- If `pts <= last_pts`, frame is skipped (timing issue)
- Prevents out-of-order frames

### Pre-Capture Handling
When pre-capture frames have negative timestamps:
```cpp
if (pts_interval < 0) {
    reset_start_time(ts);  // Adjust base time
    pts_interval = 0;
}
```

### Timelapse Mode (src/movie.cpp:229-231)
```cpp
// Simple increment for timelapse
last_pts++;
picture->pts = last_pts;
```

---

## Output File Naming

### Movie Output
**Template**: `movie_filename = %v-%{movienbr}-%Y%m%d%H%M%S`

**Format Specifiers**:
- `%v`: Camera name (device_name)
- `%{movienbr}`: Movie number (increments per event)
- `%Y%m%d%H%M%S`: Timestamp (year/month/day/hour/minute/second)
- Motion-specific conversion codes documented in motion_guide.html

**File Location**: `target_dir/movie_filename.{extension}`
**Extension**: Automatically appended based on container (`.mkv`, `.mp4`, etc.)

### Timelapse Output
**Template**: `timelapse_filename = %Y%m%d-timelapse`
**Extension**: Appended based on `timelapse_container` (`.mkv` or `.mpg`)

### Test Mode
Setting `movie_container = test` cycles through all formats:
```cpp
event_number % 6:
    1 = mov
    2 = webm
    3 = mp4
    4 = mkv
    5 = hevc
    0 = mp4 (default)
```
**Output**: `target_dir/{container}_{filename}.{ext}`

---

## Hardware Encoder Support

### h264_v4l2m2m (Raspberry Pi Hardware Encoder)

**Codec Name**: `h264_v4l2m2m`
**Usage**: Set via extended container syntax:
```
movie_container = mkv:h264_v4l2m2m
# Format: {container}:{codec_name}
```

**Characteristics**:
- Uses VideoCore GPU encoder on Raspberry Pi
- Bitrate-based quality control (not CRF)
- Fixed preset: `ultrafast`, tune: `zerolatency`
- Profile: `MY_PROFILE_H264_HIGH`
- Requires special buffer alignment (32-byte, src/movie.cpp:465-532)

**NAL Unit Handling** (src/movie.cpp:56-76):
- h264_v4l2m2m separates NAL units from first frame
- Motion captures and prepends NAL info to next keyframe
- Ensures player compatibility

**Bitrate Calculation** (src/movie.cpp:284-292):
```cpp
bitrate = width * height * fps * movie_quality / 128;
if (bitrate < 4000) bitrate = 4000;  // Minimum 4 Kbps
```

### Preferred Codec Selection

**Configuration**:
```
movie_container = mp4:libx264      # Use libx264 software encoder
movie_container = mkv:h264_v4l2m2m # Use Pi hardware encoder
```

**Fallback** (src/movie.cpp:328-352):
```cpp
if (preferred_codec != "") {
    codec = avcodec_find_encoder_by_name(preferred_codec);
    if (!codec) {
        // Fall back to container's default codec
        codec = avcodec_find_encoder(oc->video_codec_id);
    }
}
```

---

## Container-Codec Compatibility Matrix

| Container | Default Codec | Alternate Codecs | Passthrough | Notes |
|-----------|---------------|------------------|-------------|-------|
| MKV | H.264 | H.265, VP8, MPEG-2 | Yes | Most flexible |
| MP4 | H.264 | H.265 | Yes | Best compatibility |
| MOV | H.264 | H.265 | Yes | Apple ecosystem |
| WEBM | VP8 | VP9 (if available) | No | Web-only |
| MPG | MPEG-2 | None | No | Timelapse append only |

**Notes**:
- MKV accepts any codec FFmpeg supports
- MP4 requires specific audio codecs for passthrough (AAC, MP3, AC3)
- WEBM designed for VP8/VP9 only
- Container override syntax: `mkv:libx265` forces H.265 in MKV

---

## Configuration Examples

### High Quality 4K Recording (Local Storage)
```
width = 3840
height = 2160
framerate = 30
movie_container = hevc
movie_quality = 75
movie_encoder_preset = medium
movie_bps = 0
```
**Result**: H.265 encoding, CRF 12, ~25 Mbps, excellent quality

### Low CPU Pi 5 Recording
```
width = 1920
height = 1080
framerate = 30
movie_container = mkv:h264_v4l2m2m
movie_quality = 60
```
**Result**: Hardware H.264, ~8 Mbps, minimal CPU usage

### Web-Optimized Streaming
```
width = 1280
height = 720
framerate = 15
movie_container = mp4
movie_quality = 50
movie_encoder_preset = superfast
```
**Result**: H.264 MP4, CRF 25, fast encoding, web-compatible

### Passthrough RTSP Camera
```
netcam_url = rtsp://192.168.1.100/stream
movie_passthrough = on
movie_container = mp4
```
**Result**: Direct copy, no re-encoding, minimal CPU

### Daily Timelapse
```
timelapse_mode = daily
timelapse_interval = 60
timelapse_fps = 30
timelapse_container = mkv
timelapse_filename = %Y%m%d-daily
```
**Result**: One frame per minute, 30 fps playback, new file daily

---

## Troubleshooting

### Large File Sizes
**Symptoms**: Files larger than expected
**Solutions**:
1. Increase `movie_quality` (higher values = smaller files with H.264/H.265)
2. Use `movie_encoder_preset = medium` or slower
3. Switch to H.265: `movie_container = hevc`
4. Check if using `ultrafast` preset (forces baseline profile)

### Poor Quality
**Symptoms**: Blocky, blurry, or artifacts
**Solutions**:
1. Decrease `movie_quality` (lower CRF = higher quality)
2. Increase `movie_bps` if using bitrate mode
3. Use slower encoder preset (`medium`, `slow`)
4. Increase resolution if capturing small details

### CPU Overload / Thermal Throttling
**Symptoms**: Dropped frames, high CPU usage, device heating
**Solutions**:
1. Use `movie_encoder_preset = ultrafast` or `superfast`
2. Enable hardware encoder: `movie_container = mkv:h264_v4l2m2m` (Pi only)
3. Lower resolution or framerate
4. Enable `movie_passthrough` for network cameras

### Playback Issues
**Symptoms**: Won't play in certain players, stuttering
**Solutions**:
1. Use MP4 container for maximum compatibility
2. Ensure FPS is standard (15, 24, 30, 60)
3. Check GOP size isn't too large
4. Avoid very low frame rates (<5 fps)
5. Use H.264 instead of H.265 for older devices

### Timelapse Not Creating Files
**Symptoms**: No timelapse output
**Solutions**:
1. Verify `timelapse_mode` is not `off`
2. Check `target_dir` permissions
3. Ensure `timelapse_interval` is appropriate for frame rate
4. Check logs for encoding errors

---

## File Locations (Source Code)

### Primary Implementation
- **src/movie.cpp**: Core movie encoding logic (1731 lines)
  - Container setup: `get_oformat()` (line 122)
  - Codec selection: `set_codec()` (line 354)
  - Quality control: `set_quality()` (line 267)
  - Frame encoding: `encode_video()` (line 192)
  - Passthrough: `passthru_*()` functions (line 723+)
  - Timelapse: `start_timelapse()` (line 1548)

- **src/movie.hpp**: Class definition, enumerations (126 lines)

### Configuration
- **src/conf.cpp**: Parameter definitions and validation
  - Line 159: `movie_quality` parameter
  - Line 161: `movie_container` parameter
  - Line 170-174: Timelapse parameters
  - Line 669-671: Default values and ranges
  - Line 800-809: Container/mode validators

- **src/conf.hpp**: Configuration class structure
  - Line 225-242: Movie and timelapse member variables

- **data/motion-dist.conf.in**: Default configuration template
  - Line 79-89: Movie parameters with comments

---

## Summary

Motion provides flexible video encoding through FFmpeg with these highlights:

**Container Support**: MKV (default), MP4, MOV, WEBM, MPG
**Codec Support**: H.264, H.265/HEVC, VP8, MPEG-2
**Quality Control**: CRF-based (1-100) or bitrate-based
**Encoder Presets**: 9 levels from ultrafast to veryslow
**Special Modes**: Passthrough, timelapse, external pipe
**Hardware Acceleration**: h264_v4l2m2m on Raspberry Pi

**Recommended Defaults**:
- **Container**: MKV (flexible, open standard)
- **Quality**: 60 (balanced)
- **Preset**: superfast (Pi 5) or medium (desktop)
- **Codec**: H.264 (best compatibility)

For space-critical applications, use H.265 (hevc container mode).
For CPU-critical applications, use hardware encoder (h264_v4l2m2m) or passthrough mode.
