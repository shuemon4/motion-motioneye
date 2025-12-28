# Configuration Parameter Refactoring - Phase 2+ Handoff

**Project**: Motion - Video Motion Detection Daemon
**Task**: Continue Configuration Parameter System Refactoring (Phases 2-6)
**Plan Document**: `doc/plans/ConfigParam-Refactor-20251211-1730.md`
**Scratchpad**: `doc/scratchpads/config-refactor-notes.md`
**Previous Handoff**: `doc/handoff-prompts/ConfigParam-Refactor-Handoff.md`

---

## Work Completed

### Phase 1: Registry Infrastructure ✅ COMPLETE

**Date Completed**: 2025-12-11

**Files Created**:
| File | Purpose |
|------|---------|
| `src/parm_registry.hpp` | Header with `ctx_parm_registry` singleton class, `enum PARM_SCOPE`, `struct ctx_parm_ext` |
| `src/parm_registry.cpp` | Implementation with O(1) hash map lookup, scope mapping, category indices |

**Files Modified**:
| File | Change |
|------|--------|
| `src/Makefile.am` | Added `parm_registry.hpp parm_registry.cpp` to motion_SOURCES |

**Key Implementation Details**:
- `ctx_parm_registry` singleton provides O(1) parameter lookup via `std::unordered_map`
- Reads from existing `config_parms[]` array at initialization (no breaking changes)
- Adds `PARM_SCOPE` (APP, CAM, SND) to each parameter based on category
- Pre-builds category indices for fast `by_category()` queries
- Thread-safe via C++11 static initialization guarantee

**Verification Status**:
- ✅ Quality-engineer sub-agent verified implementation
- ⏳ Build verification pending (requires Pi target or cross-compile)

---

## Work Remaining

### Phase 2: Scoped Parameter Structs (Medium Risk)
**Files**: `conf.hpp`, new `parm_structs.hpp`
**Effort**: ~300 lines refactored

**Tasks**:
1. Create `ctx_parm_app` struct with application-only parameters (~30 members)
2. Create `ctx_parm_cam` struct with camera parameters (~40 members)
3. Create `ctx_parm_snd` struct with sound parameters (~5 members)
4. Add scoped structs as members of `cls_config`
5. Create reference aliases for backward compatibility (`int& threshold = cam.threshold;`)

**Validation**: All existing `->cfg->member` accesses must compile unchanged

### Phase 3: Edit Handler Consolidation (Medium Risk)
**Files**: `conf.cpp`
**Effort**: ~1000 lines reduced to ~300

**Tasks**:
1. Replace 19 `edit_catXX()` functions with registry-based dispatch
2. Consolidate 182 individual `edit_*()` handlers into 4 type-based handlers
3. Use registry for validation metadata

### Phase 4: File I/O Separation (Low Risk)
**Files**: `conf.cpp` → split to `conf_file.cpp`
**Effort**: ~500 lines moved

**Tasks**:
1. Extract `init()`, `process()`, `parms_write()`, `parms_log()` to `cls_config_file`
2. Extract `cmdline()` and deprecated handling
3. Keep `cls_config` focused on parameter storage and editing

### Phase 5: Scoped Copy Operations (Low Risk)
**Files**: `conf.cpp`
**Effort**: ~100 lines refactored

**Tasks**:
1. Replace `parms_copy()` O(n) iteration with direct struct copy
2. Implement `copy_cam_params()`, `copy_snd_params()`, `copy_app_params()`

### Phase 6: Consumer Updates (Low Risk)
**Files**: 28 files with `->cfg->` access
**Effort**: Minimal

**Tasks**:
1. Update includes to use new headers
2. Update any direct `config_parms[]` iteration to use registry

---

## Instructions for Next Session

### 1. Initial Setup

Before starting any work:

```
Read doc/plans/ConfigParam-Refactor-20251211-1730.md
Read doc/scratchpads/config-refactor-notes.md
```

### 2. Verify Phase 1 Build (Optional)

If you have access to a build environment:
```bash
cd /path/to/motion
autoreconf -fiv
./configure --with-libcam --with-sqlite3
make -j4
```

### 3. Begin Phase 2

The next phase creates scoped parameter structs. Key considerations:

**Backward Compatibility is Critical**:
- Existing code uses `cam->cfg->threshold` pattern (469 occurrences in 28 files)
- This pattern MUST continue to work after refactoring
- Use reference aliases in `cls_config` to redirect to scoped structs

**Reference the Plan**:
- See `doc/plans/ConfigParam-Refactor-20251211-1730.md` lines 171-260 for struct designs
- See scratchpad for parameter category → scope mapping

**Follow the Protocol**:
- Update scratchpad with current session status
- Create todo list for phase subtasks
- After completing phase, add completion summary to plan
- Spawn verification sub-agent (MANDATORY)

### 4. Phase Execution Protocol

For each phase:

#### Before Starting
- Update scratchpad: current phase, start time, planned approach
- Create todo list for phase subtasks
- Review dependencies from previous phases

#### During Implementation
- Follow code patterns in `doc/project/PATTERNS.md`
- Use naming conventions: `cls_` for classes, `ctx_` for structs
- Preserve backward compatibility for `->cfg->member` access patterns
- 4-space indentation, no tabs, K&R brace style

#### After Completing Phase
1. Update plan's "Work Completed" section with completion summary
2. Spawn verification sub-agent to verify implementation
3. Update scratchpad with decisions, issues, and next steps

---

## Key Files Reference

| Purpose | File |
|---------|------|
| Implementation Plan | `doc/plans/ConfigParam-Refactor-20251211-1730.md` |
| Working Notes | `doc/scratchpads/config-refactor-notes.md` |
| Current Config | `src/conf.hpp`, `src/conf.cpp` |
| New Registry | `src/parm_registry.hpp`, `src/parm_registry.cpp` |
| Code Patterns | `doc/project/PATTERNS.md` |
| Code Standard | `doc/code_standard` |

---

## Success Criteria (Overall)

The refactoring is complete when:

- [x] Phase 1: Registry Infrastructure ✅
- [ ] Phase 2: Scoped Parameter Structs
- [ ] Phase 3: Edit Handler Consolidation
- [ ] Phase 4: File I/O Separation
- [ ] Phase 5: Scoped Copy Operations
- [ ] Phase 6: Consumer Updates
- [ ] Build succeeds with no new warnings
- [ ] Parameter lookup is O(1) via hash map ✅
- [ ] Camera config memory reduced ~60%
- [ ] Backward compatibility preserved (`->cfg->member` works)
- [ ] All verification sub-agents passed

---

## Start Command

To begin work on Phase 2, execute:

```
/bf:task Continue configuration parameter refactoring. Phase 1 (Registry Infrastructure) is complete. Begin Phase 2 (Scoped Parameter Structs). Read the plan at doc/plans/ConfigParam-Refactor-20251211-1730.md and scratchpad at doc/scratchpads/config-refactor-notes.md first. Follow the phase execution protocol in doc/handoff-prompts/ConfigParam-Refactor-Phase2-Handoff.md.
```

Or simply:

```
Read doc/handoff-prompts/ConfigParam-Refactor-Phase2-Handoff.md
```

Then proceed with Phase 2 implementation.
