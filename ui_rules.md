# UI Rules

These rules apply to this project by default.

## Workflow

- Read this file before starting any UI work.
- Treat this file as the source of truth for layout, component usage, shape language, and interaction behavior.
- If a new UI decision conflicts with this file, update this file in the same task so future work stays consistent.

## Component Policy

- Use `Nuxt UI` components for all visible interactive UI by default.
- Preferred controls:
  - `UFormField`
  - `UInput`, `UTextarea`
  - `USelectMenu` / `USelect`
  - `UButton`
  - `UCard`
  - `UAlert`
  - `UModal` / `USlideover`
  - `UBadge`
  - `UTable` when a data table is needed
- Do not hardcode native `<input>`, `<select>`, `<textarea>`, or `<button>` when a `Nuxt UI` equivalent already exists.
- Native hidden inputs are allowed only for hardware integration flows such as HID scanner capture, focus routing, or browser-specific device behavior.
- If a native control is required, document the reason in the same file or PR.

## Shape Language

- Inputs are rectangular with small corner radius.
- Default visible input radius: about `8px`.
- Readonly display fields follow the same rectangular shape.
- Dropdown trigger fields follow the same rectangular shape as inputs.
- Buttons are pill-shaped or half-round.
- Action buttons should use `rounded-full`.
- Cards should use moderate rounding only, not oversized bubble corners.
- Default card radius target: around `15px`.
- Modals should not use giant round corners.
- Avoid very large rounded corners for cards, tables, status panels, and modals.
- Large round corners are reserved mainly for buttons and small capsules.

## Battery QC System Specific

- Apply all rules in this file to every page in the Battery QC System flow:
  - landing / mode selection
  - admin page
  - admin login / auth entry page
  - worker pages
  - calibration page
  - summary / report page
- On Battery QC pages:
  - visible editable fields should use `UFormField` + `UInput`
  - editable fields should use `variant="outline"`
  - readonly display fields should use `UInput` with `variant="ghost"` and `readonly`
  - placeholders must be present on editable fields
- Keep hardware workflow UI simple and obvious:
  - input fields must not look like buttons
  - buttons must not look like text fields
  - mode buttons should feel like actions, not form fields

## Contrast

- Text and background must have strong contrast in office, warehouse, and factory lighting.
- Labels, values, and actionable text must remain readable at a glance.
- Placeholder text may be softer, but must still be readable.
- Informational or warning panels on bright backgrounds must keep dark, high-contrast text; do not rely on pale yellow text on pale cards.
- When a message is informational rather than destructive, prefer blue / slate treatments over washed-out yellow if readability is better.
- Do not use low-contrast gray-on-gray or color combinations that disappear on hover, focus, or disabled states.
- Hover states must preserve readability.
- Focus states must preserve field boundaries and not make borders disappear.
- Avoid pale text on pale cards.
- Avoid color combinations that clash with the PUMA logo and make branding unreadable.

## Theme

- Pages that introduce or gate a workflow, such as admin login, should support both light and dark theme when the surrounding workspace already supports theme switching.
- Theme variants must preserve contrast parity; a readable light theme is not enough if the dark theme becomes muddy, and vice versa.
- Brand imagery such as the PUMA logo must remain visible and intentional in both themes.
- For the admin login page, prefer a muted blue-gray light theme over bright white backgrounds; avoid washed-out light screens that bloom under strong ambient light.
- Keep the current admin login light theme in the soft powder-blue / slate family as the baseline reference for future auth-page styling.
- On the admin login page, place the brand block directly above the sign-in section instead of repeating separate hero copy elsewhere.
- On Battery QC admin workspace pages, keep the same muted powder-blue / slate page background family, but elevate cards, tables, and filter areas with clearer neutral surfaces so content does not blend into the page background.
- Light-theme admin pages should separate page background, cards, tables, and inputs with visible contrast while staying in the same blue-gray family.

## Forms

- Search, filter, scan, and data-entry controls must be clearly distinguishable from passive content.
- Inputs should visually read as fields first.
- Buttons should visually read as actions first.
- Known database values should prefer controlled selection behavior where practical.
- If a single field supports both scan and manual selection, keep it visually one field, not two duplicated fields.
- Keep default input height at `size="md"` unless a screen has a clear reason to differ.

## Layout

- Prefer clean grids with aligned columns between related rows.
- Related rows in the same form should use matching column widths where possible.
- Avoid unnecessary repeated headings when the same state is already obvious from a larger banner or phase card.
- Keep headers compact and avoid wasting vertical space.
- Wide tablet and desktop layouts should prioritize single-row readability over stacked noise.
- Mobile layouts must reduce font sizes and wrapping pressure to avoid awkward line breaks.

## Interaction

- Critical information must not depend on hover.
- Avoid hover-only explanations or hidden state indicators.
- Prefer layouts that remain understandable at a glance.
- Focus flow matters:
  - after mode changes or confirmed scan actions, focus should move to the next expected field
  - when a modal interrupts flow, closing it should return the user to the correct next action state
- If a workflow is scanner-driven, optimize for scan -> fill -> move next with minimal clicks.

## Status and Readability

- Use short, clear labels for compact status badges.
- Prefer concise status words such as `HERE`, `WAIT`, `DONE` when space is tight.
- Do not overload cards with duplicate state labels if color and context already communicate the same thing.

## Forbidden

- Do not use `UTooltip` for critical help or workflow instructions.
- If help text is needed, use inline helper text, `UAlert`, `UPopover`, or static labels instead.
- Do not default to oversized pill inputs.
- Do not use giant rounded corners on cards or modals.
- Do not introduce hardcoded control styles that diverge from the shared UI rules without updating this file.
