# SPI-Controlled Multi Information Display
Drop-in SPI GUI for maker hardware: add screens and key input without reinventing UI.
A host MCU (SPI master) provisions UI structure via shrinked format of JSON and updates values/events at runtime.

The slave MCU runs tiny UI renderer firmware for SSD1306 (128x32/64) on CH32V003.
Rendering is tile-based (u8g2-style): a 128-byte page buffer is streamed over non-blocking I2C DMA.

## System overview
- Host MCU converts nested JSON via `tool/nested_to_flat.py`, then sends one element object per SPI frame.
- The gfx_slave parses and stores elements in a shared arena (tables + attributes + runtime).
- Local buttons on the slave update UI state; the slave raises an interrupt so the host can read status/state.

## Documentation
- C4 model docs live in `docs/c4/` (context/container/component/code). Entry points:
  `docs/c4/context/context.md`, `docs/c4/container/container.md`,
  `docs/c4/component/component.md`, `docs/c4/code/ui_architecture.md`
- ADRs (design decisions) live in `docs/adr/` with index in `docs/adr/README.md`
- Key details: `docs/c4/component/spi_protocol.md`, `docs/c4/component/ssd1306_driver.md`,
  `docs/c4/container/container_json_converter.md`

## Master-side development (what to read)
- `docs/c4/container/container_gfx_master.md` (role/constraints)
- `docs/c4/component/spi_protocol.md` (framing + commands)
- `docs/c4/component/ui_model.md` (JSON model + update rules)
- `docs/c4/container/container_json_converter.md` (converter rules)
- `tool/nested_to_flat.py` (converter implementation)
- `include/master/master_spi.h`, `src/master/master_spi.c` (SPI transport)
- `src/master/master_main.c` (reference host flow)
- `include/master/demo_json.h` (example flat JSON)
- `include/common/status_codes.h`, `include/common/cobs.h`, `include/common/ui_buttons.h` (shared protocol)

## Repository layout
- `src/slave/`, `include/slave/`: display slave firmware (protocol, UI, renderer, SSD1306)
- `src/master/`, `include/master/`: reference SPI master
- `src/common/`, `include/common/`: shared utilities (COBS, status codes, button indices)
- `docs/`, `test/`, `tool/`

## Protocol summary
- Framing: `[0xA5][0x5A][LEN][COBS(cmd||payload)]` (no CRC)
- JSON: one element object per frame, HEAD/COMMIT for batching
- Rendering: tile/page buffer + non-blocking I2C DMA (main-loop driven)
