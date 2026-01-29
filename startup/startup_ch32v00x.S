/**
 * @file startup_ch32v00x.S
 * @brief Startup code for CH32V003 series
 *
 * This file provides:
 * - Interrupt vector table
 * - Reset handler
 * - Default interrupt handlers
 * - Data/BSS initialization
 */

    .section .init, "ax"
    .global _start
    .align 2

_start:
    j handle_reset

    .section .vector, "ax"
    .align 2
    .global _vector_base
_vector_base:
    .option norvc
    .word 0                         /* 0:  Reserved */
    .word 0                         /* 1:  Reserved */
    .word NMI_Handler               /* 2:  NMI Handler */
    .word HardFault_Handler         /* 3:  Hard Fault Handler */
    .word 0                         /* 4:  Reserved */
    .word 0                         /* 5:  Reserved */
    .word 0                         /* 6:  Reserved */
    .word 0                         /* 7:  Reserved */
    .word 0                         /* 8:  Reserved */
    .word 0                         /* 9:  Reserved */
    .word 0                         /* 10: Reserved */
    .word 0                         /* 11: Reserved */
    .word SysTick_Handler           /* 12: SysTick Handler */
    .word 0                         /* 13: Reserved */
    .word SW_Handler                /* 14: Software Interrupt */
    .word 0                         /* 15: Reserved */

    /* External Interrupts */
    .word WWDG_IRQHandler           /* 16: Window Watchdog */
    .word PVD_IRQHandler            /* 17: PVD through EXTI */
    .word FLASH_IRQHandler          /* 18: Flash */
    .word RCC_IRQHandler            /* 19: RCC */
    .word EXTI7_0_IRQHandler        /* 20: EXTI Line 7..0 */
    .word AWU_IRQHandler            /* 21: AWU */
    .word DMA1_Channel1_IRQHandler  /* 22: DMA1 Channel 1 */
    .word DMA1_Channel2_IRQHandler  /* 23: DMA1 Channel 2 */
    .word DMA1_Channel3_IRQHandler  /* 24: DMA1 Channel 3 */
    .word DMA1_Channel4_IRQHandler  /* 25: DMA1 Channel 4 */
    .word DMA1_Channel5_IRQHandler  /* 26: DMA1 Channel 5 */
    .word DMA1_Channel6_IRQHandler  /* 27: DMA1 Channel 6 */
    .word DMA1_Channel7_IRQHandler  /* 28: DMA1 Channel 7 */
    .word ADC1_IRQHandler           /* 29: ADC1 */
    .word I2C1_EV_IRQHandler        /* 30: I2C1 Event */
    .word I2C1_ER_IRQHandler        /* 31: I2C1 Error */
    .word USART1_IRQHandler         /* 32: USART1 */
    .word SPI1_IRQHandler           /* 33: SPI1 */
    .word TIM1_BRK_IRQHandler       /* 34: TIM1 Break */
    .word TIM1_UP_IRQHandler        /* 35: TIM1 Update */
    .word TIM1_TRG_COM_IRQHandler   /* 36: TIM1 Trigger/Commutation */
    .word TIM1_CC_IRQHandler        /* 37: TIM1 Capture Compare */
    .word TIM2_IRQHandler           /* 38: TIM2 */

    .option rvc

    .text
    .align 2
handle_reset:
    /* Disable interrupts */
    csrw mstatus, zero
    csrw mie, zero

    /* Setup stack pointer */
    la sp, _stack_top

    /* Initialize data section (copy from FLASH to RAM) */
    la a0, _data_lma        /* Source (Flash) */
    la a1, _data_start      /* Destination (RAM) */
    la a2, _data_end        /* End of data section */
    bgeu a1, a2, 2f         /* Skip if no data to copy */
1:
    lw t0, (a0)
    sw t0, (a1)
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b
2:

    /* Clear BSS section */
    la a0, _bss_start
    la a1, _bss_end
    bgeu a0, a1, 4f         /* Skip if no BSS to clear */
3:
    sw zero, (a0)
    addi a0, a0, 4
    bltu a0, a1, 3b
4:

    /* Enable global interrupts in mstatus */
    li t0, 0x88             /* Set MIE (bit 3) and MPIE (bit 7) */
    csrw mstatus, t0

    /* Set interrupt vector base */
    la t0, _vector_base
    ori t0, t0, 3           /* Set mode to vectored interrupts */
    csrw mtvec, t0

    /* Jump to main */
    call main

    /* If main returns, loop forever */
5:
    j 5b

/* ============================================================================ */
/* Default interrupt handlers (weak aliases)                                  */
/* ============================================================================ */

    .weak NMI_Handler
    .weak HardFault_Handler
    .weak SysTick_Handler
    .weak SW_Handler
    .weak WWDG_IRQHandler
    .weak PVD_IRQHandler
    .weak FLASH_IRQHandler
    .weak RCC_IRQHandler
    .weak EXTI7_0_IRQHandler
    .weak AWU_IRQHandler
    .weak DMA1_Channel1_IRQHandler
    .weak DMA1_Channel2_IRQHandler
    .weak DMA1_Channel3_IRQHandler
    .weak DMA1_Channel4_IRQHandler
    .weak DMA1_Channel5_IRQHandler
    .weak DMA1_Channel6_IRQHandler
    .weak DMA1_Channel7_IRQHandler
    .weak ADC1_IRQHandler
    .weak I2C1_EV_IRQHandler
    .weak I2C1_ER_IRQHandler
    .weak USART1_IRQHandler
    .weak SPI1_IRQHandler
    .weak TIM1_BRK_IRQHandler
    .weak TIM1_UP_IRQHandler
    .weak TIM1_TRG_COM_IRQHandler
    .weak TIM1_CC_IRQHandler
    .weak TIM2_IRQHandler

NMI_Handler:
HardFault_Handler:
SysTick_Handler:
SW_Handler:
WWDG_IRQHandler:
PVD_IRQHandler:
FLASH_IRQHandler:
RCC_IRQHandler:
EXTI7_0_IRQHandler:
AWU_IRQHandler:
DMA1_Channel1_IRQHandler:
DMA1_Channel2_IRQHandler:
DMA1_Channel3_IRQHandler:
DMA1_Channel4_IRQHandler:
DMA1_Channel5_IRQHandler:
DMA1_Channel6_IRQHandler:
DMA1_Channel7_IRQHandler:
ADC1_IRQHandler:
I2C1_EV_IRQHandler:
I2C1_ER_IRQHandler:
USART1_IRQHandler:
SPI1_IRQHandler:
TIM1_BRK_IRQHandler:
TIM1_UP_IRQHandler:
TIM1_TRG_COM_IRQHandler:
TIM1_CC_IRQHandler:
TIM2_IRQHandler:
1:
    j 1b

    .end
