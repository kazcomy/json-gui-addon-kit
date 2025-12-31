# JSON converter

## Roles and Responsibilities
- Host-side tool that converts nested, long-key UI JSON into a flat element list.
- Emits short keys and short type tokens required by the slave parser.
- Emits a header element (`t=h`) with element count (`n`) required by the slave.
- Assigns parent indices and preserves ordering so parents appear before children.
- Lints input (keys, types, ranges) before conversion.
- Computes exact arena usage by running the real slave JSON parser via a host-side
  memcalc library and fails on overflow.

## Constraints
- Input must be nested (elements arrays); flat input is rejected.
- Short keys/tokens are not accepted in input.
- Output uses the short-key format enforced by the slave.
- TEXT capacity `c` is clamped to 0..20; `0` means auto (use `tx` length, clamped to 20).
- Header is required; output without `t=h` is rejected by the slave.
- A host C compiler is required to build the memcalc shared library on demand.
