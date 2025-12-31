# SSD1306 panel

## Roles and Responsibilities
- Monochrome OLED display hardware driven by `gfx_slave` over I2C.
- Displays rendered frames produced by the slave firmware.

## Constraints
- I2C interface; panel sizes supported are 128x32 or 128x64.
- Addressing and timing are determined by the SSD1306 controller.
