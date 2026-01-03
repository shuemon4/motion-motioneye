Here is a complete list of all slider controls in the MotionEye Settings page, organized by
  section:

  Preferences Section

  | Slider ID              | Label             | Range  | Description
                                        |
  |------------------------|-------------------|--------|---------------------------------------
  --------------------------------------|
  | layoutColumnsSlider    | Layout Columns    | 1-4    | Number of columns for camera frame
  layout                                   |
  | layoutRowsSlider       | Layout Rows       | 1-4    | Number of rows for camera frame layout
   (when Fit Frames Vertically enabled) |
  | framerateDimmerSlider  | Frame Rate Dimmer | 0-100% | Dims global frame rate to save
  bandwidth                                    |
  | resolutionDimmerSlider | Resolution Dimmer | 1-100% | Dims resolution of all cameras to save
   bandwidth                            |

  Video Device Section

  | Slider ID          | Label                | Range         | Description
                                                  |
  |--------------------|----------------------|---------------|---------------------------------
  ------------------------------------------------|
  | framerateSlider    | Frame Rate           | 2-30 fps      | Frames captured per second
                                                  |
  | lensPositionSlider | Position (Autofocus) | 0-15 dioptres | Manual focus position (libcamera
   only, when autofocus = Manual)                 |
  | isoSlider          | Gain (ISO)           | 1-64          | Analog gain (libcamera only,
  logarithmic)                                       |
  | brightnessSlider   | Brightness           | -1 to 1       | Camera brightness (libcamera
  only)                                              |
  | contrastSlider     | Contrast             | 0-32          | Camera contrast (libcamera only)
                                                  |
  | colourTempSlider   | Temperature (°K)     | 0-10000 K     | Manual colour temperature
  (libcamera only, when AWB disabled, temperature mode) |
  | colourGainRSlider  | Red Gain             | 0-8           | Manual red channel gain
  (libcamera only, when AWB disabled, gains mode)         |
  | colourGainBSlider  | Blue Gain            | 0-8           | Manual blue channel gain
  (libcamera only, when AWB disabled, gains mode)        |

  Video Streaming Section

  | Slider ID                 | Label                | Range    | Description
                               |
  |---------------------------|----------------------|----------|-------------------------------
  -----------------------------|
  | streamingFramerateSlider  | Streaming Frame Rate | 1-30 fps | Frames transmitted per second
  for live streaming           |
  | streamingQualitySlider    | Streaming Quality    | 0-100%   | Live streaming quality
                               |
  | streamingResolutionSlider | Streaming Resolution | 0-100%   | Streaming resolution as
  percent of video device resolution |

  Still Images Section

  | Slider ID          | Label         | Range  | Description        |
  |--------------------|---------------|--------|--------------------|
  | imageQualitySlider | Image Quality | 0-100% | JPEG image quality |

  Movies Section

  | Slider ID          | Label         | Range  | Description        |
  |--------------------|---------------|--------|--------------------|
  | movieQualitySlider | Movie Quality | 0-100% | MPEG video quality |

  Motion Detection Section

  | Slider ID                   | Label                   | Range  | Description
                                |
  |-----------------------------|-------------------------|--------|----------------------------
  ------------------------------|
  | frameChangeThresholdSlider  | Frame Change Threshold  | 0-20%  | Min percent of image change
   to trigger motion            |
  | noiseLevelSlider            | Noise Level             | 0-25%  | Fixed noise level (when
  auto noise detection disabled)   |
  | lightSwitchDetectSlider     | Light Switch Detection  | 0-100% | Image change percent to
  detect as light change vs motion |
  | smartMaskSluggishnessSlider | Smart Mask Sluggishness | 1-10   | Smart mask responsiveness
  (when smart mask enabled)      |

  Text Overlay Section

  | Slider ID       | Label      | Range | Description          |
  |-----------------|------------|-------|----------------------|
  | textScaleSlider | Text Scale | 1-10  | Size of overlay text |

  ---
  Total: 22 sliders across 6 settings sections. All sliders use class="range styled" and are
  rendered using the makeSlider() JavaScript function in ui.js.