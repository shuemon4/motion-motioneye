I'll deploy Motion to the Pi 4 for you. Let me start by checking if the Pi is
  powered on and accessible, then proceed with the deployment.

⏺ Bash(ssh -o ConnectTimeout=5 admin@192.168.1.246 "echo 'Pi 4 is online'" 2>&1)
  ⎿  Pi 4 is online

⏺ Great! Pi 4 is online. Now I'll deploy Motion to it. I'll transfer the code,
  build it, and verify the result.

⏺ Bash(rsync -avz --delete \
        --exclude='.git' \…)
  ⎿  src/webu_ans.hpp              
     src/webu_file.cpp
     src/webu_file.hpp
     … +17 lines (ctrl+o to expand)

⏺ Transfer complete! Now building on Pi 4:

⏺ Bash(ssh admin@192.168.1.246 << 'EOF'
      cd ~/motion…)
  ⎿ Linux pi4-motion 6.12.47+rpt-rpi-v8 #1 SMP PREEMPT Debian 1:6.12.47-1+rpt1~
    bookworm (2025-09-16) aarch64
    … +8 lines (ctrl+o to expand)


⏺ I see - autoreconf is not installed on Pi 4. Let me check what build tools are
   available and install the necessary autotools:

⏺ Bash(ssh admin@192.168.1.246 "sudo apt-get update && sudo apt-get install -y
      autoconf automake libtool pkgconf 2>&1 | tail -20")
  ⎿ Hit:1 http://archive.raspberrypi.com/debian bookworm InRelease             
    Get:2 http://raspbian.raspberrypi.com/raspbian bookworm InRelease [15.0 kB]
    Fetched 15.0 kB in 1s (18.2 kB/s)
    … +25 lines (ctrl+o to expand)


⏺ Good! Autotools installed. Now let's build Motion:

⏺ Bash(ssh admin@192.168.1.246 << 'EOF'
      cd ~/motion…)
  ⎿ Linux pi4-motion 6.12.47+rpt-rpi-v8 #1 SMP PREEMPT Debian 1:6.12.47-1+rpt1~
    bookworm (2025-09-16) aarch64
    … +9 lines (ctrl+o to expand)


⏺ Missing autopoint. Let me install gettext:

⏺ Bash(ssh admin@192.168.1.246 "sudo apt-get install -y gettext 2>&1 | grep -E
      '(Setting up|Processing)' | tail -5")
  ⎿  Setting up gettext (0.21-12) ...                           
     Processing triggers for man-db (2.11.2-2) ...
     Processing triggers for install-info (6.8-6+b1) ...
     Processing triggers for libc-bin (2.36-9+rpt2+deb12u13) ...

⏺ Now let's try autoreconf again:

⏺ Bash(ssh admin@192.168.1.246 "cd ~/motion && autoreconf -fiv 2>&1 | tail -15")
  ⎿  autoreconf: export WARNINGS=          
     autoreconf: Entering directory '.'
     autoreconf: running: autopoint --force
     … +3 lines (ctrl+o to expand)

⏺ Let me check what's in configure.ac that requires autopoint and try a
  workaround:

⏺ Bash(ssh admin@192.168.1.246 "cd ~/motion && autoreconf -fiv --install 2>&1 | tail
       -20")
  ⎿  autoreconf: export WARNINGS=          
     autoreconf: Entering directory '.'
     autoreconf: running: autopoint --force
     … +3 lines (ctrl+o to expand)

⏺ Let me check if we have a pre-built configure script and try using that
  instead:

⏺ Bash(ssh admin@192.168.1.246 "ls -la ~/motion/configure 2>&1 | head -5 && echo
      '---' && cd ~/motion && ./configure --with-libcam --with-sqlite3 2>&1 | head
      -20")
  ⎿  -rwxr-xr-x 1 admin admin 337268 Dec 22 14:38 /home/admin/motion/configure
     ---
     checking for a BSD-compatible install... /usr/bin/install -c
     … +19 lines (ctrl+o to expand)

⏺ Excellent! The configure script exists. Let me continue with the build:

⏺ Bash(ssh admin@192.168.1.246 "cd ~/motion && ./configure --with-libcam
      --with-sqlite3 2>&1 | tail -30")
  ⎿  checking for sys/time.h... yes 
     checking for sys/wait.h... yes
     checking for sys/ioctl.h... yes
     … +28 lines (ctrl+o to expand)

⏺ Missing libjpeg. Let me install the required dependencies:

⏺ Bash(ssh admin@192.168.1.246 "sudo apt-get install -y libjpeg-dev libavcodec-dev
      libavformat-dev libavutil-dev libswscale-dev libmicrohttpd-dev libsqlite3-dev
      libcam…)
  ⎿  Setting up libavformat-dev:armhf (8:5.1.8-0+deb12u1+rpt1) ...
     Setting up libavfilter8:armhf (8:5.1.8-0+deb12u1+rpt1) ...
     Setting up libswscale-dev:armhf (8:5.1.8-0+deb12u1+rpt1) ...
     … +7 lines (ctrl+o to expand)

⏺ Now let's run configure again and then build:

⏺ Bash(ssh admin@192.168.1.246 << 'EOF'
      cd ~/motion…)
  ⎿ Linux pi4-motion 6.12.47+rpt-rpi-v8 #1 SMP PREEMPT Debian 1:6.12.47-1+rpt1~
    bookworm (2025-09-16) aarch64
    … +29 lines (ctrl+o to expand)


⏺ FFmpeg packages still missing. Let me check what was actually installed and
  install the missing dev packages:

⏺ Bash(ssh admin@192.168.1.246 "sudo apt-get install -y libavdevice-dev 2>&1 | tail
      -5")
  ⎿  Preparing to unpack .../libavdevice-dev_8%3a5.1.8-0+deb12u1+rpt1_armhf.deb
     ...
     Unpacking libavdevice-dev:armhf (8:5.1.8-0+deb12u1+rpt1) ...
     … +3 lines (ctrl+o to expand)

⏺ Now let's try configure once more and then make:

⏺ Bash(ssh admin@192.168.1.246 << 'EOF'
      cd ~/motion…)
  ⎿ Linux pi4-motion 6.12.47+rpt-rpi-v8 #1 SMP PREEMPT Debian 1:6.12.47-1+rpt1~
    bookworm (2025-09-16) aarch64
    … +13 lines (ctrl+o to expand)


⏺ Great! Configure succeeded. Now let's build:

⏺ Bash(ssh admin@192.168.1.246 "cd ~/motion && make -j4 2>&1 | tail   timeout: 10m 
      -30")                                                           0s
  ⎿  da.po: 755 translated messages.                              
     de.po: 755 translated messages.
     es.po: fi.po: 755 translated messages755 translated messages.
     … +27 lines (ctrl+o to expand)

⏺ Build looks good! Let me verify the binary was created:

⏺ Bash(ssh admin@192.168.1.246 "ls -lh ~/motion/src/motion && ~/motion/src/motion
      --help 2>&1 | head -10")
  ⎿  -rwxr-xr-x 1 admin admin 15M Dec 22 19:58 /home/admin/motion/src/motion
     /home/admin/motion/src/motion: invalid option -- '-'
     Motion version 5.0.0-dirty-20251222-456a465, Copyright 2020-2025
     … +8 lines (ctrl+o to expand)

⏺ Perfect! Deployment complete. Here's the summary:

  /pi deploy (Pi 4)

  Target: admin@192.168.1.246 (Pi 4)

  Transfer

  ✅ Code transferred (1.4 MB, 26x compression)

  Build

  ✅ Dependencies installed (autoconf, automake, libtool, FFmpeg, libcamera,
  etc.)
  ✅ Configure successful (libcamera detected)
  ✅ Make completed (translations compiled)

  Result

  Motion binary: ~/motion/src/motion (15 MB)
  Version: 5.0.0-dirty-20251222-456a465
  Status: Ready for testing

  Motion is now built on Pi 4 and ready to run. The binary supports libcamera,
  SQLite, and FFmpeg recording.