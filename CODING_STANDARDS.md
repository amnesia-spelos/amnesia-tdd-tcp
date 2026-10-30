# Coding Standards

These are the judgement calls a reviewer applies to this repository. Mechanical rules, such as formatting and brace style, belong in automated checks and are not listed here.

## Glossary casing

User-facing strings and comments use the terms in `GLOSSARY.md` with the casing the glossary gives them.

**Why:** one spelling per concept keeps the code, logs and issues searchable and matches the shared domain language.

**Example:** `scripts/test-chat-focus.ps1` prints `Chat Composer focus invariant is enforced.`, with both words capitalised as the glossary gives them.

## Engine accessor pairs

Engine accessors come in Set/Get pairs. Accessors, parameters and members use Hungarian names. A getter that nothing calls yet but completes a pair follows this convention, so a reviewer should not flag it as speculative generality.

**Why:** the HPL2 engine is written this way throughout, and new code should read like the code around it.

**Example:** `cWidgetTextBox` in `src/HPL2/core/include/gui/WidgetTextBox.h` pairs `SetUIKeyboardNextFocus(iWidget* apWidget)` with `GetUIKeyboardNextFocus()`, and `SetUIKeyboardEnabled(bool abX)` with `GetUIKeyboardEnabled()`. Its members are `mbUIKeyboardOpen`, `mbUIKeyboardEnabled` and `mpUIKeyboardNextFocus`.

## Header layout changes need a full rebuild

Adding or removing a data member on an engine or game header class means building with `scripts/build-windows.ps1 -Rebuild`. The commit message or PR description says the change needs a rebuild.

**Why:** incremental builds miss layout changes and can leave objects compiled against the old layout, which crash at runtime.

**Example:** commit 19fa630 (#115) added `mbUIKeyboardEnabled` to `cWidgetTextBox` and its message ends with "cWidgetTextBox gains a member, so build with build-windows.ps1 -Rebuild."
