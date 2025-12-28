# Dispatch Edit Function - Usage Guide

## Overview
The `dispatch_edit()` function consolidates 154 configuration parameter handlers into a single centralized dispatcher. This guide shows how to use it and how to integrate it into Motion's configuration system.

## Function Signature

```cpp
void cls_config::dispatch_edit(const std::string& name, std::string& parm, enum PARM_ACT pact);
```

### Parameters
- `name`: Parameter name string (e.g., "daemon", "width", "log_level")
- `parm`: Parameter value as string (used for SET, returned for GET)
- `pact`: Parameter action (PARM_ACT_DFLT, PARM_ACT_SET, PARM_ACT_GET, PARM_ACT_LIST)

## Usage Examples

### Setting a Boolean Parameter
```cpp
cls_config cfg;
std::string value = "1";  // or "true", "yes", "on"
cfg.dispatch_edit("daemon", value, PARM_ACT_SET);
// cfg.daemon is now true
```

### Setting an Integer Parameter with Range Validation
```cpp
cls_config cfg;
std::string value = "1920";
cfg.dispatch_edit("width", value, PARM_ACT_SET);
// cfg.width is now 1920 (valid range 64-9999)

std::string invalid = "50";
cfg.dispatch_edit("width", value, PARM_ACT_SET);
// logs error, value unchanged (50 < 64)
```

### Setting a Float Parameter
```cpp
cls_config cfg;
std::string brightness = "0.5";
cfg.dispatch_edit("libcam_brightness", brightness, PARM_ACT_SET);
// cfg.parm_cam.libcam_brightness is now 0.5 (range -1.0 to 1.0)
```

### Setting a String Parameter
```cpp
cls_config cfg;
std::string value = "/tmp/motion.log";
cfg.dispatch_edit("log_file", value, PARM_ACT_SET);
// cfg.log_file is now "/tmp/motion.log"
```

### Setting a List Parameter (Constrained Values)
```cpp
cls_config cfg;
std::string value = "mjpeg";
cfg.dispatch_edit("stream_preview_method", value, PARM_ACT_SET);
// cfg.stream_preview_method is now "mjpeg" (valid: mjpeg, snapshot)

std::string invalid = "invalid_method";
cfg.dispatch_edit("stream_preview_method", value, PARM_ACT_SET);
// logs error, value unchanged
```

### Getting Parameter Value
```cpp
cls_config cfg;
cfg.width = 1920;

std::string value;
cfg.dispatch_edit("width", value, PARM_ACT_GET);
// value is now "1920"
```

### Getting List of Valid Values
```cpp
cls_config cfg;
std::string list;
cfg.dispatch_edit("stream_preview_method", list, PARM_ACT_LIST);
// list is now "[\"mjpeg\",\"snapshot\"]"
```

### Resetting to Default
```cpp
cls_config cfg;
cfg.width = 1920;

std::string dummy;
cfg.dispatch_edit("width", dummy, PARM_ACT_DFLT);
// cfg.width is now 640 (default value)
```

### Custom Handler (Special Logic)
```cpp
cls_config cfg;

// These still call their custom handlers:
std::string logfile = "%Y-%m-%d.log";
cfg.dispatch_edit("log_file", logfile, PARM_ACT_SET);
// Performs strftime() conversion

std::string dir = "/var/motion/";
cfg.dispatch_edit("target_dir", dir, PARM_ACT_SET);
// Removes trailing slash automatically
```

## Integration into Configuration Loading

### Before (Individual Handler Calls)
```cpp
void cls_config::load_config_from_file(const char *cfgfile) {
    // ... read config lines ...

    if (parm_name == "daemon") {
        edit_daemon(parm_value, PARM_ACT_SET);
    } else if (parm_name == "width") {
        edit_width(parm_value, PARM_ACT_SET);
    } else if (parm_name == "height") {
        edit_height(parm_value, PARM_ACT_SET);
    } else if (parm_name == "log_level") {
        edit_log_level(parm_value, PARM_ACT_SET);
    }
    // ... 150+ more handlers
}
```

### After (Unified Dispatch)
```cpp
void cls_config::load_config_from_file(const char *cfgfile) {
    // ... read config lines ...

    dispatch_edit(parm_name, parm_value, PARM_ACT_SET);
}
```

## Integration into Web API

### Example: Web Control Parameter Update
```cpp
// Handler for web request: POST /config?param=width&value=1920
void handle_config_update(const std::string& param, const std::string& value) {
    if (value.empty()) {
        // Reset to default
        std::string dummy;
        cfg->dispatch_edit(param, dummy, PARM_ACT_DFLT);
    } else {
        // Set new value
        std::string parm_copy = value;
        cfg->dispatch_edit(param, parm_copy, PARM_ACT_SET);
    }
}
```

### Example: Generate Parameter Schema for UI
```cpp
// Build JSON schema of all parameters
void generate_config_schema() {
    const char *params[] = {
        "daemon", "width", "height", "framerate", "log_level",
        "libcam_brightness", "picture_output", "database_type",
        // ... all parameter names
    };

    json schema = json::array();

    for (const auto& param : params) {
        json param_obj;
        param_obj["name"] = param;

        // Get default value
        std::string value;
        cfg->dispatch_edit(param, value, PARM_ACT_DFLT);
        param_obj["default"] = value;

        // Get current value
        cfg->dispatch_edit(param, value, PARM_ACT_GET);
        param_obj["value"] = value;

        // Get list of valid values (if applicable)
        std::string list;
        if (cfg->dispatch_edit(param, list, PARM_ACT_LIST) != nullptr) {
            param_obj["list"] = json::parse(list);
        }

        schema.push_back(param_obj);
    }

    return schema.dump();
}
```

## Parameter Categories by Use Case

### Camera Configuration
```cpp
std::string width_val = "1920";
std::string height_val = "1080";
std::string framerate_val = "30";

cfg->dispatch_edit("width", width_val, PARM_ACT_SET);
cfg->dispatch_edit("height", height_val, PARM_ACT_SET);
cfg->dispatch_edit("framerate", framerate_val, PARM_ACT_SET);
```

### Libcam Camera Control (Pi 5 + Camera v3)
```cpp
std::string brightness = "0.1";
std::string contrast = "1.2";
std::string iso = "400";

cfg->dispatch_edit("libcam_brightness", brightness, PARM_ACT_SET);
cfg->dispatch_edit("libcam_contrast", contrast, PARM_ACT_SET);
cfg->dispatch_edit("libcam_iso", iso, PARM_ACT_SET);
```

### Motion Detection Tuning
```cpp
std::string threshold = "1500";
std::string threshold_ratio = "10";
std::string noise_level = "32";

cfg->dispatch_edit("threshold", threshold, PARM_ACT_SET);
cfg->dispatch_edit("threshold_ratio", threshold_ratio, PARM_ACT_SET);
cfg->dispatch_edit("noise_level", noise_level, PARM_ACT_SET);
```

### Video Output Configuration
```cpp
std::string codec = "medium";
std::string container = "mp4";
std::string quality = "75";

cfg->dispatch_edit("movie_encoder_preset", codec, PARM_ACT_SET);
cfg->dispatch_edit("movie_container", container, PARM_ACT_SET);
cfg->dispatch_edit("movie_quality", quality, PARM_ACT_SET);
```

### Web Control Setup
```cpp
std::string port = "8080";
std::string auth = "basic";
std::string localhost = "1";

cfg->dispatch_edit("webcontrol_port", port, PARM_ACT_SET);
cfg->dispatch_edit("webcontrol_auth_method", auth, PARM_ACT_SET);
cfg->dispatch_edit("webcontrol_localhost", localhost, PARM_ACT_SET);
```

## Error Handling

The dispatch function follows Motion's existing error handling patterns:

1. **Invalid integer**: Logs notice, value unchanged
2. **Invalid float**: Logs notice, value unchanged
3. **Invalid list value**: Logs notice, value unchanged
4. **Unknown parameter**: Silently returns (could be enhanced with warning)

### Example Error Output
```
[NTC] Invalid width 50 (range 64-9999)
[NTC] Invalid stream_preview_method invalid_method
```

## Performance Considerations

### Dispatch Overhead
- Single string comparison per parameter (O(n) where n = parameter name length)
- No dynamic memory allocation for dispatch decision
- List static variables are initialized once (lazy evaluation)

### Compared to Direct Function Calls
- **Direct**: ~1 function call, 3-4 branches (DFLT/SET/GET/LIST)
- **Dispatch**: ~1 if-chain check + 1 function call + same branches
- **Net**: Minimal overhead, improved maintainability

### Optimization Options
1. Convert if-chain to `std::map<std::string, function_ptr>` for large parameter counts
2. Use hashing for O(1) dispatch
3. Pre-compute static list vectors during initialization

## Testing Checklist

When implementing dispatch_edit():

- [ ] Test all boolean parameters with "true", "1", "yes", "on" variants
- [ ] Test all integer parameters with valid and out-of-range values
- [ ] Test all float parameters with decimal precision
- [ ] Test all string parameters with paths and special characters
- [ ] Test all list parameters with valid and invalid values
- [ ] Test PARM_ACT_DFLT resets values correctly
- [ ] Test PARM_ACT_GET returns correct current values
- [ ] Test PARM_ACT_LIST returns valid value lists as JSON arrays
- [ ] Test custom handlers (log_file, target_dir, etc.) still work
- [ ] Verify error messages appear in logs for invalid values
- [ ] Benchmark performance vs. original handler calls
- [ ] Test concurrent parameter updates (if threaded)

## Custom Handler Integration

The following handlers are NOT in dispatch_edit() and must be called separately:

```cpp
// Date/time formatting
if (name == "log_file") {
    edit_log_file(parm, pact);
}

// Path normalization and validation
if (name == "target_dir" || name == "picture_filename" ||
    name == "movie_filename" || name == "snapshot_filename" ||
    name == "timelapse_filename") {
    // Call corresponding edit_XXX function
}

// Special duplicate checking
if (name == "device_id") {
    edit_device_id(parm, pact);
}

// Special deprecation handling
if (name == "pause") {
    edit_pause(parm, pact);
}

// Special event flag logic
if (name == "text_changes") {
    edit_text_changes(parm, pact);
}
```

## Migration Guide

### Step 1: Add dispatch_edit() to conf.hpp
Add function declaration to cls_config class.

### Step 2: Implement dispatch_edit() in conf.cpp
Copy implementation from `dispatch_edit_function.cpp`.

### Step 3: Update Configuration Loading
Replace individual handler calls with `dispatch_edit()` calls.

### Step 4: Update Web API Handler
Consolidate parameter update logic to use dispatch_edit().

### Step 5: Testing
Run full test suite, especially:
- Configuration file loading
- Web control parameter updates
- Motion detection with various thresholds
- Video encoding with different presets

### Step 6: Documentation
Update API documentation to reference dispatch_edit().

## See Also
- `dispatch_edit_function.cpp`: Complete implementation
- `dispatch_edit_metadata.md`: Detailed parameter metadata
- `/Users/tshuey/Documents/GitHub/motion/src/conf.cpp`: Original handlers
- `/Users/tshuey/Documents/GitHub/motion/src/conf.hpp`: Class definition
