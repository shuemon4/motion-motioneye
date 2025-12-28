# Dispatch Function Build Notes

## Strategy
Due to the large number of handlers (162), building the dispatch function incrementally:
1. Start with 20-30 common handlers
2. Test compilation
3. Progressively add remaining handlers in batches
4. Final cleanup and testing

## Handlers Identified So Far

### Booleans
- daemon: false
- native_language: true

### Strings (simple)
- conf_filename: ""
- pid_file: ""
- device_name: ""
- v4l2_device: ""
- v4l2_params: ""
- netcam_url: ""
- schedule_params: ""
- cleandir_params: ""
- config_dir: ""

### Integers with range
- log_level: 6, 1-9
- log_fflevel: 3, 1-9
- device_id: 0, 1-32000 (CUSTOM - has duplicate check logic)
- device_tmo: 30, 1-INT_MAX
- watchdog_tmo: 30, 1-INT_MAX
- watchdog_kill: 10, 1-INT_MAX

### Lists
- log_type: "ALL", ["ALL","COR","STR","ENC","NET","DBL","EVT","TRK","VID"]

### Custom (preserve as-is)
- log_file: strftime expansion
- target_dir: trailing slash removal
- pause: complex backward compatibility logic
- device_id: duplicate checking

## Next Steps
1. Read more handlers systematically (by category)
2. Build initial dispatch function
3. Add to conf.cpp before line 3161
4. Test compilation
5. Expand with remaining handlers
