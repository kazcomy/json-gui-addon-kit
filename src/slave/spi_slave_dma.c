#include "spi_slave_dma.h"

#include "ch32v/ch32v00x.h"
#include "ch32v/ch32fun.h"

/* Pin definitions for compatibility */
#define PC0 0
#define PC5 5
#define PC6 6
#define PC7 7

/* Helper macro for GPIO port selection */
#define GpioOf(pin) GPIOC

/* ch32v003fun compatibility macros */
#define RCC_APB2Periph_AFIO     RCC_APB2PCENR_AFIOEN
#define RCC_APB2Periph_GPIOC    RCC_APB2PCENR_IOPCEN
#define RCC_APB2Periph_SPI1     RCC_APB2PCENR_SPI1EN
#define RCC_AHBPeriph_DMA1      RCC_AHBPCENR_DMA1EN
#define SPI_NSS_Soft            SPI_CTLR1_SSM
#define SPI_NSSInternalSoft_Set SPI_CTLR1_SSI
#define DMA_CFGR1_EN            DMA_CFGR_EN
#define DMA_CFGR1_TCIE          DMA_CFGR_TCIE
#define DMA_CFGR1_TEIE          DMA_CFGR_TEIE
#define DMA_CFGR1_MINC          DMA_CFGR_MINC
#define DMA_CFGR1_DIR           DMA_CFGR_DIR
#define DMA_CFGR1_PL_1          (2 << DMA_CFGR_PL_Pos)
#define DMA_CFGR1_PL_0          (1 << DMA_CFGR_PL_Pos)
#define DMA1_IT_TE3             DMA_TEIF3
#define DMA1_IT_TC3             DMA_TCIF3

// static volatile uint8_t dma_tx_transfer_complete = 0;

/**
 * @brief Initialize SPI1 peripheral for slave operation and setup TX DMA path.
 * Responsibility: transport layer only. No protocol logic here.
 */
void spi_slave_transport_init(void)
{
  /* Enable clocks: AFIO, GPIOC, SPI1, DMA1 */
  RCC->APB2PCENR |= RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC | RCC_APB2Periph_SPI1;
  RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;

  /* Enable SPI1 remap to match wiring (PC5/PC6/PC7). PC0 is CS (input with pull-up). */
  AFIO->PCFR1 |= (1u << 0);
  funPinMode(GPIOC, PC0, GPIO_CNF_IN_PUPD);
  GpioOf(PC0)->OUTDR |= (1u << (PC0 & 0xF));
  funPinMode(GPIOC, PC5, GPIO_CNF_IN_FLOATING);
  funPinMode(GPIOC, PC6, GPIO_CNF_IN_FLOATING);
  funPinMode(GPIOC, PC7, GPIO_CFGLR_OUT_50Mhz_AF_PP);

  /* Initialize TX DMA engine for SPI1 */
  spi_slave_dma_init();

  /* Reset SPI1 control registers */
  SPI1->CTLR1 = 0;
  SPI1->CTLR2 = 0;

  /* Set SPI mode 0 (CPOL=0, CPHA=0) */
  SPI1->CTLR1 &= ~(SPI_CTLR1_CPOL | SPI_CTLR1_CPHA);

  /* Slowest baud prescaler (f_PCLK/256) for stability; keeps timing margin */
  SPI1->CTLR1 = (SPI1->CTLR1 & ~(7u << 3)) | (7u << 3);

  /* Use hardware-controlled NSS (do not switch to software NSS) */
  SPI1->CTLR1 &= ~SPI_NSS_Soft;
  SPI1->CTLR1 &= ~SPI_NSSInternalSoft_Set;

  /* Enable SPI and required interrupts/DMAs */
  SPI1->CTLR1 |= SPI_CTLR1_SPE;
  SPI1->CTLR2 |= SPI_CTLR2_RXNEIE;
  NVIC_EnableIRQ(SPI1_IRQn);
  SPI1->CTLR2 |= SPI_CTLR2_TXDMAEN;

  /* Prime TX with a dummy byte if TXE is set to avoid underrun at first frame */
  if (SPI1->STATR & SPI_STATR_TXE) {
    SPI1->DATAR = 0xFF;
  }
}

void spi_slave_dma_init(void)
{
  // Disable TX DMA channel during configuration
  DMA1_Channel3->CFGR &= ~DMA_CFGR1_EN;
}

void spi_slave_tx_dma_start(const uint8_t* buffer, uint16_t length)
{
  spi_slave_tx_dma_wait_complete();

  DMA1_Channel3->CFGR &= ~DMA_CFGR1_EN;

  DMA1_Channel3->PADDR = (uint32_t) & (SPI1->DATAR);
  DMA1_Channel3->MADDR = (uint32_t) buffer;
  DMA1_Channel3->CNTR  = length;

  // spi tx has highest priority
  DMA1_Channel3->CFGR = DMA_CFGR1_TCIE | DMA_CFGR1_TEIE | DMA_CFGR1_MINC | DMA_CFGR1_DIR |
                        DMA_CFGR1_PL_1 | DMA_CFGR1_PL_0;

  DMA1_Channel3->CFGR |= DMA_CFGR1_EN;
}

void spi_slave_tx_dma_stop(void)
{
  DMA1_Channel3->CFGR &= ~DMA_CFGR1_EN;
}

int spi_slave_tx_dma_is_complete(void)
{
  // Consider complete when remaining count reaches zero
  return (DMA1_Channel3->CNTR == 0) ? 1 : 0;
}

void spi_slave_tx_dma_wait_complete(void)
{
  // Wait until DMA CNTR reaches zero; IRQ alone may be early
  for (;;) {
    uint32_t cntr = DMA1_Channel3->CNTR;
    uint32_t isr  = DMA1->INTFR;

    // Error: transfer error flag on channel 3
    if (isr & DMA1_IT_TE3) {
      // Clear TE flag then hang for debug visibility
      DMA1->INTFCR = DMA1_IT_TE3;
      while (1) {}
    }
    if (cntr == 0) {
      break;
    }
  }
}

/* DMA1 Channel 3 (SPI TX) Interrupt Handler (optional; NVIC not enabled) */
void __attribute__((interrupt)) DMA1_Channel3_IRQHandler(void)
{
  uint32_t isr = DMA1->INTFR;

  if (isr & DMA1_IT_TC3) {
    DMA1->INTFCR = DMA1_IT_TC3;
    // dma_tx_transfer_complete = 1;

    /* Call UI protocol completion handler */
    // extern void spi_tx_dma_complete_callback(void);
    // spi_tx_dma_complete_callback();  // TEMP: Disable callback
  }

  if (isr & DMA1_IT_TE3) {
    DMA1->INTFCR = DMA1_IT_TE3;
    // dma_tx_transfer_complete = 1; // Mark as complete even on error

    /* Call UI protocol completion handler */
    // extern void spi_tx_dma_complete_callback(void);
    // spi_tx_dma_complete_callback();  // TEMP: Disable callback
  }
}
