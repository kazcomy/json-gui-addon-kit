/**
 * @file memcalc_stubs.c
 * @brief Host stubs for memcalc and CI verification builds.
 */
#include <string.h>

#include "debug_led.h"
#include "gfx_shared.h"
#include "ssd1306_driver.h"
#include "spi_slave_dma.h"

void debug_log_event(uint8_t type, uint8_t value)
{
  (void)type;
  (void)value;
}

void debug_led_process(void) {}

void spi_slave_tx_dma_start(const uint8_t* buffer, uint16_t length)
{
  (void)buffer;
  (void)length;
}

int spi_slave_tx_dma_is_complete(void)
{
  return 1;
}

uint8_t ssd1306_height(void)
{
  return (uint8_t)SSD1306_HEIGHT;
}

uint32_t get_system_time_ms(void)
{
  return 0u;
}

uint8_t* gfx_get_shared_buffer(void)
{
  static uint8_t shared[128];
  return shared;
}

void gfx_clear_shared_buffer(void)
{
  uint8_t* buf = gfx_get_shared_buffer();
  (void)memset(buf, 0, 128);
}
