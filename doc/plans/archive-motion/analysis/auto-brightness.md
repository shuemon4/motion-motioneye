Brightness and Contrast Settings Analysis

  1. Yes, remove auto_brightness from documentation

  The auto_brightness section should be removed from:
  - man/motion.1 (lines 341-353)
  - doc/motion_config.html (if present)

  2. Brightness and Contrast Exposure

  Current Status: Brightness and contrast settings are exposed, but through a different 
  mechanism than the old direct parameters.

  How They're Exposed Now

  Old Way (Deprecated):
  brightness 50
  contrast 75

  New Way (Current):
  # For V4L2 cameras (USB webcams)
  v4l2_params brightness=50,contrast=75

  # For libcamera (Pi Camera Module v3)
  libcam_params Brightness=0.5,Contrast=1.2

  The old brightness and contrast parameters were deprecated and replaced with:
  - V4L2 devices: v4l2_params (see src/conf.cpp:282,286)
  - libcamera devices: libcam_params (supports Brightness and Contrast controls at
  src/libcam.cpp:385,388)

  3. Hot Reload Status: NOT SUPPORTED

  Answer: These parameters require a restart - they do NOT support hot reload.

  Evidence from Code

  From src/conf.cpp:70,77:
  /* Category 02 - Source parameters - NOT hot reloadable (device changes) */
  {"v4l2_params",    PARM_TYP_PARAMS, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},  // ← 
  false = no hot reload
  {"libcam_params",  PARM_TYP_PARAMS, PARM_CAT_02, PARM_LEVEL_ADVANCED, false},  // ← 
  false = no hot reload

  The 5th parameter (false) indicates these are not hot-reloadable.

  Why They Require Restart

  For libcamera (src/libcam.cpp:829-835):
  int cls_libcam::start_capture()
  {
      retcd = camera->start(&this->controls);  // Controls passed at camera start
      if (retcd) {
          return -1;
      }
      controls.clear();  // Controls cleared immediately after startup
      ...
  }

  The libcamera controls (including Brightness/Contrast) are:
  1. Parsed from libcam_params during initialization (config_controls() at line 534)
  2. Applied once when the camera starts (camera->start(&this->controls) at line 829)
  3. Immediately cleared (line 835)
  4. Cannot be changed without restarting the camera

  For V4L2: Similar pattern - controls are set during device initialization and not
  updated at runtime.

  Summary Table

  | Setting    | Old Parameter           | New Parameter                | Hot Reload?
           |
  |------------|-------------------------|------------------------------|----------------
  ---------|
  | Brightness | brightness (deprecated) | v4l2_params or libcam_params | ❌ No -
  requires restart |
  | Contrast   | contrast (deprecated)   | v4l2_params or libcam_params | ❌ No -
  requires restart |
  | Resolution | width, height           | Same                         | ❌ No -
  requires restart |

  Impact on MotionEye

  MotionEye users who want to adjust brightness/contrast will need to:
  1. Update the configuration parameter (v4l2_params or libcam_params)
  2. Restart the camera (or entire Motion daemon)
  3. Changes cannot be made in real-time like some other parameters (threshold, text
  overlays, etc.)

  This is listed in doc/project/Update-Motion-MotionEye.md:134 as a removed parameter
  that MotionEye needs to handle differently.

Feasibility Check: Hot Brightness/Contrast for libcamera (Pi Camera v3)

  1) libcamera control support (RPi* pipeline)
     - libcamera exposes standard controls::Brightness (float, ~-1.0 to 1.0, default 0.0)
       and controls::Contrast (float, ~0.0 to 32.0, default 1.0) on the RPi pipeline
       used by Pi 5 + Camera Module v3 (mirrors the rpicam-* CLI flags).
     - Controls can be changed while streaming via Camera::setControls or by setting
       controls on queued Requests; no reconfigure/restart is required and cost is a
       single control list submission (minimal CPU).

  2) How Motion applies them today
     - Parsed once in libcam_params: see src/libcam.cpp config_control_item()
       where Brightness/Contrast map to controls::Brightness/Contrast.
     - Applied only at startup: controls are passed to camera->start(&controls)
       in start_capture() (src/libcam.cpp:821) and immediately cleared; there is no
       control submission after streaming begins.
     - Config gating: libcam_params is marked non-hot in src/conf.cpp (PARM_CAT_02,
       hot_reload=false), so /config/set refuses runtime updates.

  3) Conclusion: Hot updates are possible
     - The libcamera API and RPi pipeline both allow live brightness/contrast changes.
     - Current Motion plumbing blocks it only because libcam_params is treated as a
       startup-only blob and controls are never resent. No hardware/driver blocker.

  4) Low-CPU implementation outline
     - Add a tiny runtime-control path in cls_libcam:
         * Track pending brightness/contrast in a lightweight struct/atomics.
         * In libcam::next() (hot path) check a dirty flag; if set, build a small
           ControlList with only changed fields and call camera->setControls().
           (One branch + occasional control submit; negligible CPU compared to copy.)
     - Expose parameters without making all libcam_params hot:
         * Option A: New hot parameters libcam_brightness/libcam_contrast (PARM_CAT_02,
           hot_reload=true) that map directly to that runtime-control path.
         * Option B: Keep libcam_params as-is but allow /config/set on it only for
           Brightness/Contrast by parsing the string and ignoring other keys; still
           feed the runtime-control path.
     - Keep startup behavior: initial values still honored from libcam_params during
       start_config(), so existing configs continue to work.
     - Threading: to avoid cross-thread calls, have the camera thread (libcam::next)
       consume the pending values; updates set the dirty flag under a mutex/atomic
       from the config/web thread, keeping the change in-line with existing patterns.

  5) Files to touch if we proceed
     - src/conf.cpp: mark the new params hot and wire edit_get/set.
     - src/parm_structs.hpp + defaults: add storage for the two floats.
     - src/webu_json.cpp: allow /config/set to pass them through at runtime.
     - src/libcam.cpp/.hpp: add the pending-control handling and a setter callable
       from config changes; keep control submissions rare to protect CPU.
