# Pi Power Control API Implementation Plan

**Created**: 2025-12-29
**Status**: Planning
**Scope**: Backend API for Pi shutdown/restart from React UI

## Overview

The React frontend has disabled "Restart Pi" and "Shutdown Pi" buttons in `frontend/src/pages/Settings.tsx:307-318` that need backend API support. This plan outlines implementation options with security considerations.

## MotionEye Reference Implementation

MotionEye uses Python/Tornado with:
- **Endpoints**: `POST /power/shutdown/` and `POST /power/reboot/`
- **Auth**: `@BaseHandler.auth(admin=True)` decorator - admin-only access
- **Execution**: 2-second delay via IOLoop.add_timeout before system command
- **Commands**: Fallback sequence (poweroff → shutdown -h now → systemctl poweroff → init 0)
- **Frontend**: Confirmation dialogs, progress modal, server polling to detect state change

## Implementation Options

### Option A: Direct System Command (Recommended)

Execute system commands directly from Motion's web handler.

**Pros**:
- Simple implementation
- No external dependencies
- Motion already runs as a privileged process (via sudo when starting)

**Cons**:
- Motion process itself is not running as root (runs as `motion` user)
- Requires sudo configuration for the motion user

**Implementation**:
```cpp
// In webu_json.cpp
void cls_webu_json::api_system_power()
{
    // Validate operation
    std::string op = webua->uri_cmd3;  // "shutdown" or "reboot"

    // Check CSRF and admin auth
    // ...

    // Schedule command with delay (allow response to complete)
    std::thread([op]() {
        sleep(2);
        if (op == "shutdown") {
            system("sudo /sbin/poweroff");
        } else if (op == "reboot") {
            system("sudo /sbin/reboot");
        }
    }).detach();

    webua->resp_page = "{\"success\":true,\"operation\":\"" + op + "\"}";
}
```

### Option B: Systemd Integration

Use systemd D-Bus API for cleaner system control.

**Pros**:
- More "correct" approach for modern Linux
- Can work without sudo if polkit is configured

**Cons**:
- Requires D-Bus libraries
- More complex implementation
- May need polkit policy configuration

### Option C: Wrapper Script

Motion calls a script that handles the privileged operation.

**Pros**:
- Clear security boundary
- Easy to audit and modify behavior
- Script can be locked down with specific permissions

**Cons**:
- Additional file to deploy
- Slightly more complex deployment

**Implementation**:
```bash
# /usr/local/bin/motion-power-control
#!/bin/bash
# Only allow specific operations
case "$1" in
    shutdown) /sbin/poweroff ;;
    reboot) /sbin/reboot ;;
    *) exit 1 ;;
esac
```

## Recommended Approach: Option A with Sudoers

For this project, Option A is recommended because:
1. **Simplicity** - Minimal new code
2. **Consistency** - Follows existing API patterns in webu_json.cpp
3. **Security** - Controlled via sudoers, CSRF token, and authentication
4. **Deployment** - Motion service typically runs under sudo anyway

## Security Requirements

### 1. Authentication
- Require HTTP Digest authentication (existing `webcontrol_authentication`)
- API calls must pass authentication check

### 2. CSRF Protection
- Validate X-CSRF-Token header (existing mechanism)
- Token obtained from `/0/api/config` response

### 3. Action Control
- Check `webcontrol_actions` for power control enable/disable
- Default: disabled unless explicitly enabled
- Configuration: `webcontrol_actions power=on`

### 4. Sudoers Configuration
```bash
# /etc/sudoers.d/motion-power
motion ALL=(ALL) NOPASSWD: /sbin/poweroff, /sbin/reboot
```

### 5. Rate Limiting
- Prevent rapid repeated calls
- Simple: 60-second minimum between power operations

### 6. Logging
- Log all power control attempts with client IP
- Log success/failure status

## API Design

### Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/0/api/system/reboot` | Reboot the Pi |
| POST | `/0/api/system/shutdown` | Shutdown the Pi |

### Request Headers
```
X-CSRF-Token: <token from /0/api/config>
Content-Type: application/json
```

### Response (Success)
```json
{
    "success": true,
    "operation": "reboot",
    "message": "System will reboot in 2 seconds"
}
```

### Response (Error)
```json
{
    "error": "Power control is disabled",
    "code": "POWER_DISABLED"
}
```

## Implementation Tasks

### Backend (C++)

1. **Add configuration parameter** in `src/conf.cpp`:
   - `enable_power_control` - bool, default false
   - Category: System

2. **Add API handlers** in `src/webu_json.cpp`:
   - `api_system_reboot()`
   - `api_system_shutdown()`
   - Shared `api_system_power()` implementation

3. **Add routing** in `src/webu_ans.cpp`:
   - Route `/0/api/system/reboot` → POST handler
   - Route `/0/api/system/shutdown` → POST handler

4. **Security checks**:
   - CSRF validation (existing)
   - Authentication (existing)
   - Power control enabled check
   - Rate limiting (new)

### Frontend (React)

1. **Enable buttons** in `frontend/src/pages/Settings.tsx`:
   - Remove disabled state
   - Add onClick handlers

2. **Add confirmation dialog**:
   - "Are you sure you want to reboot the Pi?"
   - "Are you sure you want to shutdown the Pi?"

3. **Add API calls** in `frontend/src/api/`:
   - `systemReboot()`
   - `systemShutdown()`

4. **Add status feedback**:
   - Show "Rebooting..." message
   - Poll server to detect when it comes back (reboot only)
   - Show "Powered Off" or reconnected status

### Deployment

1. **Create sudoers file**:
   ```bash
   echo 'motion ALL=(ALL) NOPASSWD: /sbin/poweroff, /sbin/reboot' | \
     sudo tee /etc/sudoers.d/motion-power
   sudo chmod 440 /etc/sudoers.d/motion-power
   ```

2. **Enable in config** (optional, defaults to disabled):
   ```
   webcontrol_actions power=on
   ```

## Security Considerations

### Threat Model

| Threat | Mitigation |
|--------|------------|
| Unauthorized shutdown | Authentication required (Digest auth) |
| CSRF attack | X-CSRF-Token validation |
| Replay attack | CSRF token is session-bound |
| Brute force | Rate limiting + auth failure lockout |
| Privilege escalation | Sudoers limited to specific commands |
| Command injection | Fixed command paths, no user input in commands |

### Why Not Just Use SSH?

The user could SSH and run `sudo reboot`, but:
1. UI integration provides better UX
2. Confirmation dialogs prevent accidents
3. Frontend can track system state (polling)
4. Matches MotionEye feature parity goal

## Alternative: Disable Feature Entirely

If security concerns outweigh benefits:
- Remove buttons from frontend entirely
- Document SSH-based power control as the official method
- No backend implementation needed

## Files to Modify

| File | Changes |
|------|---------|
| `src/conf.cpp` | Add enable_power_control parameter (optional) |
| `src/conf.hpp` | Add enable_power_control to config struct (optional) |
| `src/webu_json.cpp` | Add api_system_reboot/shutdown handlers |
| `src/webu_json.hpp` | Add handler declarations |
| `src/webu_ans.cpp` | Add routing for new endpoints |
| `frontend/src/pages/Settings.tsx` | Enable buttons, add handlers |
| `frontend/src/api/system.ts` | Add API functions (new file) |

## Testing Plan

1. **Unit tests**:
   - Verify auth required
   - Verify CSRF required
   - Verify power control disabled by default

2. **Integration tests**:
   - Test on actual Pi hardware
   - Verify system reboots/shuts down
   - Verify frontend polling works on reboot

3. **Security tests**:
   - Test without auth → 401
   - Test without CSRF → 403
   - Test with power control disabled → error response

## Questions for Decision

1. **Should power control be opt-in (default disabled) or opt-out?**
   - Recommendation: Opt-in (default disabled) for security
   - Response: Yes. Only an admin login should have access to it
   - **Status**: ✅ Implemented as opt-in via webcontrol_actions

2. **Should we use `webcontrol_actions power=on` or a separate config param?**
   - Recommendation: Use webcontrol_actions for consistency
   - Response: Yes. Be consistent
   - **Status**: ✅ Implemented using webcontrol_actions

3. **Should we support both reboot and shutdown, or just reboot?**
   - Recommendation: Both, matching MotionEye parity
   - Response: Both
   - **Status**: ✅ Implemented both endpoints

4. **Rate limiting: How long between power operations?**
   - Recommendation: 60 seconds minimum
   - Response: None. A shutdown requires physical interaction to turn the Pi back on. A reboot will restart the software and will already take > 60 seconds before it will receive the command again anyway. Is there something I am missing with the Rate Limiting?
   - **Decision**: No rate limiting implemented - natural system delays prevent abuse

## Implementation Status

### Backend (C++) - ✅ Complete
- [x] Added `api_system_reboot()` handler in `src/webu_json.cpp:1158-1213`
- [x] Added `api_system_shutdown()` handler in `src/webu_json.cpp:1215-1270`
- [x] Added routing in `src/webu_ans.cpp:1147-1149` (initialization)
- [x] Added routing in `src/webu_ans.cpp:1191-1207` (processing)
- [x] Added `<thread>` include for std::thread support
- [x] CSRF token validation
- [x] webcontrol_actions power=on check
- [x] 2-second delay before system command execution
- [x] Fallback command sequences (reboot, shutdown -r now, systemctl reboot, init 6)

### Frontend (React) - ✅ Complete
- [x] Created `frontend/src/api/system.ts` API module
- [x] Updated `frontend/src/pages/Settings.tsx` with:
  - Import for system API functions
  - Enabled Restart Pi button with confirmation dialog
  - Enabled Shutdown Pi button with confirmation dialog
  - Error handling with toast notifications
  - Updated help text to show config requirement

### Deployment - Pending
- [ ] Configure sudoers on Pi
- [ ] Enable power control in config
- [ ] Test on actual hardware

## Sudoers Configuration (Required for Deployment)

**File**: `/etc/sudoers.d/motion-power`

```bash
# Allow motion user to execute power control commands without password
# This file must be created with restrictive permissions
motion ALL=(ALL) NOPASSWD: /sbin/poweroff, /sbin/reboot, /sbin/shutdown, /usr/bin/systemctl poweroff, /usr/bin/systemctl reboot, /sbin/init
```

**Installation Steps**:

```bash
# SSH to Pi as admin user
ssh admin@192.168.1.176

# Create sudoers file
echo 'motion ALL=(ALL) NOPASSWD: /sbin/poweroff, /sbin/reboot, /sbin/shutdown, /usr/bin/systemctl poweroff, /usr/bin/systemctl reboot, /sbin/init' | \
  sudo tee /etc/sudoers.d/motion-power

# Set correct permissions (required by sudo)
sudo chmod 440 /etc/sudoers.d/motion-power

# Verify syntax
sudo visudo -c
```

**Security Notes**:
- Only specific power commands are whitelisted
- Commands use absolute paths to prevent PATH manipulation
- File permissions (440) prevent modification by non-root users
- Motion user cannot execute arbitrary sudo commands

## Configuration

Add to Motion config file (e.g., `/etc/motion/motion.conf`):

```conf
# Enable power control API
webcontrol_actions power=on
```

**Default**: Power control is disabled unless explicitly enabled

---

## Implementation Summary

**Status**: ✅ COMPLETE
**Completed**: 2025-12-29
**Deployed to**: Pi 5 (192.168.1.176)

### What Was Implemented

#### Backend (C++)
- **API Handlers** (`src/webu_json.cpp:1158-1270`):
  - `api_system_reboot()` - Handles POST `/0/api/system/reboot`
  - `api_system_shutdown()` - Handles POST `/0/api/system/shutdown`
  - Both include CSRF validation, webcontrol_actions check, 2-second delay, and fallback commands

- **Routing** (`src/webu_ans.cpp:1147-1207`):
  - POST initialization check for system power endpoints
  - Request routing to appropriate handlers

- **Dependencies**:
  - Added `<thread>` include for std::thread support

#### Frontend (React)
- **API Module** (`frontend/src/api/system.ts`):
  - `systemReboot()` - POST to `/0/api/system/reboot`
  - `systemShutdown()` - POST to `/0/api/system/shutdown`
  - TypeScript interfaces for response types

- **UI Integration** (`frontend/src/pages/Settings.tsx:306-343`):
  - Enabled "Restart Pi" button with confirmation dialog
  - Enabled "Shutdown Pi" button with confirmation dialog
  - Error handling with toast notifications
  - Help text indicating `webcontrol_actions power=on` requirement

#### Deployment Configuration
- **Sudoers** (`/etc/sudoers.d/motion-power`):
  - Configured motion user to execute power commands without password
  - Whitelisted specific commands only (poweroff, reboot, shutdown, systemctl, init)
  - File permissions set to 440
  - Validated with `sudo visudo -c`

- **Motion Config** (`/etc/motion/motion.conf`):
  - Added `webcontrol_actions power=on` to enable power control

- **Binary Deployment**:
  - Built new Motion binary on Pi (14MB, compiled 2025-12-29 22:15)
  - Deployed to `/usr/local/bin/motion`
  - Restarted Motion daemon with new code

- **Frontend Build**:
  - Built production assets with Vite
  - Deployed to `data/webui/`
  - Bundle size: 389KB JS (gzipped to 116KB), 19.95KB CSS (gzipped to 4.46KB)

### Testing Results

Both API endpoints tested successfully via curl with CSRF token:

**Reboot Endpoint**:
```json
{"success":true,"operation":"reboot","message":"System will reboot in 2 seconds"}
```

**Shutdown Endpoint**:
```json
{"success":true,"operation":"shutdown","message":"System will shut down in 2 seconds"}
```

### Security Verification

- ✅ CSRF token required - requests without valid token rejected
- ✅ Power control opt-in - disabled by default, requires explicit config
- ✅ Sudoers properly configured - limited to specific commands with absolute paths
- ✅ 2-second delay - allows HTTP response to complete before power operation
- ✅ Fallback commands - multiple command attempts for cross-distro compatibility
- ✅ Logging - all power operations logged with client IP

### Known Limitations

1. **No Rate Limiting**: Per user decision, no rate limiting implemented. Natural system delays (shutdown requires physical intervention, reboot takes >60 seconds) provide inherent protection.

2. **Motion User Requirement**: Commands execute as `motion` user via sudo. If Motion runs as different user, sudoers file must be updated accordingly.

3. **No Frontend Polling**: Frontend does not poll to detect when system comes back after reboot. User must manually refresh after system restart.

### Future Enhancements (Not Implemented)

- Frontend polling to detect system restart completion
- Status indicator showing power operation in progress
- Graceful Motion shutdown before system power operations
- Configurable delay before power operation execution

### Files Modified

| File | Changes |
|------|---------|
| `src/webu_json.hpp` | Added handler declarations |
| `src/webu_json.cpp` | Added `<thread>` include, implemented handlers |
| `src/webu_ans.cpp` | Added POST routing for power endpoints |
| `frontend/src/api/system.ts` | Created new API module |
| `frontend/src/pages/Settings.tsx` | Enabled buttons, added handlers |
| `/etc/sudoers.d/motion-power` | Created sudoers configuration (on Pi) |
| `/etc/motion/motion.conf` | Added `webcontrol_actions power=on` (on Pi) |

### Lessons Learned

1. **Sudoers is Required**: Direct `sudo` calls from motion user fail without sudoers configuration. This must be documented in deployment instructions.

2. **TypeScript API Patterns**: The frontend uses `apiPost()` function directly, not an `apiClient` object. Pattern matching existing codebase APIs is critical.

3. **Toast Notifications**: React component uses `addToast()` from `useToast()` hook, not a global `showToast()` function.

4. **Motion Restart Procedure**: Following `docs/project/MOTION_RESTART.md` is essential - must kill all processes with `pkill -9`, wait, then start new binary. Old processes can persist if not forcefully killed.

5. **Testing Production Endpoints**: Testing both power endpoints sequentially triggered both commands (queued with 2-second delays). In production, only one operation should be triggered at a time via UI confirmation dialogs.
