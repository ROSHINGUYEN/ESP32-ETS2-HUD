---
name: ui-preservation
description: Fixes frontend or mobile UI issues without redesigning screens or breaking existing navigation, layout, state, or visual behavior.
---

# UI Preservation

1. Identify the exact screen, state, device size and interaction that is wrong.
2. Inspect the current component tree, theme, routing, state management and shared design tokens.
3. Reuse existing components and styles before creating replacements.
4. Change only the relevant layout, state or interaction.
5. Preserve navigation, accessibility labels, loading/error/empty states and responsive behavior.
6. Do not invent API data or replace live data with mock values to make the UI look correct.
7. If screenshots are available, compare before/after at the same viewport and state.
8. Run relevant UI tests/build checks and state what was not visually verified.
9. Do not redesign the whole app unless explicitly requested.
