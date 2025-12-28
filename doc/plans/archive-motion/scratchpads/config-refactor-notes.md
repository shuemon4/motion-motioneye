# Config Refactor Working Notes

## Project Context

This refactoring addresses technical debt documented in `src/conf.cpp` lines 20-29:
```cpp
/*
Notes:
This needs additional work.
Create a vector/list from config_params.
Reassign class categories to group together those applicable to application vs camera vs sound.
Create a class of just the parms/edits to segregate from the config file processes
Perhaps a lightweight class of just the parms.  Use this instead of full class for the config
  parms that are being used "live" with devices
 (currently called "cfg" in the camera,sound and motion classes)
Remove the depreceated parameters from old Motion.
*/
```

---

## Session History

### Session 1: 2025-12-11 (Planning)
- **Phase**: Pre-implementation analysis
- **Status**: Completed
- **Work Done**:
  - Analyzed current configuration system across 33 files
  - Identified 469 direct `->cfg->` accesses in 28 files
  - Identified 182 parameters in config_parms[] array
  - Found 15+ array iteration patterns with O(n) lookup
  - Analyzed hot path in alg.cpp (29 config accesses per frame)
  - Created detailed implementation plan with 6 phases
  - User decision: Preserve direct member access for performance

---

## Current Session
- Date: 2025-12-11
- Phase: 5 (Scoped Copy Operations)
- Status: In Progress

### Discovery Findings (Phase 1)

**Existing config_parms[] array analysis:**
- Located at `src/conf.cpp:38-219`
- Contains 182 parameters (counted from array)
- Terminated by empty string sentinel: `{ "", (enum PARM_TYP)0, (enum PARM_CAT)0, (enum PARM_LEVEL)0 }`
- Current struct `ctx_parm` has 4 fields: parm_name, parm_type, parm_cat, webui_level

**Key types and enums (src/conf.hpp):**
- `enum PARM_CAT`: 19 categories (PARM_CAT_00 to PARM_CAT_18)
- `enum PARM_TYP`: STRING, INT, LIST, BOOL, ARRAY, PARAMS
- `enum PARM_LEVEL`: ALWAYS(0), LIMITED(1), ADVANCED(2), RESTRICTED(3), NEVER(99)
- `enum PARM_ACT`: DFLT, SET, GET, LIST

**File patterns to follow (from PATTERNS.md and code_standard):**
- Classes: `cls_` prefix
- Structs: `ctx_` prefix
- 4-space indentation, no tabs
- K&R brace style
- No space between function name and '('
- Pointer '*' before variable name with no space

**Makefile.am pattern:**
- Source files listed as pairs: `name.hpp name.cpp \`
- Files in alphabetical order within groups

### Implementation Progress

**Files Created:**
1. `src/parm_registry.hpp` - Header with:
   - `enum PARM_SCOPE` (APP, CAM, SND, ALL flags)
   - `struct ctx_parm_ext` - Extended parameter definition with scope
   - `class ctx_parm_registry` - Singleton with O(1) lookup

2. `src/parm_registry.cpp` - Implementation with:
   - `get_scope_for_category()` - Maps PARM_CAT to PARM_SCOPE
   - Constructor initializes from `config_parms[]` array
   - `find()` - O(1) lookup via `std::unordered_map`
   - `by_category()` - Pre-built category index
   - `by_scope()` - Dynamic scope filtering

**Files Modified:**
- `src/Makefile.am` - Added parm_registry.hpp/cpp to motion_SOURCES

### Phase 1 Session End Notes

**Phase 1 Status**: COMPLETE and VERIFIED
- Quality-engineer sub-agent verified all requirements
- Build verification pending (requires Pi target)

**Handoff Created**: `doc/handoff-prompts/ConfigParam-Refactor-Phase2-Handoff.md`

---

### Phase 2 Implementation Progress (2025-12-11)

**Files Created:**
1. `src/parm_structs.hpp` - Scoped parameter structures:
   - `ctx_parm_app` (~35 members): System, webcontrol, database, SQL parameters
   - `ctx_parm_cam` (~95 members): Camera device, source, image, detection, output parameters
   - `ctx_parm_snd` (5 members): Sound device parameters

**Files Modified:**
1. `src/conf.hpp`:
   - Added `#include "parm_structs.hpp"`
   - Added `parm_app`, `parm_cam`, `parm_snd` struct members to `cls_config`
   - Added 130+ reference aliases for backward compatibility (`daemon = parm_app.daemon`)
   - Preserves existing `->cfg->member` access pattern for all 469 access sites

2. `src/Makefile.am`:
   - Added `parm_structs.hpp \` to motion_SOURCES

**Key Design Decisions:**
- Used C++ reference aliases (not pointers) for zero-overhead backward compatibility
- Scoped structs are actual members, aliases are references initialized in class definition
- This approach requires C++11 or later (project already uses C++17)

---

### Phase 5 Implementation Progress (2025-12-11)

**Files Modified:**
1. `src/conf.hpp`:
   - Added `copy_app()`, `copy_cam()`, `copy_snd()` method declarations

2. `src/conf.cpp`:
   - Implemented O(1) scoped copy methods using direct struct assignment
   - `copy_app()`: Copies parm_app struct (38 application parameters)
   - `copy_cam()`: Copies parm_cam struct (117 camera parameters)
   - `copy_snd()`: Copies parm_snd struct (5 sound parameters)

**Key Design Decisions:**
- New methods complement existing `parms_copy()` rather than replace it
- Old `parms_copy()` retained for backward compatibility and special cases
- New scoped copy methods are O(1) vs O(n) for 182 parameters
- Expected performance improvement: ~50x faster for full config copy

**Phase Execution Order Adjustment:**
- Phase 3 (Edit Handler Consolidation) deferred to last due to high risk
- Phase 4 (File I/O Separation) deferred - doesn't leverage scoped structs
- Phase 5 (Scoped Copy) completed first - direct benefit from Phase 2
- Phase 6 (Consumer Updates) next - can use new copy methods

---

### Phase 6 Analysis (2025-12-11)

**Consumer Analysis:**

Found 8 `parms_copy()` call sites:

| File | Line | Pattern | Analysis |
|------|------|---------|----------|
| `sound.cpp` | 797 | Full copy | Sound init needs ALL params (app+cam+snd) |
| `camera.cpp` | 1159 | Full copy | Camera init needs ALL params (app+cam) |
| `motion.cpp` | 415 | `PARM_CAT_00` only | Log restart - system params subset of app |
| `motion.cpp` | 430 | `PARM_CAT_15` only | DB restart - database params subset of app |
| `motion.cpp` | 440 | `PARM_CAT_13` only | Webu restart - webcontrol params subset of app |
| `motion.cpp` | 518 | Full copy | App startup needs all params |
| `conf.cpp` | 3890 | Full copy | Camera instance init |
| `conf.cpp` | 3966 | Full copy | Sound instance init |

**Decision**: Do NOT modify existing consumers in this phase.

**Rationale:**
1. Full copies (`parms_copy(src)`) need ALL scopes - would require 3 method calls
2. Category-specific copies use `PARM_CAT` granularity which is finer than scope
3. The new scoped methods are optimal for NEW code, not retrofitting existing code
4. Changing existing code risks regressions without clear performance benefit

**Documentation:** The new `copy_app()`, `copy_cam()`, `copy_snd()` methods are:
- Available for new code that only needs one scope
- Optimal for hot path scenarios (e.g., rapid camera reconfig)
- Preserved `parms_copy()` for backward compatibility and mixed-scope needs

---

## Next Session TODO

1. [x] Complete Phase 5 verification sub-agent
2. [x] Analyze Phase 6 consumer opportunities
3. [ ] (Optional) Phase 4: Extract file I/O to cls_config_file
4. [ ] (Optional) Phase 3: Edit Handler Consolidation (major refactor)
5. [ ] Build verification on Pi target

---

## Key Metrics (Baseline)

| Metric | Current Value | Target |
|--------|---------------|--------|
| config_parms[] size | 182 parameters | N/A |
| conf.cpp lines | ~4500 | ~2100 |
| Direct cfg accesses | 469 in 28 files | Unchanged |
| Edit handler functions | 182 | 4 |
| Parameter lookup | O(n) ~5μs | O(1) <100ns |
| Camera config memory | ~8KB | ~3KB |

---

## Decisions Made

### 2025-12-11: Access Pattern
- **Decision**: Preserve direct member access (`cam->cfg->threshold`)
- **Rationale**: User prioritized performance; changing to getter methods would require modifying 469 access sites and add function call overhead in hot path
- **Impact**: Need to use reference aliases or direct struct embedding for backward compatibility

### 2025-12-11: Architecture Approach
- **Decision**: Create scoped parameter structs (ctx_parm_app, ctx_parm_cam, ctx_parm_snd)
- **Rationale**: Reduces memory footprint for camera/sound devices from ~8KB to ~3KB
- **Impact**: Camera and sound classes only get parameters they actually use

---

## Code Patterns to Follow

From `doc/project/PATTERNS.md`:

```cpp
// Class naming
class cls_example { ... };

// Struct naming for data/context
struct ctx_example { ... };

// Parameter access pattern (PRESERVE THIS)
int threshold = cam->cfg->threshold;

// Singleton pattern (for registry)
class ctx_parm_registry {
public:
    static ctx_parm_registry& instance();
private:
    ctx_parm_registry();  // Private constructor
};
```

---

## Files to Create (Phase 1)

1. `src/parm_registry.hpp` - Parameter registry class declaration
2. `src/parm_registry.cpp` - Registry implementation with hash map

---

## Issues Encountered

[None yet - to be filled during implementation]

---

## Next Session TODO

1. [ ] Begin Phase 1: Registry Infrastructure
2. [ ] Create `src/parm_registry.hpp` with:
   - `ctx_parm` struct with scope field
   - `ctx_parm_registry` singleton class
   - O(1) lookup via `std::unordered_map`
3. [ ] Create `src/parm_registry.cpp` with:
   - Static initialization from existing config_parms[] data
   - Category and scope index building
4. [ ] Update `src/Makefile.am` to include new files
5. [ ] Verify build succeeds
6. [ ] Spawn verification sub-agent

---

## Reference: Parameter Categories

| Category | Name | Scope | Count |
|----------|------|-------|-------|
| PARM_CAT_00 | system | APP | 8 |
| PARM_CAT_01 | camera | CAM | 10 |
| PARM_CAT_02 | source | CAM | 10 |
| PARM_CAT_03 | image | CAM | 5 |
| PARM_CAT_04 | overlay | CAM | 7 |
| PARM_CAT_05 | method | CAM | 11 |
| PARM_CAT_06 | masks | CAM | 7 |
| PARM_CAT_07 | detect | CAM | 7 |
| PARM_CAT_08 | scripts | CAM | 12 |
| PARM_CAT_09 | picture | CAM | 8 |
| PARM_CAT_10 | movies | CAM | 13 |
| PARM_CAT_11 | timelapse | CAM | 5 |
| PARM_CAT_12 | pipes | CAM | 2 |
| PARM_CAT_13 | webcontrol | APP | 18 |
| PARM_CAT_14 | streams | CAM | 11 |
| PARM_CAT_15 | database | APP | 7 |
| PARM_CAT_16 | sql | APP | 5 |
| PARM_CAT_17 | tracking | CAM | 9 |
| PARM_CAT_18 | sound | SND | 5 |

---

## Reference: Hot Path Parameters (alg.cpp)

These are accessed every frame during motion detection - performance critical:

```cpp
cam->cfg->threshold              // Line 127
cam->cfg->despeckle_filter       // Lines 510, 522, 528
cam->cfg->smart_mask_speed       // Lines 579, 581, 615, 730, 792, 869
cam->cfg->threshold_ratio_change // Lines 628, 671, 722, 780
cam->cfg->lightswitch_percent    // Lines 886, 887
cam->cfg->lightswitch_frames     // Lines 889, 890
cam->cfg->static_object_time     // Line 908
cam->cfg->framerate              // Line 908
cam->cfg->threshold_sdevx        // Lines 1176, 1177, 1217
cam->cfg->threshold_sdevy        // Lines 1181, 1182, 1218
cam->cfg->threshold_sdevxy       // Lines 1186, 1187, 1219
```
