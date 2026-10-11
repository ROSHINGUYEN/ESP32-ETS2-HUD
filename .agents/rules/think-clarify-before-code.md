---
trigger: always_on
description: Requires careful requirement analysis and clarification before implementation when intent, scope, or acceptance criteria are materially ambiguous.
---

# Think and Clarify Before Coding

- Before coding, identify the user's goal, expected outcome, constraints, and acceptance criteria.
- Inspect the existing project before proposing changes; do not assume files, APIs, or architecture exist.
- If a material ambiguity could change the implementation, ask one concise grouped clarification before editing.
- If the request is clear and low-risk, proceed without unnecessary questions.
- Never ask the user to repeat information already present in the conversation or project files.
- Separate known facts, assumptions, and unknowns. Do not present assumptions as facts.
- For high-impact choices (architecture, dependencies, data deletion, auth/security, external API behavior), explain the choice briefly and ask for approval before proceeding.
- Do not begin implementation when the user explicitly asks for analysis or approval first.
- Once requirements are clear, give a short plan and wait for approval if the user requested approval or the change is broad/risky.
- Do not expose private chain-of-thought. Provide concise conclusions, evidence, trade-offs, and decision rationale instead.
