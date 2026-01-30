/**
 * @file ch32v00x.h
 * @brief CH32V003 register definitions and peripheral structures
 *
 * Based on WCH CH32V003 Reference Manual
 * CH32V003 register definitions and peripheral structures
 */

#ifndef __CH32V00X_H
#define __CH32V00X_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ================================================================================ */
/* ================                  Core Definitions                  ============ */
/* ================================================================================ */

/* Interrupt Number Definition */
typedef enum IRQn {
    NonMaskableInt_IRQn = 2,
    EXTILine7_0_IRQn = 3,
    AWU_IRQn = 4,
    DMA1_Channel1_IRQn = 5,
    DMA1_Channel2_IRQn = 6,
    DMA1_Channel3_IRQn = 7,
    DMA1_Channel4_IRQn = 8,
    DMA1_Channel5_IRQn = 9,
    DMA1_Channel6_IRQn = 10,
    DMA1_Channel7_IRQn = 11,
    ADC1_IRQn = 12,
    I2C1_EV_IRQn = 13,
    I2C1_ER_IRQn = 14,
    USART1_IRQn = 15,
    SPI1_IRQn = 16,
    TIM1_BRK_IRQn = 17,
    TIM1_UP_IRQn = 18,
    TIM1_TRG_COM_IRQn = 19,
    TIM1_CC_IRQn = 20,
    TIM2_IRQn = 21,
} IRQn_Type;

/* ================================================================================ */
/* ================             Peripheral memory map              ================ */
/* ================================================================================ */

#define FLASH_BASE            ((uint32_t)0x08000000)
#define SRAM_BASE             ((uint32_t)0x20000000)
#define PERIPH_BASE           ((uint32_t)0x40000000)

#define APB1PERIPH_BASE       (PERIPH_BASE)
#define APB2PERIPH_BASE       (PERIPH_BASE + 0x00010000)
#define AHB1PERIPH_BASE       (PERIPH_BASE + 0x00020000)

/* APB1 Peripherals */
#define TIM2_BASE             (APB1PERIPH_BASE + 0x0000)
#define WWDG_BASE             (APB1PERIPH_BASE + 0x2C00)
#define IWDG_BASE             (APB1PERIPH_BASE + 0x3000)
#define I2C1_BASE             (APB1PERIPH_BASE + 0x5400)
#define PWR_BASE              (APB1PERIPH_BASE + 0x7000)

/* APB2 Peripherals */
#define AFIO_BASE             (APB2PERIPH_BASE + 0x0000)
#define EXTI_BASE             (APB2PERIPH_BASE + 0x0400)
#define GPIOA_BASE            (APB2PERIPH_BASE + 0x0800)
#define GPIOC_BASE            (APB2PERIPH_BASE + 0x1000)
#define GPIOD_BASE            (APB2PERIPH_BASE + 0x1400)
#define ADC1_BASE             (APB2PERIPH_BASE + 0x2400)
#define TIM1_BASE             (APB2PERIPH_BASE + 0x2C00)
#define SPI1_BASE             (APB2PERIPH_BASE + 0x3000)
#define USART1_BASE           (APB2PERIPH_BASE + 0x3800)

/* AHB1 Peripherals */
#define DMA1_BASE             (AHB1PERIPH_BASE + 0x0000)
#define DMA1_Channel1_BASE    (DMA1_BASE + 0x0008)
#define DMA1_Channel2_BASE    (DMA1_BASE + 0x001C)
#define DMA1_Channel3_BASE    (DMA1_BASE + 0x0030)
#define DMA1_Channel4_BASE    (DMA1_BASE + 0x0044)
#define DMA1_Channel5_BASE    (DMA1_BASE + 0x0058)
#define DMA1_Channel6_BASE    (DMA1_BASE + 0x006C)
#define DMA1_Channel7_BASE    (DMA1_BASE + 0x0080)
#define RCC_BASE              (AHB1PERIPH_BASE + 0x1000)
#define FLASH_R_BASE          (AHB1PERIPH_BASE + 0x2000)

/* System Control */
#define PFIC_BASE             ((uint32_t)0xE000E000)
#define NVIC_BASE             (PFIC_BASE + 0x0100)
#define SYSTICK_BASE          (PFIC_BASE + 0x0010)
#define EXTI_BASE             (APB2PERIPH_BASE + 0x0400)
#define PWR_BASE              (APB1PERIPH_BASE + 0x7000)

/* ================================================================================ */
/* ================             Peripheral Structures              ================ */
/* ================================================================================ */

/* GPIO Register Structure */
typedef struct {
    volatile uint32_t CFGLR;    /* GPIO configuration register low,     offset: 0x00 */
    volatile uint32_t CFGHR;    /* GPIO configuration register high,    offset: 0x04 */
    volatile uint32_t INDR;     /* GPIO input data register,            offset: 0x08 */
    volatile uint32_t OUTDR;    /* GPIO output data register,           offset: 0x0C */
    volatile uint32_t BSHR;     /* GPIO bit set/reset register,         offset: 0x10 */
    volatile uint32_t BCR;      /* GPIO bit clear register,             offset: 0x14 */
    volatile uint32_t LCKR;     /* GPIO lock register,                  offset: 0x18 */
} GPIO_TypeDef;

/* RCC Register Structure */
typedef struct {
    volatile uint32_t CTLR;     /* RCC clock control register,          offset: 0x00 */
    volatile uint32_t CFGR0;    /* RCC clock configuration register,    offset: 0x04 */
    volatile uint32_t INTR;     /* RCC interrupt register,              offset: 0x08 */
    volatile uint32_t APB2PRSTR;/* RCC APB2 peripheral reset register,  offset: 0x0C */
    volatile uint32_t APB1PRSTR;/* RCC APB1 peripheral reset register,  offset: 0x10 */
    volatile uint32_t AHBPCENR; /* RCC AHB peripheral clock enable,     offset: 0x14 */
    volatile uint32_t APB2PCENR;/* RCC APB2 peripheral clock enable,    offset: 0x18 */
    volatile uint32_t APB1PCENR;/* RCC APB1 peripheral clock enable,    offset: 0x1C */
    volatile uint32_t RSTSCKR;  /* RCC reset and clock source register, offset: 0x24 */
} RCC_TypeDef;

/* Alias for compatibility */
#define RCC_CR      CTLR

/* SPI Register Structure */
typedef struct {
    volatile uint32_t CTLR1;    /* SPI control register 1,              offset: 0x00 */
    volatile uint32_t CTLR2;    /* SPI control register 2,              offset: 0x04 */
    volatile uint32_t STATR;    /* SPI status register,                 offset: 0x08 */
    volatile uint32_t DATAR;    /* SPI data register,                   offset: 0x0C */
    volatile uint32_t CRCR;     /* SPI CRC polynomial register,         offset: 0x10 */
    volatile uint32_t RCRCR;    /* SPI RX CRC register,                 offset: 0x14 */
    volatile uint32_t TCRCR;    /* SPI TX CRC register,                 offset: 0x18 */
    volatile uint32_t HSCR;     /* SPI high speed control register,     offset: 0x1C */
} SPI_TypeDef;

/* DMA Channel Register Structure */
typedef struct {
    volatile uint32_t CFGR;     /* DMA channel configuration register,  offset: 0x00 */
    volatile uint32_t CNTR;     /* DMA channel transfer counter,        offset: 0x04 */
    volatile uint32_t PADDR;    /* DMA channel peripheral address,      offset: 0x08 */
    volatile uint32_t MADDR;    /* DMA channel memory address,          offset: 0x0C */
} DMA_Channel_TypeDef;

/* DMA Controller Register Structure */
typedef struct {
    volatile uint32_t INTFR;    /* DMA interrupt flag register,         offset: 0x00 */
    volatile uint32_t INTFCR;   /* DMA interrupt flag clear register,   offset: 0x04 */
} DMA_TypeDef;

/* AFIO Register Structure */
typedef struct {
    volatile uint32_t ECR;      /* Event control register,              offset: 0x00 */
    volatile uint32_t PCFR1;    /* AF remap and debug I/O config reg 1, offset: 0x04 */
    volatile uint32_t EXTICR[4];/* External interrupt config registers, offset: 0x08 */
    uint32_t RESERVED0;
    volatile uint32_t PCFR2;    /* AF remap and debug I/O config reg 2, offset: 0x1C */
} AFIO_TypeDef;

/* I2C Register Structure */
typedef struct {
    volatile uint32_t CTLR1;    /* I2C control register 1,              offset: 0x00 */
    volatile uint32_t CTLR2;    /* I2C control register 2,              offset: 0x04 */
    volatile uint32_t OADDR1;   /* I2C own address register 1,          offset: 0x08 */
    volatile uint32_t OADDR2;   /* I2C own address register 2,          offset: 0x0C */
    volatile uint32_t DATAR;    /* I2C data register,                   offset: 0x10 */
    volatile uint32_t STAR1;    /* I2C status register 1,               offset: 0x14 */
    volatile uint32_t STAR2;    /* I2C status register 2,               offset: 0x18 */
    volatile uint32_t CKCFGR;   /* I2C clock control register,          offset: 0x1C */
} I2C_TypeDef;

/* NVIC Register Structure */
typedef struct {
    volatile uint32_t ISER[1];  /* Interrupt set-enable register,       offset: 0x00 */
    uint32_t RESERVED0[31];
    volatile uint32_t ICER[1];  /* Interrupt clear-enable register,     offset: 0x80 */
    uint32_t RESERVED1[31];
    volatile uint32_t ISPR[1];  /* Interrupt set-pending register,      offset: 0x100 */
    uint32_t RESERVED2[31];
    volatile uint32_t ICPR[1];  /* Interrupt clear-pending register,    offset: 0x180 */
    uint32_t RESERVED3[31];
    volatile uint32_t IABR[1];  /* Interrupt active bit register,       offset: 0x200 */
    uint32_t RESERVED4[63];
    volatile uint32_t IPR[8];   /* Interrupt priority registers,        offset: 0x300 */
} NVIC_TypeDef;

/* PFIC Register Structure (Platform Interrupt Controller) */
typedef struct {
    volatile uint32_t ISR[8];   /* Interrupt status registers */
    volatile uint32_t IPR[8];   /* Interrupt pending registers */
    volatile uint32_t ITHRESDR; /* Interrupt threshold register */
    volatile uint32_t RESERVED0;
    volatile uint32_t CFGR;     /* Configuration register */
    volatile uint32_t GISR;     /* Global interrupt status register */
    uint32_t RESERVED1[10];
    volatile uint32_t VTFIDR;   /* VTF interrupt ID register */
    uint32_t RESERVED2[3];
    volatile uint32_t VTFADDR[4]; /* VTF interrupt addresses */
    uint32_t RESERVED3[90];
    volatile uint32_t IENR[8];  /* Interrupt enable registers */
    uint32_t RESERVED4[24];
    volatile uint32_t IRER[8];  /* Interrupt reset enable registers */
    uint32_t RESERVED5[24];
    volatile uint32_t IPSR[8];  /* Interrupt pending set registers */
    uint32_t RESERVED6[24];
    volatile uint32_t IPRR[8];  /* Interrupt pending reset registers */
    uint32_t RESERVED7[24];
    volatile uint32_t IACTR[8]; /* Interrupt activate registers */
    uint32_t RESERVED8[24];
    volatile uint32_t IPRIOR[64]; /* Interrupt priority config registers */
    uint32_t RESERVED9[516];
    volatile uint32_t SCTLR;    /* System control register */
} PFIC_TypeDef;

/* EXTI Register Structure */
typedef struct {
    volatile uint32_t INTENR;   /* Interrupt enable register */
    volatile uint32_t EVENR;    /* Event enable register */
    volatile uint32_t RTENR;    /* Rising trigger enable register */
    volatile uint32_t FTENR;    /* Falling trigger enable register */
    volatile uint32_t SWIEVR;   /* Software interrupt event register */
    volatile uint32_t INTFR;    /* Interrupt flag register */
} EXTI_TypeDef;

/* PWR Register Structure */
typedef struct {
    volatile uint32_t CTLR;     /* Power control register */
    volatile uint32_t CSR;      /* Power control/status register */
} PWR_TypeDef;

/* ================================================================================ */
/* ================             Peripheral Pointers                ================ */
/* ================================================================================ */

#define GPIOA               ((GPIO_TypeDef *)GPIOA_BASE)
#define GPIOC               ((GPIO_TypeDef *)GPIOC_BASE)
#define GPIOD               ((GPIO_TypeDef *)GPIOD_BASE)
#define RCC                 ((RCC_TypeDef *)RCC_BASE)
#define SPI1                ((SPI_TypeDef *)SPI1_BASE)
#define DMA1                ((DMA_TypeDef *)DMA1_BASE)
#define DMA1_Channel1       ((DMA_Channel_TypeDef *)DMA1_Channel1_BASE)
#define DMA1_Channel2       ((DMA_Channel_TypeDef *)DMA1_Channel2_BASE)
#define DMA1_Channel3       ((DMA_Channel_TypeDef *)DMA1_Channel3_BASE)
#define DMA1_Channel4       ((DMA_Channel_TypeDef *)DMA1_Channel4_BASE)
#define DMA1_Channel5       ((DMA_Channel_TypeDef *)DMA1_Channel5_BASE)
#define DMA1_Channel6       ((DMA_Channel_TypeDef *)DMA1_Channel6_BASE)
#define DMA1_Channel7       ((DMA_Channel_TypeDef *)DMA1_Channel7_BASE)
#define AFIO                ((AFIO_TypeDef *)AFIO_BASE)
#define I2C1                ((I2C_TypeDef *)I2C1_BASE)
#define NVIC                ((NVIC_TypeDef *)NVIC_BASE)
#define PFIC                ((PFIC_TypeDef *)PFIC_BASE)
#define EXTI                ((EXTI_TypeDef *)EXTI_BASE)
#define PWR                 ((PWR_TypeDef *)PWR_BASE)

/* ================================================================================ */
/* ================            Register Bit Definitions            ================ */
/* ================================================================================ */

/* RCC_AHBPCENR Register Bits */
#define RCC_AHBPCENR_DMA1EN             (1 << 0)
#define RCC_AHBPCENR_SRAMEN             (1 << 2)
#define RCC_DMA1EN                      RCC_AHBPCENR_DMA1EN

/* RCC_APB2PCENR Register Bits */
#define RCC_APB2PCENR_AFIOEN            (1 << 0)
#define RCC_APB2PCENR_IOPAEN            (1 << 2)
#define RCC_APB2PCENR_IOPBEN            (1 << 3)
#define RCC_APB2PCENR_IOPCEN            (1 << 4)
#define RCC_APB2PCENR_IOPDEN            (1 << 5)
#define RCC_APB2PCENR_ADC1EN            (1 << 9)
#define RCC_APB2PCENR_TIM1EN            (1 << 11)
#define RCC_APB2PCENR_SPI1EN            (1 << 12)
#define RCC_APB2PCENR_USART1EN          (1 << 14)
#define RCC_APB2Periph_GPIOC            RCC_APB2PCENR_IOPCEN
#define RCC_APB2Periph_AFIO             RCC_APB2PCENR_AFIOEN
#define RCC_APB2Periph_SPI1             RCC_APB2PCENR_SPI1EN
#define RCC_AHBPeriph_DMA1              RCC_AHBPCENR_DMA1EN

/* RCC_APB1PCENR Register Bits */
#define RCC_APB1PCENR_TIM2EN            (1 << 0)
#define RCC_APB1PCENR_WWDGEN            (1 << 11)
#define RCC_APB1PCENR_I2C1EN            (1 << 21)
#define RCC_APB1PCENR_PWREN             (1 << 28)
#define RCC_APB1Periph_I2C1             RCC_APB1PCENR_I2C1EN

/* RCC_CFGR0 Register Bits */
#define RCC_HPRE_DIV1                   (0 << 4)
#define RCC_HPRE_DIV2                   (8 << 4)
#define RCC_HPRE_DIV3                   (13 << 4)
#define RCC_HPRE_DIV4                   (9 << 4)
#define RCC_HPRE_DIV8                   (10 << 4)

/* SPI_CTLR1 Register Bits */
#define SPI_CTLR1_CPHA                  (1 << 0)
#define SPI_CTLR1_CPOL                  (1 << 1)
#define SPI_CTLR1_MSTR                  (1 << 2)
#define SPI_CTLR1_BR_Pos                3
#define SPI_CTLR1_BR_Msk                (7 << SPI_CTLR1_BR_Pos)
#define SPI_CTLR1_SPE                   (1 << 6)
#define SPI_CTLR1_LSBFIRST              (1 << 7)
#define SPI_CTLR1_SSI                   (1 << 8)
#define SPI_CTLR1_SSM                   (1 << 9)
#define SPI_CTLR1_RXONLY                (1 << 10)
#define SPI_CTLR1_DFF                   (1 << 11)
#define SPI_CTLR1_CRCNEXT               (1 << 12)
#define SPI_CTLR1_CRCEN                 (1 << 13)
#define SPI_CTLR1_BIDIOE                (1 << 14)
#define SPI_CTLR1_BIDIMODE              (1 << 15)

/* SPI legacy names */
#define SPI_NSS_Soft                    SPI_CTLR1_SSM
#define SPI_NSSInternalSoft_Set         SPI_CTLR1_SSI
#define SPI_Mode_Master                 SPI_CTLR1_MSTR
#define SPI_DataSize_8b                 0  /* DFF=0 for 8-bit */
#define SPI_Direction_2Lines_FullDuplex 0  /* BIDIMODE=0, RXONLY=0 */
#define CTLR1_SPE_Set                   SPI_CTLR1_SPE
#define CTLR2_SSOE_Set                  SPI_CTLR2_SSOE

/* SPI_CTLR2 Register Bits */
#define SPI_CTLR2_RXDMAEN               (1 << 0)
#define SPI_CTLR2_TXDMAEN               (1 << 1)
#define SPI_CTLR2_SSOE                  (1 << 2)
#define SPI_CTLR2_ERRIE                 (1 << 5)
#define SPI_CTLR2_RXNEIE                (1 << 6)
#define SPI_CTLR2_TXEIE                 (1 << 7)

/* SPI_STATR Register Bits */
#define SPI_STATR_RXNE                  (1 << 0)
#define SPI_STATR_TXE                   (1 << 1)
#define SPI_STATR_CHSIDE                (1 << 2)
#define SPI_STATR_UDR                   (1 << 3)
#define SPI_STATR_CRCERR                (1 << 4)
#define SPI_STATR_MODF                  (1 << 5)
#define SPI_STATR_OVR                   (1 << 6)
#define SPI_STATR_BSY                   (1 << 7)

/* DMA_CFGR Register Bits */
#define DMA_CFGR_EN                     (1 << 0)
#define DMA_CFGR_TCIE                   (1 << 1)
#define DMA_CFGR_HTIE                   (1 << 2)
#define DMA_CFGR_TEIE                   (1 << 3)
#define DMA_CFGR_DIR                    (1 << 4)
#define DMA_CFGR_CIRC                   (1 << 5)
#define DMA_CFGR_PINC                   (1 << 6)
#define DMA_CFGR_MINC                   (1 << 7)
#define DMA_CFGR_PSIZE_Pos              8
#define DMA_CFGR_PSIZE_8BIT             (0 << DMA_CFGR_PSIZE_Pos)
#define DMA_CFGR_PSIZE_16BIT            (1 << DMA_CFGR_PSIZE_Pos)
#define DMA_CFGR_PSIZE_32BIT            (2 << DMA_CFGR_PSIZE_Pos)
#define DMA_CFGR_MSIZE_Pos              10
#define DMA_CFGR_MSIZE_8BIT             (0 << DMA_CFGR_MSIZE_Pos)
#define DMA_CFGR_MSIZE_16BIT            (1 << DMA_CFGR_MSIZE_Pos)
#define DMA_CFGR_MSIZE_32BIT            (2 << DMA_CFGR_MSIZE_Pos)
#define DMA_CFGR_PL_Pos                 12
#define DMA_CFGR_PL_LOW                 (0 << DMA_CFGR_PL_Pos)
#define DMA_CFGR_PL_MEDIUM              (1 << DMA_CFGR_PL_Pos)
#define DMA_CFGR_PL_HIGH                (2 << DMA_CFGR_PL_Pos)
#define DMA_CFGR_PL_VERY_HIGH           (3 << DMA_CFGR_PL_Pos)
#define DMA_CFGR_MEM2MEM                (1 << 14)

/* DMA_INTFR Register Bits */
#define DMA_GIF1                        (1 << 0)
#define DMA_TCIF1                       (1 << 1)
#define DMA_HTIF1                       (1 << 2)
#define DMA_TEIF1                       (1 << 3)
#define DMA_GIF2                        (1 << 4)
#define DMA_TCIF2                       (1 << 5)
#define DMA_HTIF2                       (1 << 6)
#define DMA_TEIF2                       (1 << 7)
#define DMA_GIF3                        (1 << 8)
#define DMA_TCIF3                       (1 << 9)
#define DMA_HTIF3                       (1 << 10)
#define DMA_TEIF3                       (1 << 11)

/* DMA IT flags (interrupt flags) */
#define DMA1_IT_GL1                     DMA_GIF1
#define DMA1_IT_TC1                     DMA_TCIF1
#define DMA1_IT_HT1                     DMA_HTIF1
#define DMA1_IT_TE1                     DMA_TEIF1
#define DMA1_IT_GL2                     DMA_GIF2
#define DMA1_IT_TC2                     DMA_TCIF2
#define DMA1_IT_HT2                     DMA_HTIF2
#define DMA1_IT_TE2                     DMA_TEIF2
#define DMA1_IT_GL3                     DMA_GIF3
#define DMA1_IT_TC3                     DMA_TCIF3
#define DMA1_IT_HT3                     DMA_HTIF3
#define DMA1_IT_TE3                     DMA_TEIF3

/* DMA CFGR legacy names (CFGR1_xxx for compatibility) */
#define DMA_CFGR1_EN                    DMA_CFGR_EN
#define DMA_CFGR1_TCIE                  DMA_CFGR_TCIE
#define DMA_CFGR1_HTIE                  DMA_CFGR_HTIE
#define DMA_CFGR1_TEIE                  DMA_CFGR_TEIE
#define DMA_CFGR1_DIR                   DMA_CFGR_DIR
#define DMA_CFGR1_CIRC                  DMA_CFGR_CIRC
#define DMA_CFGR1_PINC                  DMA_CFGR_PINC
#define DMA_CFGR1_MINC                  DMA_CFGR_MINC
#define DMA_CFGR1_PL_0                  (1 << DMA_CFGR_PL_Pos)
#define DMA_CFGR1_PL_1                  (2 << DMA_CFGR_PL_Pos)
#define DMA_CFGR1_PL_MASK               (3 << DMA_CFGR_PL_Pos)

/* GPIO Configuration Modes */
#define GPIO_CNF_IN_ANALOG              0x00
#define GPIO_CNF_IN_FLOATING            0x04
#define GPIO_CNF_IN_PUPD                0x08
#define GPIO_CNF_OUT_PP                 0x00
#define GPIO_CNF_OUT_OD                 0x04
#define GPIO_CNF_OUT_PP_AF              0x08
#define GPIO_CNF_OUT_OD_AF              0x0C

#define GPIO_MODE_IN                    0x00
#define GPIO_MODE_OUT_10MHz             0x01
#define GPIO_MODE_OUT_2MHz              0x02
#define GPIO_MODE_OUT_50MHz             0x03

/* GPIO Speed aliases for compatibility */
#define GPIO_Speed_10MHz                GPIO_MODE_OUT_10MHz
#define GPIO_Speed_2MHz                 GPIO_MODE_OUT_2MHz
#define GPIO_Speed_50MHz                GPIO_MODE_OUT_50MHz

/* AFIO_PCFR1 Register Bits */
#define AFIO_PCFR1_SPI1_REMAP           (1 << 0)
#define AFIO_PCFR1_I2C1_REMAP           (1 << 1)
#define AFIO_PCFR1_USART1_REMAP         (1 << 2)

/* PWR_CTLR Register Bits */
#define PWR_CTLR_LPDS                   (1 << 0)
#define PWR_CTLR_PDDS                   (1 << 1)
#define PWR_CTLR_CWUF                   (1 << 2)
#define PWR_CTLR_CSBF                   (1 << 3)
#define PWR_CTLR_PVDE                   (1 << 4)

/* EXTI Line definitions */
#define EXTI_Line0                      (1 << 0)

/* NVIC Functions */
#define NVIC_EnableIRQ(IRQn)            (NVIC->ISER[0] = (1 << ((uint32_t)(IRQn))))
#define NVIC_DisableIRQ(IRQn)           (NVIC->ICER[0] = (1 << ((uint32_t)(IRQn))))

/* I2C_CTLR1 Register Bits */
#define I2C_CTLR1_PE                    (1 << 0)
#define I2C_CTLR1_SMBUS                 (1 << 1)
#define I2C_CTLR1_SMBTYPE               (1 << 3)
#define I2C_CTLR1_ENARP                 (1 << 4)
#define I2C_CTLR1_ENPEC                 (1 << 5)
#define I2C_CTLR1_ENGC                  (1 << 6)
#define I2C_CTLR1_NOSTRETCH             (1 << 7)
#define I2C_CTLR1_START                 (1 << 8)
#define I2C_CTLR1_STOP                  (1 << 9)
#define I2C_CTLR1_ACK                   (1 << 10)
#define I2C_CTLR1_POS                   (1 << 11)
#define I2C_CTLR1_PEC                   (1 << 12)
#define I2C_CTLR1_ALERT                 (1 << 13)
#define I2C_CTLR1_SWRST                 (1 << 15)

/* I2C_CTLR2 Register Bits */
#define I2C_CTLR2_FREQ_Pos              0
#define I2C_CTLR2_FREQ_Msk              (0x3F << I2C_CTLR2_FREQ_Pos)
#define I2C_CTLR2_FREQ                  I2C_CTLR2_FREQ_Msk
#define I2C_CTLR2_ITERREN               (1 << 8)
#define I2C_CTLR2_ITEVTEN               (1 << 9)
#define I2C_CTLR2_ITBUFEN               (1 << 10)
#define I2C_CTLR2_DMAEN                 (1 << 11)
#define I2C_CTLR2_LAST                  (1 << 12)

/* I2C_STAR1 Register Bits */
#define I2C_STAR1_SB                    (1 << 0)
#define I2C_STAR1_ADDR                  (1 << 1)
#define I2C_STAR1_BTF                   (1 << 2)
#define I2C_STAR1_ADD10                 (1 << 3)
#define I2C_STAR1_STOPF                 (1 << 4)
#define I2C_STAR1_RXNE                  (1 << 6)
#define I2C_STAR1_TXE                   (1 << 7)
#define I2C_STAR1_BERR                  (1 << 8)
#define I2C_STAR1_ARLO                  (1 << 9)
#define I2C_STAR1_AF                    (1 << 10)
#define I2C_STAR1_OVR                   (1 << 11)
#define I2C_STAR1_PECERR                (1 << 12)
#define I2C_STAR1_TIMEOUT               (1 << 14)
#define I2C_STAR1_SMBALERT              (1 << 15)

/* I2C_STAR2 Register Bits */
#define I2C_STAR2_MSL                   (1 << 0)
#define I2C_STAR2_BUSY                  (1 << 1)
#define I2C_STAR2_TRA                   (1 << 2)
#define I2C_STAR2_GENCALL               (1 << 4)
#define I2C_STAR2_SMBDEFAULT            (1 << 5)
#define I2C_STAR2_SMBHOST               (1 << 6)
#define I2C_STAR2_DUALF                 (1 << 7)

/* I2C_CKCFGR Register Bits */
#define I2C_CKCFGR_CCR_Pos              0
#define I2C_CKCFGR_CCR_Msk              (0xFFF << I2C_CKCFGR_CCR_Pos)
#define I2C_CKCFGR_CCR                  I2C_CKCFGR_CCR_Msk
#define I2C_CKCFGR_DUTY                 (1 << 14)
#define I2C_CKCFGR_FS                   (1 << 15)

/* RCC_CR (CTLR) Register Bits */
#define RCC_CR_HSION                    (1 << 0)
#define RCC_CR_HSIRDY                   (1 << 1)
#define RCC_CR_HSITRIM_Pos              3
#define RCC_CR_HSITRIM_Msk              (0x1F << RCC_CR_HSITRIM_Pos)
#define RCC_CR_HSICAL_Pos               8
#define RCC_CR_HSICAL_Msk               (0xFF << RCC_CR_HSICAL_Pos)
#define RCC_CR_HSEON                    (1 << 16)
#define RCC_CR_HSERDY                   (1 << 17)
#define RCC_CR_HSEBYP                   (1 << 18)
#define RCC_CR_CSSON                    (1 << 19)
#define RCC_CR_PLLON                    (1 << 24)
#define RCC_CR_PLLRDY                   (1 << 25)

/* RCC_RSTSCKR Register Bits (Reset and Clock Status) */
#define RCC_LSION                       (1 << 0)
#define RCC_LSIRDY                      (1 << 1)

/* RCC_CFGR0 Register Bits */
#define RCC_CFGR0_SW_Pos                0
#define RCC_CFGR0_SW_Msk                (0x3 << RCC_CFGR0_SW_Pos)
#define RCC_CFGR0_SW_HSI                (0x0 << RCC_CFGR0_SW_Pos)
#define RCC_CFGR0_SW_HSE                (0x1 << RCC_CFGR0_SW_Pos)
#define RCC_CFGR0_SW_PLL                (0x2 << RCC_CFGR0_SW_Pos)
#define RCC_CFGR0_SWS_Pos               2
#define RCC_CFGR0_SWS_Msk               (0x3 << RCC_CFGR0_SWS_Pos)
#define RCC_CFGR0_SWS                   RCC_CFGR0_SWS_Msk
#define RCC_CFGR0_HPRE_Pos              4
#define RCC_CFGR0_HPRE_Msk              (0xF << RCC_CFGR0_HPRE_Pos)
#define RCC_CFGR0_HPRE                  RCC_CFGR0_HPRE_Msk
#define RCC_CFGR0_PPRE1_Pos             8
#define RCC_CFGR0_PPRE1_Msk             (0x7 << RCC_CFGR0_PPRE1_Pos)
#define RCC_CFGR0_PPRE2_Pos             11
#define RCC_CFGR0_PPRE2_Msk             (0x7 << RCC_CFGR0_PPRE2_Pos)
#define RCC_CFGR0_ADCPRE_Pos            14
#define RCC_CFGR0_ADCPRE_Msk            (0x3 << RCC_CFGR0_ADCPRE_Pos)
#define RCC_CFGR0_PLLSRC                (1 << 16)
#define RCC_CFGR0_PLLXTPRE              (1 << 17)
#define RCC_CFGR0_PLLMUL_Pos            18
#define RCC_CFGR0_PLLMUL_Msk            (0xF << RCC_CFGR0_PLLMUL_Pos)
#define RCC_CFGR0_PLLMUL                RCC_CFGR0_PLLMUL_Msk

/* System clock frequency (default 48MHz HSI) */
#ifndef HSI_VALUE
#define HSI_VALUE                       48000000
#endif

#ifndef HSE_VALUE
#define HSE_VALUE                       24000000
#endif

#ifndef SYSCLK_FREQ_48MHz_HSI
#define SYSCLK_FREQ_48MHz_HSI           48000000
#endif

#ifndef FUNCONF_SYSTEM_CORE_CLOCK
#define FUNCONF_SYSTEM_CORE_CLOCK       SYSCLK_FREQ_48MHz_HSI
#endif

/* External declarations */
extern uint32_t SystemCoreClock;
void SystemInit(void);
void SystemCoreClockUpdate(void);
void Delay_Us(uint32_t us);
void Delay_Ms(uint32_t ms);

/* WFI instruction wrapper */
static inline void __WFI(void) {
    __asm__ volatile ("wfi");
}

/* ================================================================================ */
/* ================        GPIO Helper Functions ()  ================ */
/* ================================================================================ */

/* Pin definitions - Encoded format
 * Upper nibble: Port (0=A, 1=C, 2=D), Lower nibble: Pin number
 */
#define PA0  0x00
#define PA1  0x01
#define PA2  0x02
#define PC0  0x10
#define PC1  0x11
#define PC2  0x12
#define PC3  0x13
#define PC4  0x14
#define PC5  0x15
#define PC6  0x16
#define PC7  0x17
#define PD0  0x20
#define PD1  0x21
#define PD2  0x22
#define PD3  0x23
#define PD4  0x24
#define PD5  0x25
#define PD6  0x26
#define PD7  0x27

/* GpioOf macro - Convert pin number to GPIO port pointer */
#define GpioOf(pin) ((GPIO_TypeDef *)(\
    ((pin) & 0xF0) == 0x00 ? (uint32_t)GPIOA : \
    ((pin) & 0xF0) == 0x10 ? (uint32_t)GPIOC : \
    ((pin) & 0xF0) == 0x20 ? (uint32_t)GPIOD : 0))

/* GPIO pin mode configuration helper - ible */
static inline void funPinMode(uint8_t pin_num, uint32_t mode) {
    GPIO_TypeDef *port = GpioOf(pin_num);
    uint8_t pin = pin_num & 0x0F;
    uint32_t config = mode & 0x0F;

    if (pin < 8) {
        uint32_t shift = pin * 4;
        port->CFGLR = (port->CFGLR & ~(0x0F << shift)) | (config << shift);
    } else {
        uint32_t shift = (pin - 8) * 4;
        port->CFGHR = (port->CFGHR & ~(0x0F << shift)) | (config << shift);
    }
}

/* GPIO digital write - ible */
static inline void funDigitalWrite(uint8_t pin_num, uint8_t value) {
    GPIO_TypeDef *port = GpioOf(pin_num);
    uint8_t pin = pin_num & 0x0F;

    if (value) {
        port->BSHR = (1 << pin);
    } else {
        port->BSHR = (1 << (pin + 16));
    }
}

/* GPIO digital read - ible */
static inline uint8_t funDigitalRead(uint8_t pin_num) {
    GPIO_TypeDef *port = GpioOf(pin_num);
    uint8_t pin = pin_num & 0x0F;
    return (port->INDR & (1 << pin)) ? 1 : 0;
}

/* GPIO port initialization - Enable all GPIO clocks */
static inline void funGpioInitAll(void) {
    RCC->APB2PCENR |= RCC_APB2PCENR_IOPAEN | RCC_APB2PCENR_IOPCEN | RCC_APB2PCENR_IOPDEN;
}

/* Pin mode constants () */
#define GPIO_CFGLR_OUT_10Mhz_PP     ((GPIO_CNF_OUT_PP) | (GPIO_MODE_OUT_10MHz))
#define GPIO_CFGLR_OUT_2Mhz_PP      ((GPIO_CNF_OUT_PP) | (GPIO_MODE_OUT_2MHz))
#define GPIO_CFGLR_OUT_50Mhz_PP     ((GPIO_CNF_OUT_PP) | (GPIO_MODE_OUT_50MHz))
#define GPIO_CFGLR_OUT_10Mhz_OD     ((GPIO_CNF_OUT_OD) | (GPIO_MODE_OUT_10MHz))
#define GPIO_CFGLR_OUT_10Mhz_AF_PP  ((GPIO_CNF_OUT_PP_AF) | (GPIO_MODE_OUT_10MHz))
#define GPIO_CFGLR_OUT_50Mhz_AF_PP  ((GPIO_CNF_OUT_PP_AF) | (GPIO_MODE_OUT_50MHz))
#define GPIO_CFGLR_IN_ANALOG        ((GPIO_CNF_IN_ANALOG) | (GPIO_MODE_IN))
#define GPIO_CFGLR_IN_FLOATING      ((GPIO_CNF_IN_FLOATING) | (GPIO_MODE_IN))
#define GPIO_CFGLR_IN_PUPD          ((GPIO_CNF_IN_PUPD) | (GPIO_MODE_IN))

#ifdef __cplusplus
}
#endif

#endif /* __CH32V00X_H */
