# Security Audit Working Notes
Date: 2025-12-18

## Files Analyzed

### Web Server / Authentication
- `src/webu.cpp` - Main web server setup using libmicrohttpd
- `src/webu_ans.cpp` - Request handling, authentication logic
- `src/webu_post.cpp` - POST request processing
- `src/webu_file.cpp` - File serving

### Command Execution
- `src/util.cpp` - util_exec_command() functions
- `src/movie.cpp` - popen() for extpipe

### Database
- `src/dbse.cpp` - SQLite3, MariaDB, PostgreSQL handling

### Configuration
- `src/conf.hpp` - Configuration structure
- `src/conf.cpp` - Configuration processing

## Key Security Observations

### 1. Authentication (GOOD)
- Supports Basic and Digest authentication via libmicrohttpd
- Account lockout mechanism exists (webcontrol_lock_attempts, webcontrol_lock_minutes)
- Lock script can be triggered on repeated failures
- TLS support available (cert/key file configuration)
- Localhost-only binding option (webcontrol_localhost)

### 2. Command Injection (MEDIUM RISK)
Location: `src/util.cpp:669-697` and `src/util.cpp:699-729`
```cpp
void util_exec_command(cls_camera *cam, const char *command, const char *filename)
{
    char stamp[PATH_MAX];
    mystrftime(cam, stamp, sizeof(stamp), command, filename);
    ...
    execl("/bin/sh", "sh", "-c", stamp, " &",(char*)NULL);
}
```
- Commands are run via shell with user-configurable strings
- mystrftime() performs variable substitution but no escaping
- User-controlled variables (%f, %$, etc.) are substituted without shell escaping
- Attack surface: if config files can be modified, command injection is possible
- MITIGATION: Config modification requires authentication

### 3. SQL Injection (MEDIUM RISK)
Location: `src/dbse.cpp:1064-1093`
```cpp
void cls_dbse::exec(cls_camera *cam, std::string fname, std::string cmd)
{
    if (cmd == "pic_save") {
        mystrftime(cam, sql, cam->cfg->sql_pic_save, fname);
    }
    ...
    exec_sql(sql);
}
```
- SQL queries are built using mystrftime() variable substitution
- No parameterized queries or escaping
- User-configurable SQL templates (sql_pic_save, sql_event_start, etc.)
- MITIGATION: SQL templates are admin-configured, not user input

### 4. File Path Handling (GOOD)
Location: `src/webu_file.cpp:66-89`
```cpp
sql  = " select * from motion ";
sql += " where device_id = " + std::to_string(webua->cam->cfg->device_id);
...
for (indx=0;indx<flst.size();indx++) {
    if (flst[indx].file_nm == webua->uri_cmd2) {
        full_nm = flst[indx].full_nm;
    }
}
```
- Files are ONLY served if they exist in the database
- Path traversal blocked by design (filename matched against DB)
- Security warning logged for file not found

### 5. Input Validation (action_user)
Location: `src/webu_post.cpp:425-432`
```cpp
for (indx2 = 0; indx2<(int)tmp.length(); indx2++) {
    if (isalnum(tmp.at((uint)indx2)) == false) {
        MOTION_LOG(NTC, TYPE_STREAM, NO_ERRNO
            , _("Invalid character included in action user \"%c\"")
            , tmp.at((uint)indx2));
        return;
    }
}
```
- action_user parameter validated to alphanumeric only (GOOD)

### 6. Memory Safety
- Using sprintf() in multiple locations with PATH_MAX buffers
- Some safe patterns (snprintf used in many places)
- strcpy found in: webu_html.cpp, picture.cpp, logger.cpp

### 7. External Command Execution via popen
Location: `src/movie.cpp:1639`
```cpp
extpipe_stream = popen(tmp, "we");
```
- Used for external encoding pipeline
- Command built from configuration (movie_extpipe)
- Same mystrftime() substitution concerns

### 8. Netcam URL Parsing
Location: `src/netcam.cpp:256-290`
- Uses regex for URL parsing
- Credentials can be embedded in URLs
- Credentials extracted and stored (userpass field)

### 9. Credentials Storage
- webcontrol_authentication stored as plaintext in config
- database_password stored as plaintext in config
- netcam_userpass stored as plaintext in config
- No encryption at rest

## Configuration Security Parameters
From `src/conf.hpp`:
- webcontrol_localhost - restrict to localhost
- webcontrol_parms - control parameter access level (0-3)
- webcontrol_auth_method - none/basic/digest
- webcontrol_tls - enable HTTPS
- webcontrol_lock_minutes - lockout duration
- webcontrol_lock_attempts - max failed attempts

## Risk Summary

| Category | Risk Level | Notes |
|----------|------------|-------|
| Authentication | LOW | Proper auth with lockout |
| Authorization | LOW | Parameter level controls |
| Command Injection | MEDIUM | Config-driven, admin only |
| SQL Injection | MEDIUM | Config-driven, admin only |
| Path Traversal | LOW | DB-validated file access |
| TLS/Encryption | LOW | Supported but optional |
| Credential Storage | MEDIUM | Plaintext in config files |
| Memory Safety | LOW-MEDIUM | Mixed safe/unsafe patterns |
