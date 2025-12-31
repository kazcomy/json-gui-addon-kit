# ADR 0003: Use a single shared RAM arena

## Status
Accepted

## Context
Multiple small pools increased fragmentation and code size. Runtime state and
attributes need deterministic allocation without a heap.

## Decision
Use one static arena. Attributes grow from the head; runtime state grows from
the tail.

## Consequences
- Deterministic memory usage and easy overflow checks.
- Structural changes require HEAD (reprovision).
