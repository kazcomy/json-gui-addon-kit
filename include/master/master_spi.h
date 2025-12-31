#ifndef MASTER_SPI_H
#define MASTER_SPI_H

#include <stdint.h>

#include "ch32fun.h"

/* Shared protocol basics (duplicated minimal to avoid slave headers) */
#define SPI_FRAME_START 0xAA
#define SPI_BUFFER_SIZE 96
#define SPI_RESP_SYNC0 0xA5u
#define SPI_RESP_SYNC1 0x5Au

/* Derived sizing for master RX/TX buffers (mirror slave framing). */
#define MASTER_MAX_FRAME_BYTES 112  /* [SYNC0][SYNC1][LEN][COBS...] -> max payload window */
#define MASTER_MAX_COBS_BYTES 109   /* encoding workspace for cmd+payload */
#define MASTER_MAX_COBS_LEN_LIMIT 108 /* max LEN value permitted in header */

/* Common command IDs used by master */
#define SPI_CMD_PING 0x00
#define SPI_CMD_JSON 0x01
#define SPI_CMD_JSON_ABORT 0x03
#define SPI_CMD_GET_STATUS 0x20
#define SPI_CMD_SHOW_OVERLAY 0x30
#define SPI_CMD_USER_CONFIG 0x40
#define SPI_CMD_INPUT_EVENT 0x41

/* JSON flags (shared with slave) */
#define JSON_FLAG_HEAD 0x01u
#define JSON_FLAG_COMMIT 0x02u

/** Initialize SPI1 in master mode using remapped pin mapping (matches slave). */
void master_spi_init(void);
/** Assert manual chip-select (active low). */
void master_spi_cs_low(void);
/** Deassert manual chip-select (inactive high). */
void master_spi_cs_high(void);
/** Perform contiguous SPI transfer; keeps CS asserted. */
void master_spi_xfer(const uint8_t* tx, uint8_t tx_len, uint8_t* rx, uint8_t rx_len);
/** Send a framed command and synchronously read response into resp/resp_len. */
int master_send_command(uint8_t        cmd,
                        const uint8_t* payload,
                        uint8_t        plen,
                        uint8_t*       resp,
                        uint8_t*       resp_len);
/** Send a framed command without waiting for a response. */
int master_send_command_no_response(uint8_t cmd, const uint8_t* payload, uint8_t plen);

#ifdef SPI_TEST_PATTERN
/** Emit a simple SPI pattern for logic-analyzer verification. */
void master_spi_test_pattern(void);
#endif

#endif /* MASTER_SPI_H */
