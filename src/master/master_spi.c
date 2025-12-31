#include "master_spi.h"

#include <stddef.h>
#include <string.h>

#include "cobs.h"
#include "status_codes.h"
#include <sys/unistd.h>

/* Protocol response codes (mirror slave definitions). */
#define RC_OK 0x00u
#define RC_BAD_LEN 0x01u
#define RC_BAD_STATE 0x02u
#define RC_UNKNOWN_ID 0x03u
#define RC_RANGE 0x04u
#define RC_INTERNAL 0x05u
#define RC_PARSE_FAIL 0x0Bu
#define RC_NO_SPACE 0x0Cu
#define RC_STREAM_ERR 0x0Du

/** Map protocol RC codes to local result codes. */
static int master_map_rc_to_result(uint8_t rc)
{
  switch (rc) {
    case RC_OK:
      return RES_OK;
    case RC_BAD_LEN:
      return RES_BAD_LEN;
    case RC_BAD_STATE:
      return RES_BAD_STATE;
    case RC_UNKNOWN_ID:
      return RES_UNKNOWN_ID;
    case RC_RANGE:
      return RES_RANGE;
    case RC_INTERNAL:
      return RES_INTERNAL;
    case RC_PARSE_FAIL:
      return RES_PARSE_FAIL;
    case RC_NO_SPACE:
      return RES_NO_SPACE;
    case RC_STREAM_ERR:
      return RES_INTERNAL;
    default:
      return RES_INTERNAL;
  }
}
/** Busy-wait delay in microseconds for SPI timing. */
static void delay_us(uint32_t us)
{
  /* CH32V003 @ 48MHz: 48 cycles per us
   * Loop unrolling to reduce for-loop overhead
   * Each unrolled block: 3x16 NOPs + minimal loop overhead
   */
  const uint32_t iterations_per_us = 16;
  for (uint32_t i = 0; i < us; i++) {
    for (volatile uint32_t j = 0; j < iterations_per_us; j++) {
      /* Unrolled 3x16 NOPs per iteration for efficiency */
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
    }
  }
}
/** Busy-wait delay in milliseconds for SPI timing. */
static void delay_ms(uint32_t ms)
{
  /* CH32V003 @ 48MHz: 48,000 cycles per ms
   * Loop unrolling to reduce for-loop overhead
   * Each unrolled block: 3x16 NOPs + minimal loop overhead
   */
  const uint32_t iterations_per_ms = 1000;

  for (uint32_t i = 0; i < ms; i++) {
    for (volatile uint32_t j = 0; j < iterations_per_ms; j++) {
      /* Unrolled 3x16 NOPs per iteration for efficiency */
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
      __asm__ volatile("nop; nop; nop; nop;");
    }
  }
}
/**
 * @brief Initialize SPI1 peripheral for master role (8-bit, Mode 0) using the remapped pins.
 *
 * Pins (SPI1_RM=1 remap to match slave wiring):
 * - PC0 : Manual chip-select (GPIO push-pull, active low)
 * - PC5 : SCK (AF push-pull)
 * - PC6 : MOSI (AF push-pull)
 * - PC7 : MISO (input floating)
 *
 * Clock prescaler is configured to the lowest speed (/256) to reduce risk of overruns
 * on the resource constrained slave until higher timings are validated. NSS is kept
 * internal/high so the core always remains master. Chip select is managed explicitly
 * by caller around framed transfers.
 */
void master_spi_init(void)
{
  // RCC->CFGR0 |= RCC_HPRE_DIV256;
  /* Enable clocks */
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO | RCC_APB2Periph_SPI1;

  /* Set Remap register: SPI1_RM = 1 (match slave configuration) */
  AFIO->PCFR1 |= (1u << 0);

  /* Configure pins */
  funPinMode(PC0, GPIO_CFGLR_OUT_50Mhz_PP);     // CS as GPIO
  funDigitalWrite(PC0, 1);                      // CS high (inactive)
  funPinMode(PC5, GPIO_CFGLR_OUT_50Mhz_AF_PP);  // SCK
  funPinMode(PC6, GPIO_CFGLR_OUT_50Mhz_AF_PP);  // MOSI
  funPinMode(PC7, GPIO_CNF_IN_FLOATING);        // MISO


  /* Configure SPI1 as master */
  SPI1->CTLR1 |= SPI_Mode_Master | SPI_DataSize_8b | SPI_Direction_2Lines_FullDuplex;
  /* Force internal NSS high to keep master regardless of external NSS; add conservative prescaler
   */
  SPI1->CTLR1 |= SPI_NSS_Soft | SPI_NSSInternalSoft_Set;
  /* Slow prescaler to reduce RX overruns on slave: BR=111 (/256) */
  SPI1->CTLR1 = (SPI1->CTLR1 & ~(7u << 3)) | (7u << 3);
  /* Set SPI Mode 0: CPOL=0, CPHA=0 */
  SPI1->CTLR1 &= ~(SPI_CTLR1_CPOL | SPI_CTLR1_CPHA);
  /* Ensure hardware NSS output is disabled (we use software CS) */
  SPI1->CTLR2 &= ~CTLR2_SSOE_Set;

  /* Enable SPI */
  SPI1->CTLR1 |= CTLR1_SPE_Set;
}

void master_spi_cs_low(void)
{
  funDigitalWrite(PC0, 0);
}

void master_spi_cs_high(void)
{
  funDigitalWrite(PC0, 1);
}

/* Simple byte transfer with timeout */
/**
 * @brief Transfer one byte full-duplex on SPI and return received byte.
 * @param b Byte to transmit (0xFF used as filler when only reading).
 * @return Received byte.
 */
static uint8_t xfer_byte(uint8_t b)
{
  /* Wait for TX buffer empty */
  while (!(SPI1->STATR & SPI_STATR_TXE)) {}
  /* Send byte */
  SPI1->DATAR = b;

  while (!(SPI1->STATR & SPI_STATR_RXNE)) {}

  /* Return received byte */
  return (uint8_t) SPI1->DATAR;
}

/**
 * @brief Perform a contiguous SPI transfer keeping CS asserted.
 * @param tx Pointer to transmit buffer (can be NULL if only reading).
 * @param tx_len Number of bytes to transmit.
 * @param rx Pointer to receive buffer (can be NULL if only writing).
 * @param rx_len Max bytes to store in rx buffer.
 */
void master_spi_xfer(const uint8_t* tx, uint8_t tx_len, uint8_t* rx, uint8_t rx_len)
{
  uint8_t n = (tx_len > rx_len) ? tx_len : rx_len;
  /* Keep CS asserted across a contiguous transfer */
  for (uint8_t i = 0; i < n; i++) {
    uint8_t tb = 0xFF;
    if (tx && i < tx_len)
      tb = tx[i];

    uint8_t rb = xfer_byte(tb);

    if (rx && i < rx_len)
      rx[i] = rb;
  }

  /* Wait for SPI to finish */
  while ((SPI1->STATR & SPI_STATR_BSY)) {}

  /* CS handled by caller */
}

/**
 * @brief Send a single framed command and synchronously read its response.
 *
 * Frame format (master->slave): [SYNC0][SYNC1][LEN][COBS(cmd||payload)].
 * Response frame mirrors the same header and carries COBS(encoded response-payload).
 *
 * @param cmd Command opcode.
 * @param payload Pointer to payload bytes (can be NULL if plen==0).
 * @param plen Payload length.
 * @param resp Buffer to receive raw decoded response (may be NULL to ignore).
 * @param resp_len In: capacity of resp buffer. Out: number of bytes written.
 * @return >=0 decoded response length on success, negative on transport/protocol error.
 */
int master_send_command(uint8_t        cmd,
                        const uint8_t* payload,
                        uint8_t        plen,
                        uint8_t*       resp,
                        uint8_t*       resp_len)
{
  if (plen > (uint8_t) (SPI_BUFFER_SIZE - 4)) {
    plen = (uint8_t) (SPI_BUFFER_SIZE - 4);
  }

  /* Build raw frame without 0xAA prefix (SYNC bytes will handle framing) */
  uint8_t raw[SPI_BUFFER_SIZE];
  if (plen > (uint8_t) (sizeof(raw) - 1)) {
    return RES_BAD_LEN;
  }
  raw[0] = cmd;
  memcpy(&raw[1], payload, plen);

  /* COBS-encode the payload */
  uint8_t enc_buf[MASTER_MAX_COBS_BYTES]; /* Reserve space for COBS encoding */
  size_t  enc = cobs_encode(raw, (size_t) (1 + plen), enc_buf, sizeof(enc_buf));
  if (enc == 0 || enc >= sizeof(enc_buf)) {
    return RES_INTERNAL;
  }

  /* Build complete frame: [SYNC0][SYNC1][LEN][COBS_ENC][0x00] */
  uint8_t txbuf[MASTER_MAX_FRAME_BYTES];
  txbuf[0] = SPI_RESP_SYNC0; /* Use same sync bytes as slave response */
  txbuf[1] = SPI_RESP_SYNC1;
  txbuf[2] = (uint8_t) (enc); /* Length includes COBS data + delimiter */
  memcpy(&txbuf[3], enc_buf, enc);
  uint8_t total_len = (uint8_t) (3 + enc);

  /* Single CS cycle: send command, wait for slave processing, then read response */
  master_spi_cs_low();

  /* Send command frame */
  master_spi_xfer(txbuf, total_len, NULL, 0);

  /* Allow slave time to process command and prepare response. Some commands (e.g. USER_CONFIG)
   * clear the entire display and take longer. Use a slightly larger pre-poll delay for it. */
  if (cmd == SPI_CMD_USER_CONFIG) {
    delay_ms(10);
  } else {
    delay_ms(2);
  }

  /* Read response: poll for sync bytes, then read header and payload */
  uint8_t  hdr[3]     = {0};
  uint8_t  first_byte = 0;
  /* Poll for SYNC0 with bounded retries. Longer window for USER_CONFIG to cover display clear. */
  uint16_t max_polls = (cmd == SPI_CMD_USER_CONFIG) ? 3000u : 400u; /* ~120ms / ~16ms @40us step */
  for (uint16_t tries = 0; tries < max_polls; ++tries) {
    first_byte = xfer_byte(0xFF);
    if (first_byte == SPI_RESP_SYNC0) {
      hdr[0] = first_byte;
      delay_us(100);
      hdr[1] = xfer_byte(0xFF);
      if (hdr[1] != SPI_RESP_SYNC1) {
        hdr[0] = 0;
        continue; /* keep polling */
      }
      delay_us(100);
      hdr[2] = xfer_byte(0xFF);
      break;
    }
    delay_us(40);
  }

  if (hdr[0] != SPI_RESP_SYNC0) {
    master_spi_cs_high();
    return RES_INTERNAL;
  }
  /* SYNC1 validation already done in the polling loop */
  uint8_t enc_len = hdr[2];
  if (enc_len == 0 || enc_len > MASTER_MAX_COBS_LEN_LIMIT) {
    master_spi_cs_high();
    return RES_BAD_LEN;
  }
  uint8_t rxb[MASTER_MAX_FRAME_BYTES];
  uint8_t rxl = 0;
  for (uint8_t i = 0; i < enc_len; ++i) {
    if (rxl < sizeof(rxb)) {
      rxb[rxl] = xfer_byte(0xFF);
      rxl++;
    } else {
      master_spi_cs_high();
      return RES_NO_SPACE;
    }
  }
  master_spi_cs_high();

  /* Decode robustly: COBS decode and validate frame (without 0xAA prefix).
   * Slave's LEN is exactly the COBS-encoded payload length (no delimiter byte).
   */
  uint8_t outb[SPI_BUFFER_SIZE];
  size_t  out_len = cobs_decode(rxb, rxl, outb, sizeof(outb));
  if (out_len < 1u) {
    return RES_INTERNAL;
  }

  uint8_t want = (uint8_t) out_len;
  int     ret  = (int) out_len;
  if (resp && resp_len) {
    uint8_t cap = *resp_len;
    if (cap > want)
      cap = want;
    for (uint8_t i = 0; i < cap; i++) {
      resp[i] = outb[i];
    }
    *resp_len = cap;
  }
  (void) want;
  int rc = master_map_rc_to_result(outb[0]);
  if (rc != RES_OK) {
    return rc;
  }
  return ret;
}

/**
 * @brief Send a framed command without waiting for a response.
 *
 * Frame format: [SYNC0][SYNC1][LEN][COBS(cmd||payload)].
 *
 * @param cmd Command opcode.
 * @param payload Pointer to payload bytes (can be NULL if plen==0).
 * @param plen Payload length.
 * @return 0 on success, negative on encode/length error.
 */
int master_send_command_no_response(uint8_t cmd, const uint8_t* payload, uint8_t plen)
{
  if (plen > (uint8_t) (SPI_BUFFER_SIZE - 4)) {
    plen = (uint8_t) (SPI_BUFFER_SIZE - 4);
  }

  uint8_t raw[SPI_BUFFER_SIZE];
  if (plen > (uint8_t) (sizeof(raw) - 1)) {
    return RES_BAD_LEN;
  }
  raw[0] = cmd;
  if (plen && payload) {
    memcpy(&raw[1], payload, plen);
  }

  uint8_t enc_buf[MASTER_MAX_COBS_BYTES];
  size_t  enc = cobs_encode(raw, (size_t) (1 + plen), enc_buf, sizeof(enc_buf));
  if (enc == 0 || enc >= sizeof(enc_buf)) {
    return RES_INTERNAL;
  }

  uint8_t txbuf[MASTER_MAX_FRAME_BYTES];
  txbuf[0] = SPI_RESP_SYNC0;
  txbuf[1] = SPI_RESP_SYNC1;
  txbuf[2] = (uint8_t) enc;
  memcpy(&txbuf[3], enc_buf, enc);
  uint8_t total_len = (uint8_t) (3 + enc);

  master_spi_cs_low();
  master_spi_xfer(txbuf, total_len, NULL, 0);
  master_spi_cs_high();
  return 0;
}

/* printf is provided by ch32v003fun when FUNCONF_USE_DEBUGPRINTF or
 * FUNCONF_USE_UARTPRINTF is enabled via build flags.
 */

#ifdef SPI_TEST_PATTERN
/* Emit a series of known patterns for logic-analyzer verification.
 * Each segment is sent under a single CS low with small inter-segment delays. */
void master_spi_test_pattern(void)
{
  static const uint8_t seq1[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};

  master_spi_cs_low();
  master_spi_xfer(seq1, sizeof(seq1), NULL, 0);
  master_spi_cs_high();
  Delay_Ms(5);
}
#endif
