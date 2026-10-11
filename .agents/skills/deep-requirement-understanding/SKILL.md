---
name: deep-requirement-understanding
description: Converts natural-language requests into a precise implementation target by identifying intent, constraints, assumptions, dependencies, and measurable acceptance criteria before coding.
---

# Deep Requirement Understanding

## Goal
Understand what success means before touching code. Do not merely match keywords in the request.

## Procedure
1. Restate the user's intended outcome in one sentence, without changing its meaning.
2. Extract:
   - required behavior
   - explicitly forbidden behavior
   - existing behavior that must be preserved
   - relevant platform/framework constraints
   - acceptance criteria that can be tested or observed
3. Inspect the current repository and identify the existing implementation path before proposing a new one.
4. Separate information into:
   - confirmed by user or source code
   - likely but unverified
   - unknown and implementation-critical
5. Identify conflicts, hidden dependencies, edge cases and failure states.
6. Ask a concise clarification when an unknown could materially change architecture, data handling, user-visible behavior, cost, security or scope.
7. Do not ask questions whose answers are already present in the prompt, prior conversation, rules, or repository.
8. If the task is sufficiently clear, state a short plan and proceed within scope.
9. Translate the request into a small checklist of observable acceptance criteria.
10. Before finishing, map each criterion to evidence: test, build, screenshot, API response, or source-level verification.

## Output format
- Goal
- Constraints / must preserve
- Assumptions or unknowns
- Questions (only if needed)
- Acceptance criteria
- Minimal plan

Keep this output brief. Do not generate implementation code during a requirements-only phase.
