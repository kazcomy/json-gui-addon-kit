/**
 * @file startup_ch32v00x.S
 * @brief WCH CH32V003 Startup Code
 *
 * Based on WCH official CH32V003 SDK startup assembly
 * RISC-V RV32EC core with compressed instructions
 */

    .section .init, "ax"
    .global InterruptVector
    .type InterruptVector, @function

InterruptVector:
    /* Jump to reset handler */
    j handle_reset
    .word 0                     /* Reserved */
    .word NMI_Handler           /* NMI */
    .word HardFault_Handler     /* HardFault */
    .word 0                     /* Reserved */
    .word 0                     /* Reserved */
    .word 0                     /* Reserved */
    .word 0                     /* Reserved */
    .word 0                     /* Reserved */
    .word 0                     /* Reserved */
    .word 0                     /* Reserved */
    .word 0                     /* Reserved */
    .word SysTick_Handler       /* SysTick */
    .word 0                     /* Reserved */
    .word SW_Handler            /* Software interrupt */
    .word 0                     /* Reserved */
    /* External Interrupts */
    .word WWDG_IRQHandler       /* Window Watchdog */
    .word PVD_IRQHandler        /* PVD through EXTI Line detect */
    .word FLASH_IRQHandler      /* Flash */
    .word RCC_IRQHandler        /* RCC */
    .word EXTI7_0_IRQHandler    /* EXTI Line 7-0 */
    .word AWU_IRQHandler        /* AWU */
    .word DMA1_Channel1_IRQHandler  /* DMA1 Channel 1 */
    .word DMA1_Channel2_IRQHandler  /* DMA1 Channel 2 */
    .word DMA1_Channel3_IRQHandler  /* DMA1 Channel 3 */
    .word DMA1_Channel4_IRQHandler  /* DMA1 Channel 4 */
    .word DMA1_Channel5_IRQHandler  /* DMA1 Channel 5 */
    .word DMA1_Channel6_IRQHandler  /* DMA1 Channel 6 */
    .word DMA1_Channel7_IRQHandler  /* DMA1 Channel 7 */
    .word ADC1_IRQHandler       /* ADC1 */
    .word I2C1_EV_IRQHandler    /* I2C1 Event */
    .word I2C1_ER_IRQHandler    /* I2C1 Error */
    .word USART1_IRQHandler     /* USART1 */
    .word SPI1_IRQHandler       /* SPI1 */
    .word TIM1_BRK_IRQHandler   /* TIM1 Break */
    .word TIM1_UP_IRQHandler    /* TIM1 Update */
    .word TIM1_TRG_COM_IRQHandler /* TIM1 Trigger and Commutation */
    .word TIM1_CC_IRQHandler    /* TIM1 Capture Compare */
    .word TIM2_IRQHandler       /* TIM2 */

    .size InterruptVector, . - InterruptVector

    .section .text.handle_reset
    .weak handle_reset
    .align 1
    .type handle_reset, @function
handle_reset:
    /* Disable interrupts */
    csrw mstatus, zero

    .option push
    .option norelax
    /* Initialize global pointer */
    la gp, __global_pointer$
    .option pop

    /* Initialize stack pointer */
    la sp, _eusrstack

    /* Copy .data section from FLASH to RAM */
    la a0, _data_lma        /* Load address in FLASH */
    la a1, _data_vma        /* Destination in RAM */
    la a2, _edata           /* End of .data in RAM */
    bgeu a1, a2, 2f         /* Skip if .data is empty */
1:
    lw t0, 0(a0)            /* Load word from FLASH */
    sw t0, 0(a1)            /* Store word to RAM */
    addi a0, a0, 4
    addi a1, a1, 4
    bltu a1, a2, 1b         /* Loop until end */
2:

    /* Clear .bss section */
    la a0, _sbss            /* Start of .bss */
    la a1, _ebss            /* End of .bss */
    bgeu a0, a1, 4f         /* Skip if .bss is empty */
3:
    sw zero, 0(a0)          /* Clear word */
    addi a0, a0, 4
    bltu a0, a1, 3b         /* Loop until end */
4:

    /* Enable global interrupts */
    li t0, 0x1880           /* MPIE=1, MIE=0 */
    csrw mstatus, t0

    /* Call SystemInit */
    call SystemInit

    /* Call C++ constructors (if any) */
    la a0, __init_array_start
    la a1, __init_array_end
    bgeu a0, a1, 6f
5:
    lw t0, 0(a0)
    jalr t0
    addi a0, a0, 4
    bltu a0, a1, 5b
6:

    /* Call main */
    call main

    /* If main returns, loop forever */
7:
    j 7b

    .size handle_reset, . - handle_reset

/* Default weak interrupt handlers */
    .section .text
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

/* Default handler - infinite loop */
    .align 1
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
