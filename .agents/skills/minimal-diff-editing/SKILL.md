---
name: minimal-diff-editing
description: Makes focused edits to an existing codebase while preserving working behavior and limiting the diff.
---

# Minimal Diff Editing

1. Read the target file and enough surrounding code to understand its contracts.
2. Search call sites, related types and tests before changing a public function or shared component.
3. Prefer a localized patch over whole-file regeneration.
4. Preserve unrelated comments, imports, naming, formatting and behavior.
5. Do not opportunistically refactor neighboring code.
6. If a patch changes more lines than expected, inspect why before proceeding.
7. Review both `git diff --stat` and full diff.
8. Reject changes that are unrelated, duplicated, destructive or not explained by the request.
9. Run focused tests after editing.
10. Summarize the exact behavior changed and what stayed untouched.
