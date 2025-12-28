# Configuration Parameter Refactoring - Handoff Prompt

**Project**: Motion - Video Motion Detection Daemon
**Task**: Execute Configuration Parameter System Refactoring
**Plan Document**: `doc/plans/ConfigParam-Refactor-20251211-1730.md`
**Scratchpad**: `doc/scratchpads/config-refactor-notes.md`

---

## Instructions for Claude Code

You are continuing work on the Motion project configuration system refactoring. This is a performance optimization effort for Pi 5 + Camera v3 support.

### 1. Initial Setup

Before starting any work:

1. **Read the implementation plan thoroughly**:
   ```
   Read doc/plans/ConfigParam-Refactor-20251211-1730.md
   ```

2. **Check the scratchpad for previous session notes**:
   ```
   Read doc/scratchpads/config-refactor-notes.md
   ```

3. **Check the plan's "Work Completed" section** at the bottom to see which phases are done.

4. **Identify the next uncompleted phase** and proceed with implementation.

### 2. Phase Execution Protocol

For each phase:

#### Before Starting
- Update scratchpad with: current phase, start time, planned approach
- Create a todo list for the phase subtasks
- Review any dependencies from previous phases

#### During Implementation
- Follow the code patterns in `doc/project/PATTERNS.md`
- Use naming conventions: `cls_` for classes, `ctx_` for structs
- Preserve backward compatibility for `->cfg->member` access patterns
- Update scratchpad with decisions, issues encountered, and solutions

#### After Completing Phase
1. **Run build verification**:
   ```bash
   cd /path/to/motion
   make clean && make -j4
   ```

2. **Add completion summary to the plan** (append to end of plan file):
   ```markdown
   ---
   ## Work Completed

   ### Phase X: [Phase Name]
   **Completed**: YYYY-MM-DD HH:MM
   **Files Changed**:
   - file1.cpp: description of changes
   - file2.hpp: description of changes

   **Verification**:
   - [ ] Build successful
   - [ ] No new warnings
   - [ ] Functionality preserved

   **Notes**: Any important observations or deviations from plan
   ```

3. **Spawn verification sub-agent** (MANDATORY):
   ```
   Use the Task tool with subagent_type="quality-engineer" to verify the phase completion.

   Prompt should include:
   - Which phase was completed
   - What files were changed
   - Specific verification criteria from the plan
   - Request to verify actual code implementation, not just existence of methods/files
   ```

### 3. Verification Sub-Agent Requirements

The verification sub-agent MUST:

1. **Read the actual source code** - not just check file existence
2. **Verify implementation matches plan specifications**:
   - For Phase 1: Confirm O(1) lookup via hash map, not array iteration
   - For Phase 2: Confirm scoped structs contain correct members
   - For Phase 3: Confirm edit handlers are consolidated, not duplicated
   - etc.
3. **Check for regressions**:
   - Existing access patterns (`->cfg->threshold`) still compile
   - No breaking changes to public interfaces
4. **Report findings** with specific code references (file:line)

### 4. Scratchpad Usage

The scratchpad (`doc/scratchpads/config-refactor-notes.md`) should contain:

```markdown
# Config Refactor Working Notes

## Current Session
- Date: YYYY-MM-DD
- Phase: X
- Status: In Progress / Completed / Blocked

## Decisions Made
- [Decision and rationale]

## Issues Encountered
- [Issue and resolution]

## Code Patterns Discovered
- [Patterns that may affect implementation]

## Next Session TODO
- [Items for next session to pick up]
```

### 5. Key Files Reference

| Purpose | File |
|---------|------|
| Implementation Plan | `doc/plans/ConfigParam-Refactor-20251211-1730.md` |
| Working Notes | `doc/scratchpads/config-refactor-notes.md` |
| Current Config | `src/conf.hpp`, `src/conf.cpp` |
| Code Patterns | `doc/project/PATTERNS.md` |
| Architecture | `doc/project/ARCHITECTURE.md` |

### 6. Phase Quick Reference

| Phase | Description | Risk | Key Deliverable |
|-------|-------------|------|-----------------|
| 1 | Registry Infrastructure | Low | `parm_registry.hpp/cpp` with O(1) lookup |
| 2 | Scoped Parameter Structs | Medium | `ctx_parm_app`, `ctx_parm_cam`, `ctx_parm_snd` |
| 3 | Edit Handler Consolidation | Medium | Reduce 182 handlers to 4 type-based |
| 4 | File I/O Separation | Low | `cls_config_file` class |
| 5 | Scoped Copy Operations | Low | `copy_cam_params()` etc. |
| 6 | Consumer Updates | Low | Update 28 files with new includes |

### 7. Success Criteria

The refactoring is complete when:

- [ ] All 6 phases completed and verified
- [ ] Build succeeds with no new warnings
- [ ] Parameter lookup is O(1) via hash map
- [ ] Camera config memory reduced ~60%
- [ ] Backward compatibility preserved (`->cfg->member` works)
- [ ] Web UI functionality unchanged
- [ ] All verification sub-agents passed

### 8. If Blocked

If you encounter a blocking issue:

1. Document the issue in the scratchpad with full context
2. Note what was attempted and why it failed
3. Propose alternative approaches
4. Update the plan's "Open Questions" section if needed
5. Leave clear handoff notes for the next session

---

## Start Command

To begin work, execute:

```
Read doc/plans/ConfigParam-Refactor-20251211-1730.md
Read doc/scratchpads/config-refactor-notes.md
```

Then identify the next phase to implement and proceed.
