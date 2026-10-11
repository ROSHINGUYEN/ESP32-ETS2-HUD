---
name: test-and-verify
description: Selects and runs relevant tests, analyzes failures, and reports verified outcomes without fabricating success.
---

# Test and Verify

1. Discover the project's existing test and build commands from manifests, scripts and documentation.
2. Run the narrowest test related to the change.
3. Classify failure as: caused by change, pre-existing, environment/tooling, flaky, or unknown. Provide evidence.
4. Do not edit unrelated code to make a test pass.
5. Do not delete tests, skip assertions, replace real integration checks with mocks, or claim unrun tests passed.
6. Run static analysis/build only when relevant and feasible.
7. Verify user-visible behavior when runtime/device testing is required; source inspection alone is not runtime proof.
8. Report commands, exit status and meaningful output.
9. Use PASS only when evidence supports the exact claim; otherwise use FAIL, BLOCKED or NOT RUN.
