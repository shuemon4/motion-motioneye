# Motion Security Assessment Report

**Date:** 2025-12-18 (Revised)
**Auditor:** Claude Code Security Analysis
**Project:** Motion (Video Motion Detection Daemon)
**Target:** Raspberry Pi 5 with Camera Module v3

---

## Executive Summary

Motion demonstrates **reasonable security posture** for a local network video monitoring application. The codebase implements essential security controls including authentication, account lockout, TLS support, and localhost binding. However, several areas warrant attention for deployments exposed to untrusted networks.

**Overall Security Level:** MODERATE

| Category | Rating | Priority |
|----------|--------|----------|
| Authentication & Access Control | ★★★☆☆ | Needs Attention |
| Web Application Security | ★★☆☆☆ | Medium Risk |
| Command Injection Prevention | ★★☆☆☆ | Medium Risk |
| SQL Security | ★★☆☆☆ | Medium Risk |
| Network Security | ★★★★☆ | Good |
| Credential Management | ★★☆☆☆ | Medium Risk |
| File Access Control | ★★★★☆ | Good |

---

## 1. Authentication & Authorization

### Strengths

**Multi-method Authentication** (`src/webu.cpp`, `src/webu_ans.cpp`)
- Basic HTTP authentication supported
- Digest HTTP authentication supported
- Feature detection at startup validates MHD capabilities

**Account Lockout Protection** (`src/webu_ans.cpp:394-431`)
- Configurable failed attempt threshold (`webcontrol_lock_attempts`)
- Configurable lockout duration (`webcontrol_lock_minutes`)
- Custom lock script execution (`webcontrol_lock_script`)
- IP-based tracking of failed authentications

**Access Control Levels**
- `webcontrol_parms` (0-3) controls parameter visibility
- Actions can be individually disabled via `webcontrol_actions`

### Weaknesses

**Digest vs Basic Authentication Reality**
- Digest auth typically uses MD5 (depends on libmicrohttpd build)
- Digest is NOT a substitute for TLS - both transmit credentials that can be replayed
- **TLS is the actual protection**, not the auth method choice
- Recommendation: Use Basic+TLS (simpler) rather than Digest without TLS

**IP-Based Lockout Limitations**
- NAT: Entire household shares one IP - one user's failures lock out everyone
- Proxies: Real client IP may not be correctly identified
- DoS vector: Attacker can intentionally trigger lockouts for legitimate users

### Recommendations

1. **Require TLS when authentication is enabled** and traffic leaves localhost
2. **Track lockout by (username + IP)** instead of IP alone
3. **Add rate limiting** separate from lockout (protects without blocking legitimate users)
4. **Document proxy configuration** for X-Forwarded-For handling (with security warnings)

**Runtime CPU Impact:** None - authentication only occurs on web requests, not camera loop

---

## 2. Web Application Security (NEW SECTION)

### Finding: Missing CSRF Protection

**Risk Level:** HIGH

**Location:** `src/webu_html.cpp:434-448`, `src/webu_post.cpp`

```javascript
// No CSRF token present
formData.append('command', 'config');
formData.append('camid', camid);
request.open('POST', pHostFull);
request.send(formData);
```

**Analysis:**
- POST endpoints accept commands without CSRF tokens
- Attacker can craft malicious page that submits forms to Motion
- If user is authenticated, attacker can: change config, restart cameras, execute actions
- Authentication alone does NOT prevent CSRF

**Recommendations:**
1. Generate random CSRF token per session
2. Include token in all forms and validate on POST
3. Check `Origin`/`Referer` headers as defense-in-depth

### Finding: Missing Security Headers

**Risk Level:** MEDIUM

No security headers are set by default (verified via grep - zero matches).

**Missing Headers:**
| Header | Purpose |
|--------|---------|
| `X-Frame-Options: DENY` | Prevents clickjacking |
| `X-Content-Type-Options: nosniff` | Prevents MIME sniffing |
| `Content-Security-Policy` | Prevents XSS |
| `Referrer-Policy: no-referrer` | Prevents credential leakage |

**Recommendation:** Add default security headers via `webcontrol_headers` or hardcode sensible defaults.

### Finding: Potential XSS in Dynamic Content

**Risk Level:** MEDIUM (requires investigation)

- HTML is generated dynamically in `src/webu_html.cpp`
- Configuration values may be reflected in HTML without encoding
- Device names, text overlays could contain malicious scripts

**Recommendation:** Audit all dynamic HTML generation for proper output encoding.

**Runtime CPU Impact:** None - headers/CSRF only affect web requests, not camera loop

---

## 3. Command Injection Analysis

### Finding: Shell Command Execution via Configuration

**Location:** `src/util.cpp:669-729`

```cpp
execl("/bin/sh", "sh", "-c", stamp, " &",(char*)NULL);
```

**Risk Level:** MEDIUM-HIGH

**Analysis:**
- Commands run via `/bin/sh -c` with full shell interpretation
- Variable substitution (%f, %$, %{host}, etc.) without escaping
- "Requires config access" is NOT complete mitigation:
  - Web UI is how most users change config
  - Admin credentials are exactly what attackers steal
  - Compromised admin = full shell access

**Recommended Approach (in order of preference):**

| Option | Security | Breaking Change? | CPU Impact |
|--------|----------|------------------|------------|
| Reject shell metacharacters | High | Partial | None |
| Allow only script path + args | High | Yes | None |
| Escape variables before shell | Medium | No | Negligible |

**Preferred: Reject metacharacters**
- Reject: `$`, `` ` ``, `|`, `;`, `&`, `(`, `)`, `{`, `}`, `<`, `>`, `\n`
- Allow: alphanumeric, `/`, `.`, `-`, `_`, `%` (for substitution)
- Existing scripts still work; shell injection blocked

### Finding: External Pipe Execution

**Location:** `src/movie.cpp:1639`

```cpp
extpipe_stream = popen(tmp, "we");
```

**Risk Level:** MEDIUM (same as above, not "LOW")

Same `/bin/sh` execution concerns. Should be treated in same remediation bucket.

**Runtime CPU Impact:** None - command execution happens on events, not in camera loop

---

## 4. SQL Security Analysis

### Finding: Dynamic SQL Construction

**Location:** `src/dbse.cpp:1064-1093`

**Risk Level:** MEDIUM

**Analysis:**
- SQL queries built via string formatting with `mystrftime()` substitution
- Full parameterization would require architectural redesign (user-defined templates)

**Incremental Remediation (practical approach):**

1. **Restrict SQL template editing from web UI**
   - Set `webcontrol_parms` requirement to highest level for SQL parameters
   - Or remove SQL template parameters from web-editable list entirely

2. **Sanitize substituted values**
   - Escape single quotes in filenames and user-controlled values
   - Reject or escape semicolons, comments (`--`, `/*`)

3. **Validate templates on save**
   - Reject multiple statements
   - Warn on dangerous keywords (DROP, DELETE without WHERE, etc.)

**Runtime CPU Impact:** Negligible - SQL operations happen on file save events, not in camera loop

---

## 5. File Access Security

### Strengths

**Database-Validated File Access** (`src/webu_file.cpp:66-89`)
- Files only served if registered in database
- Filename must match exactly (no path traversal)
- Security warning logged for unauthorized requests
- Per-device_id isolation

**Secure File Open Patterns:**
- `myfopen()` with mode "rbe" (read, binary, close-on-exec)

### Recommendations

1. Add defense-in-depth path traversal checks (reject `../`)
2. Implement file extension whitelist (`.jpg`, `.mp4`, etc.)

**Runtime CPU Impact:** None

---

## 6. Credential Management

### Finding: Plaintext Credential Storage

**Risk Level:** MEDIUM

**Affected Parameters:**
- `webcontrol_authentication` - Web control credentials
- `database_password` - Database access password
- `netcam_userpass` - Network camera credentials

### Concrete Remediation Plan

**1. Webcontrol Password: Store HA1 Digest**

Instead of plaintext `admin:password`, store:
```
admin:MD5(admin:Motion:password)
```
This is the HA1 value used in Digest auth. Password never stored in cleartext.

**2. Support Environment Variable Expansion**

Allow configuration like:
```conf
database_password ${DB_PASSWORD}
netcam_userpass ${NETCAM_CREDS}
```

For systemd deployments, use `LoadCredential=` for secure injection.

**3. Mask Secrets in Logs**

**Current problem** (`src/util.cpp:695`):
```cpp
MOTION_LOG(DBG, TYPE_EVENTS, NO_ERRNO
    ,_("Executing external command '%s'"), stamp);  // May contain secrets
```

Mask or redact sensitive patterns before logging.

**4. Separate Credentials from URLs**

Discourage: `netcam_url rtsp://user:pass@camera/stream`
Prefer: `netcam_url rtsp://camera/stream` + `netcam_userpass user:pass`

**Runtime CPU Impact:** None - credential handling is at startup/config load

---

## 7. Memory Safety & Compiler Hardening

### Finding: No Compiler Hardening Flags

**Location:** `configure.ac`

Current build has no security-focused compiler flags.

### Recommended Compiler Flags

| Flag | Purpose | Runtime CPU Impact |
|------|---------|-------------------|
| `-D_FORTIFY_SOURCE=2` | Buffer overflow detection | **1-2% camera loop** |
| `-fstack-protector-strong` | Stack smashing protection | **1-3% camera loop** |
| `-fPIE -pie` | ASLR support | Startup only |
| `-Wl,-z,relro,-z,now` | GOT hardening | Startup only |
| `-Wformat -Wformat-security` | Format string warnings | Compile-time only |

**Total runtime overhead: 2-5% during camera loop**

This is acceptable for a security-sensitive daemon. The Pi 5's Cortex-A76 handles this well, but note the thermal budget impact for sustained operation.

### Finding: Mixed Safe/Unsafe String Patterns

- `sprintf()` used in 30+ locations (some bounded by PATH_MAX)
- Priority: Audit web-facing paths first (request parsing, header handling)

**Recommendations:**
1. Enable `-Wformat` warnings in build
2. Prioritize fixes in `src/webu*.cpp` (web-facing)
3. Migrate `sprintf()` to `snprintf()` in hot paths

**Runtime CPU Impact:** snprintf vs sprintf - identical performance

---

## 8. Security Configuration

### Recommended Production Settings

```conf
# Authentication - use Basic with TLS, not Digest without TLS
webcontrol_auth_method basic
webcontrol_authentication admin:strong_password_here

# REQUIRE TLS when not localhost-only
webcontrol_tls on
webcontrol_cert /path/to/cert.pem
webcontrol_key /path/to/key.pem

# Or restrict to localhost (no TLS needed)
webcontrol_localhost on

# Minimize attack surface
webcontrol_parms 0  # read-only unless changes needed

# Account lockout
webcontrol_lock_attempts 5
webcontrol_lock_minutes 30

# Disable dangerous actions
webcontrol_actions config=off,config_write=off,camera_add=off,camera_delete=off

# Security headers (add when feature exists)
webcontrol_headers X-Frame-Options=DENY,X-Content-Type-Options=nosniff
```

### Default Secure Configuration Stance

For this fork, recommend changing defaults:

| Setting | Current Default | Recommended Default |
|---------|-----------------|---------------------|
| `webcontrol_localhost` | off | **on** |
| `webcontrol_parms` | 2 | **0** (read-only) |
| `webcontrol_auth_method` | none | **basic** (require setup) |

Rationale: Most real-world compromises come from insecure defaults.

### File Permission Requirements

```bash
# Configuration files (contain credentials)
chmod 600 /etc/motion/motion.conf
chown motion:motion /etc/motion/motion.conf

# TLS certificates
chmod 600 /etc/motion/*.pem
chown motion:motion /etc/motion/*.pem
```

---

## 9. Threat Model Summary

### Attack Surfaces

| Surface | Access | Risk | Mitigation |
|---------|--------|------|------------|
| Web Interface | Network | High | Auth + TLS + localhost + CSRF |
| Configuration Files | Local | High | File permissions |
| Video Streams | Network | Medium | Auth required |
| Script Execution | Config | Medium | Metacharacter rejection |
| Database | Local | Low | Internal only |

### Trust Boundaries

1. **Network -> Application**: Web authentication + TLS
2. **Config -> Runtime**: Auth required for web config changes
3. **Database -> Files**: DB validates file access

---

## 10. Recommendations Summary

### High Priority (Security Critical)

| # | Recommendation | Runtime CPU Impact |
|---|----------------|-------------------|
| 1 | Add CSRF tokens to POST endpoints | None |
| 2 | Add default security headers | None |
| 3 | Reject shell metacharacters in command substitution | None |
| 4 | Enable compiler hardening flags | 2-5% |

### Medium Priority (Significant Improvement)

| # | Recommendation | Runtime CPU Impact |
|---|----------------|-------------------|
| 5 | Store webcontrol password as HA1 digest | None |
| 6 | Support environment variables for secrets | None |
| 7 | Mask secrets in log output | Negligible |
| 8 | Track lockout by (username + IP) | None |
| 9 | Restrict SQL template editing from web UI | None |
| 10 | Sanitize values in SQL substitution | Negligible |

### Low Priority (Defense in Depth)

| # | Recommendation | Runtime CPU Impact |
|---|----------------|-------------------|
| 11 | Audit sprintf() in web-facing code | None (compile-time) |
| 12 | Add path traversal checks to file serving | Negligible |
| 13 | Change secure defaults for new installs | None |

### Dropped Recommendations

| Original | Reason Dropped |
|----------|----------------|
| Session timeout | Impractical with Basic/Digest auth model |
| "Digest is more secure than Basic" | Incorrect - TLS is what matters |

---

## 11. Conclusion

Motion implements security controls appropriate for trusted local network deployment. The main controls (authentication, TLS, localhost binding, parameter access levels) are functional.

**Key gaps identified in this revision:**
- CSRF protection is completely absent
- Security headers are not set
- Shell command execution uses full shell interpretation
- Lockout mechanism has NAT/proxy blind spots

**For Pi 5 deployments specifically:**
- Compiler hardening adds 2-5% CPU overhead during camera loop
- All other recommendations have zero impact on camera loop performance
- TLS overhead (discussed separately) affects streaming, not detection

**For deployments on untrusted networks:**
- Enable authentication AND TLS together
- Set `webcontrol_localhost on` if web access not needed remotely
- Disable config modification via web (`webcontrol_parms 0`)
- Monitor logs for failed authentication attempts
