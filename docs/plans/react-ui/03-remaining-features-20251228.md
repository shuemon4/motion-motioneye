# React UI Completion Plan - Remaining Items

**Date**: 2025-12-28
**Status**: Final

---

## User Requirements

- **Environment**: Both local/trusted network and internet-exposed deployments
- **Scope**: Complete feature set (personal use, "production ready" when satisfied)
- **Backend**: C++ changes OK when needed
- **Validation**: Strict validation with clear error messages

---

## Context

Based on analysis of `docs/scratchpads/20251228-connection-analysis.md` and `docs/scratchpads/20251228-endpoint-mapping.md`, plus exploration of the current frontend implementation.

**Already In Progress:**
- CSRF Protection (being addressed separately - see `02-csrf-protection-20251228.md`)

---

## Remaining Work Items (Prioritized)

### HIGH Priority

#### 1. Fix Config Endpoint Path (Quick Fix)
**File**: `frontend/src/api/queries.ts`
**Change**: `/0/config` → `/0/api/config`
**Effort**: 5 minutes
**Rationale**: Current path routes to legacy handler instead of new React-specific endpoint.

#### 2. Authentication Flow
**Current State**: Type definitions exist, `useAuth()` hook exists but is never called.
**Missing**:
- Login page/modal component
- Auth context/provider for session state
- Protected route wrapper
- Logout functionality
- 401/403 response handling with login prompt

**Files to Create**:
- `frontend/src/contexts/AuthContext.tsx`
- `frontend/src/components/LoginModal.tsx`
- `frontend/src/components/ProtectedRoute.tsx`

**Files to Modify**:
- `frontend/src/api/client.ts` - Add auth error interception
- `frontend/src/App.tsx` - Add AuthProvider and route protection

**Backend Note**: Motion uses HTTP Basic/Digest auth via `webcontrol_authentication` config. Need to verify if this works with React's `credentials: 'same-origin'`.

#### 3. Frontend Validation (Zod)
**Current State**: No validation library, forms accept any input.
**Recommendation**: Add Zod for schema validation.

**Files to Create**:
- `frontend/src/lib/validation.ts` - Common validation schemas
- `frontend/src/lib/schemas/config.ts` - Config parameter schemas

**Files to Modify**:
- `frontend/src/pages/Settings.tsx` - Add validation to form inputs
- `frontend/src/components/form/*.tsx` - Add error state support

**Validation Patterns** (from original MotionEye):
- Device name: `^[A-Za-z0-9\-\_\+\ ]+$`
- Filename: `^([A-Za-z0-9 \(\)/._-]|%[CYmdHMSqv])+$`
- Directory: `^[A-Za-z0-9 \(\)/._-]+$`
- Email: `^[A-Za-z0-9 _+.@^~<>,-]+$`

---

### MEDIUM Priority

#### 4. Enhanced Error Handling
**Current State**: Generic "HTTP 500: Internal Server Error" messages.
**Goal**: User-friendly error messages with appropriate actions.

**Implementation**:
- Add error code mapping in `client.ts`
- Add retry logic for transient failures (network errors, 503)
- Show specific messages for common errors (401, 403, 404, 500)

#### 5. Camera Capabilities Detection
**Current State**: UI shows all controls regardless of camera support.
**Goal**: Hide unsupported controls (e.g., autofocus on fixed-focus camera).

**Files to Create**:
- `frontend/src/hooks/useCameraCapabilities.ts`

**Backend Requirement**: Need to verify if Motion exposes camera capabilities via API. May need C++ backend enhancement.

#### 6. Media File Operations
**Current State**: View and download only.
**Missing**: Delete pictures/movies.

**Files to Modify**:
- `frontend/src/api/queries.ts` - Add delete mutations
- `frontend/src/pages/Media.tsx` - Add delete UI with confirmation

**Backend Requirement**: Verify `/api/media/pictures/{id}` and `/api/media/movies/{id}` DELETE endpoints exist.

#### 7. Camera Management
**Current State**: Can view and configure existing cameras.
**Missing**: Add/delete cameras.

**Note**: This is more complex as Motion requires config file changes. May need:
- Backend endpoint for camera add/delete
- Or admin-mode for direct config file editing

---

### LOW Priority

#### 8. Streaming Improvements
**Current State**: Basic MJPEG display with simple error handling.
**Improvements**:
- Auto-reconnect with exponential backoff
- Stream quality selector (if Motion supports multiple qualities)
- Full-screen mode
- Connection status indicator

#### 9. System Management
**Current State**: View-only system metrics in header.
**Missing**:
- Daemon restart button
- Log viewer
- Config backup/restore

#### 10. Config Batching
**Current State**: Each parameter change triggers immediate save.
**Improvement**: Batch changes and save together.

**Note**: Settings.tsx already has unsaved changes tracking - just needs batch save implementation.

---

## Implementation Order (Recommended)

### Sprint 1: Quick Fixes & Foundation
1. Fix config endpoint path (`/0/config` → `/0/api/config`)
2. Install Zod: `npm install zod`
3. Create validation schemas for Motion config parameters
4. Wire validation into Settings form components

### Sprint 2: Authentication System
1. Create AuthContext with session state management
2. Create LoginModal component (username/password)
3. Add auth error interception in client.ts (401/403 handling)
4. Add ProtectedRoute wrapper for route guarding
5. Test with Motion's `webcontrol_authentication` config

### Sprint 3: Error Handling & UX
1. Implement error code mapping (401→"Login required", 403→"Access denied", etc.)
2. Add retry logic for transient failures (network errors, 503)
3. Improve toast messages with actionable guidance

### Sprint 4: Media Operations
1. Verify/add DELETE endpoints in C++ backend (`webu_json.cpp`)
2. Add delete mutations to `queries.ts`
3. Add delete UI to Media page with confirmation modal
4. Test on Raspberry Pi hardware

### Sprint 5: Camera Management
1. Add C++ endpoint for camera add/delete (config file operations)
2. Create Add Camera modal (camera type, URL, name)
3. Create Delete Camera confirmation flow
4. Handle config reload after camera changes

### Sprint 6: Advanced Features
1. Camera capabilities detection (query libcamera for supported controls)
2. Streaming improvements (auto-reconnect, quality selector)
3. System management (daemon restart, log viewer)
4. Config backup/restore

---

## Critical Files Reference

**Frontend - Modify**:
- `frontend/src/api/queries.ts` - Fix endpoint, add mutations
- `frontend/src/api/client.ts` - Auth interception, error handling
- `frontend/src/pages/Settings.tsx` - Validation integration
- `frontend/src/pages/Media.tsx` - Delete functionality
- `frontend/src/App.tsx` - Auth provider, route protection

**Frontend - Create**:
- `frontend/src/contexts/AuthContext.tsx`
- `frontend/src/components/LoginModal.tsx`
- `frontend/src/components/ProtectedRoute.tsx`
- `frontend/src/lib/validation.ts`
- `frontend/src/lib/schemas/config.ts`

**Backend - May Need Verification/Changes**:
- `src/webu_json.cpp` - Verify endpoints exist
- `src/webu_ans.cpp` - Verify routing

---

## Backend Endpoints Needed

Based on feature requirements, these C++ endpoints need to be verified or added:

| Endpoint | Method | Purpose | Status |
|----------|--------|---------|--------|
| `/api/media/pictures/{id}` | DELETE | Delete picture | Verify/Add |
| `/api/media/movies/{id}` | DELETE | Delete movie | Verify/Add |
| `/api/cameras` | POST | Add camera | Add |
| `/api/cameras/{id}` | DELETE | Delete camera | Add |
| `/api/system/restart` | POST | Restart daemon | Add |
| `/api/config/backup` | GET | Export config | Add |
| `/api/config/restore` | POST | Import config | Add |
| `/api/cameras/{id}/capabilities` | GET | Camera controls info | Add |

---

## Validation Schemas (Zod)

Key schemas to implement based on original MotionEye patterns:

```typescript
// Device/camera names
const deviceNameSchema = z.string().regex(/^[A-Za-z0-9\-_+ ]+$/);

// File paths (for target_dir, snapshot_filename, etc.)
const filenameSchema = z.string().regex(/^([A-Za-z0-9 ()/._-]|%[CYmdHMSqv])+$/);
const dirnameSchema = z.string().regex(/^[A-Za-z0-9 ()/._-]+$/);

// Numeric ranges
const framerateSchema = z.number().min(1).max(100);
const qualitySchema = z.number().min(1).max(100);
const widthSchema = z.number().min(160).max(4096);
const heightSchema = z.number().min(120).max(2160);

// Network
const portSchema = z.number().min(1).max(65535);
const emailSchema = z.string().regex(/^[A-Za-z0-9 _+.@^~<>,-]+$/);
```

---

## Summary

**Total Items**: 10 major features across 6 sprints
**Already Handled**: CSRF protection (separate effort)

**Sprint Priority**:
1. Foundation (endpoint fix + validation) - Quick wins
2. Authentication - Required for internet exposure
3. Error handling - Better UX
4. Media operations - Core functionality
5. Camera management - Full CRUD
6. Advanced features - Polish
