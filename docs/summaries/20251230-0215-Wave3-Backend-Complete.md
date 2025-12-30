# Wave 3 Backend Implementation - Complete

**Date:** 2025-12-30
**Status:** Complete
**Tested On:** Raspberry Pi 5 (192.168.1.176)

## Overview

Wave 3 focused on implementing the Mask Editor API backend and notification script foundation. All components have been implemented, built, deployed, and tested on the Raspberry Pi.

## Completed Work

### 1. Mask Editor API

Full CRUD API for motion and privacy mask management via polygon-based editing.

#### Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/{camId}/api/mask/{type}` | Get mask status (exists, path, dimensions) |
| POST | `/{camId}/api/mask/{type}` | Create mask from polygon array |
| DELETE | `/{camId}/api/mask/{type}` | Remove mask file and clear config |

**Type:** `motion` or `privacy`

#### POST Request Format

```json
{
  "polygons": [[[x1,y1], [x2,y2], [x3,y3], ...]],
  "width": 640,
  "height": 480,
  "invert": false
}
```

#### Response Examples

**GET (mask exists):**
```json
{
  "type": "motion",
  "exists": true,
  "path": "/var/lib/motion/cam1_motion.pgm",
  "width": 640,
  "height": 480
}
```

**POST (success):**
```json
{
  "success": true,
  "path": "/var/lib/motion/cam1_motion.pgm",
  "width": 640,
  "height": 480,
  "message": "Mask saved. Reload camera to apply."
}
```

**DELETE (success):**
```json
{
  "success": true,
  "deleted": true,
  "message": "Mask removed. Reload camera to apply."
}
```

### 2. Notification Script Foundation

Created shell-based notification dispatcher for Motion event hooks.

#### Files Created

| File | Purpose |
|------|---------|
| `data/scripts/motion-notify.sh` | Event dispatcher script |
| `data/scripts/notify.conf.example` | Configuration template |

#### Supported Notification Channels

- **Email:** via msmtp or sendmail
- **Telegram:** Bot API integration
- **Webhook:** Generic HTTP POST

#### Supported Events

- `start` - Motion event started
- `end` - Motion event ended
- `picture` - Picture saved
- `movie_start` - Movie recording started
- `movie_end` - Movie recording ended

## Files Modified

### Backend (C++)

| File | Changes |
|------|---------|
| `src/webu_json.hpp` | Added mask API method declarations |
| `src/webu_json.cpp` | Implemented `api_mask_get()`, `api_mask_post()`, `api_mask_delete()`, helper functions |
| `src/webu_ans.cpp` | Added routing for GET/POST/DELETE mask endpoints |

### Helper Functions Added

- `fill_polygon()` - Scanline polygon fill algorithm for CPU efficiency
- `build_mask_path()` - Generates mask file path in target_dir

## Technical Details

### Mask File Format

- **Format:** PGM (Portable Gray Map) - P5 binary
- **Location:** `{target_dir}/cam{id}_{type}.pgm`
- **Header:** Includes comment identifying mask type
- **Size:** ~300KB for 640x480 (uncompressed grayscale)

### Security

- CSRF validation required for POST and DELETE operations
- Uses `X-CSRF-Token` header (same as PATCH endpoints)
- Path traversal protection on DELETE operations
- Token obtained from `/0/api/config` response (`csrf_token` field)

### Design Decisions

1. **Scanline algorithm** for polygon fill - O(n) per scanline, CPU efficient
2. **PGM format** - Native to Motion, no conversion needed
3. **Same CSRF pattern** as existing PATCH endpoints for consistency
4. **Config update on save** - Sets `mask_file` or `mask_privacy` parameter

## Test Results

All endpoints tested successfully on Raspberry Pi 5:

```
GET /1/api/mask/motion  → {"type":"motion","exists":false,"path":""}
POST /1/api/mask/motion → {"success":true,"path":"/var/lib/motion/cam1_motion.pgm",...}
GET /1/api/mask/motion  → {"type":"motion","exists":true,"path":"/var/lib/motion/cam1_motion.pgm",...}
DELETE /1/api/mask/motion → {"success":true,"deleted":true,...}
GET /1/api/mask/motion  → {"type":"motion","exists":false,"path":""}
```

Existing functionality verified:
- Camera list API: Working
- MJPEG stream: Working
- Config API: Working

## Next Steps (Wave 4)

1. **Frontend Mask Editor UI** - React component for drawing polygons on video feed
2. **Integrate notification hooks** - Connect Motion events to notification script
3. **Settings UI for notifications** - Configure notification channels from web UI

## Related Documents

- Plan: `docs/plans/20251229-1800-Wave3-Backend-Implementation.md`
- Handoff: `docs/scratchpads/20251229-1830-Wave3-Handoff.md`
