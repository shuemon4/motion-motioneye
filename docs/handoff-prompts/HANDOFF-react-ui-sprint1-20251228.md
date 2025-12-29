# Handoff Prompt: React UI Sprint 1 - Foundation

**Date**: 2025-12-28
**Target**: Fresh Claude Code session with sub-agent capability
**Estimated Effort**: 4-7 hours

---

## Context

You are continuing development of a React UI for the Motion video surveillance daemon. The project combines Motion (C++ backend) with a modern React frontend, eliminating the Python/MotionEye layer entirely.

### Repository
- **Location**: `/Users/tshuey/Documents/GitHub/motion-motioneye`
- **Frontend**: `/Users/tshuey/Documents/GitHub/motion-motioneye/frontend`

### What's Already Done
- C++ backend serves React from `data/webui/` with SPA routing
- React app initialized with Vite + TypeScript + Tailwind + React Query
- Basic camera streaming works via `<img src="/{camId}/stream">`
- 3 API endpoints: `/0/api/auth/me`, `/0/api/media/pictures`, `/0/api/system/temperature`
- Build output: 81 KB gzipped

### Current Problem
The Dashboard hardcodes camera IDs `[0, 1]` instead of fetching from the API. There's no proper API layer with TypeScript types or React Query hooks.

---

## Sprint 1 Objectives

### Goal
Create a solid API foundation and dynamic camera discovery so the Dashboard shows real cameras from Motion's API.

### Deliverables

1. **TypeScript API types** (`frontend/src/api/types.ts`)
2. **React Query hooks** (`frontend/src/api/queries.ts`)
3. **Dynamic camera list** in Dashboard
4. **Error boundary** + **toast notifications**
5. **Sub-agent summary** in `docs/sub-agent-summaries/`

---

## Task Breakdown

### Task 1: Create API Types (30 min)

**File**: `frontend/src/api/types.ts`

```typescript
// Camera from Motion's config
export interface Camera {
  id: number;
  name: string;
  enabled: boolean;
  width?: number;
  height?: number;
}

// Single config parameter
export interface ConfigParam {
  name: string;
  value: string | number | boolean;
  category: string;
  type: 'string' | 'number' | 'boolean' | 'list';
}

// Full config response
export interface Config {
  [key: string]: string | number | boolean;
}

// Media item (snapshot or movie)
export interface MediaItem {
  id: number;
  filename: string;
  path: string;
  date: string;
  time?: string;
  size: number;
}

// Pictures API response
export interface PicturesResponse {
  pictures: MediaItem[];
}

// System temperature response
export interface TemperatureResponse {
  celsius: number;
  fahrenheit: number;
}

// Auth status response
export interface AuthResponse {
  authenticated: boolean;
  auth_method?: string;
}

// API error
export interface ApiError {
  message: string;
  status?: number;
}
```

---

### Task 2: Create React Query Hooks (1-2 hours)

**File**: `frontend/src/api/queries.ts`

Create hooks using TanStack Query:

```typescript
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiGet, apiPost } from './client';
import type { Config, PicturesResponse, TemperatureResponse, AuthResponse, Camera } from './types';

// Query keys for cache management
export const queryKeys = {
  config: (camId?: number) => ['config', camId] as const,
  cameras: ['cameras'] as const,
  pictures: (camId: number) => ['pictures', camId] as const,
  temperature: ['temperature'] as const,
  auth: ['auth'] as const,
};

// Fetch all config (camera 0 = global)
export function useConfig(camId: number = 0) {
  return useQuery({
    queryKey: queryKeys.config(camId),
    queryFn: () => apiGet<Config>(`/${camId}/config`),
  });
}

// Fetch camera list
// Motion doesn't have a dedicated /cameras endpoint, so we parse from config
// Camera IDs are typically 0, 1, 2, etc. Check webcontrol_port and camera configs
export function useCameras() {
  return useQuery({
    queryKey: queryKeys.cameras,
    queryFn: async () => {
      // Try to get camera count from config or detect available cameras
      // Motion uses camera_id in each camera config section
      // For now, probe cameras 0-9 and see which respond
      const cameras: Camera[] = [];
      for (let id = 0; id <= 9; id++) {
        try {
          const response = await fetch(`/${id}/detection/status`);
          if (response.ok) {
            cameras.push({ id, name: `Camera ${id}`, enabled: true });
          }
        } catch {
          // Camera doesn't exist, skip
        }
      }
      return cameras;
    },
    staleTime: 60000, // Cache for 1 minute
  });
}

// Fetch pictures for a camera
export function usePictures(camId: number) {
  return useQuery({
    queryKey: queryKeys.pictures(camId),
    queryFn: () => apiGet<PicturesResponse>(`/${camId}/api/media/pictures`),
  });
}

// Fetch system temperature
export function useTemperature() {
  return useQuery({
    queryKey: queryKeys.temperature,
    queryFn: () => apiGet<TemperatureResponse>(`/0/api/system/temperature`),
    refetchInterval: 30000, // Refresh every 30s
  });
}

// Fetch auth status
export function useAuth() {
  return useQuery({
    queryKey: queryKeys.auth,
    queryFn: () => apiGet<AuthResponse>(`/0/api/auth/me`),
  });
}

// Update config parameter
export function useUpdateConfig() {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ camId, param, value }: { camId: number; param: string; value: string }) => {
      return apiPost(`/${camId}/config/set?${param}=${encodeURIComponent(value)}`, {});
    },
    onSuccess: (_, { camId }) => {
      queryClient.invalidateQueries({ queryKey: queryKeys.config(camId) });
    },
  });
}
```

**Note**: Motion's API uses different URL patterns. Research the actual endpoints by checking:
- `src/webu_json.cpp` for JSON endpoints
- `src/webu_ans.cpp` for URL routing
- Test with `curl http://localhost:7999/0/detection/status`

---

### Task 3: Update API Client (30 min)

**File**: `frontend/src/api/client.ts`

Enhance the existing client:

```typescript
import type { ApiError } from './types';

const API_TIMEOUT = 10000; // 10 seconds

class ApiClientError extends Error {
  status?: number;

  constructor(message: string, status?: number) {
    super(message);
    this.name = 'ApiClientError';
    this.status = status;
  }
}

export async function apiGet<T>(endpoint: string): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  try {
    const response = await fetch(endpoint, {
      method: 'GET',
      headers: {
        'Accept': 'application/json',
      },
      credentials: 'same-origin',
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    if (!response.ok) {
      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    return response.json();
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}

export async function apiPost<T>(endpoint: string, data: unknown): Promise<T> {
  const controller = new AbortController();
  const timeoutId = setTimeout(() => controller.abort(), API_TIMEOUT);

  try {
    const response = await fetch(endpoint, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Accept': 'application/json',
      },
      credentials: 'same-origin',
      body: JSON.stringify(data),
      signal: controller.signal,
    });

    clearTimeout(timeoutId);

    if (!response.ok) {
      throw new ApiClientError(`HTTP ${response.status}: ${response.statusText}`, response.status);
    }

    // Some endpoints return empty response
    const text = await response.text();
    return text ? JSON.parse(text) : ({} as T);
  } catch (error) {
    clearTimeout(timeoutId);
    if (error instanceof ApiClientError) throw error;
    if (error instanceof Error && error.name === 'AbortError') {
      throw new ApiClientError('Request timeout', 408);
    }
    throw new ApiClientError(error instanceof Error ? error.message : 'Unknown error');
  }
}
```

---

### Task 4: Update Dashboard with Dynamic Cameras (1 hour)

**File**: `frontend/src/pages/Dashboard.tsx`

Replace hardcoded cameras with API data:

```typescript
import { useCameras } from '@/api/queries';
import CameraStream from '@/components/CameraStream';

export default function Dashboard() {
  const { data: cameras, isLoading, error } = useCameras();

  if (isLoading) {
    return (
      <div className="p-6">
        <h1 className="text-2xl font-bold mb-6">Cameras</h1>
        <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-3">
          {[1, 2].map((i) => (
            <div key={i} className="bg-surface-elevated rounded-lg p-4 animate-pulse">
              <div className="h-6 bg-surface rounded w-1/3 mb-4"></div>
              <div className="aspect-video bg-surface rounded"></div>
            </div>
          ))}
        </div>
      </div>
    );
  }

  if (error) {
    return (
      <div className="p-6">
        <h1 className="text-2xl font-bold mb-6">Cameras</h1>
        <div className="bg-danger/10 border border-danger rounded-lg p-4">
          <p className="text-danger">Failed to load cameras: {error.message}</p>
          <button
            className="mt-2 text-sm text-primary hover:underline"
            onClick={() => window.location.reload()}
          >
            Retry
          </button>
        </div>
      </div>
    );
  }

  if (!cameras || cameras.length === 0) {
    return (
      <div className="p-6">
        <h1 className="text-2xl font-bold mb-6">Cameras</h1>
        <div className="bg-surface-elevated rounded-lg p-8 text-center">
          <p className="text-gray-400">No cameras configured</p>
          <p className="text-sm text-gray-500 mt-2">
            Add cameras in Motion's configuration file
          </p>
        </div>
      </div>
    );
  }

  return (
    <div className="p-6">
      <h1 className="text-2xl font-bold mb-6">Cameras ({cameras.length})</h1>
      <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-3">
        {cameras.map((camera) => (
          <div key={camera.id} className="bg-surface-elevated rounded-lg overflow-hidden">
            <div className="p-3 border-b border-surface">
              <h2 className="font-medium">{camera.name}</h2>
            </div>
            <CameraStream camId={camera.id} />
          </div>
        ))}
      </div>
    </div>
  );
}
```

---

### Task 5: Add Error Boundary (30 min)

**File**: `frontend/src/components/ErrorBoundary.tsx`

```typescript
import { Component, ReactNode } from 'react';

interface Props {
  children: ReactNode;
  fallback?: ReactNode;
}

interface State {
  hasError: boolean;
  error?: Error;
}

export class ErrorBoundary extends Component<Props, State> {
  constructor(props: Props) {
    super(props);
    this.state = { hasError: false };
  }

  static getDerivedStateFromError(error: Error): State {
    return { hasError: true, error };
  }

  componentDidCatch(error: Error, errorInfo: React.ErrorInfo) {
    console.error('ErrorBoundary caught:', error, errorInfo);
  }

  render() {
    if (this.state.hasError) {
      return this.props.fallback || (
        <div className="min-h-screen bg-surface flex items-center justify-center p-6">
          <div className="bg-surface-elevated rounded-lg p-8 max-w-md text-center">
            <h1 className="text-xl font-bold text-danger mb-4">Something went wrong</h1>
            <p className="text-gray-400 mb-4">
              {this.state.error?.message || 'An unexpected error occurred'}
            </p>
            <button
              onClick={() => window.location.reload()}
              className="px-4 py-2 bg-primary hover:bg-primary-hover rounded text-white"
            >
              Reload page
            </button>
          </div>
        </div>
      );
    }

    return this.props.children;
  }
}
```

---

### Task 6: Add Toast Notifications (1 hour)

**File**: `frontend/src/components/Toast.tsx`

```typescript
import { createContext, useContext, useState, useCallback, ReactNode } from 'react';

type ToastType = 'success' | 'error' | 'info' | 'warning';

interface Toast {
  id: string;
  message: string;
  type: ToastType;
}

interface ToastContextType {
  toasts: Toast[];
  addToast: (message: string, type?: ToastType) => void;
  removeToast: (id: string) => void;
}

const ToastContext = createContext<ToastContextType | null>(null);

export function ToastProvider({ children }: { children: ReactNode }) {
  const [toasts, setToasts] = useState<Toast[]>([]);

  const addToast = useCallback((message: string, type: ToastType = 'info') => {
    const id = Math.random().toString(36).slice(2);
    setToasts((prev) => [...prev, { id, message, type }]);

    // Auto-remove after 5 seconds
    setTimeout(() => {
      setToasts((prev) => prev.filter((t) => t.id !== id));
    }, 5000);
  }, []);

  const removeToast = useCallback((id: string) => {
    setToasts((prev) => prev.filter((t) => t.id !== id));
  }, []);

  return (
    <ToastContext.Provider value={{ toasts, addToast, removeToast }}>
      {children}
      <ToastContainer toasts={toasts} onRemove={removeToast} />
    </ToastContext.Provider>
  );
}

export function useToast() {
  const context = useContext(ToastContext);
  if (!context) throw new Error('useToast must be used within ToastProvider');
  return context;
}

function ToastContainer({ toasts, onRemove }: { toasts: Toast[]; onRemove: (id: string) => void }) {
  if (toasts.length === 0) return null;

  const typeStyles: Record<ToastType, string> = {
    success: 'bg-green-600',
    error: 'bg-danger',
    warning: 'bg-yellow-600',
    info: 'bg-primary',
  };

  return (
    <div className="fixed bottom-4 right-4 z-50 flex flex-col gap-2">
      {toasts.map((toast) => (
        <div
          key={toast.id}
          className={`${typeStyles[toast.type]} text-white px-4 py-3 rounded-lg shadow-lg flex items-center gap-3 min-w-[250px] animate-slide-in`}
        >
          <span className="flex-1">{toast.message}</span>
          <button
            onClick={() => onRemove(toast.id)}
            className="text-white/70 hover:text-white"
          >
            &times;
          </button>
        </div>
      ))}
    </div>
  );
}
```

**Add animation to** `frontend/src/index.css`:
```css
@keyframes slide-in {
  from {
    transform: translateX(100%);
    opacity: 0;
  }
  to {
    transform: translateX(0);
    opacity: 1;
  }
}

.animate-slide-in {
  animation: slide-in 0.3s ease-out;
}
```

---

### Task 7: Update main.tsx with Providers (15 min)

**File**: `frontend/src/main.tsx`

```typescript
import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { BrowserRouter } from 'react-router-dom';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import { ErrorBoundary } from '@/components/ErrorBoundary';
import { ToastProvider } from '@/components/Toast';
import App from './App';
import './index.css';

const queryClient = new QueryClient({
  defaultOptions: {
    queries: {
      staleTime: 30000, // 30 seconds
      retry: 2,
      refetchOnWindowFocus: false,
    },
  },
});

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <ErrorBoundary>
      <QueryClientProvider client={queryClient}>
        <ToastProvider>
          <BrowserRouter>
            <App />
          </BrowserRouter>
        </ToastProvider>
      </QueryClientProvider>
    </ErrorBoundary>
  </StrictMode>
);
```

---

## Research Required

Before implementing `useCameras()`, investigate Motion's actual API:

1. **Check existing endpoints**:
   ```bash
   # Test on Pi or with Motion running
   curl http://localhost:7999/0/config | head -50
   curl http://localhost:7999/0/detection/status
   curl http://localhost:7999/1/detection/status
   ```

2. **Read source code**:
   - `src/webu_json.cpp` - Look for camera list functionality
   - `src/webu_ans.cpp` - URL routing patterns

3. **Document findings** in the sub-agent summary

---

## Sub-Agent Usage

You may spawn sub-agents for parallel work:

| Agent | Task |
|-------|------|
| Explore | Research Motion's camera API endpoints |
| Implementation | Create API types and hooks |
| Implementation | Update Dashboard component |
| Implementation | Create Toast/ErrorBoundary |

**All sub-agents MUST write a summary to** `docs/sub-agent-summaries/` with naming convention:
```
YYYYMMDD-hhmm-description.md
```

Example: `20251228-1430-sprint1-api-layer.md`

---

## Testing

### Development Testing
```bash
cd frontend
npm run dev
# Opens http://localhost:5173
# Vite proxies API calls to Motion at localhost:7999
```

### Production Testing on Pi
```bash
# Build
cd frontend && npm run build

# Deploy
rsync -avz --exclude='node_modules' \
  /Users/tshuey/Documents/GitHub/motion-motioneye/ \
  admin@192.168.1.176:~/motion-motioneye/

# Install webui
ssh admin@192.168.1.176 "sudo cp -r ~/motion-motioneye/data/webui /usr/share/motion/"

# Restart Motion
ssh admin@192.168.1.176 "sudo systemctl restart motion"

# Test
curl http://192.168.1.176:7999/
```

---

## Success Criteria

- [ ] TypeScript types cover all API responses
- [ ] React Query hooks work for config, cameras, media, temperature
- [ ] Dashboard dynamically loads cameras from API
- [ ] Loading and error states display correctly
- [ ] Toast notifications appear for errors
- [ ] ErrorBoundary catches React errors
- [ ] No TypeScript errors (`npm run build` succeeds)
- [ ] Works on Pi 5 (if available for testing)
- [ ] Sub-agent summary written to `docs/sub-agent-summaries/`

---

## Reference Files

| File | Purpose |
|------|---------|
| `docs/plans/react-ui/README.md` | Project overview |
| `docs/plans/react-ui/01-architecture-20251227.md` | Architecture decisions |
| `docs/plans/react-ui/02-detailed-execution-20251227.md` | Full execution plan |
| `docs/plans/react-ui/03-completion-plan-20251228.md` | Sprint breakdown |
| `docs/sub-agent-summaries/20251227-react-ui-phase1-backend.md` | C++ work done |
| `docs/sub-agent-summaries/20251227-react-ui-phase2-frontend.md` | React setup done |
| `docs/sub-agent-summaries/20251228-0020-react-ui-phase3-integration.md` | Integration tested |

---

## Important Notes

1. **CPU efficiency is paramount** - Pi has limited resources
2. **Ask before connecting to Pi** - Verify it's powered on
3. **Don't modify `src/alg.cpp`** - Motion detection algorithm is off-limits
4. **Use existing patterns** - Follow established code conventions
5. **Write summary when done** - Required in `docs/sub-agent-summaries/`
