---
name: feature-completion
description: Completes a feature end to end against acceptance criteria while preserving existing behavior and clearly reporting blockers.
---

# Feature Completion

1. Inspect existing feature flow before implementing.
2. Convert the request into observable acceptance criteria.
3. Trace UI, state, persistence, service/API and error states as applicable.
4. Reuse existing architecture; do not create a parallel implementation without justification.
5. Implement one vertical slice at a time.
6. Test success, loading, empty, failure and retry paths where relevant.
7. Verify permissions and real integration requirements.
8. Review diff for unrelated changes and duplicated logic.
9. Report each acceptance criterion as PASS, FAIL, BLOCKED or NOT RUN with evidence.
10. Do not claim full completion if a required external service, credential, device or permission was unavailable.
