/**
 * @file main.c
 * @brief Main application for SPI Display Slave system.
 *
 * This is the main entry point for the CH32V003-based SPI slave display system that:
 * - Initializes the SSD1306 OLED display via I2C
 * - Sets up the UI protocol stack for SPI communication
 * - Manages the main application loop and timing
 * - Handles incoming SPI commands and renders UI updates
 * - Supports configurable input sources (SPI or local buttons)
 *
 * The system supports real-time UI updates and can dynamically create
 * display elements from JSON descriptions received via SPI.
 *
 * Memory usage (optimized): 82.0% flash (13,432 bytes), 90.4% RAM (1,852 bytes)
 *
 *
 * Note: This file implements the SPI slave variant intended for host-driven UI.
 * Standalone self-rendering builds are outside the scope of this binary.
 */
/* ============================================================================
 * SPI Display Core - Main Application
 * ============================================================================ */
#include <string.h>

#include "ch32fun.h"
#include "i2c_custom.h"
#include "ssd1306_driver.h"
#include "gfx_font.h"
#include "gfx_shared.h"
#include "ui_protocol.h"
#include "spi_slave_dma.h"
#include "debug_led.h"
/* GLOBALS */

/** Main loop delay in milliseconds. */
#define MAIN_LOOP_DELAY_MS 1


#define LED_PIN PD0
#define INTERRUPT_PIN PD3

static uint8_t debug_led_state = 0U;

/** Set debug LED output to a stable on/off state. */
static inline void debug_led_write(uint8_t on)
{
  debug_led_state = (on != 0U) ? 1U : 0U;
  funDigitalWrite(LED_PIN, debug_led_state);
}

/** Toggle debug LED output state. */
static inline void debug_led_toggle(void)
{
  debug_led_state ^= 1U;
  funDigitalWrite(LED_PIN, debug_led_state);
}

/** System time counter in milliseconds. */
static uint32_t g_system_time_ms = 0;

typedef struct
{
  uint8_t type;
  uint8_t value;
} debug_led_event_entry_t;

#define DEBUG_LED_QUEUE_SIZE 8u
#define DEBUG_LED_VALUE_MAX 7u
#define DEBUG_LED_PULSE_SPACING_MS 20u
#define DEBUG_LED_STAGE_GAP_MS 40u
#define DEBUG_LED_EVENT_GAP_MS 80u

static volatile uint8_t          debug_event_head        = 0u;
static volatile uint8_t          debug_event_tail        = 0u;
static debug_led_event_entry_t   debug_event_queue[DEBUG_LED_QUEUE_SIZE];
static uint8_t                   debug_led_active        = 0u;
static uint8_t                   debug_led_stage         = 0u;
static uint8_t                   debug_led_pulses        = 0u;
static uint8_t                   debug_led_value_pulses  = 0u;
static uint32_t                  debug_led_next_toggle   = 0u;
static uint32_t                  debug_led_idle_until    = 0u;

void debug_log_event(uint8_t type, uint8_t value)
{
  uint8_t capped_type  = (type == 0u) ? 1u : ((type > 15u) ? 15u : type);
  uint8_t stored_value = (value <= DEBUG_LED_VALUE_MAX) ? value : 0xFFu;
  uint8_t next         = (uint8_t) ((debug_event_head + 1u) % DEBUG_LED_QUEUE_SIZE);
  if (next == debug_event_tail) {
    return;
  }
  debug_event_queue[debug_event_head].type  = capped_type;
  debug_event_queue[debug_event_head].value = stored_value;
  debug_event_head                          = next;
}

void debug_led_process(void)
{
  uint32_t now = g_system_time_ms;
  if (debug_led_active == 0u) {
    if (now < debug_led_idle_until) {
      return;
    }
    if (debug_event_head == debug_event_tail) {
      return;
    }
    debug_led_event_entry_t ev = debug_event_queue[debug_event_tail];
    debug_event_tail = (uint8_t) ((debug_event_tail + 1u) % DEBUG_LED_QUEUE_SIZE);
    debug_led_active       = 1u;
    debug_led_stage        = 0u;
    debug_led_pulses       = ev.type;
    debug_led_value_pulses = (ev.value <= DEBUG_LED_VALUE_MAX) ? (uint8_t) (ev.value + 1u)
                                                               : 0u;
    debug_led_next_toggle = now;
    debug_led_write(1u);
    return;
  }

  if (now < debug_led_next_toggle) {
    return;
  }

  if (debug_led_pulses > 0u) {
    debug_led_toggle();
    debug_led_pulses--;
    debug_led_next_toggle = now + (uint32_t) DEBUG_LED_PULSE_SPACING_MS;
    if (debug_led_pulses == 0u) {
      debug_led_write(0u);
      if ((debug_led_stage == 0u) && (debug_led_value_pulses > 0u)) {
        debug_led_stage        = 1u;
        debug_led_pulses       = debug_led_value_pulses;
        debug_led_value_pulses = 0u;
        debug_led_next_toggle  = now + (uint32_t) DEBUG_LED_STAGE_GAP_MS;
      } else {
        debug_led_active     = 0u;
        debug_led_stage      = 0u;
        debug_led_idle_until = now + (uint32_t) DEBUG_LED_EVENT_GAP_MS;
      }
    }
  }
}

/* Local button support. Default pins match reference wiring; adjust for your HW. */
#ifndef LB_OK_PIN
#define LB_OK_PIN PD4
#endif
#ifndef LB_UP_PIN
#define LB_UP_PIN PD6
#endif
#ifndef LB_DOWN_PIN
#define LB_DOWN_PIN PC4
#endif
#ifndef LB_BACK_PIN
#define LB_BACK_PIN PC3
#endif
#ifndef LB_LEFT_PIN
#define LB_LEFT_PIN PD2
#endif
#ifndef LB_RIGHT_PIN
#define LB_RIGHT_PIN PD5
#endif

static const uint8_t pins[UI_BUTTON_COUNT] = {
  [UI_BUTTON_UP]    = LB_UP_PIN,
  [UI_BUTTON_DOWN]  = LB_DOWN_PIN,
  [UI_BUTTON_OK]    = LB_OK_PIN,
  [UI_BUTTON_BACK]  = LB_BACK_PIN,
  [UI_BUTTON_LEFT]  = LB_LEFT_PIN,
  [UI_BUTTON_RIGHT] = LB_RIGHT_PIN,
};

/** Configure GPIOs for the optional local button inputs. */
static void local_buttons_setup(void)
{
  for (uint8_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    funPinMode(pins[i], GPIO_CNF_IN_FLOATING);
  }
}

/* reuse protocol's input handler via SPI_CMD_INPUT_EVENT semantics */
static void local_buttons_poll(void)
{
  static uint8_t prev[UI_BUTTON_COUNT] = {0, 0, 0, 0, 0, 0};
  static uint8_t curr[UI_BUTTON_COUNT] = {0, 0, 0, 0, 0, 0};

  /* Translate local GPIO edges into protocol input events. */
  for (uint8_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
    curr[i] = funDigitalRead(pins[i]);
    if (prev[i] && !curr[i]) {
      static uint8_t pl[2] = {0, 0};
      pl[0] = i;
      cmd_input_event(pl, sizeof(pl));
    }
    prev[i] = curr[i];
  }
}

/**
 * @brief Show a simple boot banner prior to host provisioning.
 */
static void show_boot_banner(void)
{
  static const char banner_text[] = "SLAVE START";
  const size_t      len           = strlen(banner_text);

  gfx_clear_shared_buffer();

  uint16_t text_width = 0U;
  if (len > 0U) {
    const uint16_t glyph_span = (uint16_t) (GFX_FONT_CHAR_WIDTH + 1U);
    text_width                = (uint16_t) (len * glyph_span);
    if (text_width > 0U) {
      text_width -= 1U;
    }
  }

  uint8_t start_x = 0U;
  if (text_width < (uint16_t) SSD1306_WIDTH) {
    start_x = (uint8_t) (((uint16_t) SSD1306_WIDTH - text_width) / 2U);
  }

  ssd1306_tile_text(start_x, 0, banner_text);

  uint8_t target_page = 0U;
  uint8_t total_pages = ssd1306_pages();
  if (total_pages > 0U) {
    target_page = (uint8_t) ((total_pages - 1U) / 2U);
  }

  ssd1306_write_page(target_page, gfx_get_shared_buffer());
}

/**
 * @brief Configure EXTI interrupt to wake on SPI CS (PC0) falling edge and enter standby.
 * Sequence: display off -> map EXTI0 to port C -> enable interrupt + falling trigger ->
 * set deep sleep -> WFI -> upon wake, restore system clock and reinit critical peripherals.
 */
static void enter_standby_wait_cs_falling(void)
{
  /* Ensure no I2C DMA chunk is in flight before powering down display */
  while (ssd1306_dma_xfer_active()) {
    ssd1306_render_async_process();
  }
  /* Turn display off to reduce consumption and avoid artifacts on wake */
  ssd1306_command(SSD1306_CMD_DISPLAY_OFF);

  /* Stop any ongoing SPI TX DMA */
  spi_slave_tx_dma_wait_complete();
  spi_slave_tx_dma_stop();

  #if 0
  /* Wait for any i2c ongoing transfers to complete */
  while(i2c_tx_dma_busy());
  #endif

  // Enable LSI clock for standby mode
  RCC->RSTSCKR |= RCC_LSION;
  while((RCC->RSTSCKR & RCC_LSIRDY) == 0);

  /* AFIO for EXTI configuration */
  RCC->APB2PCENR |= RCC_APB2Periph_AFIO;
  /* Map EXTI line 0 to Port C (00=PA,01=PB,10=PC,11=PD) */
  //AFIO->EXTICR &= ~((uint32_t) (0x3u << (2 * 0)));
  AFIO->EXTICR = 0; 
  AFIO->EXTICR |= ((uint32_t) (0x2u << (2 * 0)));
  /* Configure EXTI0 interrupt on falling edge (CS goes low) */
  EXTI->EVENR &= ~EXTI_Line0;         /* event not used */
  EXTI->RTENR &= ~EXTI_Line0;         /* rising disabled */
  EXTI->FTENR |= EXTI_Line0;          /* falling enabled */
  EXTI->INTFR  = EXTI_Line0;          /* clear any pending */
  EXTI->INTENR |= EXTI_Line0;         /* enable interrupt */
  /* Enable EXTI7_0 interrupt in PFIC (IRQ#20) */
  PFIC->IENR[0] |= (1u << 20);

  /* Select deep sleep (power-down) and wait for event */
  /* Enable PWR peripheral clock then select Standby (PDDS=1) */
  //RCC->APB1PCENR |= (1u << 28); /* PWREN: Power interface clock enable */
  PWR->CTLR     |= PWR_CTLR_PDDS;
  PFIC->SCTLR |= (1u << 2); /* SLEEPDEEP */
  __WFI();

  /* After wake: restore system clocks and reinitialize critical interfaces */
  SystemInit();
  //spi_slave_transport_init();
  /* Reinitialize SSD1306 display to turn it back on and clear residual state */
  if (ssd1306_init() != 0) {
    ssd1306_set_height(64);
    ssd1306_clear();
  }
  //local_buttons_setup();
  g_render_requested = 1;
}

/**
 * @brief Initialize the entire system.
 *
 * Performs the following initialization steps:
 * 1. System clock and GPIO initialization
 * 2. LCD power supply setup
 * 3. SSD1306 display driver initialization
 * 4. UI protocol stack initialization
 * 5. Boot screen display
 */
void system_init(void)
{
  Delay_Ms(100);  // Allow power of ssd1306 to stabilize
  SystemInit();
  funGpioInitAll();
  funPinMode(LED_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
  debug_led_write(0U);
  funPinMode(INTERRUPT_PIN, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);
  funDigitalWrite(INTERRUPT_PIN, 1);
  /* Initialize SPI slave transport early so we're ready before any host traffic */
  protocol_init();
  ssd1306_init();
  ssd1306_set_height(64);
  ssd1306_clear();
  show_boot_banner();
  /* Startup prints are disabled to reduce logs */
  spi_slave_transport_init();
  debug_led_write(1U);
  local_buttons_setup();
}

/**
 * @brief Get current system time in milliseconds.
 * @return System time in milliseconds
 */
uint32_t get_system_time_ms(void)
{
  return g_system_time_ms;
}

/** Advance protocol/UI animations based on system time. */
void update_animations(void)
{
  protocol_tick_animations();
}

/** Clamp active screen index to a valid range after state changes. */
static void normalize_active_screen(void)
{
  if (g_protocol_state.active_screen >= g_protocol_state.screen_count) {
    g_protocol_state.active_screen = 0u;
  }
}

/** Trigger an async render when a render request is pending. */
static void handle_render_request(void)
{
  if (!g_render_requested) {
    return;
  }
  g_render_requested = 0;
  normalize_active_screen();
  ssd1306_render_async_start_or_request(render_screen_tile);
}

/** Enter standby if a host request is pending. */
static void handle_standby_request(void)
{
  if (g_request_standby == 0u) {
    return;
  }
  g_request_standby = 0u;
  enter_standby_wait_cs_falling();
}

/** Sleep for a tick and advance the system time base. */
static void main_loop_delay_and_tick(uint32_t delay_ms)
{
  Delay_Ms(delay_ms);
  g_system_time_ms += delay_ms;
}

/** Execute one main loop iteration in a fixed order. */
static void main_loop_iteration(void)
{
  /* Drive async SSD1306 frame transfer (non-blocking). */
  ssd1306_render_async_process();
  /* Apply any deferred protocol operations. */
  protocol_service_deferred_ops();
  update_animations();
  local_buttons_poll();
  debug_led_process();
  /* Handle host-requested standby: display_off -> standby, wake on CS falling edge. */
  handle_standby_request();
  handle_render_request();
  main_loop_delay_and_tick(MAIN_LOOP_DELAY_MS);
}

/* Legacy demo structures are not used in this build. */

int main(void)
{
  system_init();
  /* Pure IRQ-driven SPI RX/TX; no polling fallback */
  while (1) {
    main_loop_iteration();
  }
  return 0;
}

/* SPI is handled via DMA; minimal IRQ for compatibility. */
void __attribute__((interrupt)) SPI1_IRQHandler(void)
{  // Don't use fast variant here
  uint32_t sr = SPI1->STATR;
  if (sr & SPI_STATR_RXNE) {
    ui_spi_rx_irq();
  }
}

/* Minimal EXTI7_0 interrupt handler: clear pending flag for EXTI0 */
void __attribute__((interrupt)) EXTI7_0_IRQHandler(void)
{
  EXTI->INTFR = EXTI_Line0;
}
#if 0
void __attribute__((interrupt)) NMI_Handler(void)
{
  while (1) {
    ;
  }
}
void __attribute__((interrupt)) HardFault_Handler(void)
{
  while (1) {
    ;
  }
}
#endif

#ifndef INVALID_ELEMENT_ID
#define INVALID_ELEMENT_ID 0xFFu
#endif

/* UP button handler - toggles LED */
void protocol_up_button_pressed(void)
{
  debug_led_toggle();
}
