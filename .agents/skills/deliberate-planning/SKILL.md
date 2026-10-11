---
name: deliberate-planning
description: Performs careful, evidence-based planning before coding, weighing simpler alternatives and identifying risks without producing unnecessarily long reasoning.
---

# Deliberate Planning Before Coding

1. Define the exact failure or desired behavior.
2. Inspect the current code path and relevant project conventions.
3. Consider the simplest plausible implementation first.
4. Compare alternatives only when the trade-off materially affects correctness, maintenance, security or performance.
5. Identify side effects, compatibility risks, edge cases and rollback options.
6. Choose a minimal approach and explain the key reason in a few bullets.
7. Define the test that should fail before the fix and pass after it, when feasible.
8. If evidence is insufficient, run a targeted read-only investigation or ask a clarification; do not fill gaps with invented details.
9. Do not write code until the implementation target is clear.
10. Avoid endless analysis: once evidence and acceptance criteria are sufficient, implement and verify.
11. Do not reveal private chain-of-thought. Present the result, concise rationale, evidence and uncertainty only.
