---
name: repo-audit
description: Audits an existing repository before coding, identifies architecture, current changes, relevant files, tests, and risks without modifying files.
---

# Repository Audit

Use before entering an unfamiliar project or when previous edits may have damaged the project.

1. Inspect root files, project manifest, README, existing agent rules and relevant directory structure.
2. Check Git status and diffs if Git is available. Never discard uncommitted changes.
3. Identify entry points, main data flow, tests and build/test commands.
4. Follow imports and call sites for the target behavior; do not infer architecture from filenames alone.
5. Summarize:
   - relevant files and symbols
   - expected behavior versus observed behavior
   - likely root cause with confidence level
   - tests that can prove or disprove the hypothesis
   - risks and files that should not be touched
6. Audit mode is read-only. Do not edit until the user authorizes implementation or the task explicitly requests fixes.
7. Do not claim the audit is complete if important files or runtime evidence are unavailable.
