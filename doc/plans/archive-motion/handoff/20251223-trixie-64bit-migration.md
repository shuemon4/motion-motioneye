# Handoff: Trixie 64-bit Migration Implementation

## Context

You are implementing the Raspberry Pi OS Trixie 64-bit migration for the Motion project. The plan has been fully researched and approved.

## Required Reading

1. **Implementation Plan:** `doc/plans/20251223-trixie-64bit-migration.md`
2. **Analysis Document:** `doc/analysis/20251223-trixie-64bit-migration-complete.md`
3. **Project Rules:** `CLAUDE.md`

## Instructions

### Execution Rules

1. **Follow the plan phases in order** - Phase 1 → Phase 2 → Phase 3 → Phase 4
2. **Sub-agents are authorized** - Use Task tool with appropriate agent types for parallel work
3. **Create a scratchpad** - Use `doc/scratchpads/20251223-trixie-migration-notes.md` for working notes, findings, and additional memory
4. **Test incrementally** - Verify each phase compiles before proceeding

### Work Tracking

After completing **each phase**, append a summary to the end of the plan file (`doc/plans/20251223-trixie-64bit-migration.md`):

```markdown
---

## Implementation Log

### Phase X: [Name] - COMPLETED [timestamp]

**Files Modified:**
- `path/to/file.cpp` - description of changes

**Notes:**
- Any issues encountered
- Deviations from plan (if any)
- Test results
```

### Key Decisions (Already Made)

- **NV12 conversion:** DEFERRED - accept NV12 but don't convert yet; test first
- **Draft controls:** Silent skip if unavailable (no warning logs)
- **Pi 4 support:** Yes - create separate build profile with V4L2
- **libcamera versions:** Support both 0.4.0 and 0.5.x

### Critical Constraints

1. **Do NOT modify `src/alg.cpp` or `src/alg.hpp`** - motion detection algorithm is proven
2. **Minimize CPU usage** - Pi runs on limited power
3. **Test builds before committing** - ensure code compiles

### Phase Summary

| Phase | Priority | Scope |
|-------|----------|-------|
| 1 | Critical | 64-bit type safety (`size_t`, mmap casts, pointer logging) |
| 2 | High | libcamera compatibility (draft control guards, NV12 acceptance) |
| 3 | Medium | Script updates (rpicam detection, Pi 4 build profile) |
| 4 | Low | FFmpeg version guard |

### Build & Test Environment

**Builds will fail on Mac** - use the Raspberry Pi devices for building and testing:

| Device | Hostname | IP | SSH Command |
|--------|----------|-----|-------------|
| Pi 5 | pi5-motioneye | 192.168.1.176 | `ssh admin@192.168.1.176` |
| Pi 4 | pi4-motion | 192.168.1.246 | `ssh admin@192.168.1.246` |

**Workflow:**
1. Make code changes locally (Mac)
2. Sync to Pi: `rsync -avz --exclude='.git' /Users/tshuey/Documents/GitHub/motion/ admin@192.168.1.176:~/motion/`
3. Build on Pi: `cd ~/motion && autoreconf -fiv && ./configure --with-libcam --with-sqlite3 && make -j4`
4. Test on Pi

### Validation

After all phases complete:
- Build must succeed on Pi 5 with `--with-libcam --with-sqlite3`
- Build must succeed on Pi 4 with `--with-libcam --with-v4l2 --with-sqlite3`
- No compiler warnings about type truncation
- Document any testing notes in scratchpad

## Begin

Read the plan file, create your scratchpad, and start with Phase 1.
