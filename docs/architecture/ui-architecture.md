# Motion Web UI Architecture

## Overview

Motion includes an embedded web server that provides a comprehensive web interface for camera monitoring, configuration, and control. The web UI is built entirely in C++ using the **libmicrohttpd** library and generates HTML, CSS, and JavaScript dynamically at runtime.

## Technology Stack

### Core Web Server
- **Library**: GNU libmicrohttpd (lightweight HTTP daemon library)
- **Language**: C++17
- **Architecture**: Multi-threaded request handling
- **Protocol Support**: HTTP/1.1, HTTPS/TLS, IPv6
- **Authentication**: HTTP Basic Auth, HTTP Digest Auth

### Response Formats
- **HTML**: Server-side generated pages with embedded CSS/JS
- **JSON**: RESTful API responses for dynamic data
- **MJPEG**: Motion JPEG streaming for video feeds
- **MPEG-TS**: MPEG Transport Stream for video streaming
- **Static Files**: Image files, saved videos

## Architecture Components

### Component Hierarchy

```
cls_webu (Web Server Core)
    ├── cls_webu_ans (Request Handler & Router)
    │   ├── cls_webu_html (HTML Page Generator)
    │   ├── cls_webu_json (JSON API)
    │   ├── cls_webu_stream (MJPEG/Video Streaming)
    │   ├── cls_webu_mpegts (MPEG-TS Streaming)
    │   ├── cls_webu_post (Form Submissions)
    │   ├── cls_webu_text (Text Responses)
    │   ├── cls_webu_file (Static File Serving)
    │   └── cls_webu_getimg (Image Retrieval)
    └── MHD_Daemon (libmicrohttpd daemon instances)
```

### File Organization

| Component | Files | Lines | Purpose |
|-----------|-------|-------|---------|
| **Core Server** | `webu.{cpp,hpp}` | ~633 | Server initialization, daemon management, TLS |
| **Request Router** | `webu_ans.{cpp,hpp}` | ~1,235 | URL parsing, authentication, response coordination |
| **HTML Generator** | `webu_html.{cpp,hpp}` | ~1,702 | Dynamic HTML/CSS/JS generation |
| **JSON API** | `webu_json.{cpp,hpp}` | ~832 | RESTful API, configuration endpoints |
| **Video Streaming** | `webu_stream.{cpp,hpp}` | ~711 | MJPEG streaming, image delivery |
| **MPEG-TS Streaming** | `webu_mpegts.{cpp,hpp}` | ~485 | MPEG Transport Stream generation |
| **Form Handling** | `webu_post.{cpp,hpp}` | ~966 | POST request processing, CSRF protection |
| **Text Responses** | `webu_text.{cpp,hpp}` | ~241 | Plain text API responses |
| **File Serving** | `webu_file.{cpp,hpp}` | ~197 | Static file delivery |
| **Image Retrieval** | `webu_getimg.{cpp,hpp}` | ~324 | Camera snapshot images |

**Total**: ~7,326 lines of web UI code

## Request Flow

### 1. Connection Initialization

```
Client Request
    ↓
MHD_Daemon (libmicrohttpd)
    ↓
webu_mhd_init()
    ↓
new cls_webu_ans() [Creates request context]
```

### 2. Request Processing

```
cls_webu_ans::answer_main()
    ├── Authentication Check (Basic/Digest)
    ├── TLS Certificate Validation
    ├── URL Parsing (/camid/cmd1/cmd2/cmd3)
    ├── Client IP Tracking (Rate Limiting)
    └── Route to Handler
```

### 3. Response Generation

Based on `cnct_type` enum:

| Connection Type | Handler | Output |
|----------------|---------|--------|
| `WEBUI_CNCT_CONTROL` | `cls_webu_html` or `cls_webu_json` | HTML page or JSON data |
| `WEBUI_CNCT_JPG_*` | `cls_webu_stream` (static) | Single JPEG image |
| `WEBUI_CNCT_TS_*` | `cls_webu_stream` (MJPEG) or `cls_webu_mpegts` | Video stream |
| `WEBUI_CNCT_FILE` | `cls_webu_file` | Static file (saved video, etc.) |
| `WEBUI_CNCT_UNKNOWN` | Error response | 404/400 |

### 4. Cleanup

```
MHD Request Complete
    ↓
webu_mhd_deinit()
    ↓
delete cls_webu_ans [Frees request context]
```

## Key Classes

### cls_webu (src/webu.{cpp,hpp})

**Purpose**: Web server lifecycle management

**Responsibilities**:
- Initialize libmicrohttpd daemon(s)
- Configure TLS/SSL certificates
- Set up authentication (Basic/Digest)
- Configure IPv4/IPv6 listening
- Manage dual-port operation (port1/port2)
- Generate/validate CSRF tokens
- Track connected clients (rate limiting)

**Key Members**:
```cpp
struct MHD_Daemon *wb_daemon;        // Primary HTTP daemon
struct MHD_Daemon *wb_daemon2;       // Secondary daemon (different port)
std::list<ctx_webu_clients> wb_clients;  // Client tracking
std::string csrf_token;              // CSRF protection token
char wb_digest_rand[12];             // Digest auth random seed
```

**Key Methods**:
- `startup()`: Initialize and start web server
- `shutdown()`: Stop web server and cleanup
- `csrf_generate()`: Generate CSRF token for form security
- `csrf_validate()`: Verify CSRF token in POST requests

### cls_webu_ans (src/webu_ans.{cpp,hpp})

**Purpose**: Request handler and response coordinator

**Responsibilities**:
- Parse incoming HTTP requests
- Extract URL components: `/camid/cmd1/cmd2/cmd3`
- Perform authentication (Basic or Digest)
- Detect client IP and track connections
- Route requests to appropriate handler classes
- Generate final HTTP response with headers
- Support gzip compression

**Key Members**:
```cpp
std::string url;                     // Full request URL
std::string uri_cmd0, uri_cmd1, uri_cmd2, uri_cmd3;  // Parsed URL segments
enum WEBUI_CNCT cnct_type;          // Connection type (HTML, JSON, stream, etc.)
enum WEBUI_RESP resp_type;          // Response format (HTML, JSON, text, etc.)
std::string resp_page;               // Generated response body
int camindx;                         // Camera index from URL
std::string clientip;                // Client IP address
bool authenticated;                  // Authentication status
```

**URL Parsing Examples**:
- `/` → HTML homepage
- `/0/config` → Camera 0 configuration page
- `/1/stream` → Camera 1 MJPEG stream
- `/api/0/config/get` → Camera 0 config JSON

### cls_webu_html (src/webu_html.{cpp,hpp})

**Purpose**: Generate dynamic HTML pages with embedded CSS and JavaScript

**Responsibilities**:
- Build complete HTML documents (DOCTYPE, head, body)
- Generate inline CSS for styling (no external CSS files)
- Embed JavaScript for client-side interactivity
- Create navigation bars, forms, and camera displays
- Implement PTZ (Pan-Tilt-Zoom) controls
- Display configuration options
- Show camera streams and snapshots

**Page Structure**:
```
HTML Document
├── <head>
│   ├── <style> [Inline CSS]
│   │   ├── Navigation (.sidenav, .dropbtn)
│   │   ├── Config Forms (.cls_config, .cls_button)
│   │   └── PTZ Controls (.arrow, .zoombtn)
│   └── <script> [Inline JavaScript]
│       ├── Navigation functions
│       ├── AJAX config submission
│       ├── Camera selection
│       └── Stream display
└── <body>
    ├── Side Navigation Bar
    ├── Main Content Area
    │   ├── Camera Grid/List
    │   ├── Configuration Forms
    │   └── Action Buttons
    └── Log Display
```

**Key JavaScript Functions** (all embedded):
- `script_send_config()`: Submit configuration changes via AJAX
- `script_send_action()`: Trigger camera actions (start, stop, snapshot)
- `script_display_cameras()`: Render camera grid with streams
- `script_camera_buttons_ptz()`: PTZ camera control buttons
- `script_movies_page()`: List and playback recorded videos

### cls_webu_json (src/webu_json.{cpp,hpp})

**Purpose**: RESTful JSON API for programmatic access

**Responsibilities**:
- Serialize configuration parameters to JSON
- List cameras and their status
- Provide parameter metadata (type, range, webui_level)
- List recorded movies/videos
- Return status information (motion detected, recording, etc.)
- Hot-reload configuration changes at runtime

**API Endpoints**:
```
/api/config                    → All configuration parameters (JSON)
/api/0/config                  → Camera 0 config parameters
/api/cameras                   → List of all cameras
/api/status                    → Current system status
/api/movies                    → List of recorded videos
/api/log                       → Recent log messages
/api/config/set               → Hot-reload single parameter (POST)
```

**JSON Response Format**:
```json
{
  "cameras": [
    {
      "id": 0,
      "name": "Camera 1",
      "status": "active"
    }
  ],
  "params": {
    "width": {
      "value": "1920",
      "category": "source",
      "webui_level": "limited"
    }
  }
}
```

**Hot Reload Feature** (src/webu_json.cpp:27):
- `config_set()`: Allows runtime parameter changes
- Validates parameter exists and supports hot reload
- Updates configuration without daemon restart
- Returns success/failure with old/new values

### cls_webu_stream (src/webu_stream.{cpp,hpp})

**Purpose**: Video streaming and image delivery

**Responsibilities**:
- Generate MJPEG (Motion JPEG) streams
- Serve single JPEG snapshots
- Coordinate with MPEG-TS handler for H.264 streams
- Manage stream frame rate
- Handle multiple concurrent stream connections
- Buffer management for smooth playback

**Stream Types**:
```cpp
enum WEBUI_CNCT {
    WEBUI_CNCT_JPG_FULL,          // Full-resolution JPEG
    WEBUI_CNCT_JPG_SUB,           // Substream JPEG (lower res)
    WEBUI_CNCT_JPG_MOTION,        // Motion-detected areas overlay
    WEBUI_CNCT_JPG_SOURCE,        // Source image (pre-processing)
    WEBUI_CNCT_JPG_SECONDARY,     // Secondary detector image
    WEBUI_CNCT_TS_FULL,           // MPEG-TS full stream
    WEBUI_CNCT_TS_SUB,            // MPEG-TS substream
    // ... etc
};
```

**MJPEG Streaming Process**:
```
1. Client connects to /1/stream
2. cls_webu_stream::main() determines stream type
3. For MJPEG: mjpeg_response() sends multipart/x-mixed-replace
4. Loop: mjpeg_one_img() fetches latest frame → sends JPEG
5. delay() enforces FPS limit
6. Repeat until client disconnects
```

### cls_webu_post (src/webu_post.{cpp,hpp})

**Purpose**: Handle POST requests (form submissions, actions)

**Responsibilities**:
- Parse POST data (multipart/form-data, application/x-www-form-urlencoded)
- Validate CSRF tokens (prevents cross-site request forgery)
- Process configuration changes from web forms
- Execute camera actions (start, stop, pause, snapshot)
- Return JSON or redirect responses

**POST Processing Flow**:
```
POST Request
    ↓
parse POST body (key=value pairs)
    ↓
CSRF validation (compare token)
    ↓
Update configuration OR execute action
    ↓
Return success/error response
```

**Security**: All POST requests require valid CSRF token (generated by `cls_webu::csrf_generate()`).

### cls_webu_file (src/webu_file.{cpp,hpp})

**Purpose**: Serve static files (recorded videos, images)

**Responsibilities**:
- Locate requested file in target directory
- Validate file path (prevent directory traversal attacks)
- Send file with appropriate MIME type
- Stream large files efficiently

**File Types Served**:
- Recorded videos (`.mp4`, `.mkv`, `.avi`)
- Snapshot images (`.jpg`)
- Timelapse videos

### cls_webu_mpegts (src/webu_mpegts.{cpp,hpp})

**Purpose**: Generate MPEG Transport Stream for H.264/H.265 video

**Responsibilities**:
- Multiplex video into MPEG-TS format
- Stream H.264/H.265 encoded video
- Synchronize with camera encoder
- Handle stream buffering

**Use Case**: Modern browsers can play MPEG-TS natively via `<video>` tag, providing lower latency than MJPEG.

## Authentication & Security

### Authentication Methods

**HTTP Basic Authentication** (`webu_ans.cpp:mhd_basic()`):
```
Client sends: Authorization: Basic base64(username:password)
Server validates against webcontrol_authentication config
Returns: 200 OK or 401 Unauthorized
```

**HTTP Digest Authentication** (`webu_ans.cpp:mhd_digest()`):
```
Server sends: WWW-Authenticate: Digest realm=...
Client computes: MD5(username:realm:password)
Server validates digest response
More secure than Basic (password never sent in clear)
```

**Configuration**:
```
webcontrol_authentication = username:password
webcontrol_auth_method = basic|digest|none
```

### Security Features

| Feature | Implementation | Purpose |
|---------|---------------|---------|
| **CSRF Protection** | `csrf_token` (64 hex chars) | Prevent cross-site form submissions |
| **Rate Limiting** | `wb_clients` list (max 10,000) | Prevent DoS via connection exhaustion |
| **Client Tracking** | IP + username + timestamp | Lockout after failed auth attempts |
| **TLS/SSL** | libmicrohttpd TLS support | Encrypted connections (HTTPS) |
| **Path Validation** | `parseurl()` checks base_path | Prevent directory traversal |
| **Session Timeout** | `WEBUI_CLIENT_TTL` (3600s) | Expire stale client entries |

### CSRF Token Flow

```
1. cls_webu::csrf_generate()
   → Generates 64-char hex token on startup

2. HTML forms include:
   <input type="hidden" name="csrf_token" value="...">

3. POST handler validates:
   cls_webu::csrf_validate(submitted_token)
   → Reject if mismatch

4. JSON API can use header:
   X-CSRF-Token: <token>
```

## Configuration Integration

### Configuration System

Motion uses a massive configuration system (~600 parameters) defined in `src/conf.cpp`.

**Web UI Integration**:
- `cls_config` class holds all camera/global parameters
- `webu_json` serializes config to JSON
- `webu_html` generates HTML forms from config metadata
- `webu_post` applies changes via `cls_config::edit_set()`

**Parameter Metadata**:
```cpp
struct ctx_parm {
    std::string parm_name;        // "width"
    enum PARM_CAT webui_level;    // WEBUI_LEVEL_LIMITED (show in UI)
    enum PARM_TYP parm_type;      // PARM_TYP_INT
    std::string parm_desc;        // "Image width (pixels)"
    int parm_min, parm_max;       // Range: 64-4096
};
```

**WebUI Levels**:
- `WEBUI_LEVEL_ALWAYS`: Always show (basic settings)
- `WEBUI_LEVEL_LIMITED`: Show in limited mode
- `WEBUI_LEVEL_ADVANCED`: Advanced users only
- `WEBUI_LEVEL_RESTRICTED`: Hidden in web UI
- `WEBUI_LEVEL_NEVER`: Never show (internal only)

### Hot Reload System

**Feature**: Change configuration at runtime without restarting daemon

**Implementation** (`webu_json.cpp:config_set()`):
```cpp
POST /api/config/set
Body: { "parm_name": "width", "parm_val": "1280" }

1. validate_hot_reload(parm_name, parm_index)
   → Check if parameter supports hot reload
   → Find parameter index in config_parms[]

2. apply_hot_reload(parm_index, parm_val)
   → Call parameter's edit handler
   → Update cls_config member variable
   → Trigger any side effects (restart stream, etc.)

3. build_response(success, parm_name, old_val, new_val, hot_reload)
   → Return JSON status
```

**Limitations**: Not all parameters support hot reload (e.g., `libcam_device` requires restart).

## URL Routing

### URL Structure

```
http(s)://hostname:port/[base_path]/[camid]/[cmd1]/[cmd2]/[cmd3]
```

**Examples**:
```
/                          → Homepage (HTML)
/0                         → Camera 0 page (HTML)
/config                    → Global config (HTML)
/0/config                  → Camera 0 config (HTML)
/0/stream                  → Camera 0 MJPEG stream
/0/detection/snapshot      → Camera 0 snapshot
/api/0/config/get          → Camera 0 config (JSON)
/movies                    → List recorded videos (HTML)
/movies/2024-01-15.mp4     → Serve video file
```

### Parsing Logic (src/webu_ans.cpp:parseurl())

```cpp
Input: /2/action/snapshot
       ↓
uri_cmd0 = "2"         (camera ID)
uri_cmd1 = "action"    (command category)
uri_cmd2 = "snapshot"  (action type)
uri_cmd3 = ""          (unused)
       ↓
camindx = 2
cnct_type = WEBUI_CNCT_CONTROL
resp_type = WEBUI_RESP_JSON (if /api/) or WEBUI_RESP_HTML
```

### Response Type Determination

```cpp
if (url.find("/api/") != std::string::npos) {
    resp_type = WEBUI_RESP_JSON;
} else if (uri_cmd1 == "stream") {
    cnct_type = WEBUI_CNCT_TS_* or WEBUI_CNCT_JPG_*;
} else if (uri_cmd1 == "movies") {
    cnct_type = WEBUI_CNCT_FILE;
} else {
    resp_type = WEBUI_RESP_HTML;
}
```

## Multi-Threading

### Thread Model

```
Main Thread (cls_motapp)
    ├── Camera Threads (one per camera)
    │   └── Capture → Detect → Record loop
    └── Web Server Thread Pool (libmicrohttpd)
        ├── Listener Thread (accept connections)
        └── Worker Threads (handle requests)
            ├── Thread 1: /0/stream (client A)
            ├── Thread 2: /1/stream (client B)
            ├── Thread 3: /api/config (client C)
            └── ...
```

### Synchronization

**Stream Access**: Camera threads write images to shared buffers:
```cpp
ctx_images {
    ctx_image_data full;      // Full resolution
    ctx_image_data sub;       // Substream
    ctx_stream_data stream;   // Web stream copy
}
```

**Locking**: Mutexes protect shared data structures
- Configuration changes: Lock before modifying `cls_config`
- Image buffers: Lock before reading latest frame
- Client list: Lock when adding/removing clients

**Thread Safety**:
- `cls_webu_ans` instance created per-request (no shared state)
- Each web request gets independent context
- libmicrohttpd handles thread pool management

## Performance Considerations

### Streaming Optimization

1. **Dual Streams**:
   - `full`: High-resolution stream (e.g., 1920x1080)
   - `sub`: Low-resolution substream (e.g., 640x480)
   - Clients can choose based on bandwidth

2. **Frame Rate Control** (`webu_stream.cpp:set_fps()`):
   - Limits stream FPS to reduce CPU usage
   - Default: 1 FPS for web streams
   - Configurable: `stream_maxrate` parameter

3. **Buffer Management**:
   - Ring buffer for pre/post event capture
   - Triple buffer for network cameras (latest/receiving/processing)
   - Prevents blocking camera thread

### Compression

**Gzip Encoding** (`webu_ans.cpp:gzip_deflate()`):
- Detects `Accept-Encoding: gzip` header
- Compresses HTML/JSON responses
- Significantly reduces bandwidth for text content
- Not applied to JPEG/MPEG streams (already compressed)

### Client Limits

**Security Limits** (src/webu.hpp):
```cpp
#define WEBUI_MAX_CLIENTS 10000   // Max tracked clients
#define WEBUI_CLIENT_TTL  3600    // Client entry TTL (seconds)
```

**Purpose**: Prevent memory exhaustion from DoS attacks

## Page Types

### 1. Homepage (/)

**Content**:
- Camera grid with live streams
- Quick access to all cameras
- Navigation to config, movies, logs

**Generated By**: `cls_webu_html::default_page()`

### 2. Camera Page (/0, /1, ...)

**Content**:
- Single camera large view
- PTZ controls (if camera supports)
- Camera-specific actions (snapshot, pause)
- Stream quality selector

**Generated By**: `cls_webu_html::user_page()`

### 3. Configuration Page (/config, /0/config)

**Content**:
- Nested forms for ~600 parameters
- Grouped by category (source, image, movie, etc.)
- Input validation (min/max, dropdowns)
- Save button with AJAX submission

**Generated By**: `cls_webu_html::script_display_config()`

### 4. Movies Page (/movies)

**Content**:
- List of recorded videos
- Playback links
- Delete functionality
- Sorted by date/time

**Generated By**: `cls_webu_html::script_movies_page()`

### 5. Log Page (embedded)

**Content**:
- Recent log messages
- Filterable by level (ERR, WRN, NTC, INF, DBG)
- Auto-refresh option

**Generated By**: `cls_webu_html::script_log_display()`

## Protocol Details

### MJPEG Streaming Protocol

**HTTP Headers**:
```http
HTTP/1.1 200 OK
Content-Type: multipart/x-mixed-replace; boundary=--BoundaryString
Connection: close

--BoundaryString
Content-Type: image/jpeg
Content-Length: 45678

[JPEG data]
--BoundaryString
Content-Type: image/jpeg
Content-Length: 46123

[JPEG data]
--BoundaryString
...
```

**Implementation**: `cls_webu_stream::mjpeg_response()`

### MPEG-TS Streaming Protocol

**HTTP Headers**:
```http
HTTP/1.1 200 OK
Content-Type: video/mp2t
Connection: close

[MPEG-TS packets...]
```

**Implementation**: `cls_webu_mpegts::main()`

## Daemon Management

### Dual-Port Support

Motion can run two HTTP daemons simultaneously:

**Port 1** (`webcontrol_port`):
- Default: 8080
- Primary web interface
- Can be TLS or non-TLS

**Port 2** (`webcontrol_port2`):
- Optional secondary port
- Typically for localhost-only access
- Allows both TLS and non-TLS simultaneously

**Use Case**: Expose HTTPS publicly, HTTP on localhost for scripts.

### Startup Sequence (src/webu.cpp:startup())

```
1. mhd_features()
   → Check libmicrohttpd capabilities (TLS, Digest, IPv6)

2. mhd_opts()
   → Configure MHD options (threads, timeouts, TLS certs)

3. start_daemon_port1()
   → MHD_start_daemon() with port1 config

4. start_daemon_port2()
   → MHD_start_daemon() with port2 config (if enabled)

5. Register callbacks:
   → mhd_answer() [request handler]
   → webu_mhd_init() [connection init]
   → webu_mhd_deinit() [connection cleanup]
```

### Shutdown Sequence (src/webu.cpp:shutdown())

```
1. Set finish = true
2. MHD_stop_daemon(wb_daemon)
3. MHD_stop_daemon(wb_daemon2)
4. Wait for active connections to close
5. Cleanup TLS certificates
6. Clear client tracking list
```

## Integration with Motion Core

### Camera Integration

```
cls_camera (Camera Thread)
    ↓
Captures frame → Detects motion → Records video
    ↓
Updates ctx_images (shared buffers)
    ↓
cls_webu_stream reads from ctx_images
    ↓
Sends frame to web clients
```

**Access Pattern**:
```cpp
// Camera thread (write)
cam->imgs.stream.jpg = latest_frame;
cam->imgs.stream.jpg_sz = frame_size;

// Web thread (read)
cls_webu_stream::mjpeg_one_img() {
    memcpy(resp_image, cam->imgs.stream.jpg, cam->imgs.stream.jpg_sz);
}
```

### Configuration Integration

```
Web Form Submission
    ↓
cls_webu_post::main()
    ↓
cls_config::edit_set(parm_name, parm_val)
    ↓
Parameter validation & conversion
    ↓
Update cls_config member variable
    ↓
Trigger side effects (restart camera, reload masks, etc.)
```

### Action Integration

**Action Examples**:
- `snapshot`: Trigger immediate image capture
- `pause`: Pause motion detection
- `start`: Resume motion detection
- `restart`: Restart camera thread

**Implementation**: Actions are routed through `webu_json` or `webu_post` to camera control functions.

## Client-Side Technologies

### Generated JavaScript Features

All JavaScript is **server-generated** and embedded in HTML (no external .js files).

**Key Functionality**:

1. **AJAX Configuration Updates**:
```javascript
function send_config(parm_name, parm_val) {
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/api/config/set", true);
    xhr.setRequestHeader("Content-Type", "application/json");
    xhr.send(JSON.stringify({
        parm_name: parm_name,
        parm_val: parm_val,
        csrf_token: document.csrf_token
    }));
}
```

2. **Dynamic Camera Selection**:
```javascript
function dropchange_cam(camid) {
    // Load new camera stream
    document.getElementById("img_full").src = "/" + camid + "/stream";
}
```

3. **PTZ Controls**:
```javascript
function pantilt(direction) {
    send_action("pantilt_" + direction);
}
```

4. **Auto-Refresh Streams**:
```javascript
// Image auto-reload for static mode
setInterval(function() {
    var img = document.getElementById("cam_img");
    img.src = img.src.split("?")[0] + "?" + new Date().getTime();
}, 1000);
```

### CSS Styling

All CSS is **inline** (embedded in `<style>` tags). No external CSS files.

**Design Philosophy**:
- Minimalist, functional interface
- Sidebar navigation (`.sidenav`)
- Responsive to different screen sizes (some media queries)
- Dark theme for config pages (`.cls_config`)

**Example Styles**:
- `.sidenav`: Fixed left sidebar (10rem width)
- `.dropbtn`: Dropdown buttons for navigation
- `.arrow`: CSS-only arrow buttons for PTZ (rotated borders)
- `.cls_config`: Black background, white text for config forms

## Extensibility

### Adding New Endpoints

**Example**: Add `/api/camera/status` endpoint

1. **Define URL Pattern** in `webu_ans.cpp:parseurl()`:
```cpp
if (uri_cmd1 == "status") {
    cnct_type = WEBUI_CNCT_CONTROL;
    resp_type = WEBUI_RESP_JSON;
}
```

2. **Route to Handler** in `webu_ans.cpp:answer_get()`:
```cpp
if (resp_type == WEBUI_RESP_JSON) {
    webu_json->main();  // Will call new method
}
```

3. **Implement in cls_webu_json**:
```cpp
void cls_webu_json::camera_status() {
    resp_page = "{";
    resp_page += "\"motion_detected\": " + std::to_string(cam->detecting);
    resp_page += ",\"recording\": " + std::to_string(cam->movie_norm != nullptr);
    resp_page += "}";
}
```

### Adding New Page Types

**Example**: Add `/diagnostics` page

1. **Add Navigation Link** in `cls_webu_html::navbar()`:
```cpp
resp_page += "<a href='/diagnostics'>Diagnostics</a>\n";
```

2. **Add Route Handler** in `webu_ans.cpp`:
```cpp
if (uri_cmd0 == "diagnostics") {
    cnct_type = WEBUI_CNCT_CONTROL;
    resp_type = WEBUI_RESP_HTML;
}
```

3. **Create Page Generator** in `cls_webu_html`:
```cpp
void cls_webu_html::page_diagnostics() {
    head();
    navbar();
    resp_page += "<h1>System Diagnostics</h1>";
    // ... add content ...
    script();
    body();
}
```

## Dependencies

### Required Libraries

```
libmicrohttpd-dev     → Web server core
libavcodec-dev        → Video encoding/decoding (for MPEG-TS)
libavformat-dev       → Container format handling
zlib-dev              → Gzip compression
```

### Build Configuration

**Autoconf Check** (`configure.ac`):
```bash
pkgconf libmicrohttpd
→ Sets CPPFLAGS and LIBS
→ Fails build if not found (required dependency)
```

**Compile Flags**:
```bash
-lmicrohttpd    # Link libmicrohttpd
-lz             # Link zlib for gzip
```

## Configuration Parameters

### Web Server Config (excerpt from `motion-dist.conf`)

```ini
# Web Control Interface
webcontrol_port 8080                    # Primary HTTP port
webcontrol_port2 0                      # Secondary port (0=disabled)
webcontrol_ipv6 off                     # Enable IPv6 listener
webcontrol_localhost off                # Restrict to localhost only
webcontrol_base_path /                  # URL base path
webcontrol_authentication username:password  # HTTP auth credentials
webcontrol_auth_method basic            # Auth method: basic|digest|none
webcontrol_cert /path/to/cert.pem      # TLS certificate
webcontrol_key /path/to/key.pem        # TLS private key

# Streaming
stream_maxrate 1                        # Max FPS for web stream
stream_quality 50                       # JPEG quality (1-100)
stream_grey off                         # Greyscale stream
```

### Hot Reload Support

**Parameters that support hot reload**:
- `stream_maxrate`, `stream_quality`, `stream_grey`
- Image processing parameters (brightness, contrast, etc.)
- Motion detection thresholds

**Parameters requiring restart**:
- `libcam_device`, `v4l2_device` (camera selection)
- `width`, `height` (resolution changes)
- Port, authentication, TLS settings

## Known Limitations

1. **No External Assets**: All CSS/JS is inline, increasing HTML size
2. **No Modern Framework**: Hand-coded HTML/JS (no React, Vue, etc.)
3. **Limited Browser Support**: Tested primarily on Chrome/Firefox
4. **MJPEG Only for Streaming**: No WebRTC or HLS (MPEG-TS available)
5. **Single-Page Apps**: Each navigation reloads full HTML page
6. **Desktop-Focused**: Limited mobile/responsive design

## Future Considerations

**Potential Improvements**:
- Separate CSS/JS into static files (reduce redundant downloads)
- Add WebSocket support for real-time events
- Implement HLS/DASH streaming (better mobile support)
- Modernize UI with responsive framework (Bootstrap, Tailwind)
- Add Progressive Web App (PWA) features
- Implement REST API versioning
- Add GraphQL endpoint as alternative to REST

## References

- **libmicrohttpd Manual**: https://www.gnu.org/software/libmicrohttpd/manual/
- **Motion Documentation**: `docs/motion_guide.html`
- **MJPEG Spec**: RFC 2046 (Multipart MIME)
- **HTTP Digest Auth**: RFC 2617
- **MPEG-TS**: ISO/IEC 13818-1

## File Reference Summary

| Purpose | Files |
|---------|-------|
| **Server Core** | `webu.{cpp,hpp}` |
| **Request Routing** | `webu_ans.{cpp,hpp}` |
| **HTML Generation** | `webu_html.{cpp,hpp}` |
| **JSON API** | `webu_json.{cpp,hpp}` |
| **Video Streaming** | `webu_stream.{cpp,hpp}`, `webu_mpegts.{cpp,hpp}` |
| **Form Handling** | `webu_post.{cpp,hpp}` |
| **Static Files** | `webu_file.{cpp,hpp}` |
| **Image Retrieval** | `webu_getimg.{cpp,hpp}` |
| **Text Responses** | `webu_text.{cpp,hpp}` |

---

**Document Version**: 1.0
**Last Updated**: 2025-12-27
**Author**: Claude Code (Automated Documentation)
