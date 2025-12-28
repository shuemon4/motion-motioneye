# Pi Deployment Workflow

**Created**: 2025-12-11
**Target**: Raspberry Pi 5 at admin@192.168.1.176

## Environment Details

| Property | Value |
|----------|-------|
| Pi Model | Raspberry Pi 5 |
| OS | Raspbian GNU/Linux 12 (bookworm) |
| Kernel | 6.12.47+rpt-rpi-v8 |
| Architecture | aarch64 |
| SSH User | admin |
| SSH Auth | Key-based (no password) |
| Motion Path | ~/motion |

## Deployment Commands

### 1. Transfer Code

**From Mac:**
```bash
rsync -avz --delete \
  --exclude='.git' \
  --exclude='*.o' \
  --exclude='*.lo' \
  --exclude='.libs' \
  --exclude='autom4te.cache' \
  /path/to/motion/ admin@192.168.1.176:~/motion/
```

**From Windows:**
```powershell
# Using scp (slower, no incremental)
scp -r "C:\path\to\motion" admin@192.168.1.176:~/

# Alternative: rsync via WSL
wsl rsync -avz --delete ...
```

### 2. Fix Line Endings (Windows Only)

```bash
ssh admin@192.168.1.176 << 'EOF'
sudo apt install -y dos2unix
find ~/motion -type f \( -name "*.sh" -o -name "configure*" -o -name "Makefile*" \) -exec dos2unix {} \;
chmod +x ~/motion/scripts/*
chmod +x ~/motion/configure
EOF
```

### 3. Build on Pi

```bash
ssh admin@192.168.1.176 << 'EOF'
cd ~/motion
autoreconf -fiv
./configure --with-libcam --with-sqlite3
make -j4
EOF
```

### 4. Install (Optional)

```bash
ssh admin@192.168.1.176 "cd ~/motion && sudo make install"
```

### 5. Service Management

```bash
# Restart motion service
ssh admin@192.168.1.176 "sudo systemctl restart motion"

# Run in foreground for debugging
ssh admin@192.168.1.176 "sudo ~/motion/src/motion -c /etc/motion/motion.conf -n -d"

# Check status
ssh admin@192.168.1.176 "sudo systemctl status motion"

# View logs
ssh admin@192.168.1.176 "sudo journalctl -u motion -f"
```

## Issues Encountered and Solutions

### Issue 1: Stale Build Artifacts

**Problem**: After code changes, build might use cached objects.

**Solution**: Clean before build:
```bash
ssh admin@192.168.1.176 "cd ~/motion && make clean && make -j4"
```

### Issue 2: Windows Line Endings (CRLF)

**Problem**: Scripts fail with `/bin/bash^M: bad interpreter`.

**Solution**: Use dos2unix on shell scripts and configure files:
```bash
dos2unix configure scripts/* Makefile.am
```

### Issue 3: Permission Issues After Windows Transfer

**Problem**: Scripts not executable after scp from Windows.

**Solution**: Restore permissions:
```bash
chmod +x scripts/* configure
```

### Issue 4: autoreconf Missing Files

**Problem**: `autoreconf: Entering directory '.'` fails with missing m4 files.

**Solution**: autoreconf -fiv should handle this automatically. If not:
```bash
autopoint --force
autoreconf -fiv
```

## Platform-Specific Notes

### Mac to Pi

- Line endings: No issues (both use LF)
- Permissions: Generally preserved with rsync
- rsync: Available natively

### Windows to Pi

- Line endings: MUST fix CRLF to LF
- Permissions: Lost during scp, must restore
- rsync: Use WSL or install via chocolatey

## Quick Reference Commands

```bash
# One-liner: Transfer and build
rsync -avz --delete --exclude='.git' --exclude='*.o' /path/to/motion/ admin@192.168.1.176:~/motion/ && ssh admin@192.168.1.176 "cd ~/motion && autoreconf -fiv && ./configure --with-libcam --with-sqlite3 && make -j4"

# Check if motion is running
ssh admin@192.168.1.176 "pgrep -a motion"

# Quick log check
ssh admin@192.168.1.176 "tail -50 /var/log/motion/motion.log"
```

## Dependencies on Pi 5 Bookworm

Ensure these are installed:
```bash
sudo apt install -y \
  libcamera-dev libcamera-tools \
  libjpeg-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libavdevice-dev \
  libmicrohttpd-dev \
  libsqlite3-dev \
  libwebp-dev \
  autoconf automake libtool pkgconf \
  gettext
```
