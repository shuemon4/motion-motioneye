# Motion Restart Procedures

## Common Issue: Stale Motion Processes

Motion processes can become "stuck" after binary updates, causing old code to continue running even after `sudo make install`. This manifests as:
- Changes to C++ code not taking effect
- API endpoints returning old responses
- New features not available

## Root Cause

When Motion is started in the background and a new binary is installed via `sudo make install`, the running process continues using the old in-memory code. Simply installing a new binary does NOT automatically restart the running process.

## Detection

Check if the running process started before the binary was updated:

```bash
# Check binary timestamp
ls -lh /usr/local/bin/motion

# Check running process start time
ps aux | grep motion | grep -v grep
```

If the process START time is earlier than the binary modification time, you're running old code.

## Resolution

### Step 1: Kill All Motion Processes

```bash
# Force kill all motion processes (as root)
ssh admin@192.168.1.176 "sudo pkill -9 motion"

# Verify all processes are dead
ssh admin@192.168.1.176 "ps aux | grep motion | grep -v grep"
# Should return nothing (exit code 1)
```

### Step 2: Start New Binary

```bash
# Start motion in background with debug logging
ssh admin@192.168.1.176 "sudo /usr/local/bin/motion -c /etc/motion/motion.conf -d -n 2>&1 | tee /tmp/motion.log &"

# Wait for startup
sleep 5

# Verify it's running
ssh admin@192.168.1.176 "ps aux | grep motion | grep -v grep"
```

### Step 3: Verify New Code is Active

Test an endpoint that should show new behavior:

```bash
# Example: Check for csrf_token in config response
ssh admin@192.168.1.176 "curl -s 'http://localhost:7999/0/api/config' | head -c 300"
```

## Prevention

### Option 1: Systemd Service (Recommended)

Create a systemd service that can be easily restarted:

```bash
sudo systemctl stop motion
sudo make install
sudo systemctl start motion
```

### Option 2: Manual Restart Script

Create a script `restart-motion.sh`:

```bash
#!/bin/bash
sudo pkill -9 motion
sleep 2
sudo /usr/local/bin/motion -c /etc/motion/motion.conf -d -n &
```

## Common Mistakes

1. **Using `pkill` without `-9`**: Regular signals may not kill the process immediately
2. **Not waiting after kill**: Start new process too quickly before old one is fully terminated
3. **Wrong binary path**: Using `motion` instead of `/usr/local/bin/motion` may run a different version
4. **Assuming install restarts**: `make install` only copies files, doesn't restart services

## Verification Checklist

- [ ] All old processes killed (`ps aux | grep motion` returns nothing)
- [ ] New binary timestamp is recent (`ls -lh /usr/local/bin/motion`)
- [ ] New process started (`ps aux | grep motion` shows running process)
- [ ] API endpoints return expected responses
- [ ] Process start time is AFTER binary modification time

---

## Common Issue: Make Showing "Nothing to be done"

### Symptom

After syncing source files to the Pi, running `make -j4` shows:
```
make[2]: Nothing to be done for 'all'.
```

This happens even when source files have been updated.

### Root Cause

`rsync` preserves file timestamps by default. If you sync files from your Mac to the Pi:
1. The file's modification timestamp is preserved
2. Make compares timestamps and sees the `.o` file is newer than the source
3. Make concludes no rebuild is needed

### Resolution

Force rebuild by touching the modified source files:

```bash
ssh admin@192.168.1.176 "cd ~/motion-motioneye && touch src/conf.cpp && make -j4"
```

Or use `rsync` without timestamp preservation:
```bash
rsync -avz --no-times --exclude='.git' --exclude='node_modules' \
  /path/to/file admin@192.168.1.176:~/motion-motioneye/src/
```

### Prevention

Always touch modified source files after rsync when doing incremental syncs.

---

## Common Issue: Misleading Mac Paths in Makefile

### Symptom

When building on Pi, you see Mac paths in compiler output:
```
-I/opt/homebrew/opt/jpeg/include -I/opt/homebrew/Cellar/libmicrohttpd/1.0.2/include
```

This looks like cross-compilation, but the binary still works.

### Explanation

This happens when:
1. You ran `./configure` on your Mac (which writes Mac paths to Makefile)
2. You synced the entire project (including Makefile) to the Pi
3. Building on Pi uses the synced Makefile with Mac paths

**The binary still works because:**
- The Pi's g++ ignores non-existent include paths
- System headers are found in default Pi locations
- The linker finds Pi-native libraries

### Verification

Check the compiled binary architecture:
```bash
file ~/motion-motioneye/src/motion
# Should show: ELF 64-bit LSB pie executable, ARM aarch64
```

### Resolution (if needed)

If you have actual build issues, reconfigure on the Pi:

```bash
ssh admin@192.168.1.176 "cd ~/motion-motioneye && make clean && autoreconf -fiv && ./configure --with-libcam --with-sqlite3 && make -j4"
```

This creates a Makefile with Pi-native paths.

### When to Reconfigure

Reconfiguration is only needed when:
- Adding new library dependencies
- Library detection fails on Pi
- Build errors reference missing headers

For typical source code changes (adding parameters, fixing bugs), the synced Mac-configured Makefile works fine.
