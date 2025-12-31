# Containers

The system has two firmware containers, one host tool, and one hardware container:
- **JSON converter**: host-side tool that converts nested JSON into flat short-key elements and checks slave constraints (e.g. element count and arena size).
- **gfx_master**: reference SPI host that sends commands and JSON.
- **gfx_slave**: display slave that parses JSON and renders to SSD1306.
- **SSD1306 panel**: I2C display hardware.
