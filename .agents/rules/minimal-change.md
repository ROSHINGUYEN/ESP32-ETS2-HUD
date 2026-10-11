---
trigger: always_on
description: Keeps changes narrow, preserves existing architecture, and avoids unnecessary rewrites.
---

# Minimal Change Policy

- Make the smallest change that satisfies the stated request.
- Preserve naming, project structure, public interfaces, formatting conventions and working behavior.
- Do not replace a file wholesale when a localized patch is sufficient.
- Do not add dependencies, new abstractions, frameworks or layers unless required and justified.
- Do not change unrelated files.
- Do not remove existing behavior unless the request explicitly asks for removal or evidence proves it is defective.
- If a broad change seems necessary, explain why and ask before proceeding.
- Inspect `git diff --stat` and `git diff` before finishing.
