# React UI Completion Plan

**Created**: 2025-12-28
**Status**: Ready for Implementation
**Author**: Claude Code Analysis

---

## Current State Summary

### Completed Work (Phases 1-3)

| Phase | Status | Description |
|-------|--------|-------------|
| 1.1 | ✅ Complete | C++ config parameters (`webcontrol_html_path`, `webcontrol_spa_mode`) |
| 1.2 | ✅ Complete | SPA static file serving in `webu_file.cpp` |
| 1.3 | ✅ Partial | 3 API endpoints (`auth/me`, `media/pictures`, `system/temperature`) |
| 2 | ✅ Complete | React app initialized with Vite + Tailwind + React Query |
| 3 | ✅ Complete | Integration tested on Pi 5, build working |

### Current Frontend Structure

```
frontend/src/
├── api/client.ts           # Basic HTTP utilities
├── hooks/useCameraStream.ts # Stream loading hook
├── components/
│   ├── CameraStream.tsx    # MJPEG stream display
│   └── Layout.tsx          # App layout with nav
├── pages/
│   ├── Dashboard.tsx       # Camera grid (hardcoded [0,1])
│   ├── Settings.tsx        # Empty placeholder
│   └── Media.tsx           # Empty placeholder
├── App.tsx                 # Router configuration
└── main.tsx                # Entry point
```

**Bundle Size**: 81 KB gzipped (target: <500 KB)

### What's Missing

1. **Dynamic camera discovery** - Currently hardcoded `[0, 1]`
2. **Settings UI** - No configuration editing
3. **Media gallery** - No browsing/deleting snapshots or videos
4. **Authentication UI** - No login form
5. **System status** - No temperature/stats display
6. **State management** - Zustand store not utilized

---

## Phase 4: Core Feature Implementation

### 4.1 API Layer Enhancement

**Priority**: P1 (Required for all other features)

**Tasks**:

1. **Create TypeScript types** (`frontend/src/api/types.ts`)
   ```typescript
   interface Camera { id: number; name: string; enabled: boolean }
   interface ConfigParam { name: string; value: string; category: string; type: string }
   interface MediaItem { id: number; filename: string; path: string; date: string; size: number }
   interface SystemStatus { temperature: { celsius: number; fahrenheit: number }; uptime: number }
   ```

2. **Create React Query hooks** (`frontend/src/api/queries.ts`)
   - `useCameras()` - Fetch `/0/config/list` or parse available cameras
   - `useConfig(camId)` - Fetch `/0/config` or `/{camId}/config`
   - `useMedia(camId, type)` - Fetch `/{camId}/api/media/pictures`
   - `useSystemStatus()` - Fetch `/0/api/system/temperature`
   - `useAuthStatus()` - Fetch `/0/api/auth/me`

3. **Create mutation hooks** (`frontend/src/api/mutations.ts`)
   - `useUpdateConfig(camId, param, value)` - POST to `/{camId}/config/{param}`
   - `useDeleteMedia(id)` - Future: DELETE media item

**Effort**: ~2-3 hours

---

### 4.2 Dynamic Camera Discovery

**Priority**: P1

**Current Issue**: Dashboard hardcodes `[0, 1]` cameras

**Tasks**:

1. **Analyze Motion's camera list endpoint**
   - Check if `/0/config` includes camera info
   - Check `/cameras` or similar endpoint

2. **Create useCameras hook**
   - Fetch camera list from API
   - Return `{ cameras: Camera[], isLoading, error }`

3. **Update Dashboard.tsx**
   - Replace hardcoded `[0, 1]` with API data
   - Handle loading/error states
   - Show "No cameras configured" if empty

**Effort**: ~1-2 hours

---

### 4.3 Settings UI Implementation

**Priority**: P1 (Core feature)

**Approach**: Schema-driven form generation from Motion's ~600 config parameters

**Tasks**:

1. **Create config schema** (`frontend/src/config/schema.ts`)
   - Define field metadata (label, type, category, dependencies)
   - Group by categories (Video, Motion Detection, Storage, etc.)

2. **Create base form components** (`frontend/src/components/form/`)
   - `FormField.tsx` - Auto-render based on type
   - `FormSection.tsx` - Collapsible category group
   - `FormInput.tsx`, `FormSelect.tsx`, `FormToggle.tsx`, `FormSlider.tsx`

3. **Create settings sections** (`frontend/src/features/settings/`)
   - `VideoDevice.tsx` - Camera source settings
   - `MotionDetection.tsx` - Threshold, sensitivity, noise
   - `Storage.tsx` - Target directory, file naming
   - `Streaming.tsx` - Port, quality, max clients
   - `Notifications.tsx` - On-event scripts

4. **Update Settings.tsx page**
   - Tabbed or accordion layout
   - Load config via `useConfig()`
   - Track dirty state
   - Save with confirmation

5. **Add C++ API for config categories** (optional)
   - Motion already has `/categories` endpoint
   - May need `/config/schema` for field types

**Effort**: ~8-12 hours (largest task)

---

### 4.4 Media Gallery Implementation

**Priority**: P2

**Tasks**:

1. **Create media API hooks**
   - `useSnapshots(camId)` - Pictures from SQLite
   - `useMovies(camId)` - Videos from SQLite

2. **Create MediaGallery component**
   - Responsive thumbnail grid
   - Lazy loading for images
   - Selection mode (multi-select)
   - Virtualization for large lists (react-window, optional)

3. **Create MediaViewer component**
   - Lightbox for images
   - Video player with controls
   - Prev/next navigation

4. **Create media actions**
   - Delete selected items
   - Download selected items
   - Filter by date range

5. **Update Media.tsx page**
   - Tab for Pictures / Movies
   - Date picker for filtering
   - Bulk action toolbar

6. **Backend: Add DELETE endpoints** (C++)
   - `DELETE /{cam_id}/api/media/pictures/{id}`
   - `DELETE /{cam_id}/api/media/movies/{id}`

**Effort**: ~6-8 hours

---

### 4.5 System Status Dashboard

**Priority**: P2

**Tasks**:

1. **Create SystemStatus component**
   - CPU temperature gauge
   - Uptime display
   - Storage usage
   - Network status (optional)

2. **Add C++ endpoints for system info**
   - `GET /0/api/system/status` - Combined system info
   - Disk free space
   - Memory usage
   - Motion version

3. **Add status to Layout header**
   - Temperature badge
   - Recording indicator per camera

**Effort**: ~3-4 hours

---

### 4.6 Authentication UI

**Priority**: P2 (if auth enabled)

**Tasks**:

1. **Create auth store** (`frontend/src/store/auth.ts`)
   - User state (authenticated, username)
   - Login/logout actions

2. **Create LoginPage** (`frontend/src/pages/Login.tsx`)
   - Username/password form
   - Error display
   - Remember me (optional)

3. **Create AuthGuard component**
   - Wrap protected routes
   - Redirect to login if unauthenticated

4. **Update routing**
   - Add login route
   - Protect other routes with AuthGuard

**Effort**: ~4-5 hours

---

## Phase 5: Polish & Production Readiness

### 5.1 Error Handling & Loading States

**Tasks**:
- Add global error boundary
- Add loading skeletons for pages
- Toast notifications for actions
- Retry logic for failed API calls

**Effort**: ~2-3 hours

---

### 5.2 Mobile Responsiveness

**Tasks**:
- Test on mobile viewports
- Improve touch targets
- Add swipe gestures for camera grid
- Responsive settings layout (single column on mobile)

**Effort**: ~2-3 hours

---

### 5.3 Performance Optimization

**Tasks**:
- Lazy load routes with React.lazy()
- Image optimization for thumbnails
- Stream quality toggle (high/low bandwidth)
- Reduce unnecessary re-renders with memo()

**Effort**: ~2-3 hours

---

### 5.4 Testing

**Tasks**:
- Vitest setup for unit tests
- Component tests with Testing Library
- E2E tests with Playwright (optional)
- API integration tests (curl scripts)

**Effort**: ~4-6 hours

---

### 5.5 Documentation & Deployment

**Tasks**:
- Update CLAUDE.md with UI documentation
- Create user guide (if requested)
- Docker build integration
- Autotools install target for webui

**Effort**: ~2-3 hours

---

## Implementation Order (Recommended)

### Sprint 1: Foundation (Day 1-2)

| Task | Description | Est. Hours |
|------|-------------|------------|
| 4.1 | API types & hooks | 2-3 |
| 4.2 | Dynamic camera discovery | 1-2 |
| 5.1 (partial) | Error boundary + toasts | 1-2 |
| **Total** | | **4-7 hours** |

**Deliverable**: Dashboard shows real cameras from API, has basic error handling

---

### Sprint 2: Settings Core (Day 2-4)

| Task | Description | Est. Hours |
|------|-------------|------------|
| 4.3a | Base form components | 3-4 |
| 4.3b | Config schema (subset) | 2-3 |
| 4.3c | First settings section (Video) | 2-3 |
| **Total** | | **7-10 hours** |

**Deliverable**: Settings page with one functional section (e.g., Video Device)

---

### Sprint 3: Settings Complete (Day 4-6)

| Task | Description | Est. Hours |
|------|-------------|------------|
| 4.3d | Remaining settings sections | 4-6 |
| 4.3e | Dirty state & save functionality | 2-3 |
| **Total** | | **6-9 hours** |

**Deliverable**: Full settings UI with all major categories

---

### Sprint 4: Media Gallery (Day 6-8)

| Task | Description | Est. Hours |
|------|-------------|------------|
| 4.4a | Media API + gallery grid | 3-4 |
| 4.4b | Media viewer (lightbox) | 2-3 |
| 4.4c | Delete/download actions | 2-3 |
| **Total** | | **7-10 hours** |

**Deliverable**: Functional media browsing and management

---

### Sprint 5: System & Auth (Day 8-9)

| Task | Description | Est. Hours |
|------|-------------|------------|
| 4.5 | System status dashboard | 3-4 |
| 4.6 | Authentication UI | 4-5 |
| **Total** | | **7-9 hours** |

**Deliverable**: System monitoring, login if auth enabled

---

### Sprint 6: Polish (Day 9-10)

| Task | Description | Est. Hours |
|------|-------------|------------|
| 5.2 | Mobile responsiveness | 2-3 |
| 5.3 | Performance optimization | 2-3 |
| 5.5 | Documentation | 2-3 |
| **Total** | | **6-9 hours** |

**Deliverable**: Production-ready UI

---

## Total Estimated Effort

| Sprint | Hours |
|--------|-------|
| 1 - Foundation | 4-7 |
| 2 - Settings Core | 7-10 |
| 3 - Settings Complete | 6-9 |
| 4 - Media Gallery | 7-10 |
| 5 - System & Auth | 7-9 |
| 6 - Polish | 6-9 |
| **Total** | **37-54 hours** |

---

## C++ Backend Work Required

| Endpoint | Method | Priority | Description |
|----------|--------|----------|-------------|
| `/{cam}/api/system/status` | GET | P2 | Combined system info (disk, memory, uptime) |
| `/{cam}/api/media/pictures/{id}` | DELETE | P2 | Delete snapshot by ID |
| `/{cam}/api/media/movies/{id}` | DELETE | P2 | Delete recording by ID |
| `/{cam}/api/media/movies` | GET | P2 | List movie recordings (similar to pictures) |

**Note**: Much of the config editing uses existing Motion endpoints (`POST /{cam}/config/{param}`)

---

## Success Criteria

| Metric | Target |
|--------|--------|
| Feature parity with MotionEye core | 80%+ |
| Bundle size (gzipped) | < 150 KB |
| Time to first meaningful paint | < 2s |
| Settings save latency | < 500ms |
| Works on Pi 4/5 | Yes |
| RAM usage (Motion + UI) | < 50 MB |
| Mobile usable | Yes |

---

## Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|------------|--------|------------|
| Settings form complexity | High | Medium | Start with subset, iterate |
| Missing API endpoints | Medium | Medium | Add C++ endpoints as needed |
| Performance on Pi | Low | High | Test regularly on hardware |
| Media gallery scale | Medium | Low | Virtualize large lists |

---

## Next Immediate Steps

1. **User decision**: Which sprint to start with?
2. **If Sprint 1**: Begin with `frontend/src/api/types.ts`
3. **Pi check**: Is Pi 5 powered on for testing?

---

## Quick Start Commands

```bash
# Development
cd frontend && npm run dev

# Build
cd frontend && npm run build

# Deploy to Pi
rsync -avz --exclude='node_modules' \
  /Users/tshuey/Documents/GitHub/motion-motioneye/ \
  admin@192.168.1.176:~/motion-motioneye/

# Copy webui to Pi install location
ssh admin@192.168.1.176 "sudo cp -r ~/motion-motioneye/data/webui /usr/share/motion/"

# Restart Motion on Pi
ssh admin@192.168.1.176 "sudo systemctl restart motion"
```
