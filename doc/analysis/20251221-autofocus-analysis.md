# Auto-Focus Feature Analysis Report

**Date:** 2025-12-21
**Analyst:** Claude Code
**Scope:** libcamera autofocus implementation in Motion

## Executive Summary

The Motion project has **partial but incomplete** autofocus implementation. The controls are exposed via `libcam_params` but there are **critical issues** that would cause incorrect behavior, including setting read-only controls.

---

## Current Implementation Status

### What's Implemented (src/libcam.cpp:477-511)

| Control | Type | Implemented | Issues |
|---------|------|-------------|--------|
| `AfMode` | Input (int32_t) | ✅ | None |
| `AfRange` | Input (int32_t) | ✅ | None |
| `AfSpeed` | Input (int32_t) | ✅ | None |
| `AfMetering` | Input (int32_t) | ✅ | None |
| `AfWindows` | Input (Rectangle[]) | ✅ | Only 1 window supported |
| `AfTrigger` | Input (int32_t) | ✅ | None |
| `AfPause` | Input (int32_t) | ✅ | None |
| `LensPosition` | Input/Output (float) | ✅ | None |
| `AfState` | **Output only** | ⚠️ **BUG** | Should NOT be set |
| `AfPauseState` | **Output only** | ⚠️ **BUG** | Should NOT be set |
| `FocusFoM` | Output only | ✅ (read) | Correctly handled |

---

## Critical Issues Found

### Issue 1: Setting Read-Only Controls (CRITICAL - BUG)

**Location:** `src/libcam.cpp:506-511`

```cpp
if (pname == "AfState") {
    controls.set(controls::AfState, mtoi(pvalue));  // BUG!
}
if (pname == "AfPauseState") {
    controls.set(controls::AfPauseState, mtoi(pvalue));  // BUG!
}
```

**Problem:** `AfState` and `AfPauseState` are **output-only controls** per the libcamera specification. They are reported by the camera to indicate the current state of the AF algorithm - they cannot be set by the application.

From the libcamera documentation:
- *"AfState: This control **reports** the current state of the AF algorithm"*
- *"AfPauseState: **Report** whether the autofocus is currently running, paused or pausing"*

**Impact:** If a user tries to set these via `libcam_params`, libcamera will likely:
1. Silently ignore the control, or
2. Return an error causing configuration validation to fail

**Severity:** Critical

---

### Issue 2: No AF Initialization Default (MODERATE)

**Problem:** There's no default AF mode configuration. According to libcamera docs:

> *"This mode (AfModeManual) is the recommended default value for the AfMode control."*

For a motion detection application, the camera starts with whatever libcamera's default is, which may vary by camera model.

**Severity:** Moderate

---

### Issue 3: AfWindows Supports Only One Window (MINOR)

**Location:** `src/libcam.cpp:489-496`

```cpp
if (pname == "AfWindows") {
    Rectangle afwin[1];  // Only 1 window
    afwin[0].x = mtoi(mtok(pvalue,"|"));
    ...
    controls.set(controls::AfWindows, afwin);
}
```

**Problem:** libcamera supports multiple AF windows, but Motion only parses one.

**Severity:** Minor (single window usually sufficient)

---

### Issue 4: Documentation Mismatch

**Location:** `src/libcam.cpp:139`

```cpp
MOTION_SHT(DBG, TYPE_VIDEO, NO_ERRNO, "  AfWindows(Pipe delimited)");
MOTION_SHT(DBG, TYPE_VIDEO, NO_ERRNO, "     x | y | h | w");
```

**Problem:** The format says `x | y | h | w` but the code parses `x | y | width | height`.

**Severity:** Low

---

### Issue 5: No Hot-Reload for AF Controls

**Observation:** Unlike brightness/contrast/AWB which have hot-reload support (`set_brightness()`, `set_awb_mode()`, etc.), there's no mechanism to change AF settings at runtime.

For motion detection, the ability to trigger a re-focus or switch modes dynamically could be valuable.

**Severity:** Medium

---

## Correct Usage Per libcamera Specification

### Mode Relationships

| Mode | Required Controls | Output States |
|------|-------------------|---------------|
| `AfModeManual (0)` | `LensPosition` | `AfState` always `Idle` |
| `AfModeAuto (1)` | `AfTrigger` to start/cancel | `AfState`: Idle→Scanning→Focused/Failed |
| `AfModeContinuous (2)` | Optional: `AfPause` | `AfState`: Scanning→Focused/Failed (auto-restarts) |

### Valid Configuration Examples

```
# Continuous autofocus (good for motion detection)
libcam_params AfMode=2

# Manual focus at 2 meters distance (dioptres = 1/distance)
libcam_params AfMode=0,LensPosition=0.5

# Auto focus with trigger (requires application to trigger)
libcam_params AfMode=1,AfRange=2,AfSpeed=1
```

---

## Control Direction Reference (from libcamera docs)

### Input Controls (can be set by application)
- `AfMode` - Set the autofocus mode
- `AfRange` - Set focus distance range (Normal/Macro/Full)
- `AfSpeed` - Set focus speed (Normal/Fast)
- `AfMetering` - Set metering mode (Auto/Windows)
- `AfWindows` - Set focus window rectangles
- `AfTrigger` - Trigger/cancel an AF scan (Auto mode only)
- `AfPause` - Pause/resume continuous AF
- `LensPosition` - Set lens position (Manual mode only, also reported as output)

### Output Controls (read-only, reported by camera)
- `AfState` - Current state of AF algorithm (Idle/Scanning/Focused/Failed)
- `AfPauseState` - Whether continuous AF is running/pausing/paused
- `FocusFoM` - Figure of Merit indicating focus quality
- `LensPosition` - Current lens position (also an input in Manual mode)

---

## Summary

| Aspect | Status | Severity |
|--------|--------|----------|
| Core AF controls | ✅ Implemented | - |
| Read-only controls being set | ❌ Bug | **Critical** |
| Documentation accuracy | ⚠️ Minor issue | Low |
| Hot-reload support | ❌ Missing | Medium |
| Default configuration | ⚠️ No defaults | Medium |
| Multi-window support | ⚠️ Limited | Minor |
