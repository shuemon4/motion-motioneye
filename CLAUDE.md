# Motion - Claude Code Instructions

## Project Identity

**What**: Fork of [Motion](https://github.com/Motion-Project/motion) - C++ video motion detection daemon
**Target**: Raspberry Pi 5 with Camera Module v3 (IMX708)
**Language**: C++17
**Build**: GNU Autotools (autoconf/automake)
**License**: GPL v3+

## Current Focus

Updating `cls_libcam` for native Pi 5 + Camera v3 support. Core motion detection algorithm (`cls_alg`) is proven and must be preserved.

- Answer directly, avoid unnecessary preambles/postambles
- Be concise.

## Project Rules

1. **Plans** MUST be documented in `~/doc/plans/`
2. **Use scratchpads for working memory** - create and use files in `doc/scratchpads/` for notes, research findings, and intermediate work
3. **Do not guess** - ask questions and/or research for answers
4. **Preserve `cls_alg`** - the motion detection algorithm works; don't modify unless explicitly asked
5. **Evidence over assumptions** - verify changes work on actual hardware when possible
6. **Minimize CPU usage** - the Pi has limited CPU, generates heat, and may run on battery; always consider CPU impact when developing code changes

## Quick Navigation

| Need | Location |
|------|----------|
| Architecture overview | `doc/project/ARCHITECTURE.md` |
| Code patterns | `doc/project/PATTERNS.md` |
| How to modify | `doc/project/MODIFICATION_GUIDE.md` |
| Agent quick-ref | `doc/project/AGENT_GUIDE.md` |
| Pi 5 implementation plan | `doc/scratchpads/pi5-camv3-implementation.md` |
| Detailed design | `doc/plans/Pi5-CamV3-Implementation-Design.md` |

## Key Files for Current Work

### Must Modify (Pi 5 Support)

| File | Purpose |
|------|---------|
| `src/libcam.cpp` | libcamera capture - stream role, buffers, camera selection |
| `src/libcam.hpp` | libcamera class interface |
| `src/conf.cpp` | Configuration parameters (~600 params) |
| `src/conf.hpp` | Config class definition |

### Do Not Modify (Unless Explicitly Asked)

| File | Reason |
|------|--------|
| `src/alg.cpp` | Core motion detection - proven algorithm |
| `src/alg.hpp` | Algorithm interface |
| `src/alg_sec.cpp` | Secondary detection methods |

### May Need Adjustment

| File | Purpose |
|------|---------|
| `src/camera.cpp` | Camera processing loop - may need frame handling tweaks |
| `src/movie.cpp` | Video recording via FFmpeg - verify encoder compatibility |
| `configure.ac` | Build configuration - libcamera detection |

## Build Commands

### Development Build (Pi 5)

```bash
# Install dependencies
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

### Target Build Configuration

```bash
./configure \
  --with-libcam \
  --with-sqlite3 \
  --without-v4l2 \
  --without-mysql \
  --without-mariadb \
  --without-pgsql
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
   rsync -avz --exclude='.git' --exclude='__pycache__' --exclude='*.pyc' --exclude='.venv' /Users/tshuey/Documents/GitHub/motioneye/ admin@192.168.1.176:~/motioneye/
   ```

2. **Install on Pi:**
   ```bash
   ssh admin@192.168.1.176 "cd ~/motioneye && sudo pip3 install . --break-system-packages"
   ```

3. **Restart service:**
   ```bash
   ssh admin@192.168.1.176 "sudo systemctl restart motioneye"
   ```

4. **Check logs:**
   ```bash
   ssh admin@192.168.1.176 "sudo journalctl -u motioneye -n 50 --no-pager"
   ```

### Verify Camera Streaming

Test Motion stream directly:
```bash
ssh admin@192.168.1.176 "curl -s --max-time 3 'http://localhost:7999/1/mjpg/stream' -o /tmp/test.dat && file /tmp/test.dat"
```

Access MotionEye web interface: `http://192.168.1.176:8765/`

### Common Issues

- **Port conflicts**: Kill orphaned motion processes with `sudo pkill -9 motion`
- **Permission errors**: Ensure `/etc/motioneye` is owned by `motion:motion`
- **Camera not detected**: Verify with `rpicam-hello --list-cameras`

---

## Deploying to Pi

### Transfer Code to Pi

**Option A: Git clone (if pushed)**
```bash
# On Pi
git clone https://github.com/YOUR_USERNAME/motion.git
cd motion
git checkout your-branch
```

**Option B: Direct transfer**
```bash
# From Windows
scp -r "C:\Users\Trent\Documents\GitHub\motion" pi@RPI_IP:~/
```

### Build on Pi

```bash
cd ~/motion
autoreconf -fiv
./configure --with-libcam --with-sqlite3
make -j4
sudo make install
```

### Restart Service After Changes

```bash
sudo systemctl restart motion
# or run in foreground for debugging:
sudo motion -c /etc/motion/motion.conf -n -d
```

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

### Image Buffers

- Ring buffer for pre/post capture
- Triple buffer pattern for network cameras (latest/receiving/processing)

## libcamera-Specific Context

### Current Issues (Why Fixing)

1. Uses `StreamRole::Viewfinder` instead of `VideoRecording`
2. Single buffer allocation causes pipeline issues on Pi 5's ISP
3. No filtering of USB cameras from CSI cameras

### Key Fixes Applied/Needed

See `doc/scratchpads/pi5-camv3-implementation.md` for detailed implementation plan.

### Camera v3 Features to Support

- Autofocus (AfMode, AfRange, LensPosition)
- HDR mode
- Wide angle lens variant

## Success Criteria

1. Camera v3 detected on Pi 5 startup
2. Consistent 30fps @ 1920x1080 capture
3. Motion detection performs identically to proven algorithm
4. H.264 recording works with proper timestamps
5. 24+ hour stability

## Documentation Structure
- Not up-to-date
```
doc/
├── project/           # AI agent reference docs
│   ├── AGENT_GUIDE.md
│   ├── ARCHITECTURE.md
│   ├── PATTERNS.md
│   └── MODIFICATION_GUIDE.md
├── plans/             # Implementation plans (REQUIRED for all plans)
│   ├── Pi5-CamV3-Implementation-Design.md
│   └── MotionUpdate-Pi5-CamV3.md
└── scratchpads/       # Working documents
    └── pi5-camv3-implementation.md
```

### Documentation Naming

- **Subdirectories** (`plans/`, `scratchpads/`, `reviews/`, etc.): Use timestamp prefix `YYYYMMDD-hhmm-`
  - Example: `20251216-2036-Pi5-CamV3-Implementation-Design.md`
  - Format: Date in YYYYMMDD, time in hhmm (24-hour), then descriptive name

- **Main `doc/` directory files** (project reference docs): No prefix required
  - Examples: `motion_build.html`, `copyright`, `motion.gif`

## Reading Files

**Caution** Some files in this project are very large intentionally