# React UI Integration - Architecture Plan

**Created**: 2025-12-27
**Repository**: motion-motioneye
**Goal**: Replace Motion's embedded HTML UI with React SPA, zero Python overhead

---

## Executive Summary

Motion already has a complete C++ web infrastructure. Instead of adding a Python/FastAPI layer, we will:

1. **Extend Motion's existing web server** to serve React static assets
2. **Enhance the JSON API** already in `webu_json.cpp`
3. **Eliminate all Python code** - React talks directly to Motion

This results in:
- **Single process** - Only Motion runs
- **~20MB RAM** total (vs ~150MB+ with Python)
- **Native C++ performance**
- **Zero proxy overhead** - Streams served directly by Motion

---

## Current Motion Web Architecture

```
Motion (C++17)
├── src/webu.cpp          - libmicrohttpd server (TLS, digest auth, CSRF)
├── src/webu_ans.cpp      - Request routing and response handling
├── src/webu_html.cpp     - Embedded HTML/CSS/JS (1,702 lines)
├── src/webu_json.cpp     - JSON API (config, status, cameras, movies)
├── src/webu_stream.cpp   - MJPEG streaming
├── src/webu_post.cpp     - POST handlers (config updates, actions)
├── src/webu_file.cpp     - Static file serving
├── src/webu_text.cpp     - Text responses
└── src/webu_mpegts.cpp   - MPEG-TS streaming
```

### Existing JSON API Endpoints

From `webu_json.cpp`:

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/config` | GET | All config parameters |
| `/config/{cam_id}` | GET | Camera-specific config |
| `/config/{cam_id}/{param}` | POST | Set config parameter (hot-reload) |
| `/cameras` | GET | List all cameras |
| `/categories` | GET | Config categories list |
| `/status` | GET | System/camera status |
| `/movies/{cam_id}` | GET | List recorded videos |
| `/log` | GET | Log history |

### Existing Stream Endpoints

From `webu_stream.cpp`:

| Endpoint | Purpose |
|----------|---------|
| `/{cam_id}/mjpg/stream` | Continuous MJPEG stream |
| `/{cam_id}/mjpg/static` | Single JPEG frame |
| `/{cam_id}/mjpg/motion` | Motion-detected areas overlay |
| `/{cam_id}/mjpg/source` | Source image (pre-processing) |
| `/{cam_id}/mjpg/secondary` | Secondary stream |

---

## Target Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                     Motion (Single Process)                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │              React Frontend (Static Files)                │  │
│  │                                                           │  │
│  │  Built assets served from /webui/ directory               │  │
│  │  index.html, *.js, *.css, images                          │  │
│  │                                                           │  │
│  └──────────────────────────────────────────────────────────┘  │
│                           │                                      │
│                    Direct API calls                              │
│                           │                                      │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │                  Motion Core (C++)                        │  │
│  │                                                           │  │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐ │  │
│  │  │ webu_   │ │ webu_   │ │ webu_   │ │ webu_           │ │  │
│  │  │ json    │ │ stream  │ │ file    │ │ post            │ │  │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘ │  │
│  │                                                           │  │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────────────┐ │  │
│  │  │ camera  │ │ libcam  │ │ alg     │ │ movie           │ │  │
│  │  │ (loop)  │ │ (Pi)    │ │ (detect)│ │ (record)        │ │  │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────────────┘ │  │
│  │                                                           │  │
│  └──────────────────────────────────────────────────────────┘  │
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## Implementation Approach

### Phase 1: Extend Motion's Web Server

Modify `webu_file.cpp` to:
1. Serve React build files from a configurable directory
2. Handle SPA routing (return `index.html` for unmatched routes)
3. Set appropriate cache headers for assets

**New Config Parameters** (in `conf.cpp`):
```cpp
webcontrol_html_path    // Path to React build directory (default: /usr/share/motion/webui)
webcontrol_spa_mode     // Enable SPA routing (default: on)
```

### Phase 2: Enhance JSON API

Add missing endpoints to `webu_json.cpp`:

| New Endpoint | Method | Purpose |
|--------------|--------|---------|
| `/api/auth/login` | POST | User authentication |
| `/api/auth/logout` | POST | End session |
| `/api/auth/me` | GET | Current user info |
| `/api/cameras/{id}` | DELETE | Remove camera |
| `/api/cameras` | POST | Add new camera |
| `/api/media/pictures/{cam_id}` | GET | List snapshots |
| `/api/media/pictures/{id}` | DELETE | Delete snapshot |
| `/api/media/movies/{id}` | DELETE | Delete recording |
| `/api/system/reboot` | POST | Reboot system |
| `/api/system/shutdown` | POST | Shutdown system |
| `/api/system/temperature` | GET | CPU temperature |
| `/api/config/backup` | GET | Export config |
| `/api/config/restore` | POST | Import config |

### Phase 3: Build React Frontend

Create React application in `/webui/` directory:
- Vite + React 18 + TypeScript
- Tailwind CSS (dark theme)
- Zustand for state
- TanStack Query for data fetching

Build output goes to `/usr/share/motion/webui/` or configurable path.

---

## Directory Structure

```
motion-motioneye/
├── src/                          # Motion C++ source
│   ├── webu.cpp                  # Web server (existing)
│   ├── webu_json.cpp             # JSON API (extend)
│   ├── webu_file.cpp             # Static files (extend for SPA)
│   └── ...
├── webui/                        # NEW: React application
│   ├── src/
│   │   ├── components/
│   │   ├── features/
│   │   ├── hooks/
│   │   ├── api/
│   │   ├── store/
│   │   └── ...
│   ├── public/
│   ├── index.html
│   ├── vite.config.ts
│   ├── tailwind.config.js
│   └── package.json
├── data/
│   └── webui/                    # Built React assets (installed)
├── doc/
├── configure.ac
└── Makefile.am
```

---

## C++ Modifications Required

### 1. webu_file.cpp - SPA Static Serving

```cpp
// Pseudocode for changes needed

void cls_webu_file::main() {
    std::string filepath = resolve_path(webua->url);

    // If file exists, serve it
    if (file_exists(filepath)) {
        serve_file(filepath);
        return;
    }

    // SPA mode: return index.html for unmatched routes
    if (app->cfg->webcontrol_spa_mode) {
        std::string index = app->cfg->webcontrol_html_path + "/index.html";
        if (file_exists(index)) {
            serve_file(index);
            return;
        }
    }

    // 404
    webua->resp_page = "Not Found";
    webua->resp_type = WEBUI_RESP_TEXT;
}

void cls_webu_file::serve_file(const std::string &path) {
    // Determine MIME type
    std::string mime = get_mime_type(path);

    // Set cache headers for immutable assets (*.js, *.css with hash)
    if (is_immutable_asset(path)) {
        // Cache-Control: public, max-age=31536000, immutable
    }

    // Read and send file
    // ...
}
```

### 2. webu_json.cpp - API Enhancements

```cpp
// Add new handler methods

void cls_webu_json::auth_login() {
    // Verify credentials
    // Generate session token
    // Set cookie
}

void cls_webu_json::media_pictures_list() {
    // Scan picture directory
    // Return JSON array with metadata
}

void cls_webu_json::media_picture_delete() {
    // Validate path (prevent directory traversal)
    // Delete file
}

void cls_webu_json::system_temperature() {
    // Read /sys/class/thermal/thermal_zone0/temp
    // Return JSON
}
```

### 3. conf.cpp - New Parameters

```cpp
// Add to config_parms[]
{"webcontrol_html_path", PARM_TYP_STRING, PARM_CAT_WEB},
{"webcontrol_spa_mode", PARM_TYP_BOOL, PARM_CAT_WEB},
```

---

## Build Integration

### Autotools Integration

Modify `Makefile.am`:
```makefile
# Install React build to share directory
webuildir = $(datadir)/motion/webui
dist_webui_DATA = webui/dist/*

# Build React before make install
webui-build:
	cd webui && npm ci && npm run build

install-data-hook: webui-build
```

### Docker Build

```dockerfile
# Multi-stage build
FROM node:20-alpine AS webui-builder
WORKDIR /webui
COPY webui/package*.json ./
RUN npm ci
COPY webui/ ./
RUN npm run build

FROM debian:bookworm-slim AS motion-builder
# Build Motion C++...

FROM debian:bookworm-slim
COPY --from=motion-builder /usr/local/bin/motion /usr/local/bin/
COPY --from=webui-builder /webui/dist /usr/share/motion/webui
```

---

## Performance Comparison

| Metric | Current (MotionEye + Motion) | New (Motion + React) |
|--------|------------------------------|----------------------|
| Processes | 2+ (Motion + Python) | 1 (Motion only) |
| RAM Usage | ~150-200 MB | ~20-30 MB |
| Startup Time | 5-10 seconds | 1-2 seconds |
| Stream Latency | +proxying overhead | Direct from Motion |
| Config Save | Python → file → Motion | Direct to Motion |
| Dependencies | Python, Tornado, etc. | libmicrohttpd only |

---

## Migration Path

### For Existing MotionEye Users

1. **Phase 1**: Release Motion with enhanced API (backward compatible)
2. **Phase 2**: Release React UI as optional replacement for `webu_html.cpp`
3. **Phase 3**: Deprecate embedded HTML, React becomes default
4. **Phase 4**: Provide migration tool for MotionEye configs

### Config Compatibility

Motion's config format is already used. MotionEye-specific settings (Python-side) that need migration:
- Admin username/password → Motion's `webcontrol_auth`
- Multiple camera layout → New config parameter
- Upload services (S3, SFTP) → Motion plugins or scripts
- Notifications → Motion's `on_event_*` scripts

---

## Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| C++ development slower than Python | Medium | Reuse existing webu_* code patterns |
| React build adds complexity | Low | CI/CD handles builds, users get binaries |
| Feature parity takes time | Medium | Prioritize core features, incremental release |
| Breaking MotionEye users | High | Gradual migration, config converter tool |

---

## Success Criteria

1. **Single binary** - Motion serves everything
2. **RAM < 30MB** - Significant reduction from current
3. **Feature parity** - All MotionEye features work
4. **Streaming performance** - Equal or better than current
5. **Pi 4/5 tested** - Works on target hardware
6. **No Python required** - Complete elimination

---

## Next Steps

1. Review and approve architecture
2. Create detailed C++ modification plan
3. Create React component inventory
4. Begin Phase 1: webu_file.cpp SPA support
