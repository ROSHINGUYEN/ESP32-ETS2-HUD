---
name: prompt-intent-optimizer
description: Converts a user's informal request into a compact, unambiguous coding task while preserving intent and avoiding invented requirements.
---

# Prompt Intent Optimizer

Use this when the user's request is informal, misspelled, incomplete, or mixes several goals.

1. Preserve the user's real goal; do not replace it with a more familiar but different task.
2. Extract desired result, current problem, constraints, must-preserve behavior and acceptance criteria.
3. Inspect repository context when available instead of asking the user for technical details the agent can discover.
4. Distinguish required work from optional improvements.
5. Flag assumptions that would materially affect implementation.
6. Ask one concise clarification before coding if the core goal or high-impact choice is ambiguous.
7. Produce a compact task statement using:
   - Goal
   - Scope
   - Constraints
   - Acceptance criteria
   - Verification
8. Do not inflate the prompt with generic roles, repeated warnings or long boilerplate.
9. Do not make impossible requirements sound guaranteed; identify external API, permission, device or environment dependencies.
10. Keep the original request as the source of truth. Do not silently add features.
