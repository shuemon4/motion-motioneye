# Security Implementation Handoff

**Date**: 2025-12-18
**Status**: Phase 1 in progress (CSRF token definition added, implementation incomplete)

## Context

Implementing security improvements based on `doc/analysis/20251218-security-assessment.md`. A detailed implementation plan has been created at `doc/plans/20251218-security-implementation-plan.md`.

## What Has Been Done

1. **Reviewed security assessment** - Read and analyzed all findings
2. **Created implementation plan** - 7 phases covering all security recommendations
3. **Started Phase 1 (CSRF)** - Added CSRF token field to `src/webu.hpp`:
   - Added `#define CSRF_TOKEN_LENGTH 64`
   - Added `std::string csrf_token` member to `cls_webu`
   - Added method declarations: `csrf_generate()` and `csrf_validate()`
   - Added `#include <cstdio>` to `src/webu.cpp`

## What Needs To Be Implemented

### Phase 1: CSRF Protection (PARTIALLY STARTED)

**Remaining work:**

1. **Implement `csrf_generate()` in `src/webu.cpp`**:
   - Generate 32 random bytes from `/dev/urandom`
   - Hex-encode to 64-character string
   - Store in `csrf_token` member
   - Call from `startup()` method

2. **Implement `csrf_validate()` in `src/webu.cpp`**:
   - Constant-time comparison to prevent timing attacks
   - Return false for empty tokens or mismatches

3. **Modify `src/webu_post.cpp`**:
   - In `parse_cmd()` or `process_actions()`, extract `csrf_token` from POST data
   - Call `webu->csrf_validate()` before processing actions
   - Reject requests with invalid/missing tokens with error response

4. **Modify `src/webu_html.cpp`**:
   - Add hidden `csrf_token` field to all forms
   - Access token via `webua->webu->csrf_token`

### Phase 2: Security Headers (NOT STARTED)

Modify `src/webu_ans.cpp` `mhd_send()` to add default headers:
- `X-Content-Type-Options: nosniff`
- `X-Frame-Options: SAMEORIGIN`
- `X-XSS-Protection: 1; mode=block`
- `Referrer-Policy: strict-origin-when-cross-origin`
- CSP header for HTML responses

### Phase 3: Command Injection Prevention (NOT STARTED)

Modify `src/util.cpp`:
- Add shell metacharacter validation/sanitization
- Reject dangerous characters: `` ` `` `$` `|` `;` `&` `>` `<` `\n` `\r`

### Phase 4: Compiler Hardening (NOT STARTED)

Modify `configure.ac`:
- Add `-fstack-protector-strong`
- Add `-D_FORTIFY_SOURCE=2`
- Add PIE flags (`-fPIE`, `-pie`)
- Add RELRO linker flags

### Phase 5: Credential Management (NOT STARTED)

- Support HA1 digest storage for passwords
- Environment variable expansion for secrets (`$VAR` or `${VAR}`)
- Mask sensitive values in logs

### Phase 6: Lockout & SQL (NOT STARTED)

- Track lockout by (username + IP) instead of IP only
- Restrict SQL template editing via web UI
- Sanitize SQL value substitutions

### Phase 7: Defense in Depth (NOT STARTED)

- Audit/replace `sprintf()` with `snprintf()` in web code
- Add path traversal validation in `src/webu_file.cpp`
- Update default config for secure defaults

## Key Files

| File | Purpose |
|------|---------|
| `src/webu.hpp` | CSRF token declaration (MODIFIED) |
| `src/webu.cpp` | CSRF implementation (PARTIALLY MODIFIED) |
| `src/webu_post.cpp` | POST processing, needs CSRF validation |
| `src/webu_html.cpp` | HTML generation, needs CSRF token in forms |
| `src/webu_ans.cpp` | Response handling, needs security headers |
| `src/util.cpp` | Command execution, needs shell sanitization |
| `configure.ac` | Build config, needs hardening flags |

## Instructions for New Session

1. Read `doc/plans/20251218-security-implementation-plan.md` for full details
2. Complete Phase 1 CSRF implementation first
3. At each phase completion:
   - Update the Phase Summary section in the plan document
   - Spawn a sub-agent to review the implementation
   - Address any issues before moving to next phase
4. Test builds with `autoreconf -fiv && ./configure --with-libcam --with-sqlite3 && make -j4`

## Execution Command

```
Review doc/plans/20251218-security-implementation-plan.md and continue implementation from where it left off. Phase 1 (CSRF) is partially complete - the token field was added to webu.hpp but the implementation functions are not yet written. Complete Phase 1 first, then proceed through remaining phases. At completion of each phase, write a summary and spawn a sub-agent to review the code changes.
```
