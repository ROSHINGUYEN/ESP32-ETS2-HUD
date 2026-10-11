---
name: safe-refactoring
description: Refactors code only when requested, preserving behavior with small steps and regression checks.
---

# Safe Refactoring

1. Confirm the requested refactor and its scope.
2. Establish a baseline using existing tests/build checks where possible.
3. Separate behavior changes from structural changes; do not combine them casually.
4. Refactor one unit at a time and keep public interfaces stable unless approved.
5. Run relevant tests after each meaningful step.
6. Do not rename, move or delete files merely for aesthetics.
7. Avoid broad formatting changes that obscure the functional diff.
8. If behavior changes unexpectedly, stop and isolate the responsible step.
9. Report behavior intended to remain unchanged and evidence supporting that claim.
