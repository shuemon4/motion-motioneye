# React UI Integration - Detailed Execution Plan

**Created**: 2025-12-27
**Repository**: motion-motioneye
**Approach**: Extend Motion's C++ web server, no Python layer

---

## Execution Strategy

This plan leverages sub-agents for efficient parallel work:
- **Explore agents**: Analyze existing C++ code patterns
- **Plan agents**: Design API schemas and component architecture
- **Sequential implementation**: C++ changes require careful ordering

---

## Phase 1: C++ Web Server Extensions

### 1.1 Analyze Existing Code

**Agent Tasks** (Parallel):

| Agent | Task | Output |
|-------|------|--------|
| Explore | Analyze `webu_file.cpp` - current static file serving | Modification points |
| Explore | Analyze `webu_json.cpp` - existing API structure | API patterns |
| Explore | Analyze `webu_post.cpp` - POST handling patterns | POST integration |
| Explore | Analyze `conf.cpp` - parameter registration pattern | Config extension |

**Key Questions to Answer**:
1. How does `webu_file.cpp` currently resolve file paths?
2. What MIME types are supported?
3. How are authentication checks applied?
4. What's the pattern for adding new JSON endpoints?

### 1.2 Add SPA Support to webu_file.cpp

**Implementation Tasks**:

```
Task 1.2.1: Add configuration parameters
  File: src/conf.cpp, src/conf.hpp
  - Add webcontrol_html_path (string, default: /usr/share/motion/webui)
  - Add webcontrol_spa_mode (bool, default: on)
  - Register in config_parms[] with PARM_CAT_WEB category

Task 1.2.2: Implement SPA fallback routing
  File: src/webu_file.cpp
  - Check if requested file exists
  - If not and SPA mode enabled, serve index.html
  - Preserve existing file serving logic

Task 1.2.3: Add MIME type detection
  File: src/webu_file.cpp
  - Detect by file extension
  - Support: .html, .js, .css, .json, .svg, .png, .jpg, .woff, .woff2

Task 1.2.4: Add cache headers for assets
  File: src/webu_file.cpp
  - Immutable assets (hashed names): Cache-Control: max-age=31536000
  - index.html: Cache-Control: no-cache
```

**Code Pattern Reference** (from existing webu_file.cpp):
```cpp
// Existing pattern to follow
void cls_webu_file::main() {
    // Current implementation checks for specific files
    // We extend this to handle SPA routing
}
```

### 1.3 Extend JSON API

**New Endpoints Required**:

| Endpoint | Handler Method | Priority |
|----------|----------------|----------|
| `POST /api/auth/login` | `auth_login()` | P1 |
| `POST /api/auth/logout` | `auth_logout()` | P1 |
| `GET /api/auth/me` | `auth_current_user()` | P1 |
| `GET /api/media/pictures/{cam}` | `media_pictures_list()` | P2 |
| `DELETE /api/media/pictures/{id}` | `media_picture_delete()` | P2 |
| `DELETE /api/media/movies/{id}` | `media_movie_delete()` | P2 |
| `GET /api/system/temperature` | `system_temperature()` | P2 |
| `POST /api/system/reboot` | `system_reboot()` | P3 |
| `POST /api/system/shutdown` | `system_shutdown()` | P3 |
| `GET /api/config/backup` | `config_backup()` | P3 |
| `POST /api/config/restore` | `config_restore()` | P3 |

**Implementation Tasks**:

```
Task 1.3.1: Add API routing in webu_ans.cpp
  - Parse /api/* paths
  - Route to webu_json or webu_post handlers
  - Apply authentication checks

Task 1.3.2: Implement auth endpoints
  File: src/webu_json.cpp
  - auth_login(): Verify credentials, return session token
  - auth_logout(): Invalidate session
  - auth_current_user(): Return user info from session

Task 1.3.3: Implement media endpoints
  File: src/webu_json.cpp
  - media_pictures_list(): Scan target_dir for images
  - media_picture_delete(): Remove file with path validation
  - media_movie_delete(): Remove recording with path validation

Task 1.3.4: Implement system endpoints
  File: src/webu_json.cpp
  - system_temperature(): Read thermal zone
  - system_reboot(): Execute reboot command
  - system_shutdown(): Execute shutdown command

Task 1.3.5: Implement config backup/restore
  File: src/webu_json.cpp
  - config_backup(): Serialize all config to JSON/tar
  - config_restore(): Parse and apply uploaded config
```

---

## Phase 2: React Application Setup

### 2.1 Initialize Project

**Implementation Tasks**:

```
Task 2.1.1: Create webui directory structure
  motion-motioneye/
  └── webui/
      ├── src/
      ├── public/
      ├── index.html
      ├── vite.config.ts
      ├── tailwind.config.js
      ├── tsconfig.json
      └── package.json

Task 2.1.2: Configure Vite
  - Output to webui/dist
  - Asset hashing for cache busting
  - Proxy /api/* to Motion during development

Task 2.1.3: Configure Tailwind
  - Dark theme as default
  - Custom colors matching Motion's existing UI
  - Maven Pro font (or system fonts for simplicity)

Task 2.1.4: Set up TypeScript
  - Strict mode enabled
  - Path aliases (@/components, @/hooks, etc.)
```

**Vite Config Template**:
```typescript
// webui/vite.config.ts
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
    },
  },
  build: {
    outDir: 'dist',
    assetsDir: 'assets',
    // Generate hashed filenames for caching
    rollupOptions: {
      output: {
        entryFileNames: 'assets/[name]-[hash].js',
        chunkFileNames: 'assets/[name]-[hash].js',
        assetFileNames: 'assets/[name]-[hash].[ext]',
      },
    },
  },
  server: {
    proxy: {
      '/api': 'http://localhost:7999',
      '/mjpg': 'http://localhost:7999',
    },
  },
});
```

### 2.2 API Client Layer

**Implementation Tasks**:

```
Task 2.2.1: Create API client
  File: webui/src/api/client.ts
  - Fetch wrapper with error handling
  - CSRF token injection
  - Auth token management

Task 2.2.2: Create type definitions
  File: webui/src/api/types.ts
  - Camera, Config, MediaItem interfaces
  - API response types

Task 2.2.3: Create TanStack Query hooks
  File: webui/src/api/queries.ts
  - useCameras()
  - useConfig(camId)
  - useMedia(camId, type)
  - useSystemStatus()

Task 2.2.4: Create mutation hooks
  File: webui/src/api/mutations.ts
  - useUpdateConfig()
  - useDeleteMedia()
  - useCameraAction()
```

**API Client Template**:
```typescript
// webui/src/api/client.ts
const API_BASE = '/api';

async function fetchApi<T>(
  endpoint: string,
  options: RequestInit = {}
): Promise<T> {
  const response = await fetch(`${API_BASE}${endpoint}`, {
    ...options,
    headers: {
      'Content-Type': 'application/json',
      ...options.headers,
    },
    credentials: 'same-origin',
  });

  if (!response.ok) {
    throw new Error(`API error: ${response.status}`);
  }

  return response.json();
}

export const api = {
  cameras: {
    list: () => fetchApi<Camera[]>('/cameras'),
    get: (id: number) => fetchApi<Camera>(`/cameras/${id}`),
  },
  config: {
    get: (camId?: number) => fetchApi<Config>(camId ? `/config/${camId}` : '/config'),
    set: (camId: number, param: string, value: string) =>
      fetchApi(`/config/${camId}/${param}`, {
        method: 'POST',
        body: JSON.stringify({ value }),
      }),
  },
  // ...
};
```

---

## Phase 3: Core UI Components

### 3.1 Component Inventory

Based on MotionEye analysis, these components are needed:

**Base Components** (no dependencies):
| Component | Purpose | Priority |
|-----------|---------|----------|
| `Button` | Primary, secondary, ghost, danger variants | P1 |
| `Input` | Text, number, password with validation | P1 |
| `Select` | Single-select dropdown | P1 |
| `Toggle` | On/off switch (IEC symbols optional) | P1 |
| `Slider` | Range input with optional ticks | P1 |
| `Card` | Content container with optional header | P1 |
| `Modal` | Dialog with backdrop | P1 |
| `Toast` | Notification system | P1 |

**Camera Components**:
| Component | Purpose | Priority |
|-----------|---------|----------|
| `CameraFrame` | MJPEG display with loading/error states | P1 |
| `CameraGrid` | Multi-camera layout | P1 |
| `CameraOverlay` | Controls overlay (pause, snapshot, etc.) | P2 |
| `CameraFullscreen` | Single camera fullscreen view | P2 |

**Settings Components**:
| Component | Purpose | Priority |
|-----------|---------|----------|
| `SettingsPanel` | Slide-out drawer | P1 |
| `SettingsSection` | Collapsible form section | P1 |
| `FormField` | Auto-render from schema | P1 |
| `DependencyResolver` | Show/hide based on other fields | P2 |

**Media Components**:
| Component | Purpose | Priority |
|-----------|---------|----------|
| `MediaGallery` | Grid of thumbnails | P2 |
| `MediaViewer` | Full-size view/playback | P2 |
| `MediaActions` | Download, delete, select | P2 |

### 3.2 Implementation Tasks

```
Task 3.2.1: Create base components
  Priority: P1
  Files: webui/src/components/ui/*.tsx
  - Button, Input, Select, Toggle, Slider
  - Card, Modal, Toast

Task 3.2.2: Create camera components
  Priority: P1
  Files: webui/src/components/camera/*.tsx
  - CameraFrame with useCameraStream hook
  - CameraGrid with responsive layout

Task 3.2.3: Create settings components
  Priority: P1
  Files: webui/src/components/settings/*.tsx
  - SettingsPanel (drawer)
  - SettingsSection (collapsible)
  - FormField (renders from type)

Task 3.2.4: Create layout components
  Priority: P1
  Files: webui/src/components/layout/*.tsx
  - Header with camera selector
  - Sidebar (optional)
  - MainContent
```

---

## Phase 4: Feature Implementation

### 4.1 Camera Streaming

**Implementation Tasks**:

```
Task 4.1.1: Create useCameraStream hook
  File: webui/src/hooks/useCameraStream.ts
  - Construct stream URL: /{camId}/mjpg/stream
  - Handle visibility (Page Visibility API)
  - Handle viewport (Intersection Observer)
  - Error recovery with backoff

Task 4.1.2: Create CameraFrame component
  File: webui/src/components/camera/CameraFrame.tsx
  - Display stream via <img> tag
  - Loading skeleton
  - Error state with retry
  - Overlay controls

Task 4.1.3: Create CameraGrid component
  File: webui/src/components/camera/CameraGrid.tsx
  - Responsive grid (1-4 columns)
  - Click to expand
  - Drag to reorder (optional)
```

**Stream Hook Template**:
```typescript
// webui/src/hooks/useCameraStream.ts
export function useCameraStream(camId: number) {
  const isPageVisible = usePageVisibility();
  const [ref, isInViewport] = useIntersectionObserver();
  const [status, setStatus] = useState<'loading' | 'connected' | 'error'>('loading');

  const streamUrl = useMemo(() => {
    if (!isPageVisible || !isInViewport) return null;
    return `/${camId}/mjpg/stream`;
  }, [camId, isPageVisible, isInViewport]);

  const staticUrl = `/${camId}/mjpg/static`;

  return { ref, streamUrl, staticUrl, status, setStatus };
}
```

### 4.2 Configuration Forms

**Implementation Tasks**:

```
Task 4.2.1: Define config schema
  File: webui/src/config/schema.ts
  - TypeScript types for all ~600 parameters
  - Zod validation schemas
  - Field metadata (label, type, category, dependencies)

Task 4.2.2: Create schema-driven form renderer
  File: webui/src/components/settings/ConfigForm.tsx
  - Render fields from schema
  - Handle dependencies (show/hide)
  - Collect changes for batch save

Task 4.2.3: Implement settings sections
  Files: webui/src/features/settings/sections/*.tsx
  - VideoDevice, FileStorage, TextOverlay
  - VideoStreaming, StillImages, Movies
  - MotionDetection, Notifications, Schedule
  - Each section uses ConfigForm with filtered schema

Task 4.2.4: Create dirty state tracking
  File: webui/src/hooks/useFormDirty.ts
  - Track changed fields
  - Warn before navigation
  - Reset on save
```

### 4.3 Media Gallery

**Implementation Tasks**:

```
Task 4.3.1: Create media API hooks
  File: webui/src/api/media.ts
  - useSnapshots(camId)
  - useMovies(camId)
  - Pagination support

Task 4.3.2: Create MediaGallery component
  File: webui/src/components/media/MediaGallery.tsx
  - Virtualized grid (react-window)
  - Thumbnail lazy loading
  - Selection mode

Task 4.3.3: Create MediaViewer component
  File: webui/src/components/media/MediaViewer.tsx
  - Image zoom/pan
  - Video playback
  - Navigation (prev/next)

Task 4.3.4: Create batch operations
  File: webui/src/features/media/MediaActions.tsx
  - Select all/none
  - Bulk delete
  - Bulk download (browser handles ZIP)
```

### 4.4 Authentication

**Implementation Tasks**:

```
Task 4.4.1: Create auth store
  File: webui/src/store/auth.ts
  - User state
  - Login/logout actions
  - Token persistence

Task 4.4.2: Create LoginPage
  File: webui/src/pages/Login.tsx
  - Username/password form
  - Error display
  - Redirect on success

Task 4.4.3: Create AuthGuard
  File: webui/src/components/auth/AuthGuard.tsx
  - Protect routes
  - Redirect to login
  - Loading state
```

---

## Phase 5: Build & Integration

### 5.1 Build System

**Implementation Tasks**:

```
Task 5.1.1: Create build script
  File: scripts/build-webui.sh
  #!/bin/bash
  cd webui
  npm ci
  npm run build
  # Copy to install location

Task 5.1.2: Integrate with Autotools
  File: Makefile.am
  - Add webui-build target
  - Install dist files to $(datadir)/motion/webui

Task 5.1.3: Create development script
  File: scripts/dev.sh
  - Start Vite dev server
  - Optionally start Motion in background
```

### 5.2 Docker Integration

**Implementation Tasks**:

```
Task 5.2.1: Create multi-stage Dockerfile
  File: docker/Dockerfile
  - Stage 1: Build webui (Node)
  - Stage 2: Build Motion (C++)
  - Stage 3: Runtime image

Task 5.2.2: Create docker-compose for development
  File: docker/docker-compose.dev.yml
  - Motion container
  - Vite dev server container
  - Shared network
```

---

## Agent Execution Patterns

### Pattern 1: C++ Code Analysis

Before modifying C++ files, spawn Explore agents:

```
User: Start Phase 1.2 (SPA support)

Claude: Spawning analysis agents:
  - Agent 1: Read webu_file.cpp, identify modification points
  - Agent 2: Read webu_ans.cpp, understand request routing
  - Agent 3: Read conf.cpp, find parameter registration pattern

[Wait for results]

Claude: Based on analysis:
  - File serving is in cls_webu_file::main()
  - Need to add SPA fallback after line 142
  - Parameter pattern: add to config_parms[] at line 847
```

### Pattern 2: Parallel Component Development

For React components with no dependencies:

```
User: Implement base UI components

Claude: These are independent, implementing in parallel:
  - Task: Button.tsx
  - Task: Input.tsx
  - Task: Select.tsx
  - Task: Toggle.tsx

[All tasks execute concurrently]
```

### Pattern 3: Sequential C++ Changes

C++ changes require careful ordering:

```
User: Add new API endpoint

Claude: Sequential implementation required:
  1. First: Add config parameter in conf.hpp
  2. Then: Register in conf.cpp config_parms[]
  3. Then: Add handler in webu_json.cpp
  4. Finally: Add routing in webu_ans.cpp
```

---

## Testing Strategy

### C++ Testing

```
Task: Create API integration tests
  - Use curl to test endpoints
  - Verify JSON response format
  - Test authentication flow
  - Test file serving with SPA mode
```

### React Testing

```
Task: Create component tests
  - Vitest + Testing Library
  - Test each component in isolation
  - Mock API calls

Task: Create E2E tests
  - Playwright
  - Test camera viewing flow
  - Test config editing flow
  - Test media management
```

---

## Quality Gates

### Per-Phase Checklist

**Phase 1 (C++) Complete When**:
- [ ] SPA mode serves React build
- [ ] All new API endpoints return valid JSON
- [ ] Authentication works with new endpoints
- [ ] No memory leaks (valgrind check)
- [ ] Compiles with -Wall -Werror

**Phase 2-4 (React) Complete When**:
- [ ] TypeScript strict mode passes
- [ ] All components render without errors
- [ ] API integration works
- [ ] Mobile responsive
- [ ] Dark theme applied

**Phase 5 (Integration) Complete When**:
- [ ] Docker build succeeds
- [ ] Motion serves React UI
- [ ] All features work on Pi 4/5
- [ ] RAM usage < 30MB

---

## File Deliverables

### C++ Files Modified

| File | Changes |
|------|---------|
| `src/conf.hpp` | Add webcontrol_html_path, webcontrol_spa_mode |
| `src/conf.cpp` | Register new parameters |
| `src/webu_file.cpp` | SPA fallback, MIME types, cache headers |
| `src/webu_json.cpp` | New API endpoints |
| `src/webu_ans.cpp` | API routing |
| `Makefile.am` | webui build/install targets |

### React Files Created

```
webui/
├── src/
│   ├── components/
│   │   ├── ui/              # ~10 files
│   │   ├── camera/          # ~5 files
│   │   ├── settings/        # ~5 files
│   │   ├── media/           # ~5 files
│   │   └── layout/          # ~3 files
│   ├── features/
│   │   ├── auth/            # ~3 files
│   │   ├── cameras/         # ~3 files
│   │   ├── settings/        # ~15 files (sections)
│   │   └── media/           # ~3 files
│   ├── hooks/               # ~10 files
│   ├── api/                 # ~5 files
│   ├── store/               # ~3 files
│   ├── config/              # ~3 files
│   └── pages/               # ~5 files
├── public/                  # ~5 files
└── [config files]           # ~5 files
```

**Total**: ~100 React files, ~6 C++ files modified

---

## Success Metrics

| Metric | Target |
|--------|--------|
| RAM Usage | < 30 MB |
| Bundle Size (gzipped) | < 150 KB |
| Time to First Stream | < 1 second |
| Config Save Latency | < 200ms |
| Lighthouse Performance | > 90 |
| C++ Compile Warnings | 0 |
| TypeScript Errors | 0 |
