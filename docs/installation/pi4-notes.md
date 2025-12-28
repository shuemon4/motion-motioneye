# Pi 4 Motion Installation Notes

**Date**: 2025-12-23
**Target**: Raspberry Pi 4 (192.168.1.246)
**OS**: Fresh Debian GNU/Linux 13 (Trixie) 64-bit - RPi OS Lite
**Kernel**: 6.12.47+rpt-rpi-v8
**Camera**: IMX219 (Pi Camera v2)

---

## Installation Timeline

### Step 1: Initial SSH Connection
**Action**: Attempt SSH connection to fresh Pi 4
**Command**: `ssh admin@192.168.1.246 "uname -a"`
**Result**: L **Problem - Host key changed error**
**Cause**: Fresh OS installation changed the SSH host key
**Fix Required**: Remove old host key from known_hosts

---

### Step 2: Remove Old SSH Host Key
**Action**: Remove old host key and accept new one
**Command**: `ssh-keygen -R 192.168.1.246`
**Result**:  **Successful** - Old host key removed
**Follow-up Issue**: L SSH key authentication not configured yet (fresh OS has no authorized_keys)

---

### Step 3: Password-Based SSH Setup
**Action**: Use password authentication to set up SSH keys
**Password**: `wwadmin` (provided by user)
**Commands**:
```bash
sshpass -p "wwadmin" ssh admin@192.168.1.246 "mkdir -p ~/.ssh && chmod 700 ~/.ssh"
cat ~/.ssh/id_ed25519.pub | sshpass -p "wwadmin" ssh admin@192.168.1.246 "cat >> ~/.ssh/authorized_keys && chmod 600 ~/.ssh/authorized_keys"
```
**Result**:  **Successful** - SSH key-based authentication configured

---

### Step 4: Verify System Information
**Action**: Confirm OS version and architecture
**Command**: `ssh admin@192.168.1.246 "uname -a && lsb_release -a"`
**Result**:  **Successful**
**Output**:
```
Linux pi4-motion 6.12.47+rpt-rpi-v8 #1 SMP PREEMPT Debian 1:6.12.47-1+rpt1 (2025-09-16) aarch64 GNU/Linux
Distributor ID: Debian
Description:    Debian GNU/Linux 13 (trixie)
Release:        13
Codename:       trixie
```

---

### Step 5: Install Build Dependencies (Initial Attempt)
**Action**: Install core build dependencies
**Command**:
```bash
sudo apt update && sudo apt install -y \
  libcamera-dev libcamera-tools libjpeg-dev \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev \
  libmicrohttpd-dev libsqlite3-dev autoconf automake libtool \
  pkgconf gcc g++ make
```
**Result**:  **Successful**
**Notes**:
- Total installed: 220 packages (158 MB download, 572 MB disk space)
- Many packages already present: pkgconf, gcc, g++, make
- New packages included GTK3, Qt6, Mesa drivers (dependencies of libcamera-dev)

---

### Step 6: Transfer Motion Code
**Action**: rsync Motion source from Mac to Pi 4
**Command**:
```bash
rsync -avz --delete \
  --exclude='.git' \
  --exclude='*.o' \
  --exclude='*.lo' \
  --exclude='.libs' \
  --exclude='autom4te.cache' \
  /Users/tshuey/Documents/GitHub/motion/ admin@192.168.1.246:~/motion/
```
**Result**:  **Successful**
**Details**: 302 files transferred (1.36 MB), 3.97x speedup from compression

---

### Step 7: Run autoreconf (First Attempt)
**Action**: Generate configure script
**Command**: `cd ~/motion && autoreconf -fiv`
**Result**: L **Problem - autopoint missing**
**Error**: `Can't exec "autopoint": No such file or directory`
**Cause**: gettext package not fully installed

---

### Step 8: Install gettext
**Action**: Install gettext package
**Command**: `sudo apt install -y gettext`
**Result**:  **Successful** (1.6 MB download, 8.2 MB disk space)
**Follow-up Issue**: L Still failed - autopoint still missing

---

### Step 9: Install autopoint Separately
**Action**: Install autopoint package explicitly
**Command**: `sudo apt install -y autopoint`
**Result**:  **Successful** (770 KB download, 802 KB disk space)

---

### Step 10: Run autoreconf (Second Attempt)
**Action**: Retry autoreconf after autopoint installation
**Command**: `cd ~/motion && autoreconf -fiv`
**Result**:  **Successful**
**Output**:
```
autoreconf: running: autopoint --force
autoreconf: running: aclocal --force -I m4
autoreconf: running: /usr/bin/autoconf --force
autoreconf: running: /usr/bin/autoheader --force
autoreconf: running: automake --add-missing --copy --force-missing
```

---

### Step 11: Run configure (First Attempt)
**Action**: Configure build with libcamera and SQLite3
**Command**: `./configure --with-libcam --with-sqlite3`
**Result**: L **Problem - zlib missing**
**Error**: `Required package zlib not found`

---

### Step 12: Install zlib Development Package
**Action**: Install zlib1g-dev
**Command**: `sudo apt install -y zlib1g-dev`
**Result**:  **Successful** (917 KB download, 1.3 MB disk space)

---

### Step 13: Run configure (Second Attempt)
**Action**: Retry configure with zlib installed
**Command**: `./configure --with-libcam --with-sqlite3`
**Result**: L **Problem - libavdevice-dev missing**
**Error**: `Required ffmpeg packages 'libavutil-dev libavformat-dev libavcodec-dev libswscale-dev libavdevice-dev' were not found`
**Cause**: libavdevice-dev was not in the initial dependency install list

---

### Step 14: Install libavdevice-dev
**Action**: Install missing FFmpeg development package
**Command**: `sudo apt install -y libavdevice-dev`
**Result**:  **Successful**
**Details**: 46 additional packages installed (55.4 MB download, 120 MB disk space)
- New dependencies: libass9, libpulse0, libavfilter10, libavdevice61
- Audio libraries: ALSA, JACK, OpenAL, PulseAudio
- Video filters and processing libraries

---

### Step 15: Run configure (Third Attempt - Final)
**Action**: Retry configure with all dependencies
**Command**: `./configure --with-libcam --with-sqlite3`
**Result**:  **Successful**
**Key Output**:
```
checking for LIBCAM... yes
checking for FFmpeg... yes
configure: Security hardening flags enabled
```
**Detected Libraries**:
- libcamera: Yes
- SQLite3: Yes
- FFmpeg (libavutil, libavformat, libavcodec, libswscale, libavdevice): Yes
- microhttpd: Yes
- JPEG: Yes
- zlib: Yes
- V4L2: Yes (headers present)
- WebP: No
- PulseAudio: No
- ALSA: No
- FFTW3: No

---

### Step 16: Build Motion
**Action**: Compile Motion with 4 parallel jobs
**Command**: `make -j4`
**Result**:  **Successful**
**Build Time**: ~2-3 minutes
**Binary Size**: 16 MB (`/home/admin/motion/src/motion`)
**Notes**: All source files compiled without errors, message catalogs generated for 14 languages

---

### Step 17: Verify Binary
**Action**: Test compiled binary
**Command**: `~/motion/src/motion -h`
**Result**:  **Successful**
**Output**: `Motion version 5.0.0-gitUNKNOWN, Copyright 2020-2025`

---

### Step 18: Install Motion to System
**Action**: Install Motion and documentation to system directories
**Command**: `sudo make install`
**Result**:  **Successful**
**Installation Locations**:
- Binary: `/usr/local/bin/motion`
- Docs: `/usr/local/share/doc/motion/`
- Config templates: `/usr/local/var/lib/motion/`
- Man page: `/usr/local/share/man/man1/motion.1`
- Translations: `/usr/local/share/locale/*/LC_MESSAGES/motion.mo`

---

### Step 19: Verify Camera Detection
**Action**: List available cameras using rpicam-hello
**Command**: `rpicam-hello --list-cameras`
**Result**:  **Successful**
**Detected Camera**:
```
0 : imx219 [3280x2464 10-bit RGGB] (/base/soc/i2c0mux/i2c@1/imx219@10)
    Modes: 'SRGGB10_CSI2P' : 640x480 [103.33 fps]
                             1640x1232 [41.85 fps]
                             1920x1080 [47.57 fps]
                             3280x2464 [21.19 fps]
           'SRGGB8' : (same resolutions as above)
```
**Note**: Camera is Pi Camera v2 (IMX219 sensor)

---

### Step 20: Create Motion Configuration
**Action**: Copy default config to /etc/motion/
**Command**: `sudo mkdir -p /etc/motion && sudo cp /usr/local/var/lib/motion/motion-dist.conf /etc/motion/motion.conf`
**Result**:  **Successful**

---

### Step 21: Test Motion Startup (First Attempt)
**Action**: Run Motion in foreground with debug logging
**Command**: `timeout 5 /usr/local/bin/motion -c /etc/motion/motion.conf -n -d 6`
**Result**:   **Partial Success - Multiple Issues**
**Problems Encountered**:
1. L `Invalid value combined` - Unknown config parameter
2. L Camera config file not found: `/usr/local/var/lib/motion/camera1.conf`
3. L Database permission error: Unable to open `/usr/local/var/lib/motion/motion.db`
4. L Port 8081 conflict (secondary webserver)
5. L `Unable to determine camera type` - No camera device specified
6. L `No Camera device specified`

---

### Step 22: Fix Database Permissions
**Action**: Change ownership of Motion data directory
**Command**: `sudo chown -R admin:admin /usr/local/var/lib/motion`
**Result**:  **Successful**

---

### Step 23: Attempt to Add Camera ID
**Action**: Add camera_id parameter to config
**Command**: `sudo bash -c 'echo "camera_id 0" >> /etc/motion/motion.conf'`
**Result**: L **Problem - Invalid config parameter**
**Error**: `Unknown config option "camera_id"`
**Cause**: Parameter name is incorrect for libcamera setup

---

### Step 24: Configuration Status (Incomplete)
**Current State**:   **Requires Camera Configuration**
**Remaining Issues**:
1. Camera device not specified - needs proper libcam configuration
2. Camera config file approach unclear (camera1.conf vs inline config)
3. No documentation yet on proper libcamera device specification

**Next Steps Required**:
1. Determine correct libcamera configuration parameters
2. Create working camera1.conf or use inline configuration
3. Verify Motion can acquire video stream from IMX219
4. Test motion detection functionality

---

## Summary of Required Packages (Missing from Initial List)

### Essential Build Dependencies
```bash
# Core tools
autopoint                    # Required for autoreconf (not in initial install)
gettext                      # Required for internationalization
zlib1g-dev                   # Required for compression support
libavdevice-dev              # Required for FFmpeg device input (missing from initial list)
```

### Complete Dependency List for Fresh Install
```bash
sudo apt update && sudo apt install -y \
  autoconf \
  automake \
  autopoint \
  gcc \
  g++ \
  gettext \
  libtool \
  make \
  pkgconf \
  libavcodec-dev \
  libavdevice-dev \
  libavformat-dev \
  libavutil-dev \
  libswscale-dev \
  libcamera-dev \
  libcamera-tools \
  libjpeg-dev \
  libmicrohttpd-dev \
  libsqlite3-dev \
  zlib1g-dev
```

**Total Additional Packages**: ~270 packages with dependencies
**Total Download Size**: ~215 MB
**Total Disk Space Required**: ~700 MB

---

## Lessons Learned

### Installation Process
1. **SSH Setup Critical First**: Fresh OS requires password-based initial setup for SSH keys
2. **Dependency Chain Complex**: Build tools require multiple rounds of package installation
3. **FFmpeg Packages Incomplete**: libavdevice-dev not included in typical "ffmpeg dev" package groups
4. **gettext Split Packages**: autopoint not included in base gettext package on Debian Trixie

### Build Process
1. **Build Successful Without Issues**: Once dependencies resolved, compilation clean
2. **Binary Size Acceptable**: 16 MB for full-featured motion detection daemon
3. **No Compiler Warnings**: Code builds cleanly on aarch64 with GCC 14.2.0

### Configuration Challenges
1. **libcamera Device Spec Unclear**: Standard camera configuration parameters not working
2. **Documentation Gap**: No clear guide for libcamera-specific setup on fresh Pi
3. **Permission Issues**: Default install creates directories not writable by motion user

### Camera Hardware
1. **Detection Works**: rpicam-hello successfully detects IMX219
2. **Multiple Modes Available**: Camera supports 640x480 to 3280x2464
3. **Frame Rates Good**: 47.57 fps @ 1920x1080, up to 103 fps @ 640x480

---

## Recommended Installation Script

```bash
#!/bin/bash
# Motion Installation Script for Raspberry Pi 4 (Trixie 64-bit)

set -e

echo "Installing build dependencies..."
sudo apt update
sudo apt install -y \
  autoconf automake autopoint gcc g++ gettext libtool make pkgconf \
  libavcodec-dev libavdevice-dev libavformat-dev libavutil-dev libswscale-dev \
  libcamera-dev libcamera-tools libjpeg-dev libmicrohttpd-dev \
  libsqlite3-dev zlib1g-dev

echo "Building Motion..."
cd ~/motion
autoreconf -fiv
./configure --with-libcam --with-sqlite3
make -j4

echo "Installing Motion..."
sudo make install

echo "Setting up permissions..."
sudo mkdir -p /etc/motion
sudo mkdir -p /usr/local/var/lib/motion
sudo chown -R $USER:$USER /usr/local/var/lib/motion

echo "Copying default configuration..."
sudo cp /usr/local/var/lib/motion/motion-dist.conf /etc/motion/motion.conf

echo "Installation complete. Configure camera in /etc/motion/motion.conf"
echo "Detected cameras:"
rpicam-hello --list-cameras
```

---

## Open Questions for Testing Phase

1. What is the correct parameter name for specifying libcamera device ID?
2. Should camera configuration be in separate file (camera1.conf) or inline in motion.conf?
3. Does `netcam_url` parameter work with libcamera device IDs (e.g., `netcam_url 0`)?
4. What libcamera-specific parameters are available in Motion config?
5. How to specify camera resolution/framerate for libcamera devices?
6. Does Motion auto-detect libcamera if no device specified but libcamera present?

---

## Status: Ready for MotionEye Integration Testing

 Motion binary compiled and installed
 Camera hardware detected by system
 Database directory permissions fixed
  Camera configuration incomplete - requires MotionEye integration testing to determine proper parameters
