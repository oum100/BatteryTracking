# UI Rules

These rules apply to this project by default.

## Components

- Use `Nuxt UI` components for inputs, buttons, cards, alerts, popovers, tables, badges, forms, and dialogs.
- Keep controls visually consistent between mobile and desktop screens.

## Forbidden

- Do not use `Nuxt UI Tooltip` or `UTooltip`.
- If help text is needed, use inline helper text, `UAlert`, `UPopover`, or static labels instead.

## Contrast

- Text and background must have strong contrast in normal office and factory lighting.
- Labels must stay readable and must not fade into the page background.
- Placeholder text may be softer, but active text, labels, and values must remain clearly visible.
- Do not place low-contrast text on pale cards.

## Forms

- Search, filter, and data-entry controls must be clearly distinguishable from passive content.
- Inputs should use lighter surfaces than action buttons.
- Buttons should use a stronger fill than inputs so the user can tell them apart immediately.
- When values come from a known set in the database, prefer controlled selection such as dropdowns.

## Interaction

- Critical information must not depend on hover.
- Avoid hover-only explanations or hidden state indicators.
- Prefer layouts that remain understandable at a glance.
