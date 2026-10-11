---
name: dependency-change-control
description: Prevents unnecessary package, framework, compiler, and configuration changes during feature work or bug fixes.
---

# Dependency Change Control

1. Inspect the existing manifest and lockfile before changing dependencies.
2. Prefer built-in project capabilities and installed packages when sufficient.
3. Add or upgrade a dependency only when the task requires it and compatibility is checked.
4. Do not upgrade framework/toolchain versions as an incidental fix.
5. Explain package size, platform support, security and migration impact when relevant.
6. Update lockfiles only through the project's package manager.
7. Run relevant install, analysis, test and build checks after a dependency change.
8. If installation is blocked by network or permissions, report BLOCKED instead of fabricating results.
