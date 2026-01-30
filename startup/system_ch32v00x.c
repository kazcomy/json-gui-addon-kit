/**
 * @file system_ch32v00x.c
 * @brief CH32V003 System Initialization
 *
 * Based on WCH official CH32V003 SDK
 * Provides SystemInit() for clock and peripheral configuration
 */

#include "ch32v00x.h"

/* Default system clock frequency (48 MHz HSI) */
uint32_t SystemCoreClock = 48000000;

/**
 * @brief System Clock Configuration
 *
 * Configures the system clock to use HSI (48 MHz internal RC oscillator)
 * This is the default configuration for CH32V003
 */
static void SystemClock_Config(void)
{
    /* RCC is already configured to use HSI 48MHz by default after reset */
    /* HSI is enabled and selected as system clock */

    /* Wait for HSI to be stable */
    while (!(RCC->CTLR & RCC_CR_HSIRDY))
        ;

    /* Configure HCLK (AHB clock) */
    /* Default: HCLK = SYSCLK (48 MHz) */
    RCC->CFGR0 &= ~RCC_CFGR0_HPRE;
    RCC->CFGR0 |= RCC_HPRE_DIV1;

    /* Flash latency configuration for 48 MHz */
    /* FLASH->ACTLR is configured by bootloader/reset */
}

/**
 * @brief Initialize system
 *
 * Called from startup code before main()
 * Sets up clocks and basic system configuration
 */
void SystemInit(void)
{
    /* Configure system clock */
    SystemClock_Config();

    /* Enable PFIC (Platform Fast Interrupt Controller) */
    /* PFIC is RISC-V interrupt controller for CH32V */
    PFIC->SCTLR = 0;  /* Disable all interrupts initially */

    /* Optional: Enable vector mode for faster interrupt response */
    /* PFIC->VTFIDR[0] = 0x00; */
}

/**
 * @brief Update SystemCoreClock variable
 *
 * Updates the SystemCoreClock variable with current clock configuration
 * Can be called after clock changes to recalculate the value
 */
void SystemCoreClockUpdate(void)
{
    uint32_t tmp;
    uint32_t pllsource, pllmul;

    /* Get SYSCLK source */
    tmp = RCC->CFGR0 & RCC_CFGR0_SWS;

    switch (tmp)
    {
        case 0x00:  /* HSI used as system clock */
            SystemCoreClock = HSI_VALUE;
            break;

        case 0x04:  /* HSE used as system clock */
            SystemCoreClock = HSE_VALUE;
            break;

        case 0x08:  /* PLL used as system clock */
            /* Get PLL configuration */
            pllsource = RCC->CFGR0 & RCC_CFGR0_PLLSRC;
            pllmul = (RCC->CFGR0 & RCC_CFGR0_PLLMUL) >> 18;
            pllmul += 2;  /* PLL multiplier is offset by 2 */

            if (pllsource == 0x00)
            {
                /* HSI/2 selected as PLL input */
                SystemCoreClock = (HSI_VALUE >> 1) * pllmul;
            }
            else
            {
                /* HSE selected as PLL input */
                SystemCoreClock = HSE_VALUE * pllmul;
            }
            break;

        default:
            SystemCoreClock = HSI_VALUE;
            break;
    }

    /* Apply AHB prescaler */
    tmp = (RCC->CFGR0 & RCC_CFGR0_HPRE) >> 4;

    const uint8_t AHBPrescTable[16] = {
        0, 0, 0, 0, 0, 0, 0, 0,
        1, 2, 3, 4, 6, 7, 8, 9
    };

    if (tmp < 8)
    {
        SystemCoreClock = SystemCoreClock;  /* No division */
    }
    else
    {
        SystemCoreClock = SystemCoreClock >> AHBPrescTable[tmp];
    }
}

/**
 * @brief Simple delay function
 *
 * Busy-wait delay for approximately 'us' microseconds
 * Assumes 48 MHz system clock
 *
 * @param us Delay in microseconds
 */
void Delay_Us(uint32_t us)
{
    /* Approximate: 12 cycles per microsecond at 48 MHz */
    volatile uint32_t count = us * 12;
    while (count--)
    {
        __asm__ volatile ("nop");
    }
}

/**
 * @brief Simple millisecond delay
 *
 * @param ms Delay in milliseconds
 */
void Delay_Ms(uint32_t ms)
{
    while (ms--)
    {
        Delay_Us(1000);
    }
}
