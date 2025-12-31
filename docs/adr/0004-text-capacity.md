# ADR 0004: Per-text capacity is structural and immutable

## Status
Accepted

## Context
A global text buffer wastes RAM and hides memory costs. The UI needs predictable
per-element text allocation.

## Decision
Text elements define a capacity `c` (0..20). The device allocates `c+1` bytes.
At runtime, updates longer than `c` are truncated. Capacity is structural and
cannot change after COMMIT.

## Consequences
- Predictable RAM usage per text element.
- Clear host-side rules for truncation.
