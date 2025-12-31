/**
 * @file i2c_custom.c
 * @brief Custom I2C library implementation with DMA support
 *
 * Compatible with lib_i2c interface but optimized for this project.
 * Uses direct register access for better performance and smaller code size.
 * Includes DMA support for large data transfers.
 */

#include "i2c_custom.h"

#include <stdio.h>

// I2C timeout value
#define I2C_TIMEOUT 10000

// DMA Channel assignments (from Reference Manual)
#define I2C1_TX_DMA_CHANNEL 6
#define I2C1_RX_DMA_CHANNEL 7

// DMA Register offsets for Channel 6 (I2C1_TX)
#define DMA_CFGR6_OFFSET (0x08 + (I2C1_TX_DMA_CHANNEL - 1) * 20)
#define DMA_CNTR6_OFFSET (0x0C + (I2C1_TX_DMA_CHANNEL - 1) * 20)
#define DMA_PADDR6_OFFSET (0x10 + (I2C1_TX_DMA_CHANNEL - 1) * 20)
#define DMA_MADDR6_OFFSET (0x14 + (I2C1_TX_DMA_CHANNEL - 1) * 20)

// DMA Configuration bits (from Reference Manual Table)
#define DMA_CFGR_EN (1 << 0)           // Channel enable
#define DMA_CFGR_TCIE (1 << 1)         // Transfer complete interrupt enable
#define DMA_CFGR_HTIE (1 << 2)         // Half transfer interrupt enable
#define DMA_CFGR_TEIE (1 << 3)         // Transfer error interrupt enable
#define DMA_CFGR_DIR (1 << 4)          // Data transfer direction (1=read from memory)
#define DMA_CFGR_CIRC (1 << 5)         // Circular mode enable
#define DMA_CFGR_PINC (1 << 6)         // Peripheral increment mode
#define DMA_CFGR_MINC (1 << 7)         // Memory increment mode
#define DMA_CFGR_PSIZE_8BIT (0 << 8)   // Peripheral size: 8 bits
#define DMA_CFGR_MSIZE_8BIT (0 << 10)  // Memory size: 8 bits
#define DMA_CFGR_PL_HIGH (2 << 12)     // Priority level: High

// DMA Interrupt flags for Channel 6
#define DMA_ISR_TCIF6 (1 << (4 * (I2C1_TX_DMA_CHANNEL - 1) + 1))  // Transfer complete
#define DMA_ISR_HTIF6 (1 << (4 * (I2C1_TX_DMA_CHANNEL - 1) + 2))  // Half transfer
#define DMA_ISR_TEIF6 (1 << (4 * (I2C1_TX_DMA_CHANNEL - 1) + 3))  // Transfer error

// Rely solely on hardware flags/CNTR for DMA status tracking

// Forward declarations
static i2c_err_t i2c_wait_flag(uint32_t flag, uint8_t state, uint32_t timeout);
static i2c_err_t i2c_wait_not_busy(void);
static i2c_err_t i2c_start(void);
static i2c_err_t i2c_stop(void);
static i2c_err_t i2c_send_address(uint8_t addr);
static void      i2c_dma_init(void);
/* Single busy poll API; no separate i2c_dma_wait_complete or public idle-wait function */

// Static helper functions implementation
/** Wait for STAR1 flag to match expected state (timeout_unused is reserved, unused). */
static i2c_err_t i2c_wait_flag(uint32_t flag, uint8_t state, uint32_t timeout_unused)
{
  (void) timeout_unused;
  while (((I2C1->STAR1 & flag) ? 1U : 0U) != state) {
    // If NACK occurs during wait, clear and hang for debugging visibility
    if (I2C1->STAR1 & I2C_STAR1_AF) {
      I2C1->STAR1 &= ~I2C_STAR1_AF;
      while (1) {}
    }
  }
  return I2C_OK;
}

/** Wait until the I2C BUSY flag clears. */
static i2c_err_t i2c_wait_not_busy(void)
{
  while (I2C1->STAR2 & I2C_STAR2_BUSY) {}
  return I2C_OK;
}

/** Issue START and wait for SB flag. */
static i2c_err_t i2c_start(void)
{
  i2c_err_t result = i2c_wait_not_busy();
  if (result != I2C_OK) {
    return result;
  }

  I2C1->CTLR1 |= I2C_CTLR1_START;
  return i2c_wait_flag(I2C_STAR1_SB, 1, I2C_TIMEOUT);
}

/** Issue STOP condition. */
static i2c_err_t i2c_stop(void)
{
  I2C1->CTLR1 |= I2C_CTLR1_STOP;
  return I2C_OK;
}

/** Send 7-bit address in write mode and wait for transmitter state. */
static i2c_err_t i2c_send_address(uint8_t addr)
{
  I2C1->DATAR = addr;

  // Wait for address sent and mode selected (infinite wait for debug)
  for (;;) {
    uint32_t sr1 = I2C1->STAR1;
    uint32_t sr2 = I2C1->STAR2;

    // Check for successful transmitter mode selection
    if ((sr1 & I2C_STAR1_ADDR) && (sr2 & I2C_STAR2_MSL) && (sr2 & I2C_STAR2_TRA)) {
      // Address acknowledged and in transmitter mode
      // IMPORTANT: Clear ADDR by reading SR1 then SR2 (hardware requirement)
      (void) I2C1->STAR1;
      (void) I2C1->STAR2;
      return I2C_OK;
    }

    // Check for NACK
    if (sr1 & I2C_STAR1_AF) {
      I2C1->STAR1 &= ~I2C_STAR1_AF;  // Clear then hang for debug
      while (1) {}
    }
  }
}

// Note: No per-byte send helper; write paths use direct DATAR writes.

// Static helper functions
/** Initialize DMA controller for I2C TX use. */
static void i2c_dma_init(void)
{
  // Enable DMA clock
  RCC->AHBPCENR |= RCC_DMA1EN;

  // Enable DMA1 interrupts. Prefer Channel6, but also enable Channel5 for shared-vector SDKs.
  NVIC_EnableIRQ(DMA1_Channel6_IRQn);

  // Clear any pending interrupts for Channel 6
  DMA1->INTFCR = DMA_ISR_TCIF6 | DMA_ISR_HTIF6 | DMA_ISR_TEIF6;

  // Reset DMA Channel 6 configuration
  *(volatile uint32_t*) (DMA1_BASE + DMA_CFGR6_OFFSET) = 0;
}

/* Non-blocking busy check (1=busy,0=idle). Pure register read: CNTR>0 or EN bit set. */
int i2c_tx_dma_busy(void)
{
  if (DMA1_Channel6->CFGR & DMA_CFGR_EN) {
    return 1; /* channel enabled */
  }
  if (DMA1_Channel6->CNTR != 0) {
    return 1; /* residual count */
  }
  return 0;
}
/* Note that buf is not overwritten until dma complete and dma shall not be started before previous
 * dma complete */
i2c_err_t i2c_write_raw_dma(const i2c_device_t* dev, const uint8_t* buf, const size_t len)
{
  if (!dev || !buf || len == 0) {
    return I2C_ERR_BERR;
  }
  i2c_err_t result;

  result = i2c_wait_not_busy();
  if (result != I2C_OK) {
    return result;
  }

  // Start I2C transaction
  result = i2c_start();
  if (result != I2C_OK) {
    return result;
  }

  // Send device address (write)
  uint8_t write_addr = (dev->addr << 1) & 0xFE;
  result             = i2c_send_address(write_addr);
  if (result != I2C_OK) {
    i2c_stop();
    return result;
  }

  // Enable I2C DMA requests and DMA channel
  // Clear hardware flags just before enabling to avoid races
  DMA1->INTFCR = DMA_ISR_TCIF6 | DMA_ISR_HTIF6 | DMA_ISR_TEIF6;

  // I2C1->CTLR2 |= I2C_CTLR2_DMAEN;
  DMA1_Channel6->CFGR  = 0;                        // Disable channel first
  DMA1_Channel6->PADDR = (uint32_t) &I2C1->DATAR;  // Set peripheral address (I2C1_DATAR)
  DMA1_Channel6->MADDR = (uint32_t) buf;           // Set memory address (source buffer)
  DMA1_Channel6->CNTR  = len;                      // Set transfer count

  // Configure DMA: Memory to Peripheral, 8-bit, Memory increment, High priority
  uint32_t dma_config = DMA_CFGR_DIR | DMA_CFGR_MINC | DMA_CFGR_PSIZE_8BIT | DMA_CFGR_MSIZE_8BIT |
                        DMA_CFGR_PL_HIGH | DMA_CFGR_TCIE | DMA_CFGR_TEIE;

  DMA1_Channel6->CFGR = dma_config;
  /* channel becomes busy once enabled */
  DMA1_Channel6->CFGR |= DMA_CFGR_EN;

  return I2C_OK;
}
// Public API implementation
i2c_err_t i2c_init(i2c_device_t* dev)
{
  if (!dev) {
    return I2C_ERR_BERR;
  }

  // Toggle the I2C Reset bit to init Registers
  RCC->APB1PRSTR |= RCC_APB1Periph_I2C1;
  RCC->APB1PRSTR &= ~RCC_APB1Periph_I2C1;

  // Enable I2C1 and GPIO clocks
  RCC->APB1PCENR |= RCC_APB1Periph_I2C1;                         // I2C1EN
  RCC->APB2PCENR |= RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO;  // IOPC enable for PC1,PC2

  // Configure PC1(SDA) and PC2(SCL) as open-drain alternate function
  // PC1 (SDA) and PC2 (SCL) using 10MHz speed like original library
  GPIOC->CFGLR &= ~(0x0F << (4 * 1));  // Clear PC1 config
  GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * 1);
  GPIOC->CFGLR &= ~(0x0F << (4 * 2));  // Clear PC2 config
  GPIOC->CFGLR |= (GPIO_Speed_10MHz | GPIO_CNF_OUT_OD_AF) << (4 * 2);

  // Set the Prerate frequency (like original library)
  uint16_t i2c_conf = I2C1->CTLR2 & ~I2C_CTLR2_FREQ;
  i2c_conf |= (FUNCONF_SYSTEM_CORE_CLOCK / 1000000) & I2C_CTLR2_FREQ;  // 1MHz prerate
  I2C1->CTLR2 = i2c_conf;

  // Set I2C Clock (matching original library logic)
  if (dev->clkr <= 100000) {
    i2c_conf = (FUNCONF_SYSTEM_CORE_CLOCK / (2 * dev->clkr)) & I2C_CKCFGR_CCR;
  } else {
    // Fast mode. Default to 33% Duty Cycle
    i2c_conf = (FUNCONF_SYSTEM_CORE_CLOCK / (3 * dev->clkr)) & I2C_CKCFGR_CCR;
    i2c_conf |= I2C_CKCFGR_FS;
  }
  I2C1->CKCFGR = i2c_conf;

  // Enable I2C1 peripheral (like original library)
  I2C1->CTLR1 |= I2C_CTLR1_PE;

  I2C1->CTLR2 |= I2C_CTLR2_DMAEN;
  i2c_dma_init();

  // Wait for I2C peripheral to stabilize
  Delay_Ms(10);
  return I2C_OK;
}


/* DMA1 Channel 6 (I2C TX) Interrupt Handler */
void __attribute__((interrupt)) DMA1_Channel6_IRQHandler(void)
{
  uint32_t dma_isr = DMA1->INTFR;
  // Clear all related flags for Ch6 to avoid residual GIF/HTIF bits
  uint8_t stop = 0;
  if (dma_isr & DMA_ISR_TCIF6) {
    DMA1->INTFCR = DMA_ISR_TCIF6;
    stop         = 1;
  }
  if (dma_isr & DMA_ISR_TEIF6) {
    DMA1->INTFCR = DMA_ISR_TEIF6;
    stop         = 1;
  }
  if (dma_isr & DMA_ISR_HTIF6) {
    DMA1->INTFCR = DMA_ISR_HTIF6;
  }
  if (stop) {
    /* Disable DMA requests first */
    DMA1_Channel6->CFGR &= ~DMA_CFGR_EN;
    /* Ensure last byte fully shifted: wait TXE then BTF (no timeout for debug).
        BTF guarantees both DR empty & shift register empty. */
    while (!(I2C1->STAR1 & I2C_STAR1_TXE)) {
      /* wait */
    }
    while (DMA1_Channel6->CNTR != 0) {
      /* wait */
    }
    i2c_stop();
    /* idle state observed by busy() */
  }
}
