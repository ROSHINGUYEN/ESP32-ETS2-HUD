---
name: context-handoff
description: Creates a concise, evidence-based handoff when a task is paused, context is low, or work must continue in a later session.
---

# Context Handoff

Write a compact handoff containing:
- User's exact goal and acceptance criteria
- Project stack and relevant paths discovered
- Current status: PASS / FAIL / BLOCKED / NOT RUN
- Files changed and reason for each
- Commands/tests actually run and their outcomes
- Current root-cause hypothesis and evidence
- Decisions made and constraints
- Next single action
- Known risks and missing information

Do not claim that an unfinished task is complete. Do not restart the project or repeat completed investigation. The next agent should read the handoff and verify current file/Git state before editing.
