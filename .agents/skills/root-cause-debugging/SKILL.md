---
name: root-cause-debugging
description: Finds the cause of a bug using reproduction, logs, call paths, and controlled experiments instead of repeated speculative edits.
---

# Root Cause Debugging

1. Reproduce the bug or record why it cannot be reproduced.
2. Capture exact error text, stack trace, logs, inputs and environment.
3. Trace the failing value or event from source to failure.
4. List plausible causes and rank them by evidence, not intuition.
5. Design the smallest experiment that distinguishes the top hypotheses.
6. Change one causal factor at a time.
7. Test the original failure and nearby regression cases.
8. If evidence contradicts the hypothesis, revert only the attempted patch safely or revise it using the new evidence.
9. Do not hide exceptions, add arbitrary delays, weaken validation or change tests to fit broken behavior.
10. Stop after two no-progress attempts and report what evidence is missing.
