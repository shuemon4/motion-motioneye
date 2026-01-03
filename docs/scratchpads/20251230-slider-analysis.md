# Slider Implementation Analysis

## Overview

This document analyzes the slider controls from the original MotionEye and compares them to the current React UI implementation.

## Reference: Original MotionEye Sliders

Total: 22 sliders across 6 settings sections

### Preferences Section (4 sliders)
| Slider ID | Label | Range | Current Status |
|-----------|-------|-------|----------------|
| layoutColumnsSlider | Layout Columns | 1-4 | ❌ Number input (PreferencesSettings.tsx:68) |
| layoutRowsSlider | Layout Rows | 1-4 | ❌ Number input (PreferencesSettings.tsx:78) |
| framerateDimmerSlider | Frame Rate Dimmer | 0-100% | ❌ Number input as "Framerate Factor" (PreferencesSettings.tsx:99) |
| resolutionDimmerSlider | Resolution Dimmer | 1-100% | ❌ Number input as "Resolution Factor" (PreferencesSettings.tsx:110) |

### Video Device Section (8 sliders)
| Slider ID | Label | Range | Current Status |
|-----------|-------|-------|----------------|
| framerateSlider | Frame Rate | 2-30 fps | ❌ Number input (DeviceSettings.tsx:94) |
| lensPositionSlider | Position (Autofocus) | 0-15 dioptres | ❌ Number input (LibcameraSettings.tsx:133) |
| isoSlider | Gain (ISO) | 1-64 | ❌ Number input (LibcameraSettings.tsx:48) |
| brightnessSlider | Brightness | -1 to 1 | ❌ Number input (LibcameraSettings.tsx:31) |
| contrastSlider | Contrast | 0-32 | ❌ Number input (LibcameraSettings.tsx:40) |
| colourTempSlider | Temperature (°K) | 0-10000 K | ❌ Number input (LibcameraSettings.tsx:90) |
| colourGainRSlider | Red Gain | 0-8 | ❌ Number input (LibcameraSettings.tsx:100) |
| colourGainBSlider | Blue Gain | 0-8 | ❌ Number input (LibcameraSettings.tsx:108) |

### Video Streaming Section (3 sliders)
| Slider ID | Label | Range | Current Status |
|-----------|-------|-------|----------------|
| streamingFramerateSlider | Streaming Frame Rate | 1-30 fps | ❌ Number input (StreamSettings.tsx:63) |
| streamingQualitySlider | Streaming Quality | 0-100% | ❌ Number input (StreamSettings.tsx:52) |
| streamingResolutionSlider | Streaming Resolution | 0-100% | ✅ Dropdown select (StreamSettings.tsx:43) |

### Still Images Section (1 slider)
| Slider ID | Label | Range | Current Status |
|-----------|-------|-------|----------------|
| imageQualitySlider | Image Quality | 0-100% | ❌ Number input (PictureSettings.tsx:93) |

### Movies Section (1 slider)
| Slider ID | Label | Range | Current Status |
|-----------|-------|-------|----------------|
| movieQualitySlider | Movie Quality | 0-100% | ❌ Number input (MovieSettings.tsx:79) |

### Motion Detection Section (4 sliders)
| Slider ID | Label | Range | Current Status |
|-----------|-------|-------|----------------|
| frameChangeThresholdSlider | Frame Change Threshold | 0-20% | ❌ Number input as "Threshold (%)" (MotionSettings.tsx:45) |
| noiseLevelSlider | Noise Level | 0-25% | ❌ Number input (MotionSettings.tsx:79) |
| lightSwitchDetectSlider | Light Switch Detection | 0-100% | ❌ Number input (MotionSettings.tsx:91) |
| smartMaskSluggishnessSlider | Smart Mask Sluggishness | 1-10 | ❌ Number input as "Smart Mask Speed" (MotionSettings.tsx:109) |

### Text Overlay Section (1 slider)
| Slider ID | Label | Range | Current Status |
|-----------|-------|-------|----------------|
| textScaleSlider | Text Scale | 1-10 | ❌ Number input (OverlaySettings.tsx:108) |

## Summary

- **Total expected sliders**: 22
- **Currently implemented as sliders**: 0 (streaming resolution uses dropdown)
- **Currently implemented as number inputs**: 21
- **Missing entirely**: 0 (all have number input equivalents)

## Implementation Plan

### 1. Create FormSlider Component

Create a new `frontend/src/components/form/FormSlider.tsx` component that:
- Accepts min, max, step, value, onChange
- Displays current value alongside slider
- Uses HTML5 range input with Tailwind styling
- Shows labels/units (%, fps, etc.)
- Maintains consistent styling with other form components

### 2. Update Settings Components

Replace number inputs with sliders for the 21 items listed above:
- PreferencesSettings.tsx (4 sliders)
- DeviceSettings.tsx (1 slider)
- LibcameraSettings.tsx (7 sliders)
- StreamSettings.tsx (2 sliders)
- PictureSettings.tsx (1 slider)
- MovieSettings.tsx (1 slider)
- MotionSettings.tsx (4 sliders)
- OverlaySettings.tsx (1 slider)

### 3. Export and Use

Add FormSlider to `frontend/src/components/form/index.ts`

### 4. Considerations

**UX Improvements over number inputs:**
- Visual feedback of range
- Easier to adjust with mouse/touch
- Clear visual representation of value within range
- Better for percentage and bounded values

**Potential issues to address:**
- Some ranges use decimal steps (brightness: -1 to 1)
- ISO is logarithmic in original (1-64)
- Need to handle edge cases gracefully

## Notes

The original MotionEye used jQuery UI sliders with class `range styled`. The React implementation should use native HTML5 range inputs styled with Tailwind for better performance and accessibility.

## Implementation Complete

All 21 sliders have been successfully implemented:

### Components Updated

1. **FormSlider.tsx** - Created new slider component with:
   - HTML5 range input
   - Visual gradient fill showing current value
   - Value display with unit support
   - Consistent styling with existing form components
   - Error state support

2. **PreferencesSettings.tsx** (4 sliders):
   - Grid Columns (1-4)
   - Grid Rows (1-4)
   - Framerate Factor (0.1-4.0x)
   - Resolution Factor (0.25-1.0x)

3. **DeviceSettings.tsx** (1 slider):
   - Framerate (2-30 fps)

4. **LibcameraSettings.tsx** (7 sliders):
   - Brightness (-1 to 1, step 0.1)
   - Contrast (0-32, step 0.5)
   - ISO (0-6400, step 100)
   - Lens Position (0-15 dioptres, step 0.5)
   - Color Temperature (0-10000 K, step 100)
   - Red Gain (0-8, step 0.1)
   - Blue Gain (0-8, step 0.1)

5. **StreamSettings.tsx** (2 sliders):
   - Stream Quality (1-100%)
   - Stream Max Framerate (1-30 fps)
   - *Note: Streaming Resolution kept as dropdown per current design*

6. **PictureSettings.tsx** (1 slider):
   - Picture Quality (1-100%)

7. **MovieSettings.tsx** (1 slider):
   - Movie Quality (1-100%)

8. **MotionSettings.tsx** (4 sliders):
   - Threshold (0-20%, step 0.1)
   - Noise Level (1-255)
   - Light Switch Detection (0-100%)
   - Smart Mask Speed (0-10)

9. **OverlaySettings.tsx** (1 slider):
   - Text Scale (1-10x)

### Features

- All sliders show current value alongside the control
- Gradient fill indicates current position visually
- Units displayed where appropriate (%, fps, K, x, dioptres)
- Consistent styling with dark theme
- Accessible with ARIA attributes
- Error state styling
- Help text support
