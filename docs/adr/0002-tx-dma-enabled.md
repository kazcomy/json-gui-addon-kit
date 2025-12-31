# ADR 0002: TX DMA for SPI responses is enabled

## Status
Accepted

## Context
Response frames are contiguous and of predictable length, which fits DMA well.
Reducing per-byte ISR load is important on CH32V003.

## Decision
Use SPI TX DMA for response frames.

## Consequences
- Lower CPU load and reduced interrupt overhead.
- Slight increase in code size for DMA setup and handling.
