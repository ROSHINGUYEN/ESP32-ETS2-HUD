---
trigger: always_on
description: Requires evidence-based testing and honest PASS, FAIL, or BLOCKED reporting.
---

# Verification Contract

- Distinguish verified behavior from assumptions, mocks, stubs and untested code.
- Run the narrowest relevant test first, then broader checks when appropriate.
- Never claim a command passed unless it actually ran and returned success.
- If tests cannot run, state the exact missing tool, permission, service, secret, device or environment.
- Do not silence errors, weaken assertions, skip tests or fake API responses merely to get green results.
- Inspect logs and test output; do not infer success from a process starting.
- Finish with: changed files, root cause, validation commands, actual results, and remaining risks.
