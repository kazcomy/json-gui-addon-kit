## Copilot Project Instructions
- All code comments must be written in English.
- All project documents(not code files) should be maintained in English.
- All Chat responses should be in Japanese.
- Do not localize filenames, code, or docs into Japanese; only conversational Chat replies should be in Japanese.

- Never try to commit anything on git.
- Never try to change SPI pin role, including pinmux and even changing sw controlled CS to hw controlled).If you find pinmap is not coincident between slave MCU and master MCU, it's natural because schematics is not open to you. Keep in mind that you're destroying hardware when you're doing so.

- Always try to find root cause of issues with evidence before applying fixes just guessing fake reason only from source code.
- Comments should be doxygen-style where applicable.
- coding style should follow google-c-style and MISRA-C++-2008.
- if something is removed from code or docs, don't leave comments like "this is removed".

## Hardware/Flashing Safety Policies

- Never perform flashing, uploading, or programming to any microcontroller without user's explicit instruction.
- (Again)Never modify pin mappings or hardware-related configurations on your own. This includes but is not limited to:
  - SPI remap or pin changes
  - I2C remap or pin changes
  - Enabling/disabling UART or changing its pins
  - GPIO mode/direction/pull configuration changes
- When editing source code, do not introduce code that changes board pin assignments or peripheral remaps unless the user explicitly requests it. If a task may require such changes, clearly ask for confirmation first.

## Agents and Roles

This project consists of synchronized roles that must remain conceptually aligned:

- **gfx_slave**: Runs on CH32V003 attached to SSD1306. Accepts SPI commands and JSON to build and render UI. Button input source is configured via SPI, independent of demo mode.
- **gfx_master**: Host/master that sends SPI frames and JSON to the slave to control screens/elements and to deliver input events when configured. It runs on CH32V003 but different MCU than the slave. Don't asssume pin mappings are the same between slave and master and same peripherals(e.g. LED) are available.
