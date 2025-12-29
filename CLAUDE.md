# Motion-MotionEye - Claude Code Instructions

## Project Identity

**What**: Combined Motion + MotionEye - C++ video motion detection daemon with modern React UI
**Origin**: Fork of [Motion](https://github.com/Motion-Project/motion), updated for 64-bit systems with integrated web interface
**Target**: Raspberry Pi 4 and newer (64-bit)
**Languages**: C++17 (backend), TypeScript/React (frontend)
**Build**: GNU Autotools (backend), Vite (frontend)
**License**: GPL v3+

## Project Context

This project combines Motion and MotionEye into a single, efficient application:

1. **Motion** (C++ daemon) - Updated for 64-bit systems and optimized from the original Motion project
2. **MotionEye** (React UI) - Complete rebuild using modern, efficient code. The original Python-based MotionEye at `/Users/tshuey/Documents/GitHub/motioneye/` is used as feature reference only
3. **No Python backend** - React talks directly to Motion's C++ API via libmicrohttpd
4. **Legacy UI removal** - Motion's original embedded HTML UI will be removed (task TBD)

## Core Principles

- Answer directly, avoid unnecessary preambles/postambles
- Be concise
- **CPU efficiency is paramount** - Pi has limited CPU, generates heat, may run on battery. Always consider CPU impact in all code changes

## Project Rules

1. **Plans** MUST be documented in `docs/plans/`
2. **Use scratchpads for working memory** - create and use files in `docs/scratchpads/` for notes, research findings, and intermediate work
3. **Do not guess** - ask questions and/or research for answers
4. **Preserve `cls_alg`** - the motion detection algorithm works; don't modify unless explicitly asked
5. **Evidence over assumptions** - verify changes work on actual hardware when possible
6. **Minimize CPU usage** - always consider CPU impact when developing code changes
7. **Efficiency first** - all new code must be planned for efficiency before implementation
8. **Updating Motion binaries** - Review `docs/project/MOTION_RESTART.md` to prevent issues, prior to updating Motion binaries on the Pi

## Technology Stack

| Layer | Technology |
|-------|------------|
| Backend | Motion C++17 |
| Web Server | libmicrohttpd (built into Motion) |
| Frontend | React 18 + TypeScript + Vite |
| Styling | Tailwind CSS |
| State | Zustand |
| Data Fetching | TanStack Query |

## Project Structure

```
motion-motioneye/
├── src/                    # Motion C++ backend
│   ├── webu_file.cpp       # Static file serving + SPA support
│   ├── webu_json.cpp       # JSON API endpoints
│   ├── webu_stream.cpp     # MJPEG streaming
│   ├── conf.cpp            # Configuration parameters
│   └── ...
├── frontend/               # React UI application
│   ├── src/
│   ├── public/
│   └── package.json
├── data/
│   └── webui/              # Built frontend assets (installed)
└── docs/                   # Documentation
    ├── project/            # Architecture and patterns
    ├── plans/              # Implementation plans (REQUIRED)
    └── scratchpads/        # Working documents
```

## Quick Navigation

| Need | Location |
|------|----------|
| React UI plans | `docs/plans/react-ui/README.md` |
| Architecture overview | `docs/project/ARCHITECTURE.md` |
| Code patterns | `docs/project/PATTERNS.md` |
| How to modify | `docs/project/MODIFICATION_GUIDE.md` |
| Agent quick-ref | `docs/project/AGENT_GUIDE.md` |

## Key Files

### Backend (C++)

| File | Purpose |
|------|---------|
| `src/libcam.cpp` | libcamera capture - Pi camera support |
| `src/conf.cpp` | Configuration parameters (~600 params) |
| `src/webu_json.cpp` | JSON API for frontend |
| `src/webu_file.cpp` | Static file serving |

### Do Not Modify (Unless Explicitly Asked)

| File | Reason |
|------|--------|
| `src/alg.cpp` | Core motion detection - proven algorithm |
| `src/alg.hpp` | Algorithm interface |
| `src/alg_sec.cpp` | Secondary detection methods |

### Frontend (React)

| Location | Purpose |
|----------|---------|
| `frontend/src/` | React application source |
| `frontend/package.json` | Frontend dependencies |

### Reference Only

| Location | Purpose |
|----------|---------|
| `/Users/tshuey/Documents/GitHub/motioneye/` | Original MotionEye - feature reference for UI rebuild |

## Build Commands

### Backend (C++ Motion)

```bash
# Install dependencies (Pi)
sudo apt install libcamera-dev libcamera-tools libjpeg-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
  libmicrohttpd-dev libsqlite3-dev autoconf automake libtool pkgconf

# Build
autoreconf -fiv
./configure --with-libcam --with-sqlite3
make -j4

# Test run (foreground, debug)
./motion -c /path/to/motion.conf -n -d
```

### Frontend (React)

```bash
cd frontend
npm install
npm run dev      # Development server
npm run build    # Production build
```

## Testing on Raspberry Pi

### Before Running Tests

**IMPORTANT: Always ask the user if the Pi is powered on before attempting to connect or run tests.**

### SSH Connection

| Device | Hostname | IP | Command |
|--------|----------|-----|---------|
| Pi 5 | pi5-motioneye | 192.168.1.176 | `ssh admin@192.168.1.176` |
| Pi 4 | pi4-motion | 192.168.1.246 | `ssh admin@192.168.1.246` |

SSH keys configured for passwordless access from the development Mac.

### Deployment Workflow

1. **Sync code to Pi:**
   ```bash
   rsync -avz --exclude='.git' --exclude='node_modules' --exclude='.venv' \
     /Users/tshuey/Documents/GitHub/motion-motioneye/ admin@192.168.1.176:~/motion-motioneye/
   ```

2. **Build on Pi:**
   ```bash
   ssh admin@192.168.1.176 "cd ~/motion-motioneye && autoreconf -fiv && ./configure --with-libcam --with-sqlite3 && make -j4"
   ```

3. **Restart service:**
   ```bash
   ssh admin@192.168.1.176 "sudo systemctl restart motion"
   ```

4. **Check logs:**
   ```bash
   ssh admin@192.168.1.176 "sudo journalctl -u motion -n 50 --no-pager"
   ```

### Verify Camera Streaming

```bash
ssh admin@192.168.1.176 "curl -s --max-time 3 'http://localhost:7999/1/mjpg/stream' -o /tmp/test.dat && file /tmp/test.dat"
```

### Common Issues

- **Port conflicts**: Kill orphaned motion processes with `sudo pkill -9 motion`
- **Camera not detected**: Verify with `rpicam-hello --list-cameras`

## Code Patterns

### Configuration Parameter (Adding New)

1. Add to `config_parms[]` in `src/conf.cpp`
2. Add member to `cls_config` in `src/conf.hpp`
3. Add edit handler in `src/conf.cpp`
4. Set default value
5. Update template in `data/motion-dist.conf.in`

### Class Naming

- All classes use `cls_` prefix: `cls_camera`, `cls_libcam`, `cls_config`
- Context structs use `ctx_` prefix: `ctx_image_data`, `ctx_images`

### Threading

- Main thread: Application control
- Per-camera thread: Capture & detection
- Web server: libmicrohttpd thread pool

## Documentation Structure

```
docs/
├── project/           # AI agent reference docs
│   ├── AGENT_GUIDE.md
│   ├── ARCHITECTURE.md
│   ├── PATTERNS.md
│   └── MODIFICATION_GUIDE.md
├── plans/             # Implementation plans (REQUIRED for all plans)
│   └── react-ui/      # React UI integration plans
└── scratchpads/       # Working documents
```

### Documentation Naming

- **Subdirectories** (`plans/`, `scratchpads/`, `reviews/`, etc.): Use timestamp prefix `YYYYMMDD-hhmm-`
  - Example: `20251216-2036-Feature-Implementation.md`
  - Format: Date in YYYYMMDD, time in hhmm (24-hour), then descriptive name

- **Main `docs/` directory files** (project reference docs): No prefix required

## Reading Files

**Caution**: Some files in this project are very large intentionally
