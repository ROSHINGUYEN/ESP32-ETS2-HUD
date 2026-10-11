---
name: context-budgeting
description: Reduces token and context consumption by using targeted repository inspection, compact plans, concise diffs and durable handoffs.
---

# Context Budgeting

## Repository reading
- Start with project manifests, agent instructions, relevant entry points and the target symbol.
- Search for specific symbols, routes, error strings or call sites before opening large files.
- Read only the surrounding ranges needed to understand behavior.
- Expand to adjacent files only when the call path or test evidence requires it.
- Do not repeatedly reread unchanged files; use the current diff and prior notes.

## Tool use
- Prefer targeted commands over broad recursive output.
- Avoid printing generated files, lockfiles, build output or huge logs into context.
- Filter logs to the relevant error and surrounding lines.
- Run focused tests before full suites unless integration risk requires broader checks.

## Communication
- Use compact Vietnamese by default for Vietnamese requests.
- Keep plans short and avoid restating the entire request.
- Report only changed files, meaningful findings, test results and blockers.
- Never claim success to save tokens; preserve essential verification and safety.

## Handoff
When a session is long or context is nearly exhausted, write a concise handoff:
- goal and acceptance criteria
- files inspected/changed
- current diff/status
- tests and actual results
- decisions and unresolved questions
- next single action

Do not duplicate existing documentation or start the investigation from scratch without checking the handoff.
