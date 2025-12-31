# UI Model and JSON Rules

## Overview
Defines the flat UI element model accepted by the slave and the JSON rules for provisioning and updates.

## Element types
| token | name | purpose | focusable |
| --- | --- | --- | --- |
| `s` | screen | root container for base or overlay screens | no |
| `l` | list | navigable list of text rows | yes |
| `t` | text | label or list row | no |
| `b` | barrel | selectable option list | yes |
| `i` | trigger | action event; OK increments version | yes |

## Common keys
| key | meaning | notes |
| --- | --- | --- |
| `t` | type | one of `s/l/t/b/i` (header uses `h`) |
| `p` | parent id | must reference a previously-defined element |
| `x`,`y` | position | list row text ignores `y` |
| `e` | element id | update mode when present |

## Header (`h`)
Purpose:
- Reserve per-element tables before provisioning.

Keys:
- `n`: element count (1..255).

Rules:
- Required for provisioning; non-header objects are rejected until the header is parsed.
- Must be the first object after a JSON HEAD reset.
- Not counted as an element id; element ids start after the header.

## Screen (`s`)
Purpose:
- Root container for base screens and overlay screens.
- Local screen for list-row navigation when parented under TEXT or LIST.

Keys:
- `ov`: overlay role when screen is root (`0` none, `1` full overlay).

Parenting behavior:
- Root screen: `p` omitted or invalid.
- Local screen: if parent is `TEXT`, it attaches to that row; if parent is `LIST`, it attaches to the most recent text row of that list.

## List (`l`)
Purpose:
- Navigable list with a cursor and scroll window.

Keys:
- `r`: desired visible rows (clamped to 1..6 at creation; further limited by display height).

Parenting behavior:
- Typical parent is a screen.
- If parented under a text row, it becomes a nested list target for that row.

Child behavior:
- Rows are `TEXT` children; order is creation order.
- Only `TEXT` children count as rows.

## Text (`t`)
Purpose:
- Label element and list row.

Keys:
- `tx`: text string.
- `c`: text capacity (0..20). `0` means auto (use `tx` length, clamped to 20).

Parenting behavior:
- Parent is `LIST`: becomes a list row (row Y derived from row index).
- Parent is `BARREL`: acts as a barrel option label.
- Parent is `SCREEN` or other: regular text.

Child behavior:
- A `BARREL` child becomes an inline barrel for that row.
- A `SCREEN` child becomes a local screen for that row.
- A `LIST` child becomes a nested list for that row.

## Barrel (`b`)
Purpose:
- Selectable option list with optional edit mode.

Keys:
- `v`: selection index.

Behavior:
- Options are `TEXT` children; count determines wrap range.
- OK toggles edit/commit; UP/DOWN cycles options while editing; BACK cancels edit.

## Trigger (`i`)
Purpose:
- Action trigger with a version counter.

Keys:
- None.

Behavior:
- OK increments the trigger version.

## Parent and navigation rules
- `p` must reference a previously created element.
- Parenting is not strictly validated by the parser; the behaviors above define how parents affect navigation and rendering.
- Nested list: `LIST` child under a row `TEXT`.
- Local screen: `SCREEN` child under a row `TEXT`, or under a `LIST` to attach to the most recent row.

## Update rules
- When `e` is present, the element is updated in place; structural keys are ignored.
- Text updates apply `tx` only; barrel updates apply `v` only; trigger updates are ignored (version changes via OK).
- If `t` is provided during update and does not match the existing type, the update is ignored.

## Focus and input handling
- Input events are processed on release.
- Focusable types: list, barrel, trigger.
- Focus traversal order: creation order, filtered by visibility and focusable type.
- Screen change sets focus to the first focusable element on the active screen.
- LEFT/RIGHT slides base screens at root and clears focus; otherwise ignored.

| focus kind | UP/DOWN | OK | BACK | LEFT/RIGHT |
| --- | --- | --- | --- | --- |
| none | move focus prev/next | move focus next | pop nav or focus first | root slide or ignored |
| list | move cursor (scrolls) | row action (inline barrel / nested list / local screen) | pop nested list if active | ignored |
| barrel (edit off) | move focus prev/next | enter edit | focus parent list | ignored |
| barrel (edit on) | change option | commit edit, return to parent list | cancel edit, return to parent list | ignored |
| trigger | move focus prev/next | increment version | focus owning list if any | ignored |

## Visibility rules
- Elements are visible when they are descendants of the active screen or the current navigation target.
- Local screens are visible only while on the navigation stack.
- Overlay screens ignore scroll/animation offsets and render on top.
