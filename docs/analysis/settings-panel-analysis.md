# Settings Panel Analysis

This document provides a comprehensive analysis of all Motion configuration parameters, their hot-reload capability, and API endpoints.

## API Endpoints

### Configuration Retrieval
- **Endpoint**: `GET /0/api/config`
- **Returns**: Full configuration including version, cameras, and all parameters with metadata
- **Response Structure**:
```typescript
{
  version: string,
  csrf_token: string,
  cameras: Record<string, unknown>,
  configuration: {
    default: Record<string, {
      value: string | number | boolean,
      enabled: boolean,
      category: number,
      type: string,
      list?: string[]
    }>
  },
  categories: Record<string, { name: string, display: string }>
}
```

### Configuration Updates
- **Endpoint**: `POST /{camId}/config/set?{param}={value}`
- **Parameters**:
  - `camId`: Camera ID (0 = global/default settings)
  - `param`: Parameter name
  - `value`: New value (URL-encoded)
- **Headers Required**: `X-CSRF-Token` header with token from config response

### Hot Reload Response
The API responds with hot-reload status for each change:
```json
{
  "param": "parameter_name",
  "new_value": "value",
  "hot_reload": true|false
}
```

## Current Settings Panel Implementation

The React Settings panel (`frontend/src/pages/Settings.tsx`) currently exposes these settings:

| Setting | Parameter | Hot-Reload | Category |
|---------|-----------|------------|----------|
| Run as Daemon | `daemon` | No | System |
| Log File | `log_file` | No | System |
| Log Level | `log_level` | No | System |
| Device Name | `device_name` | Yes | Camera |
| Device ID | `device_id` | No | Camera |
| Threshold | `threshold` | Yes | Detection |
| Noise Level | `noise_level` | Yes | Masks |
| Despeckle Filter | `despeckle_filter` | Yes | Masks |
| Minimum Motion Frames | `minimum_motion_frames` | Yes | Detect |
| Target Directory | `target_dir` | No | Camera |
| Snapshot Filename | `snapshot_filename` | Yes | Picture |
| Picture Filename | `picture_filename` | Yes | Picture |
| Movie Filename | `movie_filename` | Yes | Movies |
| Movie Container | `movie_container` | No | Movies |
| Stream Port | `stream_port` | No | Streams |
| Stream Quality | `stream_quality` | Yes | Streams |
| Stream Max Rate | `stream_maxrate` | Yes | Streams |
| Stream Motion | `stream_motion` | Yes | Streams |
| Stream Authentication | `stream_auth_method` | No | Webcontrol |

## Complete Parameter Reference

### Category 00: System Parameters
All system parameters require a restart to take effect.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `daemon` | bool | No | Run Motion in background mode |
| `conf_filename` | string | No | Configuration file path |
| `pid_file` | string | No | PID file path |
| `log_file` | string | No | Log file path |
| `log_level` | list | No | Log verbosity (1-9) |
| `log_fflevel` | list | No | File log level |
| `log_type` | list | No | Log type filter |
| `native_language` | bool | No | Use native language |

### Category 01: Camera Parameters

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `device_name` | string | **Yes** | Display name only |
| `device_id` | int | No | Unique camera identifier |
| `device_tmo` | int | No | Device timeout |
| `pause` | list | **Yes** | Runtime pause control |
| `schedule_params` | params | No | Schedule configuration |
| `cleandir_params` | params | No | Directory cleanup |
| `target_dir` | string | No | Output directory |
| `watchdog_tmo` | int | No | Watchdog timeout |
| `watchdog_kill` | int | No | Watchdog kill threshold |
| `config_dir` | string | No | Config directory |
| `camera` | string | No | Camera configuration file |

### Category 02: Source Parameters
Most source parameters require restart as they affect device initialization.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `v4l2_device` | string | No | V4L2 device path |
| `v4l2_params` | params | No | V4L2 parameters |
| `netcam_url` | string | No | Network camera URL |
| `netcam_params` | params | No | Network camera params |
| `netcam_high_url` | string | No | High-res stream URL |
| `netcam_high_params` | params | No | High-res stream params |
| `netcam_userpass` | string | No | Auth credentials |
| `libcam_device` | string | No | Libcamera device |
| `libcam_params` | params | No | Libcamera parameters |
| `libcam_buffer_count` | int | No | Buffer count |
| `libcam_brightness` | string | **Yes** | Applied immediately via libcamera API |
| `libcam_contrast` | string | **Yes** | Applied immediately via libcamera API |
| `libcam_iso` | int | **Yes** | Applied immediately via libcamera API |
| `libcam_awb_enable` | bool | **Yes** | Auto white balance toggle |
| `libcam_awb_mode` | int | **Yes** | AWB mode selection |
| `libcam_awb_locked` | bool | **Yes** | Lock AWB |
| `libcam_colour_temp` | int | **Yes** | Manual color temperature |
| `libcam_colour_gain_r` | string | **Yes** | Red color gain |
| `libcam_colour_gain_b` | string | **Yes** | Blue color gain |
| `libcam_af_mode` | int | **Yes** | Autofocus mode |
| `libcam_lens_position` | string | **Yes** | Manual focus position |
| `libcam_af_range` | int | **Yes** | AF range |
| `libcam_af_speed` | int | **Yes** | AF speed |
| `libcam_af_trigger` | int | **Yes** | Trigger AF scan (0=start, 1=cancel) |

### Category 03: Image Parameters
Image parameters require restart due to buffer reallocation.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `width` | int | No | Image width |
| `height` | int | No | Image height |
| `framerate` | int | No | Capture framerate |
| `rotate` | list | No | Rotation (0, 90, 180, 270) |
| `flip_axis` | list | No | Flip none/horizontal/vertical |

### Category 04: Overlay Parameters
All overlay parameters are hot-reloadable.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `locate_motion_mode` | list | **Yes** | Motion box display mode |
| `locate_motion_style` | list | **Yes** | Motion box style |
| `text_left` | string | **Yes** | Left overlay text |
| `text_right` | string | **Yes** | Right overlay text |
| `text_changes` | bool | **Yes** | Show changed pixel count |
| `text_scale` | list | **Yes** | Text scale factor |
| `text_event` | string | **Yes** | Event text |

### Category 05: Detection Method Parameters
Most detection parameters are hot-reloadable for tuning.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `emulate_motion` | bool | **Yes** | Force continuous motion |
| `threshold` | int | **Yes** | Changed pixel threshold |
| `threshold_maximum` | int | **Yes** | Maximum threshold |
| `threshold_sdevx` | int | **Yes** | X std deviation |
| `threshold_sdevy` | int | **Yes** | Y std deviation |
| `threshold_sdevxy` | int | **Yes** | XY std deviation |
| `threshold_ratio` | int | **Yes** | Ratio threshold |
| `threshold_ratio_change` | int | **Yes** | Ratio change threshold |
| `threshold_tune` | bool | **Yes** | Auto-tune threshold |
| `secondary_method` | list | No | May need model reload |
| `secondary_params` | params | No | May need model reload |

### Category 06: Mask Parameters

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `noise_level` | int | **Yes** | Noise tolerance (0-255) |
| `noise_tune` | bool | **Yes** | Auto-tune noise |
| `despeckle_filter` | string | **Yes** | Despeckle filter string |
| `area_detect` | string | **Yes** | Area detection config |
| `mask_file` | string | No | Requires PGM file reload |
| `mask_privacy` | string | No | Requires PGM file reload |
| `smart_mask_speed` | list | **Yes** | Smart mask adaptation |

### Category 07: Detect Parameters
All detect parameters are hot-reloadable.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `lightswitch_percent` | int | **Yes** | Light change threshold % |
| `lightswitch_frames` | int | **Yes** | Frames to detect light change |
| `minimum_motion_frames` | int | **Yes** | Min frames for event |
| `static_object_time` | int | **Yes** | Static object timeout |
| `event_gap` | int | **Yes** | Gap between events (sec) |
| `pre_capture` | int | **Yes** | Pre-event buffer frames |
| `post_capture` | int | **Yes** | Post-event buffer frames |

### Category 08: Script Parameters
All script parameters are hot-reloadable but have RESTRICTED access level.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `on_event_start` | string | **Yes** | Script on event start |
| `on_event_end` | string | **Yes** | Script on event end |
| `on_picture_save` | string | **Yes** | Script on picture save |
| `on_area_detected` | string | **Yes** | Script on area detection |
| `on_motion_detected` | string | **Yes** | Script on motion detection |
| `on_movie_start` | string | **Yes** | Script on movie start |
| `on_movie_end` | string | **Yes** | Script on movie end |
| `on_camera_lost` | string | **Yes** | Script on camera lost |
| `on_camera_found` | string | **Yes** | Script on camera found |
| `on_secondary_detect` | string | **Yes** | Script on secondary detection |
| `on_action_user` | string | **Yes** | User action script |
| `on_sound_alert` | string | **Yes** | Sound alert script |

### Category 09: Picture Parameters
Mostly hot-reloadable for filename/quality changes.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `picture_output` | list | **Yes** | Picture output mode |
| `picture_output_motion` | list | **Yes** | Motion picture mode |
| `picture_type` | list | **Yes** | Output format (jpeg/webp/ppm) |
| `picture_quality` | int | **Yes** | JPEG quality (1-100) |
| `picture_exif` | string | **Yes** | EXIF string |
| `picture_filename` | string | **Yes** | Filename format |
| `snapshot_interval` | int | **Yes** | Snapshot interval (sec) |
| `snapshot_filename` | string | **Yes** | Snapshot filename format |

### Category 10: Movie Parameters
Most movie parameters require restart due to encoder configuration.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `movie_output` | bool | No | Recording state |
| `movie_output_motion` | bool | No | Motion recording state |
| `movie_max_time` | int | **Yes** | Max movie duration |
| `movie_bps` | int | No | Encoder bitrate |
| `movie_quality` | int | No | Encoder quality |
| `movie_encoder_preset` | list | No | Encoder preset |
| `movie_container` | string | No | Container format |
| `movie_passthrough` | bool | No | Passthrough mode |
| `movie_filename` | string | **Yes** | Filename format |
| `movie_retain` | list | **Yes** | Retention policy |
| `movie_all_frames` | bool | No | Encoder config |
| `movie_extpipe_use` | bool | No | External pipe mode |
| `movie_extpipe` | string | No | External pipe command |

### Category 11: Timelapse Parameters
Timelapse parameters require restart except filename.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `timelapse_interval` | int | No | Timelapse interval |
| `timelapse_mode` | list | No | Timelapse mode |
| `timelapse_fps` | int | No | Timelapse framerate |
| `timelapse_container` | list | No | Container format |
| `timelapse_filename` | string | **Yes** | Filename format |

### Category 12: Pipe Parameters
Pipe parameters require restart.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `video_pipe` | string | No | Video pipe path |
| `video_pipe_motion` | string | No | Motion video pipe |

### Category 13: Webcontrol Parameters
Web server parameters require restart.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `webcontrol_port` | int | No | Web server port |
| `webcontrol_port2` | int | No | Secondary port |
| `webcontrol_base_path` | string | No | URL base path |
| `webcontrol_ipv6` | bool | No | IPv6 support |
| `webcontrol_localhost` | bool | No | Localhost only |
| `webcontrol_parms` | list | No | Parameter access level |
| `webcontrol_interface` | list | No | Interface type |
| `webcontrol_auth_method` | list | No | Authentication method |
| `webcontrol_authentication` | string | No | Credentials |
| `webcontrol_tls` | bool | No | TLS/HTTPS |
| `webcontrol_cert` | string | No | TLS certificate path |
| `webcontrol_key` | string | No | TLS key path |
| `webcontrol_headers` | params | No | Custom headers |
| `webcontrol_html` | string | No | Legacy HTML path |
| `webcontrol_actions` | params | No | Action configurations |
| `webcontrol_lock_minutes` | int | No | Lock duration |
| `webcontrol_lock_attempts` | int | No | Lock threshold |
| `webcontrol_lock_script` | string | No | Lock event script |
| `webcontrol_trusted_proxies` | string | No | Proxy IPs |
| `webcontrol_html_path` | string | No | HTML UI path |
| `webcontrol_spa_mode` | bool | No | SPA routing mode |

### Category 14: Stream Parameters

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `stream_preview_scale` | int | No | Preview scale % |
| `stream_preview_newline` | bool | No | Newline per camera |
| `stream_preview_params` | params | No | Preview parameters |
| `stream_preview_method` | list | No | Preview method |
| `stream_preview_ptz` | bool | No | PTZ in preview |
| `stream_quality` | int | **Yes** | JPEG quality |
| `stream_grey` | bool | **Yes** | Greyscale mode |
| `stream_motion` | bool | **Yes** | Show motion boxes |
| `stream_maxrate` | int | **Yes** | Max framerate |
| `stream_scan_time` | int | No | Scan time |
| `stream_scan_scale` | int | No | Scan scale |

### Category 15: Database Parameters
Database parameters require restart.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `database_type` | list | No | sqlite3/mysql/pgsql |
| `database_dbname` | string | No | Database name/path |
| `database_host` | string | No | Database host |
| `database_port` | int | No | Database port |
| `database_user` | string | No | Database user |
| `database_password` | string | No | Database password |
| `database_busy_timeout` | int | No | SQLite busy timeout |

### Category 16: SQL Parameters
All SQL strings are hot-reloadable.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `sql_event_start` | string | **Yes** | Event start SQL |
| `sql_event_end` | string | **Yes** | Event end SQL |
| `sql_movie_start` | string | **Yes** | Movie start SQL |
| `sql_movie_end` | string | **Yes** | Movie end SQL |
| `sql_pic_save` | string | **Yes** | Picture save SQL |

### Category 17: PTZ/Tracking Parameters
All PTZ parameters are hot-reloadable for runtime control.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `ptz_auto_track` | bool | **Yes** | Auto tracking |
| `ptz_wait` | int | **Yes** | Wait between moves |
| `ptz_move_track` | string | **Yes** | Track move command |
| `ptz_pan_left` | string | **Yes** | Pan left command |
| `ptz_pan_right` | string | **Yes** | Pan right command |
| `ptz_tilt_up` | string | **Yes** | Tilt up command |
| `ptz_tilt_down` | string | **Yes** | Tilt down command |
| `ptz_zoom_in` | string | **Yes** | Zoom in command |
| `ptz_zoom_out` | string | **Yes** | Zoom out command |

### Category 18: Sound Parameters
Sound parameters require restart due to device configuration.

| Parameter | Type | Hot-Reload | Notes |
|-----------|------|------------|-------|
| `snd_device` | string | No | Audio device |
| `snd_params` | params | No | Audio parameters |
| `snd_alerts` | array | No | Alert configurations |
| `snd_window` | list | No | Analysis window |
| `snd_show` | bool | No | Show sound display |

## Hot-Reload Summary

### Hot-Reloadable Categories
1. **Category 04 (Overlay)** - 100% hot-reloadable
2. **Category 05 (Method)** - Mostly hot-reloadable (except secondary detection)
3. **Category 06 (Masks)** - Mostly hot-reloadable (except mask files)
4. **Category 07 (Detect)** - 100% hot-reloadable
5. **Category 08 (Scripts)** - 100% hot-reloadable
6. **Category 09 (Picture)** - 100% hot-reloadable
7. **Category 16 (SQL)** - 100% hot-reloadable
8. **Category 17 (PTZ)** - 100% hot-reloadable

### Non-Hot-Reloadable Categories
1. **Category 00 (System)** - Requires daemon restart
2. **Category 03 (Image)** - Requires buffer reallocation
3. **Category 11 (Timelapse)** - Encoder configuration
4. **Category 12 (Pipes)** - Pipe initialization
5. **Category 13 (Webcontrol)** - Server configuration
6. **Category 15 (Database)** - Connection configuration
7. **Category 18 (Sound)** - Device configuration

### Special Hot-Reloadable Parameters
The following libcamera parameters are immediately applied to the camera hardware:
- `libcam_brightness`, `libcam_contrast`, `libcam_iso`
- `libcam_awb_enable`, `libcam_awb_mode`, `libcam_awb_locked`
- `libcam_colour_temp`, `libcam_colour_gain_r`, `libcam_colour_gain_b`
- `libcam_af_mode`, `libcam_lens_position`, `libcam_af_range`, `libcam_af_speed`, `libcam_af_trigger`

## UI Implementation Notes

### Current Gaps in Settings Panel
The current Settings panel only exposes a small subset of available parameters. Categories that could benefit from UI exposure:

1. **Libcamera Controls** (Category 02) - All hot-reloadable for real-time adjustment
2. **Overlay Settings** (Category 04) - All hot-reloadable
3. **Detection Tuning** (Categories 05, 06, 07) - Mostly hot-reloadable
4. **PTZ Controls** (Category 17) - All hot-reloadable

### Parameter Access Levels
Parameters have access levels that affect visibility:
- `PARM_LEVEL_LIMITED` - Normal user access
- `PARM_LEVEL_ADVANCED` - Advanced settings
- `PARM_LEVEL_RESTRICTED` - Security-sensitive (scripts, auth)
- `PARM_LEVEL_NEVER` - Hidden from web UI

The `webcontrol_parms` configuration determines maximum visible level.

## Source References

- Parameter definitions: `src/conf.cpp:44-263`
- Hot-reload validation: `src/webu_json.cpp:579-595`
- Hot-reload application: `src/webu_json.cpp:600-700`
- Category enum: `src/conf.hpp:27-46`
- Frontend Settings: `frontend/src/pages/Settings.tsx`
- API client: `frontend/src/api/client.ts`
- API queries: `frontend/src/api/queries.ts`
