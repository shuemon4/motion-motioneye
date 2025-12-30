# libcam "Not Available" - Root Cause Analysis

## Date: 2025-12-29

## Issue
Motion binary reported `libcam not available` error on Pi 5, preventing camera access despite:
- libcamera installed and functional on Pi (`rpicam-hello` works)
- Camera hardware detected
- `--with-libcam` flag used in configure
- `libcam_device 0` set in motion.conf

## Root Cause

**config.hpp was generated on Mac (where libcamera doesn't exist) and synced to Pi, causing libcamera support to be excluded from the build.**

### Evidence

1. **Mac-specific config.hpp synced to Pi:**
   ```
   # config.hpp line 60
   /* #undef HAVE_LIBCAM */

   # Lines 19-21 show Mac CoreFoundation
   #define HAVE_CFLOCALECOPYCURRENT 1
   #define HAVE_CFPREFERENCESCOPYAPPVALUE 1
   ```

2. **This matched the warning in MOTION_RESTART.md:**
   > ## Common Issue: Misleading Mac Paths in Makefile
   > This happens when:
   > 1. You ran `./configure` on your Mac (which writes Mac paths to Makefile)
   > 2. You synced the entire project (including Makefile) to the Pi

3. **Binary lacked libcamera linkage:**
   ```bash
   ldd /usr/local/bin/motion | grep camera  # No libcamera found
   strings /usr/local/bin/motion | grep libcam  # Had config strings but no actual code
   ```

## Why This Kept Occurring

The issue persisted because of the **selective update workflow**:

1. Developer edits source on Mac
2. Runs `rsync` to sync entire project to Pi (including config.hpp)
3. Runs `make -j4` on Pi
4. Make sees source is newer than object files, so it rebuilds
5. **BUT**: make uses the Mac-generated config.hpp which has `/* #undef HAVE_LIBCAM */`
6. Compiler conditionally excludes libcamera code due to `#ifndef HAVE_LIBCAM`
7. Linker doesn't link libcamera libraries

The libcamera code in `src/libcam.cpp` was compiled but essentially disabled:
```cpp
#ifdef HAVE_LIBCAM
    // Actual camera code
#else
    MOTLOG(NTC, TYPE_VIDEO, NO_ERRNO, "libcam not available");
#endif
```

## Solution

**Reconfigure on the Pi to generate Pi-native config.hpp:**

```bash
cd ~/motion-motioneye
make clean
autoreconf -fiv
./configure --with-libcam --with-sqlite3
make -j4
sudo make install
```

### Verification After Fix

1. **config.hpp now has libcamera enabled:**
   ```
   #define HAVE_LIBCAM 1
   ```

2. **Binary links libcamera:**
   ```
   LIBS:  ... -lcamera -lcamera-base ...
   ```

3. **Camera functionality confirmed:**
   ```
   [NTC][VID][cl01] libcam_start: Camera started
   [NTC][ALL][cl01] init: Camera 1 started: motion detection enabled
   [NTC][ALL][cl01] detected_trigger: Motion detected - starting event 1
   ```

## Prevention

### Option 1: Don't Sync Config Files (Recommended)

Update rsync to exclude configure-generated files:

```bash
rsync -avz \
  --exclude='.git' \
  --exclude='node_modules' \
  --exclude='config.hpp' \
  --exclude='config.log' \
  --exclude='config.status' \
  --exclude='Makefile' \
  /path/to/motion-motioneye/ admin@192.168.1.176:~/motion-motioneye/
```

Then always reconfigure on Pi after syncing:
```bash
ssh admin@192.168.1.176 "cd ~/motion-motioneye && autoreconf -fiv && ./configure --with-libcam --with-sqlite3 && make -j4"
```

### Option 2: Configure Check Script

Add to deployment scripts:

```bash
#!/bin/bash
# Check if config.hpp was generated on this system
if grep -q "CFLOCALE" ~/motion-motioneye/config.hpp 2>/dev/null; then
    echo "ERROR: config.hpp appears to be from Mac - reconfiguring..."
    cd ~/motion-motioneye
    autoreconf -fiv
    ./configure --with-libcam --with-sqlite3
fi
```

### Option 3: Build-Time Verification

Add to Makefile or CI:

```bash
# Before building, verify libcamera is enabled
grep -q "define HAVE_LIBCAM 1" config.hpp || \
  { echo "ERROR: libcamera not enabled in config.hpp"; exit 1; }
```

## Related Documentation

- `docs/project/MOTION_RESTART.md` - Section "Common Issue: Misleading Mac Paths in Makefile"
- Motion configure output shows: `checking for LIBCAM... yes` when correctly configured on Pi
- Motion configure output shows: `libcamera: yes(0.6.0)` in summary

## Lessons Learned

1. **Cross-platform development requires platform-specific configuration**
   - Don't sync autotools-generated files (config.hpp, Makefile, config.status)
   - Always run configure on the target platform

2. **Build system doesn't validate runtime capabilities**
   - Make doesn't check if HAVE_LIBCAM is correct for the target
   - Motion compiles successfully without libcamera (just logs warning at runtime)

3. **Development workflow must account for platform differences**
   - Mac lacks libcamera → configure disables it
   - Pi has libcamera → must run configure on Pi to enable it

4. **Incremental builds can mask configuration issues**
   - `make` rebuilds changed sources but doesn't recheck dependencies
   - `make clean` forces full rebuild but still uses existing config.hpp
   - Only `./configure` regenerates config.hpp with current platform detection
