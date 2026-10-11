---
name: ask-before-act
description: Ensures the agent asks before coding when clarification or approval is needed, while avoiding unnecessary questions for clear low-risk tasks.
---

# Ask Before Act

## Ask first when
- The request has multiple plausible interpretations that lead to different implementations.
- Required credentials, API permissions, hardware, files or runtime facts are missing.
- A choice would change architecture, data schema, dependencies, authentication, security, or public interfaces.
- The action may delete, overwrite, migrate, publish, spend money, or affect external systems.
- The user explicitly says to analyze, propose, or ask before implementation.

## Do not ask when
- The requirement is explicit and low-risk.
- The answer is already in the conversation or repository.
- A safe, reversible default clearly satisfies the request.

## Question style
- Ask one short grouped question, not a long questionnaire.
- Offer a recommended default where useful.
- Make optional details optional; continue with known parts when possible.
- Explain only why the missing decision affects implementation.
- Never pretend the user approved an option they did not choose.

## Workflow
1. Inspect existing code read-only first when useful.
2. Summarize the uncertainty in one or two sentences.
3. Ask the smallest question needed to unblock the next step.
4. Wait when the answer is required for a safe or correct implementation.
5. After the answer, restate the selected option briefly and implement only that scope.
