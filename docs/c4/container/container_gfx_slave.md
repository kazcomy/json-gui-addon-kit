# gfx_slave

## Roles and Responsibilities
- SPI slave firmware for the SSD1306 display; renders UI on the panel over I2C.
- Accepts SPI commands and flat JSON element updates from the host to build/update UI state.
- Processes input events (SPI or local) and manages navigation/overlay runtime state.

## Constraints
- WCH CH32V003F4P6 (2 KB RAM, 16 KB flash)
- Arena/bump allocator on static memory
- Element capacity is provided by the JSON header (`t=h`, `n`), which is required.
- Single shared arena: element tables + attributes grow from the head; runtime nodes allocate from the tail.
