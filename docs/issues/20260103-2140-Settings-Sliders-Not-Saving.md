# Settings Page Sliders Not Saving

**Date**: 2026-01-03
**Status**: RESOLVED
**Severity**: High
**Component**: Frontend (React UI)

---

## Summary

Settings sliders were not saving values. Users could adjust sliders and click Save, but values would revert to their original positions. The "Successfully saved" message appeared despite no changes being persisted.

---

## Symptoms

1. Sliders visually update when dragged
2. "Save" button becomes enabled showing unsaved changes
3. Clicking "Save" shows success message
4. Values immediately revert to original positions
5. Refreshing page confirms no changes were saved

---

## Root Cause Analysis

### Issue 1: Slider Values Not Updating Visually (Initial)

The Settings page passed `activeConfig` to child components, but pending changes were stored separately in `changes` state. Child components read values from config, not from the merged state.

**Fix**: Modified `activeConfig` useMemo to merge pending changes into the config before passing to child components.

```typescript
// Before: activeConfig only contained original values
const activeConfig = useMemo(() => {
  return { ...defaultConfig, ...cameraConfig }
}, [config, selectedCamera])

// After: activeConfig includes pending changes
const activeConfig = useMemo(() => {
  const mergedConfig = { ...baseConfig }
  for (const [param, value] of Object.entries(changes)) {
    if (mergedConfig[param]) {
      mergedConfig[param] = { ...mergedConfig[param], value }
    }
  }
  return mergedConfig
}, [config, selectedCamera, changes])
```

### Issue 2: CSRF Token Not Being Stored (Root Cause of Save Failure)

The Settings page used an inline `useQuery` to fetch config, but did not store the CSRF token from the response. The `useBatchUpdateConfig` mutation requires a valid CSRF token for PATCH requests.

**Comparison**:

```typescript
// useMotionConfig (in queries.ts) - CORRECTLY stores token
queryFn: async () => {
  const config = await apiGet<MotionConfig>('/0/api/config')
  if (config.csrf_token) {
    setCsrfToken(config.csrf_token)  // Token stored!
  }
  return config
}

// Settings.tsx inline query - WAS MISSING token storage
queryFn: () => apiGet<MotionConfig>('/0/api/config')  // No token storage!
```

**Result**: All PATCH requests failed with "CSRF validation failed" because no token was available.

**Fix**: Added CSRF token storage to the Settings page query:

```typescript
const { data: config, isLoading, error } = useQuery({
  queryKey: ['config'],
  queryFn: async () => {
    const cfg = await apiGet<MotionConfig & { csrf_token?: string }>('/0/api/config')
    if (cfg.csrf_token) {
      setCsrfToken(cfg.csrf_token)  // Now stores token!
    }
    return cfg
  },
})
```

### Issue 3: Hot-Reload vs Restart-Required Parameters

Some parameters (like `framerate`, `width`, `height`) require a daemon restart and cannot be changed at runtime. The API returns success but with `errors > 0` in these cases.

**Fix**: Added response checking to show appropriate messages:

```typescript
if (summary.errors > 0) {
  const failedParams = applied.filter((p) => p.error).map((p) => p.param)
  addToast(`Settings require daemon restart: ${failedParams.join(', ')}`, 'warning')
}
```

---

## Files Modified

| File | Changes |
|------|---------|
| `frontend/src/pages/Settings.tsx` | Added CSRF token storage, merged changes into activeConfig, improved error handling |
| `frontend/src/components/form/FormSlider.tsx` | New component for slider inputs |
| `frontend/src/components/form/index.ts` | Export FormSlider |
| `frontend/src/components/settings/*.tsx` | Replaced number inputs with sliders |

---

## Hot-Reloadable Parameters

The following parameters can be changed at runtime without restart:

| Category | Parameters |
|----------|------------|
| Motion Detection | threshold, threshold_maximum, noise_level, noise_tune, lightswitch_percent, smart_mask_speed, minimum_motion_frames, event_gap, pre_capture, post_capture |
| Text Overlay | text_left, text_right, text_scale, text_changes, locate_motion_mode |
| Streaming | stream_quality, stream_maxrate, stream_motion, stream_grey |
| Pictures | picture_quality, picture_output, snapshot_interval |
| Movies | movie_max_time |

## Restart-Required Parameters

These parameters require daemon restart:

| Category | Parameters |
|----------|------------|
| Device | width, height, framerate, rotate |
| libcamera | libcam_brightness, libcam_contrast, libcam_iso (and other libcam_* params) |
| Ports | webcontrol_port, stream_port |

---

## Testing

### Verify CSRF Token Storage

```bash
# In browser console after loading Settings page:
# Check if token is stored (requires adding debug logging)
```

### Verify Hot-Reload API

```bash
# Test from Pi directly:
CSRF=$(curl -s http://localhost:7999/0/api/config | python3 -c "import sys,json; print(json.load(sys.stdin).get('csrf_token',''))")
curl -X PATCH -H "Content-Type: application/json" -H "X-CSRF-Token: $CSRF" \
  -d '{"stream_quality": 75}' http://localhost:7999/1/api/config

# Expected response:
# {"status":"ok","applied":[{"param":"stream_quality","old":"50","new":"75","hot_reload":true}],"summary":{"total":1,"success":1,"errors":0}}
```

---

## Lessons Learned

1. **CSRF tokens must be stored consistently**: Any component fetching config should store the CSRF token, or use a shared hook that does.

2. **Test with actual API calls**: The sliders appeared to work visually, but the actual save was failing silently until proper error handling was added.

3. **Check API response details**: The API returns `status: "ok"` even with partial failures. Always check `summary.errors` for accurate success/failure reporting.

4. **Hot-reload limitations**: Not all parameters can be changed at runtime. The UI should indicate which parameters require a restart.

---

## Related Documentation

- `docs/plans/MotionEye-Plans/hot-reload-implementation-plan.md` - Original hot-reload API implementation plan
- `docs/scratchpads/20251230-slider-analysis.md` - Slider implementation analysis
- `src/conf.cpp` - Parameter definitions with `hot_reload` flags
