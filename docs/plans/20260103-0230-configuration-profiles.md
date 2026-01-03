# Configuration Profiles Implementation Plan
**Date**: 2026-01-03 02:30
**Status**: Planning
**Priority**: Medium
**Complexity**: Medium-High

## Overview

Implement a configuration profile system that allows users to save and quickly switch between different camera settings configurations. This system will store snapshots of libcamera controls, motion detection settings, and device framerate for quick recall.

## Background

Users need the ability to quickly switch between different camera configurations for different scenarios (e.g., "Daytime", "Nighttime", "High Motion", "Low Motion"). Currently, all settings must be manually adjusted each time, which is time-consuming and error-prone.

**Research Document**: `docs/scratchpads/20260103-config-profiles-research.md`

## Goals

### Primary Goals
1. Allow users to save current settings as named profiles
2. Enable quick switching between saved profiles
3. Support profiles per camera (camera_id specific)
4. Persist profiles across daemon restarts
5. Maintain CPU efficiency (paramount for Pi)

### Non-Goals (Future Enhancements)
- Profile sharing/export/import (v2)
- Profile scheduling (auto-switch by time/event) (v2)
- Profile inheritance/templates (v2)
- Cloud sync of profiles (out of scope)

## Scope

### Included Parameters (31 total)

#### 1. Libcamera Controls (14 parameters)
```
brightness          # -1 to 1
contrast            # 0-32
iso                 # 100-6400
awb_enable          # bool
awb_mode            # 0=Auto
awb_locked          # bool
colour_temp         # 0-10000 K
colour_gain_r       # 0-8
colour_gain_b       # 0-8
af_mode             # 0=Manual, 1=Auto, 2=Continuous
lens_position       # 0-15 dioptres
af_range            # 0=Normal, 1=Macro, 2=Full
af_speed            # 0=Normal, 1=Fast
libcam_params       # string (arbitrary params)
```

#### 2. Motion Detection (16 parameters)
```
threshold                   # Frame change threshold
threshold_maximum
threshold_sdevx
threshold_sdevy
threshold_sdevxy
threshold_ratio
threshold_ratio_change
threshold_tune              # bool
noise_level
noise_tune                  # bool
despeckle_filter            # string
area_detect                 # string
lightswitch_percent
lightswitch_frames
minimum_motion_frames
event_gap
```

#### 3. Device Settings (1 parameter)
```
framerate                   # 2-100 fps (requires camera restart)
```

### Excluded Parameters
- Application-level settings (daemon, logging, webcontrol)
- File paths and output settings
- Network/streaming parameters (these may be added later if requested)
- SQL/database parameters (security restriction)

## Architecture

### 1. Database Schema (SQLite)

```sql
-- Profile metadata table
CREATE TABLE config_profiles (
    profile_id INTEGER PRIMARY KEY AUTOINCREMENT,
    camera_id INTEGER NOT NULL DEFAULT 0,     -- 0 = global/default camera
    profile_name TEXT NOT NULL,               -- User-visible name
    description TEXT,                         -- Optional description
    is_default BOOLEAN DEFAULT 0,             -- Only one default per camera_id
    created_at INTEGER NOT NULL,              -- Unix timestamp
    updated_at INTEGER NOT NULL,              -- Unix timestamp
    UNIQUE(camera_id, profile_name)           -- Prevent duplicate names per camera
);

-- Profile parameters table (key-value storage)
CREATE TABLE config_profile_params (
    param_id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    param_name TEXT NOT NULL,                 -- Parameter name from config_parms[]
    param_value TEXT NOT NULL,                -- String representation of value
    FOREIGN KEY(profile_id) REFERENCES config_profiles(profile_id) ON DELETE CASCADE,
    UNIQUE(profile_id, param_name)
);

-- Indexes for performance
CREATE INDEX idx_profiles_camera ON config_profiles(camera_id);
CREATE INDEX idx_profile_params_profile ON config_profile_params(profile_id);
```

**Database Location**: `<config_dir>/config_profiles.db` or `<target_dir>/config_profiles.db`

**Migration Strategy**: Create tables on first access if they don't exist

### 2. Backend Implementation (C++)

#### 2.1 New Class: `cls_config_profile`

**File**: `src/conf_profile.hpp`, `src/conf_profile.cpp`

```cpp
class cls_config_profile {
public:
    cls_config_profile(cls_motapp *p_app);
    ~cls_config_profile();

    // Profile CRUD operations
    int create_profile(int camera_id, const std::string &name,
                       const std::string &desc, const std::map<std::string, std::string> &params);
    int load_profile(int profile_id, std::map<std::string, std::string> &params);
    int update_profile(int profile_id, const std::map<std::string, std::string> &params);
    int delete_profile(int profile_id);

    // Profile queries
    std::vector<ProfileInfo> list_profiles(int camera_id);
    int get_default_profile(int camera_id);
    int set_default_profile(int profile_id);

    // Snapshot current config
    std::map<std::string, std::string> snapshot_config(cls_config *cfg);

    // Apply profile to config (returns list of params that need restart)
    std::vector<std::string> apply_profile(cls_config *cfg, int profile_id);

private:
    cls_motapp *app;
    sqlite3 *db;
    std::string db_path;

    int init_database();
    void close_database();

    // Helper to determine which params to include in profile
    bool is_profileable_param(const std::string &param_name);
};

struct ProfileInfo {
    int profile_id;
    int camera_id;
    std::string name;
    std::string description;
    bool is_default;
    time_t created_at;
    time_t updated_at;
    int param_count;
};
```

**Responsibilities**:
- SQLite database connection management
- Profile CRUD operations with transaction support
- Config snapshot generation
- Profile application with hot-reload awareness
- Parameter filtering (only profileable params)

#### 2.2 API Endpoints (webu_json.cpp)

Add new JSON API handlers:

```cpp
// GET /0/api/profiles - List all profiles for camera
void cls_webu_json::api_profiles_list();

// GET /0/api/profiles/:id - Get specific profile with parameters
void cls_webu_json::api_profiles_get();

// POST /0/api/profiles - Create new profile from current config or provided params
void cls_webu_json::api_profiles_create();

// PATCH /0/api/profiles/:id - Update profile (rename, change params)
void cls_webu_json::api_profiles_update();

// DELETE /0/api/profiles/:id - Delete profile
void cls_webu_json::api_profiles_delete();

// POST /0/api/profiles/:id/apply - Apply profile to current camera
void cls_webu_json::api_profiles_apply();

// POST /0/api/profiles/:id/default - Set as default profile
void cls_webu_json::api_profiles_set_default();
```

**API Response Format**:
```json
{
  "status": "ok",
  "data": {
    "profile_id": 1,
    "camera_id": 0,
    "name": "Daytime",
    "description": "Optimized for daytime outdoor",
    "is_default": true,
    "created_at": 1735876200,
    "updated_at": 1735876200,
    "params": {
      "brightness": "0.2",
      "contrast": "1.5",
      "threshold": "1500",
      "framerate": "15"
    }
  },
  "requires_restart": ["framerate"]  // If applying profile
}
```

#### 2.3 Integration Points

**cls_motapp**:
- Add `cls_config_profile *profiles` member
- Initialize in constructor, cleanup in destructor
- Make available to webu_json handlers

**webu_json**:
- Add profile API route handling in `api_handler()`
- Reuse existing CSRF token validation
- Reuse existing parameter validation from `api_config_patch()`

### 3. Frontend Implementation (React)

#### 3.1 New Components

**File**: `frontend/src/components/settings/ProfileManager.tsx`

```tsx
interface Profile {
  profile_id: number;
  camera_id: number;
  name: string;
  description?: string;
  is_default: boolean;
  created_at: number;
  updated_at: number;
  param_count: number;
}

export function ProfileManager({ cameraId }: { cameraId: number }) {
  // Profile list management
  // Current active profile indicator
  // Profile selector dropdown
  // Save/Load/Delete buttons
  // New profile dialog
}
```

**File**: `frontend/src/components/settings/ProfileSaveDialog.tsx`

```tsx
export function ProfileSaveDialog({
  open,
  onClose,
  onSave
}: ProfileSaveDialogProps) {
  // Profile name input
  // Description textarea
  // Save as default checkbox
  // Save/Cancel buttons
}
```

#### 3.2 API Client

**File**: `frontend/src/api/profiles.ts`

```typescript
export const profilesApi = {
  list: (cameraId: number) =>
    fetch(`/0/api/profiles?camera_id=${cameraId}`),

  get: (profileId: number) =>
    fetch(`/0/api/profiles/${profileId}`),

  create: (data: CreateProfileData) =>
    fetch('/0/api/profiles', { method: 'POST', body: JSON.stringify(data) }),

  update: (profileId: number, data: UpdateProfileData) =>
    fetch(`/0/api/profiles/${profileId}`, { method: 'PATCH', body: JSON.stringify(data) }),

  delete: (profileId: number) =>
    fetch(`/0/api/profiles/${profileId}`, { method: 'DELETE' }),

  apply: (profileId: number) =>
    fetch(`/0/api/profiles/${profileId}/apply`, { method: 'POST' }),

  setDefault: (profileId: number) =>
    fetch(`/0/api/profiles/${profileId}/default`, { method: 'POST' }),
};
```

#### 3.3 State Management

Use TanStack Query for profile data caching:

```typescript
// frontend/src/hooks/useProfiles.ts
export function useProfiles(cameraId: number) {
  return useQuery({
    queryKey: ['profiles', cameraId],
    queryFn: () => profilesApi.list(cameraId),
  });
}

export function useApplyProfile() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: profilesApi.apply,
    onSuccess: () => {
      // Invalidate config cache
      queryClient.invalidateQueries(['config']);
    },
  });
}
```

#### 3.4 UI Integration

Add to Settings page (`frontend/src/pages/Settings.tsx`):
- Profile selector at top of page (sticky header area)
- "Save as Profile" button next to settings categories
- Profile management dialog (list/edit/delete)

**Visual Design**:
- Dropdown selector: Shows current profile (or "Custom" if modified)
- Save icon button: Opens save dialog
- Settings icon button: Opens profile management dialog
- Warning indicator: Shows when current settings differ from loaded profile

### 4. CPU Efficiency Considerations

#### Profile Loading
- **Single Batch Request**: Use existing PATCH /0/api/config endpoint
- **No UI Re-render Cascade**: Update all settings in one state update
- **Minimal Database Queries**: Cache profile list, only query params on load

#### Profile Saving
- **Snapshot Only Changed Params**: Compare with default/previous profile
- **Transaction Batching**: Single SQLite transaction for all params
- **Lazy Write**: Only write to DB when user explicitly saves

#### Memory Usage
- **No In-Memory Profile Cache**: Load from DB on demand
- **Small Footprint**: Profile metadata ~200 bytes, params ~50 bytes each
- **Bounded List**: Limit to 50 profiles per camera (configurable)

## Implementation Phases

### Phase 1: Backend Foundation (2-3 hours)
**Files to Create**:
- `src/conf_profile.hpp` - Profile manager class declaration
- `src/conf_profile.cpp` - Profile manager implementation

**Files to Modify**:
- `src/Makefile.am` - Add new source files
- `src/motion.hpp` - Forward declare `cls_config_profile`
- `src/motapp.hpp` - Add `cls_config_profile *profiles` member
- `src/motapp.cpp` - Initialize/cleanup profile manager

**Tasks**:
1. Implement `cls_config_profile` class
2. Create SQLite schema and migration logic
3. Implement CRUD operations with error handling
4. Add `snapshot_config()` to extract profileable params
5. Add `apply_profile()` with hot-reload awareness
6. Unit test SQLite operations (manual testing on Pi)

**Validation**:
- Database created on first run
- Profile CRUD operations work correctly
- Transactions properly rollback on error
- Profile params correctly filtered

### Phase 2: API Endpoints (1-2 hours)
**Files to Modify**:
- `src/webu_json.hpp` - Add profile API method declarations
- `src/webu_json.cpp` - Implement profile API handlers

**Tasks**:
1. Add route parsing for `/api/profiles/*` URLs
2. Implement all 7 profile API endpoints
3. Add CSRF token validation
4. Add JSON request/response formatting
5. Add error handling and logging

**Validation**:
- All endpoints accessible via curl/Postman
- CSRF validation works correctly
- Error responses formatted properly
- Profile operations reflected in database

### Phase 3: Frontend Components (2-3 hours)
**Files to Create**:
- `frontend/src/api/profiles.ts` - API client
- `frontend/src/hooks/useProfiles.ts` - React Query hooks
- `frontend/src/components/settings/ProfileManager.tsx` - Main UI
- `frontend/src/components/settings/ProfileSaveDialog.tsx` - Save dialog

**Files to Modify**:
- `frontend/src/pages/Settings.tsx` - Integrate ProfileManager
- `frontend/src/types/api.ts` - Add profile types

**Tasks**:
1. Create API client with TypeScript types
2. Implement React Query hooks for caching
3. Build ProfileManager component
4. Build ProfileSaveDialog component
5. Integrate into Settings page header
6. Add visual feedback (loading states, errors)

**Validation**:
- Profile dropdown shows available profiles
- Save dialog captures name/description
- Apply profile updates all settings
- Delete profile prompts confirmation
- Loading states show during API calls

### Phase 4: Integration & Polish (1-2 hours)
**Tasks**:
1. Add "unsaved changes" indicator
2. Add confirmation dialog for destructive actions
3. Add restart warning when profile contains framerate
4. Test profile save/load/delete workflows
5. Test edge cases (no profiles, many profiles, long names)
6. Performance testing on Pi (profile load time)
7. Update documentation

**Validation**:
- Full workflow works end-to-end
- No memory leaks or performance degradation
- Error messages are user-friendly
- Documentation accurate

## Testing Strategy

### Backend Testing
1. **Database Operations**:
   - Profile CRUD operations
   - Constraint enforcement (unique names)
   - Foreign key cascades (delete profile → delete params)
   - Transaction rollback on error

2. **API Endpoints**:
   - Success responses
   - Error responses (invalid JSON, missing params, etc.)
   - CSRF validation
   - Parameter validation

3. **Profile Application**:
   - Hot-reload params applied immediately
   - Non-hot-reload params flagged for restart
   - Invalid params rejected

### Frontend Testing
1. **Component Rendering**:
   - Profile list displays correctly
   - Save/load/delete buttons work
   - Dialogs open/close properly

2. **State Management**:
   - Profile cache invalidated on changes
   - Settings updated on profile load
   - Unsaved changes detected

3. **User Workflows**:
   - Create profile from current settings
   - Load existing profile
   - Update profile
   - Delete profile
   - Set default profile

### Integration Testing (on Pi)
1. **End-to-End Workflows**:
   - Save daytime profile → switch to nighttime profile → switch back
   - Verify camera applies settings correctly
   - Verify motion detection uses new thresholds
   - Verify libcamera controls updated

2. **Performance**:
   - Profile load time < 200ms
   - No camera glitches during profile switch
   - No memory leaks after 100 profile switches

3. **Edge Cases**:
   - Profile with framerate change (requires restart)
   - Profile for unsupported camera
   - Concurrent profile operations (multiple users)

## Security Considerations

1. **CSRF Protection**: All write operations require valid CSRF token
2. **SQL Injection**: Use parameterized queries exclusively
3. **Path Traversal**: Validate database path
4. **Input Validation**: Sanitize profile names/descriptions (max length, allowed chars)
5. **Access Control**: Profile operations restricted to authenticated users (future)

## Rollback Plan

If issues arise:
1. Profile system is **optional** - existing config system unchanged
2. Database can be deleted without affecting core functionality
3. API endpoints can be disabled via compile flag
4. Frontend component can be hidden via feature flag

## Success Criteria

1. User can save current settings as a named profile
2. User can load a saved profile and see all settings update
3. User can delete unwanted profiles
4. System handles framerate changes with restart warning
5. Profile operations complete in < 500ms on Pi 4/5
6. No memory leaks or CPU spikes
7. Database survives daemon restarts

## Future Enhancements (Post-MVP)

1. **Profile Export/Import**: JSON export for backup/sharing
2. **Profile Scheduling**: Auto-switch profiles by time of day
3. **Profile Templates**: Pre-built profiles for common scenarios
4. **Profile Diff**: Show differences between profiles
5. **Profile History**: Track changes to profiles over time
6. **Cloud Sync**: Sync profiles across multiple cameras (if cloud feature added)

## Open Questions

1. **Q**: Should profiles include streaming settings (quality, resolution)?
   **A**: Not in MVP - focus on camera/detection only. Can add later if requested.

2. **Q**: Should there be a "factory reset" profile?
   **A**: Not in MVP - user can manually save a "Default" profile. Can add built-in later.

3. **Q**: How to handle profile compatibility when parameters change?
   **A**: Store param name as string - skip unknown params on load, log warning.

4. **Q**: Should profile save be automatic (on settings change)?
   **A**: No - explicit save only. Auto-save would be confusing and CPU-intensive.

5. **Q**: Maximum number of profiles per camera?
   **A**: 50 profiles (soft limit in UI), no hard limit in DB. Can adjust based on feedback.

## Resources Required

- **Development Time**: 6-10 hours
- **Testing Time**: 2-3 hours
- **Documentation**: 1 hour
- **Pi Hardware**: For testing (Pi 4 or Pi 5)
- **SQLite3**: Already available on system

## Dependencies

- SQLite3 library (already configured)
- Existing config system (cls_config)
- Existing API framework (cls_webu_json)
- React UI framework (already in place)

## Timeline Estimate

- **Phase 1**: 2-3 hours
- **Phase 2**: 1-2 hours
- **Phase 3**: 2-3 hours
- **Phase 4**: 1-2 hours
- **Total**: 6-10 hours

Can be completed in 2-3 development sessions.

---

## Appendix A: Profileable Parameters

### Libcamera Controls (14)
```cpp
brightness          // ctx_pending_controls.brightness
contrast            // ctx_pending_controls.contrast
iso                 // ctx_pending_controls.iso
awb_enable          // ctx_pending_controls.awb_enable
awb_mode            // ctx_pending_controls.awb_mode
awb_locked          // ctx_pending_controls.awb_locked
colour_temp         // ctx_pending_controls.colour_temp
colour_gain_r       // ctx_pending_controls.colour_gain_r
colour_gain_b       // ctx_pending_controls.colour_gain_b
af_mode             // ctx_pending_controls.af_mode
lens_position       // ctx_pending_controls.lens_position
af_range            // ctx_pending_controls.af_range
af_speed            // ctx_pending_controls.af_speed
libcam_params       // parm_cam.libcam_params (string)
```

### Motion Detection (16)
```cpp
threshold                   // parm_cam.threshold
threshold_maximum           // parm_cam.threshold_maximum
threshold_sdevx             // parm_cam.threshold_sdevx
threshold_sdevy             // parm_cam.threshold_sdevy
threshold_sdevxy            // parm_cam.threshold_sdevxy
threshold_ratio             // parm_cam.threshold_ratio
threshold_ratio_change      // parm_cam.threshold_ratio_change
threshold_tune              // parm_cam.threshold_tune
noise_level                 // parm_cam.noise_level
noise_tune                  // parm_cam.noise_tune
despeckle_filter            // parm_cam.despeckle_filter
area_detect                 // parm_cam.area_detect
lightswitch_percent         // parm_cam.lightswitch_percent
lightswitch_frames          // parm_cam.lightswitch_frames
minimum_motion_frames       // parm_cam.minimum_motion_frames
event_gap                   // parm_cam.event_gap
```

### Device Settings (1)
```cpp
framerate                   // parm_cam.framerate (requires restart)
```

## Appendix B: Database Schema DDL

```sql
-- config_profiles.db schema

CREATE TABLE IF NOT EXISTS config_profiles (
    profile_id INTEGER PRIMARY KEY AUTOINCREMENT,
    camera_id INTEGER NOT NULL DEFAULT 0,
    profile_name TEXT NOT NULL,
    description TEXT,
    is_default BOOLEAN NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    UNIQUE(camera_id, profile_name)
);

CREATE TABLE IF NOT EXISTS config_profile_params (
    param_id INTEGER PRIMARY KEY AUTOINCREMENT,
    profile_id INTEGER NOT NULL,
    param_name TEXT NOT NULL,
    param_value TEXT NOT NULL,
    FOREIGN KEY(profile_id) REFERENCES config_profiles(profile_id) ON DELETE CASCADE,
    UNIQUE(profile_id, param_name)
);

CREATE INDEX IF NOT EXISTS idx_profiles_camera
    ON config_profiles(camera_id);

CREATE INDEX IF NOT EXISTS idx_profiles_default
    ON config_profiles(camera_id, is_default);

CREATE INDEX IF NOT EXISTS idx_profile_params_profile
    ON config_profile_params(profile_id);

-- Trigger to ensure only one default profile per camera
CREATE TRIGGER IF NOT EXISTS enforce_single_default
BEFORE INSERT ON config_profiles
WHEN NEW.is_default = 1
BEGIN
    UPDATE config_profiles
    SET is_default = 0
    WHERE camera_id = NEW.camera_id AND is_default = 1;
END;

CREATE TRIGGER IF NOT EXISTS enforce_single_default_update
BEFORE UPDATE ON config_profiles
WHEN NEW.is_default = 1 AND OLD.is_default = 0
BEGIN
    UPDATE config_profiles
    SET is_default = 0
    WHERE camera_id = NEW.camera_id AND is_default = 1 AND profile_id != NEW.profile_id;
END;
```

## Appendix C: API Examples

### Create Profile
```bash
curl -X POST http://192.168.1.176:7999/0/api/profiles \
  -H "Content-Type: application/json" \
  -H "X-CSRF-Token: abc123" \
  -d '{
    "name": "Daytime Outdoor",
    "description": "Optimized for bright outdoor conditions",
    "camera_id": 0,
    "is_default": false,
    "snapshot_current": true
  }'
```

### List Profiles
```bash
curl http://192.168.1.176:7999/0/api/profiles?camera_id=0
```

### Apply Profile
```bash
curl -X POST http://192.168.1.176:7999/0/api/profiles/1/apply \
  -H "X-CSRF-Token: abc123"
```

### Delete Profile
```bash
curl -X DELETE http://192.168.1.176:7999/0/api/profiles/1 \
  -H "X-CSRF-Token: abc123"
```
