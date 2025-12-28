# User:
Can you verify or reject this:

The `libcam_awb_mode` hot-reload parameter appears to succeed (returns success to MotionEye) but has no effect on the video stream. Investigation revealed that the IMX708 sensor (Pi Camera v3) does not expose the `controls::AwbMode` control through libcamera's per-request ControlList interface.
Thought for 47s

# Response
I’d reject the second sentence (“IMX708 … does not expose controls::AwbMode through libcamera’s per-request ControlList interface”).

Here’s why:

controls::AwbMode is a real libcamera control (it’s defined in libcamera’s control IDs / API docs). 
libcamera.org
+1

On Raspberry Pi’s libcamera stack, AWB mode is exposed to userspace tools as a runtime option (e.g., rpicam-still --awb <mode>). That wouldn’t exist if the control didn’t exist in the pipeline. 
Linux Command Library

People are seeing AwbMode show up in the “available controls” listing from Pi camera tooling (e.g., rpicam-vid ... --verbose 2 output includes AwbMode). 
Raspberry Pi Forums

libcamera apps commonly apply controls by merging them and sending them with the next request, which is exactly the “per-request ControlList” model. 
Raspberry Pi Forums

There’s even recent Pi Camera Module 3 (IMX708) discussion where it’s stated there’s “no need to set AwbEnable or AwbMode” because they’re defaults—again implying those controls exist and are expected to work. 
Raspberry Pi Forums

What could still be true (and matches your symptom)

The first sentence (“hot-reload appears to succeed but has no effect”) is totally plausible, but the root cause is more likely one of these:

MotionEye reports “success” but doesn’t actually push the control into the active request stream
It might be updating config state without calling into the live libcamera request controls path.

You’re using a layer that doesn’t forward that control
Example: libcamera-v4l2 (V4L2 compatibility) or another intermediary that doesn’t map AwbMode (or maps it differently). In that case MotionEye might think it set something, but the camera pipeline never receives it.

AWB mode changes are being overridden by other controls
For example, if you’re also setting manual WB controls (ColourGains) then AWB behavior changes (AWB gets disabled / ignored depending on stack). libcamera explicitly documents interactions like ColourGains requiring AWB disabled. 
libcamera.org
+1

Quick way to prove it on your Pi (no guessing)

Run a control dump and look for AwbMode:

Use your installed Pi camera tool’s “list controls / verbose” output and confirm AwbMode is listed (people do see it listed). 
Raspberry Pi Forums

Then change AWB mode via the tool (--awb daylight, etc.) and confirm the stream changes. 
Linux Command Library

If that works, then the issue is almost certainly in MotionEye/Motion’s hot-reload plumbing, not in IMX708 “not exposing the control”.

If you tell me which capture path Motion is using on your setup (native libcamera app integration, Picamera2, libcamera-v4l2, etc.), I can point to the most likely “it returns success but doesn’t apply” failure point and the correct place to inject the control update.