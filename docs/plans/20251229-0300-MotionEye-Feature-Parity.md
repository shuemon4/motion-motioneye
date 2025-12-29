# MotionEye Feature Parity Implementation Plan

**Date**: 2025-12-29
**Status**: Planning
**Priority**: High
**Dependency**: Requires `20251229-0115-JSON-API-Architecture.md` completion first

## Executive Summary

This plan maps MotionEye's ~150 UI configuration options to Motion's 213 backend parameters, identifies gaps, and provides implementation guidance for achieving feature parity in the React UI.

## Feature Mapping Analysis

### Legend
- ✅ **Direct Match** - Motion parameter exists, just needs UI exposure
- 🔄 **Translation Needed** - Motion has equivalent but different naming/format
- ➕ **Frontend Only** - UI preference, no backend needed
- 🔧 **Backend Required** - New Motion C++ code needed
- ❌ **Not Applicable** - MotionEye-specific, won't port

---

## 1. DEVICE SETTINGS

### Camera Identification & Basic Setup

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| enabled | `pause` | ✅ | pause="on" = disabled |
| name | `device_name` | ✅ | Direct match |
| proto | (derived) | 🔄 | Derived from which source param is set |
| device_url | `netcam_url` / `v4l2_device` / `libcam_device` | ✅ | Multiple params based on proto |
| rotation | `rotate` | ✅ | Direct match (0, 90, 180, 270) |
| framerate | `framerate` | ✅ | Direct match |
| resolution | `width` + `height` | 🔄 | MotionEye uses "WxH" string, Motion uses separate params |

### libcamera Controls (Raspberry Pi)

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| brightness | `libcam_brightness` | ✅ | Direct match (-1.0 to 1.0) |
| contrast | `libcam_contrast` | ✅ | Direct match (0.0 to 32.0) |
| iso | `libcam_iso` | ✅ | Direct match (100-6400) |
| awb_enable | `libcam_awb_enable` | ✅ | Direct match |
| awb_mode | `libcam_awb_mode` | ✅ | Direct match (0-7) |
| awb_locked | `libcam_awb_locked` | ✅ | Direct match |
| colour_temp | `libcam_colour_temp` | ✅ | Direct match (0-10000K) |
| colour_gain_r | `libcam_colour_gain_r` | ✅ | Direct match (0.0-8.0) |
| colour_gain_b | `libcam_colour_gain_b` | ✅ | Direct match (0.0-8.0) |
| autofocus_mode | `libcam_af_mode` | ✅ | Direct match (0=Manual, 1=Auto, 2=Continuous) |
| autofocus_range | `libcam_af_range` | ✅ | Direct match (0=Normal, 1=Macro, 2=Full) |
| autofocus_speed | `libcam_af_speed` | ✅ | Direct match (0=Normal, 1=Fast) |
| lens_position | `libcam_lens_position` | ✅ | Direct match (0.0-15.0 dioptres) |
| libcam_buffer_count | `libcam_buffer_count` | ✅ | Direct match (2-8) |

### Image Adjustments (V4L2)

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| video_controls | `v4l2_params` | 🔄 | V4L2 controls passed as params string |

### Privacy Mask

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| privacy_mask | `mask_privacy` | 🔄 | Motion uses PGM file path |
| privacy_mask_lines | (none) | 🔧 | Need UI to generate PGM from coordinates |

---

## 2. TEXT OVERLAY

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| text_overlay | (implicit) | 🔄 | Enabled if text_left or text_right set |
| left_text | `text_left` | ✅ | Direct match |
| custom_left_text | `text_left` | ✅ | Same param, custom value |
| right_text | `text_right` | ✅ | Direct match |
| custom_right_text | `text_right` | ✅ | Same param, custom value |
| text_scale | `text_scale` | ✅ | Direct match (1-10) |

**Translation Note**: MotionEye uses presets (camera-name, timestamp, disabled, custom). Frontend should translate:
- `camera-name` → `%$` (device name)
- `timestamp` → `%Y-%m-%d\n%T`
- `disabled` → empty string
- `custom` → user-provided value

---

## 3. VIDEO STREAMING

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| video_streaming | (implicit) | 🔄 | Always enabled if webcontrol_port > 0 |
| streaming_port | `webcontrol_port` | ✅ | Direct match |
| streaming_framerate | `stream_maxrate` | ✅ | Direct match |
| streaming_quality | `stream_quality` | ✅ | Direct match (1-100) |
| streaming_resolution | `stream_preview_scale` | 🔄 | Motion uses percentage (1-100%) |
| streaming_server_resize | (implicit) | ✅ | Always server-side in Motion |
| streaming_auth_mode | `webcontrol_auth_method` | ✅ | Direct match (none, basic, digest) |
| streaming_motion | `stream_motion` | ✅ | Direct match |

---

## 4. STILL IMAGES (Snapshots)

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| still_images | `picture_output` | ✅ | off/on/first/best/center |
| capture_mode | `picture_output` | 🔄 | Maps to picture_output values |
| image_file_name | `picture_filename` | ✅ | Direct match |
| image_quality | `picture_quality` | ✅ | Direct match (1-100) |
| snapshot_interval | `snapshot_interval` | ✅ | Direct match (seconds) |
| manual_snapshots | (API endpoint) | ✅ | Already available via /action/snapshot |
| preserve_pictures | (none) | 🔧 | Needs cleandir_params implementation |

**Capture Mode Translation**:
- `motion-triggered` → `picture_output=on`
- `motion-triggered-one` → `picture_output=first`
- `interval-snapshots` → `snapshot_interval > 0`
- `all-frames` → `picture_output=on` + high threshold
- `manual` → `picture_output=off`

---

## 5. MOVIES (Video Recording)

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| movies | `movie_output` | ✅ | Direct match (bool) |
| recording_mode | `movie_output_motion` | 🔄 | motion-triggered vs continuous |
| movie_file_name | `movie_filename` | ✅ | Direct match |
| movie_quality | `movie_quality` | ✅ | Direct match (1-100) |
| movie_format | `movie_container` | 🔄 | Motion uses mkv/mp4/3gp |
| movie_passthrough | `movie_passthrough` | ✅ | Direct match |
| max_movie_length | `movie_max_time` | ✅ | Direct match (seconds) |
| preserve_movies | (none) | 🔧 | Needs cleandir_params implementation |

**Format Translation**:
- `mp4` → `movie_container=mp4`
- `mkv` → `movie_container=mkv`
- `avi` → Not directly supported, use mkv
- `hevc` → `movie_encoder_preset` + specific codec params

---

## 6. MOTION DETECTION

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| motion_detection | `pause` | 🔄 | pause=off enables detection |
| frame_change_threshold | `threshold` | 🔄 | MotionEye uses %, Motion uses pixel count |
| max_frame_change_threshold | `threshold_maximum` | ✅ | Direct match |
| auto_threshold_tuning | `threshold_tune` | ✅ | Direct match |
| auto_noise_detect | `noise_tune` | ✅ | Direct match |
| noise_level | `noise_level` | ✅ | Direct match (1-255) |
| light_switch_detect | `lightswitch_percent` | ✅ | Direct match (0-100%) |
| event_gap | `event_gap` | ✅ | Direct match (seconds) |
| pre_capture | `pre_capture` | ✅ | Direct match (frames) |
| post_capture | `post_capture` | ✅ | Direct match (frames) |
| minimum_motion_frames | `minimum_motion_frames` | ✅ | Direct match |
| show_frame_changes | `locate_motion_mode` | ✅ | on/off/preview |
| despeckle_filter | `despeckle_filter` | ✅ | Direct match (string) |

### Motion Masks

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| motion_mask | `mask_file` | 🔄 | Motion uses PGM file path |
| motion_mask_type | (none) | 🔄 | Motion only has file-based masks |
| smart_mask_sluggishness | `smart_mask_speed` | ✅ | Direct match (0-10) |
| motion_mask_lines | (none) | 🔧 | Need UI to generate PGM from coordinates |

**Threshold Translation**:
MotionEye uses percentage (1.5% of frame). Motion uses pixel count.
```
threshold = (frame_change_threshold / 100) * width * height
```
For 640x480 at 1.5%: `threshold = 0.015 * 640 * 480 = 4608`

---

## 7. MOTION NOTIFICATIONS

### Email Notifications

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| email_notifications_enabled | `on_event_start` | 🔄 | Set to email script |
| email_notifications_* | (none) | 🔧 | Need notification service |

**Implementation**: Motion doesn't have built-in email. Options:
1. **Script-based**: Use `on_event_start` to call external email script
2. **Built-in service**: Add notification module to Motion (significant work)
3. **Frontend service**: React app handles notifications (requires always-on frontend)

**Recommendation**: Script-based approach. Create `motion-notify.sh` script that handles email, Telegram, webhooks.

### Telegram Notifications

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| telegram_notifications_enabled | `on_event_start` | 🔄 | Set to telegram script |
| telegram_notifications_api | (none) | 🔧 | Store in config, pass to script |
| telegram_notifications_chat_id | (none) | 🔧 | Store in config, pass to script |

### Webhook Notifications

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| web_hook_notifications_enabled | `on_event_start` | ✅ | Use curl in script |
| web_hook_notifications_url | `on_event_start` | 🔄 | Embed URL in script |
| web_hook_notifications_http_method | `on_event_start` | 🔄 | Embed method in script |
| web_hook_end_notifications_* | `on_event_end` | ✅ | Same pattern |

### Command Notifications

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| command_notifications_enabled | `on_event_start` | ✅ | Direct match |
| command_notifications_exec | `on_event_start` | ✅ | Direct match |
| command_end_* | `on_event_end` | ✅ | Direct match |

---

## 8. FILE STORAGE

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| storage_device | (none) | ➕ | Frontend preference for path selection |
| root_directory | `target_dir` | ✅ | Direct match |
| network_server | (none) | 🔧 | Need mount helper or built-in SMB |
| network_share_name | (none) | 🔧 | Same |
| network_username | (none) | 🔧 | Same |
| network_password | (none) | 🔧 | Same |

**Network Storage Implementation**:
Option 1: Mount network shares at OS level, use path in `target_dir`
Option 2: Add SMB/NFS mounting to Motion (complex, not recommended)
**Recommendation**: Document that network storage should be mounted via fstab/systemd

---

## 9. CLOUD UPLOAD

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| upload_enabled | `on_picture_save` / `on_movie_end` | 🔄 | Script-based |
| upload_service | (none) | 🔧 | Need upload script/module |
| upload_* | (none) | 🔧 | All upload params need implementation |

**Implementation Options**:
1. **Script-based**: Use `on_picture_save` / `on_movie_end` to call rclone/s3cmd
2. **Built-in module**: Add upload service to Motion (significant work)

**Recommendation**: Script-based with rclone. Create `motion-upload.sh` that reads config and uses rclone for GDrive/Dropbox/S3/etc.

---

## 10. WORKING SCHEDULE

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| working_schedule | `schedule_params` | ✅ | Motion has schedule support |
| working_schedule_type | `schedule_params` | 🔄 | Needs parsing |
| monday_from/to ... sunday_from/to | `schedule_params` | 🔄 | Format differs |

**Schedule Format Translation**:
MotionEye: Pipe-separated `from-to|from-to|...|from-to` for each day
Motion: `schedule_params` format TBD - need to verify exact format

---

## 11. UI PREFERENCES (Frontend Only)

| MotionEye Option | Status | Notes |
|------------------|--------|-------|
| layout_columns | ➕ | Store in localStorage or user prefs API |
| layout_rows | ➕ | Same |
| fit_frames_vertically | ➕ | Same |
| framerate_factor | ➕ | Same |
| resolution_factor | ➕ | Same |
| lang | ➕ | i18n, store in localStorage |

---

## 12. AUTHENTICATION

| MotionEye Option | Motion Parameter | Status | Notes |
|------------------|------------------|--------|-------|
| admin_username | `webcontrol_authentication` | 🔄 | Motion uses user:pass format |
| admin_password | `webcontrol_authentication` | 🔄 | Same |
| normal_username | (none) | 🔧 | Motion has single user |
| normal_password | (none) | 🔧 | Same |

**Note**: Motion currently supports single user authentication. Multi-user with roles would require backend changes.

---

## Implementation Phases

### Phase 1: Core Settings UI (Week 1-2)
**Goal**: Basic camera configuration via React UI

**Tasks**:
1. [ ] Create Settings page component structure
2. [ ] Implement Device Settings panel
   - Camera name, enable/disable
   - Resolution (width/height) with presets
   - Framerate
   - Rotation
3. [ ] Implement libcamera controls panel (Pi-specific)
   - Brightness, contrast, ISO
   - AWB controls
   - Autofocus controls
4. [ ] Implement Text Overlay panel
   - Left/right text with presets
   - Text scale
5. [ ] Implement Video Streaming panel
   - Quality, framerate
   - Authentication mode

**Backend Changes**: None - all params exist

**Frontend Files**:
- `frontend/src/pages/Settings.tsx` (expand existing)
- `frontend/src/components/settings/DeviceSettings.tsx`
- `frontend/src/components/settings/LibcameraSettings.tsx`
- `frontend/src/components/settings/OverlaySettings.tsx`
- `frontend/src/components/settings/StreamSettings.tsx`

### Phase 2: Motion Detection UI (Week 2-3)
**Goal**: Full motion detection configuration

**Tasks**:
1. [ ] Implement Motion Detection panel
   - Threshold (with percentage conversion helper)
   - Auto-tuning toggles
   - Noise level
   - Light switch detection
2. [ ] Implement Event Timing panel
   - Event gap
   - Pre/post capture
   - Minimum motion frames
3. [ ] Implement Despeckle filter selector
4. [ ] Implement Smart Mask speed control

**Backend Changes**: None - all params exist

### Phase 3: Output Settings UI (Week 3-4)
**Goal**: Picture and movie output configuration

**Tasks**:
1. [ ] Implement Still Images panel
   - Enable/disable
   - Capture mode selector
   - Filename pattern
   - Quality
   - Snapshot interval
2. [ ] Implement Movies panel
   - Enable/disable
   - Recording mode
   - Filename pattern
   - Quality, container format
   - Max duration
   - Passthrough option

**Backend Changes**: None - all params exist

### Phase 4: Mask Editor (Week 4-5)
**Goal**: Visual mask editing in browser

**Tasks**:
1. [ ] Create canvas-based mask editor component
2. [ ] Implement polygon drawing for motion mask
3. [ ] Implement polygon drawing for privacy mask
4. [ ] Create PGM generation from polygon coordinates
5. [ ] Add mask upload/download functionality
6. [ ] Implement API endpoint to save generated PGM files

**Backend Changes**:
- New endpoint: `POST /api/mask` to save PGM file
- New endpoint: `GET /api/mask` to retrieve current mask

**Frontend Files**:
- `frontend/src/components/settings/MaskEditor.tsx`
- `frontend/src/utils/pgm.ts` (PGM generation)

### Phase 5: Notification System (Week 5-6)
**Goal**: Email, Telegram, Webhook notifications

**Tasks**:
1. [ ] Create notification configuration UI
2. [ ] Implement notification script (`motion-notify.sh`)
3. [ ] Add notification config parameters to Motion (or use params file)
4. [ ] Create Email settings panel
5. [ ] Create Telegram settings panel
6. [ ] Create Webhook settings panel
7. [ ] Create Command execution panel

**Backend Changes**:
- Option A: Add notification params to Motion config
- Option B: Use separate `notify.conf` file read by script

**New Files**:
- `data/scripts/motion-notify.sh`
- `frontend/src/components/settings/NotificationSettings.tsx`

### Phase 6: Storage & Upload (Week 6-7)
**Goal**: Local storage management and cloud upload

**Tasks**:
1. [ ] Implement Storage Settings panel
   - Target directory selection
   - Storage usage display
2. [ ] Implement file retention (cleandir_params)
3. [ ] Create upload script (`motion-upload.sh`)
4. [ ] Implement Cloud Upload panel
   - Service selection
   - Credentials management
5. [ ] Document network storage setup

**Backend Changes**:
- Implement `cleandir_params` parsing and execution
- Or create separate cleanup service

**New Files**:
- `data/scripts/motion-upload.sh`
- `frontend/src/components/settings/StorageSettings.tsx`
- `frontend/src/components/settings/UploadSettings.tsx`

### Phase 7: Schedule & Preferences (Week 7-8)
**Goal**: Time-based schedules and UI preferences

**Tasks**:
1. [ ] Implement Working Schedule panel
   - Enable/disable
   - Schedule type (during/outside)
   - Per-day time ranges
2. [ ] Implement UI Preferences
   - Layout columns/rows
   - Frame fitting
   - Playback factors
3. [ ] Add localStorage persistence for preferences
4. [ ] Implement i18n framework for language support

**Backend Changes**:
- Verify `schedule_params` format and implement if needed

### Phase 8: Polish & Documentation (Week 8)
**Goal**: Final integration and documentation

**Tasks**:
1. [ ] End-to-end testing of all settings
2. [ ] Responsive design for mobile
3. [ ] Error handling and validation
4. [ ] User documentation
5. [ ] Migration guide from MotionEye

---

## Gap Analysis Summary

### Features with Direct Motion Support (Ready for UI)
- ✅ Device settings (name, resolution, framerate, rotation)
- ✅ libcamera controls (all 14 parameters)
- ✅ Text overlay (all parameters)
- ✅ Streaming (quality, framerate, auth)
- ✅ Still images (all core parameters)
- ✅ Movies (all core parameters)
- ✅ Motion detection (all core parameters)
- ✅ Script triggers (on_event_start, on_event_end, etc.)

### Features Requiring Frontend Translation
- 🔄 Resolution (WxH string ↔ separate width/height)
- 🔄 Threshold (percentage ↔ pixel count)
- 🔄 Text presets (camera-name, timestamp ↔ format strings)
- 🔄 Streaming resolution (presets ↔ percentage)
- 🔄 Capture mode (names ↔ picture_output values)

### Features Requiring Backend Work
- 🔧 Mask editor → PGM generation and storage
- 🔧 File retention → cleandir_params implementation
- 🔧 Notification params → Extended config or separate file
- 🔧 Cloud upload → Script + config storage
- 🔧 Multi-user auth → Backend enhancement

### Features Not Ported (MotionEye-Specific)
- ❌ MotionEye-to-MotionEye camera sharing (motioneye:// protocol)
- ❌ Python-based upload services (replaced with shell scripts)
- ❌ Built-in SMB mounting (document manual setup)

---

## API Endpoints Required

### Existing (via JSON API plan)
- `GET /api/config` - Get all config
- `PATCH /api/config` - Batch update config
- `GET /api/cameras` - List cameras
- `GET /api/status` - System status

### New Endpoints Needed
| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/api/mask/motion` | GET/POST | Get/set motion mask PGM |
| `/api/mask/privacy` | GET/POST | Get/set privacy mask PGM |
| `/api/storage/usage` | GET | Disk usage for target_dir |
| `/api/storage/cleanup` | POST | Trigger manual cleanup |
| `/api/notifications/test` | POST | Send test notification |
| `/api/upload/test` | POST | Test upload configuration |
| `/api/preferences` | GET/PUT | User UI preferences |

---

## File Structure (Final)

```
frontend/src/
├── components/
│   └── settings/
│       ├── DeviceSettings.tsx
│       ├── LibcameraSettings.tsx
│       ├── OverlaySettings.tsx
│       ├── StreamSettings.tsx
│       ├── MotionSettings.tsx
│       ├── PictureSettings.tsx
│       ├── MovieSettings.tsx
│       ├── MaskEditor.tsx
│       ├── NotificationSettings.tsx
│       ├── StorageSettings.tsx
│       ├── UploadSettings.tsx
│       ├── ScheduleSettings.tsx
│       └── PreferencesSettings.tsx
├── utils/
│   ├── pgm.ts          # PGM file generation
│   ├── threshold.ts    # Threshold conversion helpers
│   └── schedule.ts     # Schedule format helpers
└── pages/
    └── Settings.tsx    # Main settings page with tabs

data/scripts/
├── motion-notify.sh    # Notification handler
└── motion-upload.sh    # Upload handler

src/
├── webu_json.cpp       # Add new endpoints
└── (mask handling)     # PGM save/load endpoints
```

---

## Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| Mask editor complexity | Medium | Start with simple rectangle, add polygon later |
| Notification reliability | Medium | Script-based approach proven, well-documented |
| Cloud upload credentials | High | Secure storage, environment variables |
| Schedule format mismatch | Low | Verify Motion format early |
| Mobile responsive design | Medium | Use Tailwind responsive utilities |

---

## Success Criteria

1. **Feature Parity**: 90%+ of MotionEye options available in React UI
2. **Performance**: Settings page loads in <500ms
3. **Usability**: All settings saveable without page reload
4. **Reliability**: No data loss on concurrent edits
5. **Documentation**: All features documented with examples

---

## Dependencies

1. **JSON API Architecture** (20251229-0115) must be completed first
2. **React UI base** already functional
3. **Motion backend** running and accessible
4. **Pi hardware** for testing libcamera controls

---

## Appendix A: Parameter Quick Reference

### Direct Mappings (Copy-Paste Ready)

```typescript
// frontend/src/utils/parameterMappings.ts

export const DIRECT_MAPPINGS = {
  // Device
  'device_name': 'device_name',
  'framerate': 'framerate',
  'rotate': 'rotate',

  // libcamera
  'brightness': 'libcam_brightness',
  'contrast': 'libcam_contrast',
  'iso': 'libcam_iso',
  'awb_enable': 'libcam_awb_enable',
  'awb_mode': 'libcam_awb_mode',
  'awb_locked': 'libcam_awb_locked',
  'colour_temp': 'libcam_colour_temp',
  'colour_gain_r': 'libcam_colour_gain_r',
  'colour_gain_b': 'libcam_colour_gain_b',
  'autofocus_mode': 'libcam_af_mode',
  'autofocus_range': 'libcam_af_range',
  'autofocus_speed': 'libcam_af_speed',
  'lens_position': 'libcam_lens_position',

  // Overlay
  'text_left': 'text_left',
  'text_right': 'text_right',
  'text_scale': 'text_scale',

  // Streaming
  'streaming_quality': 'stream_quality',
  'streaming_framerate': 'stream_maxrate',
  'streaming_auth_mode': 'webcontrol_auth_method',
  'streaming_motion': 'stream_motion',

  // Motion Detection
  'threshold': 'threshold',
  'threshold_maximum': 'threshold_maximum',
  'auto_threshold_tuning': 'threshold_tune',
  'auto_noise_detect': 'noise_tune',
  'noise_level': 'noise_level',
  'light_switch_detect': 'lightswitch_percent',
  'despeckle_filter': 'despeckle_filter',
  'smart_mask_speed': 'smart_mask_speed',

  // Events
  'event_gap': 'event_gap',
  'pre_capture': 'pre_capture',
  'post_capture': 'post_capture',
  'minimum_motion_frames': 'minimum_motion_frames',

  // Pictures
  'picture_quality': 'picture_quality',
  'picture_filename': 'picture_filename',
  'snapshot_interval': 'snapshot_interval',

  // Movies
  'movie_output': 'movie_output',
  'movie_quality': 'movie_quality',
  'movie_filename': 'movie_filename',
  'movie_max_time': 'movie_max_time',
  'movie_passthrough': 'movie_passthrough',

  // Scripts
  'on_event_start': 'on_event_start',
  'on_event_end': 'on_event_end',
  'on_picture_save': 'on_picture_save',
  'on_movie_start': 'on_movie_start',
  'on_movie_end': 'on_movie_end',
};
```

### Translation Functions

```typescript
// frontend/src/utils/translations.ts

// Resolution: "640x480" <-> {width: 640, height: 480}
export function parseResolution(res: string): {width: number, height: number} {
  const [w, h] = res.split('x').map(Number);
  return { width: w, height: h };
}

export function formatResolution(width: number, height: number): string {
  return `${width}x${height}`;
}

// Threshold: percentage <-> pixel count
export function percentToPixels(percent: number, width: number, height: number): number {
  return Math.round((percent / 100) * width * height);
}

export function pixelsToPercent(pixels: number, width: number, height: number): number {
  return Number(((pixels / (width * height)) * 100).toFixed(2));
}

// Text presets
export const TEXT_PRESETS = {
  'disabled': '',
  'camera-name': '%$',
  'timestamp': '%Y-%m-%d\\n%T',
  'custom': null, // Use actual value
};

// Capture mode
export const CAPTURE_MODE_MAP = {
  'motion-triggered': { picture_output: 'on' },
  'motion-triggered-one': { picture_output: 'first' },
  'interval-snapshots': { picture_output: 'off', snapshot_interval: 60 },
  'best': { picture_output: 'best' },
  'manual': { picture_output: 'off', snapshot_interval: 0 },
};
```

---

## Appendix B: MotionEye Features NOT Being Ported

| Feature | Reason |
|---------|--------|
| motioneye:// camera sharing | Proprietary protocol, not needed |
| Built-in SMB client | OS-level mounting preferred |
| Python upload handlers | Shell scripts more portable |
| Multiple user roles | Single admin sufficient for home use |
| Remote MotionEye management | Out of scope |

---

## Next Steps

1. **Review this plan** with stakeholder
2. **Complete JSON API plan** (prerequisite)
3. **Begin Phase 1** implementation
4. **Create test checklist** for each phase
