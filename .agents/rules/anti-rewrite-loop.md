---
trigger: always_on
description: Prevents repeated editing, deleting, and rewriting of the same code without new evidence.
---

# Anti Rewrite Loop

- Never repeat an edit unless you can state what new evidence requires it.
- Before editing a previously touched region, inspect the current on-disk content and current diff.
- Track attempts for the current failure. After two unsuccessful attempts with no new evidence, stop editing and report.
- Do not alternate between two implementations without identifying a concrete failing condition.
- Do not reformat or rewrite unrelated code during a bug fix.
- After a successful targeted test, do not keep changing the same code for speculative improvements.
- If test output is unchanged after an edit, explain why the edit should affect that output before trying again.
- When uncertain whether a file write was applied, read the file; do not blindly reapply the patch.
- Stop if the same lines are repeatedly added and removed. Report the exact file, symbol, diff pattern, tests and blocker.
