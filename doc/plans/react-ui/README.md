# React UI Integration Plans

**Goal**: Replace Motion's embedded HTML UI with a modern React SPA while eliminating the Python/MotionEye layer entirely.

---

## Key Decision: No Python Backend

Motion already has a C++ web server with:
- libmicrohttpd (HTTP/HTTPS)
- JSON API (`webu_json.cpp`)
- MJPEG streaming (`webu_stream.cpp`)
- Static file serving (`webu_file.cpp`)
- Authentication (digest auth, CSRF)

**Result**: React talks directly to Motion's C++ API. No Python. No proxying.

---

## Plan Documents

| Document | Purpose |
|----------|---------|
| [01-architecture-20251227.md](./01-architecture-20251227.md) | System architecture, API design, C++ modifications |
| [02-detailed-execution-20251227.md](./02-detailed-execution-20251227.md) | Task breakdown, implementation details, agent strategy |

---

## Quick Reference

### Resource Comparison

| Metric | MotionEye + Motion | Motion + React |
|--------|-------------------|----------------|
| Processes | 2+ | 1 |
| RAM | ~150-200 MB | ~20-30 MB |
| Dependencies | Python, Tornado, etc. | libmicrohttpd |
| Stream latency | Proxied | Direct |

### Technology Stack

| Layer | Technology |
|-------|------------|
| Backend | Motion C++17 (existing) |
| Web Server | libmicrohttpd (existing) |
| Frontend | React 18 + TypeScript + Vite |
| Styling | Tailwind CSS |
| State | Zustand |
| Data Fetching | TanStack Query |

### Phase Summary

| # | Phase | Scope |
|---|-------|-------|
| 1 | C++ Extensions | SPA serving, API endpoints |
| 2 | React Setup | Vite, TypeScript, Tailwind |
| 3 | Core Components | UI library, camera display |
| 4 | Features | Settings, media, auth |
| 5 | Integration | Build, Docker, testing |

---

## Directory Structure

```
motion-motioneye/
├── src/                    # Motion C++ (modify)
│   ├── webu_file.cpp       # Add SPA support
│   ├── webu_json.cpp       # Add API endpoints
│   └── conf.cpp            # Add config params
├── webui/                  # NEW: React app
│   ├── src/
│   ├── public/
│   └── package.json
└── data/
    └── webui/              # Built assets (installed)
```

---

## Status

- [x] Architecture plan created
- [x] Detailed execution plan created
- [ ] User approval
- [ ] Phase 1 started
