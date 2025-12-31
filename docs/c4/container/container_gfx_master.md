# gfx_master

## Roles and Responsibilities
- Reference SPI master firmware used for bring-up and testing.
- Sends SPI commands and flat JSON element updates to `gfx_slave`.
- Optionally forwards local input events to the slave.

## Constraints
- Reference implementation; host MCU can be different. Current build targets CH32V003F4P6 (2 KB RAM, 16 KB flash).
- Input forwarding (when enabled) uses release events only.
- Uses the unified SPI framing defined by the protocol.
