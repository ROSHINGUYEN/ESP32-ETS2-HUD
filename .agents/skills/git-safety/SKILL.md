---
name: git-safety
description: Protects user changes with safe Git inspection, checkpoints, and careful rollback practices.
---

# Git Safety

1. Start with `git status --short` and inspect existing diffs.
2. Do not overwrite, stage, commit, reset, clean or discard user changes without clear authorization.
3. Before risky work, suggest a checkpoint commit or branch if the working tree state allows it.
4. Never run destructive commands such as `git reset --hard`, `git clean -fd`, or broad checkout restore unless explicitly authorized after explaining consequences.
5. To undo your own change, identify the exact hunk/file and preserve unrelated user edits.
6. Inspect staged and unstaged diffs separately.
7. Before finishing, report modified, added and deleted files.
