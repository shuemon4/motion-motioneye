# Hot Reload API Implementation - Handoff Prompt

**Date Created**: 2025-12-13
**Task**: Implement Motion 5.0 Hot Reload Configuration API
**Priority**: Enhancement

---

## Instructions for Executing Agent

You are implementing the Hot Reload Configuration API for Motion 5.0. This allows MotionEye and other clients to update specific configuration parameters at runtime without restarting the daemon.

### Required Reading (In Order)

1. **Implementation Plan** (PRIMARY): `doc/plans/MotionEye-Plans/hot-reload-implementation-plan.md`
2. **Original Request**: `doc/plans/MotionEye-Plans/motion-hot-reload-api-request-20251213-0830.md`
3. **Research Notes**: `doc/scratchpads/hot-reload-research-20251213.md`
4. **Architecture Reference**: `doc/project/ARCHITECTURE.md`

### Working Documents

- **Your Scratchpad**: Create `doc/scratchpads/hot-reload-implementation-progress.md` for tracking work, notes, and issues encountered
- **Plan Updates**: After completing each phase, append a completion summary to the implementation plan

---

## Execution Protocol

### Before Starting Each Phase

1. Read the phase requirements from the implementation plan
2. Update your scratchpad with the phase you're starting
3. Review relevant source files before modifying

### After Completing Each Phase

1. **Update your scratchpad** with:
   - Files modified
   - Key changes made
   - Any deviations from the plan
   - Issues encountered and resolutions

2. **Spawn a review sub-agent** using the Task tool with this prompt:

```
Review the Hot Reload API implementation Phase {N} against the plan.

PLAN: Read doc/plans/MotionEye-Plans/hot-reload-implementation-plan.md, specifically Phase {N} requirements.

SCRATCHPAD: Read doc/scratchpads/hot-reload-implementation-progress.md for what was implemented.

SOURCE FILES: Read the modified source files listed in the scratchpad.

VERIFICATION CHECKLIST:
1. Are all code changes from the plan implemented?
2. Do the implementations match the code samples in the plan?
3. Are there any missing methods, parameters, or logic?
4. Are there syntax errors or incomplete code blocks?
5. Does the code follow existing Motion patterns (check doc/project/PATTERNS.md)?

REPORT FORMAT:
- Phase: {N}
- Status: COMPLETE | INCOMPLETE | ISSUES_FOUND
- Discrepancies: [List any missing or incorrect implementations]
- Recommendations: [What needs to be fixed]

Be thorough - check every requirement in the phase against what was actually implemented.
```

3. **Fix any discrepancies** identified by the review agent before proceeding

4. **Append completion summary** to the implementation plan:

```markdown
---

## Phase {N} Completion Summary

**Date Completed**: YYYY-MM-DD
**Status**: Complete

### Files Modified
- `path/to/file.cpp` - Description of changes

### Deviations from Plan
- None / List any deviations and rationale

### Review Agent Findings
- Summary of review results
- Any issues found and how they were resolved

### Notes
- Any important observations for future phases
```

---

## Phase Breakdown

### Phase 1: Core Infrastructure

**Goal**: Add `hot_reload` flag to parameter definitions

**Files to Modify**:
- `src/conf.hpp` - Add `hot_reload` field to `ctx_parm` struct
- `src/conf.cpp` - Update `config_parms[]` array with hot_reload flags for ALL parameters
- `src/parm_registry.hpp` - Add `is_hot_reloadable()` helper function

**Key Implementation Details**:
- The `ctx_parm` struct is at approximately line 65 in `conf.hpp`
- The `config_parms[]` array starts at approximately line 40 in `conf.cpp`
- There are approximately 100+ parameters to classify
- Use the classification table in Appendix A of the plan

**Validation**:
- Code compiles without errors
- All parameters have explicit hot_reload value (no defaults)

---

### Phase 2: API Endpoint

**Goal**: Implement `/config/set` HTTP endpoint

**Files to Modify**:
- `src/webu_json.hpp` - Declare new methods
- `src/webu_json.cpp` - Implement `config_set()`, `validate_hot_reload()`, `apply_hot_reload()`, `build_response()`
- `src/webu_ans.cpp` - Add routing for `/config/set` endpoint in `answer_get()`

**Key Implementation Details**:
- Follow the code samples in the plan exactly
- The endpoint format is: `GET /{camera_id}/config/set?{param}={value}`
- Response must be JSON with: status, parameter, old_value, new_value, hot_reload
- URL decode the parameter value using `MHD_http_unescape()`

**Validation**:
- Code compiles without errors
- Endpoint returns proper JSON for:
  - Valid hot-reloadable parameter (status: ok, hot_reload: true)
  - Restart-required parameter (status: error, hot_reload: false)
  - Unknown parameter (status: error)

---

### Phase 3: Testing

**Goal**: Create test infrastructure and validation scripts

**Files to Create**:
- `tests/test_hot_reload.cpp` - Unit tests (if test framework exists)
- `scripts/test_hot_reload.sh` - Manual testing script

**Key Implementation Details**:
- Check if `tests/` directory exists and what testing framework is used
- The shell script should test the API endpoint with curl
- Include tests for success, failure, and edge cases

**Validation**:
- Test script runs without errors
- All test cases pass when Motion is running

---

### Phase 4: Documentation

**Goal**: Update project documentation

**Files to Modify**:
- Update relevant documentation in `doc/` if needed
- Add API reference to appropriate location

**Validation**:
- Documentation accurately describes the new API
- Examples are correct and work

---

### Phase 5: Thread Safety Enhancement (OPTIONAL)

**Goal**: Add mutex protection for string parameters

**Only implement if**:
- Testing reveals actual thread safety issues
- Specifically requested

**Files to Modify**:
- `src/conf.hpp` - Add `pthread_rwlock_t`
- `src/conf.cpp` - Initialize/destroy rwlock
- Camera thread code - Add read locks around string parameter access

---

## Important Reminders

1. **Do NOT modify `cls_alg`** - The motion detection algorithm is proven and must be preserved
2. **Follow existing patterns** - Check `doc/project/PATTERNS.md` for code style
3. **Test compilation** after each file modification
4. **Use your scratchpad** to track progress and issues
5. **Spawn review agent** after each phase - do not skip this step
6. **Fix all discrepancies** before moving to next phase

---

## Build Commands

```bash
# From project root
autoreconf -fiv
./configure --with-libcam --with-sqlite3
make -j4

# Run in foreground for testing
./motion -c /path/to/motion.conf -n -d
```

---

## Success Criteria

The implementation is complete when:

1. All 4 essential phases are complete with review agent approval
2. The `/config/set` endpoint works as documented
3. Hot-reloadable parameters update immediately without restart
4. Restart-required parameters return appropriate error response
5. No regressions in existing functionality
6. Implementation plan has completion summaries for all phases

---

## Getting Started

1. Read the implementation plan thoroughly
2. Create your scratchpad: `doc/scratchpads/hot-reload-implementation-progress.md`
3. Begin Phase 1
4. Good luck!
