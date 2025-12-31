# ADR 0005: Linear scan for attribute lookup

## Status
Accepted

## Context
Attribute entries are stored in a compact shared arena to maximize RAM usage.
Maintaining per-element index tables would add fixed overhead and duplicate state
for data that is typically small.

## Decision
Use a linear scan across the attribute entries to resolve `(tag, element_id)`
lookups. This keeps the arena format simple and avoids per-element index arrays.

## Consequences
- Lookup cost is O(n) in number of attributes but acceptable for the expected UI sizes.
- RAM usage is minimized by avoiding fixed per-element mapping tables.
- If attribute counts grow significantly, revisit indexing or caching strategies.
