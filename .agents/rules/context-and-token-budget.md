---
trigger: always_on
description: Keeps agent context and token use focused by reusing known information and avoiding redundant repository reads or long explanations.
---

# Context and Token Budget

- Be concise by default. Prefer short Vietnamese responses when the user communicates in Vietnamese.
- Do not repeat the full prompt, plan, logs, or repository summary unless requested.
- Reuse information already established in the current conversation and handoff notes.
- Read only files relevant to the task; expand the search only when evidence requires it.
- Do not dump entire large files into context when a targeted range, symbol search, or diff is sufficient.
- Prefer one focused command or test that answers a specific question over broad repeated scans.
- Keep plans to a few actionable steps and report only meaningful findings.
- Do not create duplicate documentation, skills, helpers, or abstractions when an existing one can be extended.
- Keep prompts explicit and compact: goal, scope, constraints, acceptance criteria, verification.
- Never omit necessary investigation or safety checks solely to save tokens.
- When context is getting long, create a concise handoff with facts, changed files, tests, blockers, and the next action.
