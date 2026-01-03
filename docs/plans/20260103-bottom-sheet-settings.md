# Bottom Sheet Settings Implementation Plan

**Created**: 2026-01-03
**Status**: Draft - Awaiting Approval
**Goal**: Add a bottom sheet component to the Dashboard page for editing hot-reloadable settings while viewing the live stream

## Overview

Implement a mobile-friendly bottom sheet that slides up from the bottom of the screen on the Dashboard page, allowing users to adjust hot-reloadable camera settings while viewing the live video stream in real-time.

## Current State

- **Dashboard** (`frontend/src/pages/Dashboard.tsx`): Shows camera streams in responsive grid
- **Settings** (`frontend/src/pages/Settings.tsx`): Full-page settings editor with 13+ setting categories
- **Form Components**: `FormSection`, `FormInput`, `FormToggle`, `FormSelect`, `FormSlider`
- **API**: `useBatchUpdateConfig()` mutation with hot-reload detection in response
- **No existing sheet/drawer component** - using fixed overlays for modals

## Design Decisions

### Which Settings to Include

Focus on **hot-reloadable settings** that have visual impact on the stream:

| Category | Settings | Visual Impact |
|----------|----------|---------------|
| **Stream** | `stream_quality`, `stream_maxrate` | Stream quality/smoothness |
| **Overlay** | `text_left`, `text_right`, `text_changes` | On-screen text |
| **Image** | `brightness`, `contrast`, `saturation`, `hue` | Color/exposure |
| **Detection** | `threshold`, `noise_level`, `despeckle_filter` | Motion boxes (if enabled) |
| **libcamera** | `libcam_brightness`, `libcam_contrast`, `libcam_saturation` | Direct camera adjustment |

### Settings NOT Included (require restart)

- Resolution, framerate, rotation
- Device/camera selection
- Storage paths
- Authentication settings

## Component Architecture

```
Dashboard.tsx
├── CameraStream (existing)
└── QuickSettingsSheet (new)
    ├── SheetHandle (drag indicator)
    ├── SheetHeader (title, camera selector, close)
    └── SheetContent (scrollable)
        ├── ConfigurationPresets (new - placeholder)
        │   ├── Preset dropdown selector
        │   └── Apply button
        ├── StreamSettings (subset)
        ├── ImageSettings (subset)
        ├── OverlaySettings (subset)
        └── DetectionSettings (subset)
```

## Configuration Presets (Placeholder)

**Status**: UI placeholder only - backend not yet implemented

This section allows users to save and load complete configuration presets. Unlike hot-reloadable settings, applying a preset may include parameters that require a daemon restart.

### UI Behavior

1. **Preset Selector**: Dropdown showing available presets
   - "Default" (always available)
   - User-saved presets (future)
   - "Save Current as..." option (future)

2. **Apply Button**: Required because presets may include non-hot-reloadable settings
   - Disabled when no preset selected or current config matches preset
   - Shows warning if restart required
   - Triggers full config update via existing API

3. **Placeholder State** (this implementation):
   - Dropdown shows "Default" only with "(Coming Soon)" indicator
   - Apply button disabled with tooltip: "Configuration presets coming soon"
   - Visual design complete, ready for backend integration

### Future Backend Requirements (separate task)

When implementing the full feature, backend needs:
- `GET /0/api/presets` - List saved presets
- `POST /0/api/presets` - Save new preset
- `DELETE /0/api/presets/{id}` - Delete preset
- `GET /0/api/presets/{id}` - Get preset config values
- Presets stored in SQLite or JSON file

## Implementation Steps

### Phase 1: Bottom Sheet Component (Core UI)

**File**: `frontend/src/components/BottomSheet.tsx`

Features:
- Fixed to bottom of screen
- Three states: closed, peek (25% height), expanded (75% height)
- Touch/drag gestures on mobile (swipe up/down)
- Click/keyboard on desktop
- Backdrop overlay when expanded
- Smooth CSS transitions
- ESC key to close
- Click outside to close

```typescript
interface BottomSheetProps {
  isOpen: boolean
  onClose: () => void
  title: string
  children: React.ReactNode
}
```

### Phase 2: Quick Settings Content

**File**: `frontend/src/components/QuickSettings.tsx`

Features:
- Reuse existing form components (`FormSlider`, `FormToggle`, etc.)
- Subset of hot-reloadable settings organized in collapsible sections
- Same `onChange` pattern as Settings page
- Immediate API calls on change (no "Save" button - settings apply instantly)
- Visual feedback: brief highlight/pulse on successful apply
- Error toast if setting fails

```typescript
interface QuickSettingsProps {
  cameraId: string
  config: ConfigData
  onSettingChange: (param: string, value: string | number | boolean) => void
}
```

### Phase 3: Dashboard Integration

**File**: `frontend/src/pages/Dashboard.tsx` (modify)

Changes:
- Add "Quick Settings" button (gear icon) to camera header
- Import and render `BottomSheet` with `QuickSettings` content
- State for sheet open/closed and selected camera
- Fetch config using existing `useConfig()` query
- Use `useBatchUpdateConfig()` for changes

### Phase 4: Gesture Handling

**File**: `frontend/src/hooks/useSheetGestures.ts`

Features:
- Touch event handlers for swipe up/down
- Velocity detection for snap behavior
- Prevent body scroll when sheet is open
- Mouse drag support for desktop testing

## UI/UX Specifications

### Visual Design

```
┌─────────────────────────────────────────┐
│  Camera Dashboard                       │
│  ┌───────────────────────────────────┐  │
│  │ ● Camera 1        1920x1080  [⚙]  │  │  ← Gear icon opens sheet
│  │ ┌─────────────────────────────────┤  │
│  │ │                                 │  │
│  │ │     [Live Video Stream]         │  │
│  │ │                                 │  │
│  │ └─────────────────────────────────┤  │
│  └───────────────────────────────────┘  │
│                                         │
├─────────────────────────────────────────┤  ← Sheet rises from here
│  ════════════════════                   │  ← Drag handle
│  Quick Settings            Camera 1  ✕  │
│─────────────────────────────────────────│
│  Configuration                          │
│  ┌─────────────────────┐ ┌───────────┐  │
│  │ Default (Coming Soon)▼│ │  Apply  │  │  ← Preset + Apply button
│  └─────────────────────┘ └───────────┘  │
│─────────────────────────────────────────│
│  ▼ Stream                               │
│    Quality          ════════○══  75%    │
│    Max FPS          ════○══════  15     │
│                                         │
│  ▼ Image                                │
│    Brightness       ════════○══  50     │
│    Contrast         ═══○═══════  40     │
│    Saturation       ════════○══  50     │
│                                         │
│  ▼ Overlay                              │
│    Show timestamp   [ON]                │
│    Show camera name [ON]                │
│                                         │
└─────────────────────────────────────────┘
```

### Responsive Behavior

| Screen Size | Sheet Max Height | Behavior |
|-------------|------------------|----------|
| Mobile (<640px) | 85vh | Full swipe gestures, larger touch targets |
| Tablet (640-1024px) | 60vh | Mixed touch/click |
| Desktop (>1024px) | 50vh | Click/keyboard, smaller sheet |

### Animation Timing

- Open: 300ms ease-out
- Close: 250ms ease-in
- Backdrop fade: 200ms

## State Management

```typescript
// In Dashboard.tsx
const [sheetOpen, setSheetOpen] = useState(false)
const [sheetCamera, setSheetCamera] = useState<string | null>(null)

// Fetch config for selected camera
const { data: config } = useConfig(sheetCamera ?? '0')

// Mutation for instant apply
const { mutate: updateConfig } = useBatchUpdateConfig()

const handleSettingChange = (param: string, value: unknown) => {
  updateConfig({
    cameraId: sheetCamera ?? '0',
    changes: { [param]: value }
  })
}
```

## File Changes Summary

| File | Action | Description |
|------|--------|-------------|
| `frontend/src/components/BottomSheet.tsx` | Create | Reusable bottom sheet component |
| `frontend/src/components/QuickSettings.tsx` | Create | Hot-reloadable settings content |
| `frontend/src/components/ConfigurationPresets.tsx` | Create | Preset selector + Apply button (placeholder) |
| `frontend/src/hooks/useSheetGestures.ts` | Create | Touch/drag gesture handling |
| `frontend/src/pages/Dashboard.tsx` | Modify | Add sheet trigger and integration |
| `frontend/src/components/form/index.ts` | Modify | Export any new form variants if needed |

## Testing Checklist

- [ ] Sheet opens/closes smoothly on desktop
- [ ] Swipe gestures work on mobile (iOS Safari, Chrome Android)
- [ ] Settings changes apply immediately to stream
- [ ] Error handling for failed setting updates
- [ ] ESC key closes sheet
- [ ] Click outside closes sheet
- [ ] Body scroll locked when sheet is open
- [ ] Keyboard navigation works (Tab through controls)
- [ ] Works with single camera layout
- [ ] Works with multi-camera layout (sheet for specific camera)
- [ ] Configuration Presets placeholder shows "Coming Soon" state
- [ ] Apply button disabled with appropriate tooltip

## Future Enhancements (Out of Scope)

- **Configuration Presets Backend** - Full save/load/delete preset API (see placeholder section above)
- Recently changed settings history
- Settings search/filter
- Haptic feedback on mobile
- Keyboard shortcuts for common adjustments

## Dependencies

- No new npm packages required
- Uses existing Tailwind CSS
- Uses existing TanStack Query patterns
- Uses existing form components

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| Touch conflicts with stream scroll | Sheet captures touch events, stream remains static |
| Too many API calls on slider drag | Debounce slider changes (300ms) |
| Settings fail silently | Toast notification on error, highlight failed field |
| Sheet covers important stream area | Peek mode shows minimal UI, user can dismiss |

## Estimated Effort

| Phase | Complexity | Files |
|-------|------------|-------|
| Phase 1: Bottom Sheet | Medium | 1 new |
| Phase 2: Quick Settings | Low | 1 new |
| Phase 2b: Configuration Presets | Low | 1 new (placeholder) |
| Phase 3: Dashboard Integration | Low | 1 modify |
| Phase 4: Gesture Handling | Medium | 1 new |

---

## Approval

- [x] User approved plan
- [x] Ready for implementation

## Implementation Status

**Completed**: 2026-01-03

### Files Created
- `frontend/src/components/BottomSheet.tsx` - Reusable bottom sheet with gesture support
- `frontend/src/components/QuickSettings.tsx` - Hot-reloadable settings content
- `frontend/src/components/ConfigurationPresets.tsx` - Preset selector placeholder (Coming Soon)
- `frontend/src/hooks/useSheetGestures.ts` - Touch/mouse drag gesture handling

### Files Modified
- `frontend/src/pages/Dashboard.tsx` - Added gear icon button and bottom sheet integration
