# ADR 0001: RX DMA for SPI framing is rejected

## Status
Rejected

## Context
The SPI protocol uses variable-length COBS frames. Frame boundaries are detected
by sync bytes and length, so per-byte inspection is required.

## Decision
Do not use RX DMA for SPI framing. Receive bytes via ISR and run the framing
state machine in software.

## Consequences
- Simpler RX logic and lower RAM usage.
- Per-byte interrupt overhead remains.
