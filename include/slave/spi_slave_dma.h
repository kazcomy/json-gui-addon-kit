#ifndef SPI_SLAVE_DMA_H
#define SPI_SLAVE_DMA_H

#include <stdint.h>

#include "ch32fun.h"

// SPI1 DMA Channels according to CH32V003 Reference Manual
#define SPI1_TX_DMA_CHANNEL 3

/** Initialize SPI1 peripheral as slave and prepare DMA/NVIC. */
void spi_slave_transport_init(void);

// Initialize SPI Slave DMA (TX only - see ADR-001 for RX DMA decision)
void spi_slave_dma_init(void);

// TX DMA functions
// Start DMA transmission from buffer
void spi_slave_tx_dma_start(const uint8_t* buffer, uint16_t length);

// Stop ongoing DMA transmission
void spi_slave_tx_dma_stop(void);

// Check if TX DMA transfer is complete
int spi_slave_tx_dma_is_complete(void);

// Busy-wait until TX DMA transfer completes (CNTR reaches 0). Hangs on TE error for visibility.
void spi_slave_tx_dma_wait_complete(void);

#endif /* SPI_SLAVE_DMA_H */
