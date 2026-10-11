---
name: anti-loop-debugging
description: Diagnoses and breaks AI coding loops where the agent repeatedly deletes, rewrites, or reverses the same code.
---

# Anti-loop Debugging

## Procedure
1. Stop edits temporarily. Capture current file contents, current diff, last command and exact error/test output.
2. Define one observable failure. Avoid vague goals such as "make it better."
3. Find the smallest relevant function/component and its callers.
4. State one root-cause hypothesis and what evidence would confirm or reject it.
5. Make one targeted change.
6. Run the narrowest relevant test and inspect the resulting diff.
7. Compare the result to the previous attempt.

## Stop conditions
- If two attempts fail without new evidence, stop and report.
- If the same lines alternate between two versions, stop and compare their behavior rather than trying a third rewrite.
- If file contents do not match the intended patch, re-read the file and inspect tool output before acting.
- If test failure is unrelated to the changed code, do not modify that code to silence it.

## Required report
- Reproduction/evidence
- Root cause or unresolved hypotheses
- Attempts and actual outcomes
- Minimal next action
- Whether any files were changed
