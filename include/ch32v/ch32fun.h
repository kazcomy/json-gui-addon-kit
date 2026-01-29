/**
 * @file ch32fun.h
 * @brief Minimal ch32v003fun compatibility layer
 *
 * Provides basic GPIO and peripheral helper functions
 * to replace ch32v003fun framework dependencies
 */

#ifndef __CH32FUN_H
#define __CH32FUN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32v00x.h"

/* Pin mode definitions (compatible with ch32v003fun) */
#define GPIO_Speed_10MHz    GPIO_MODE_OUT_10MHz
#define GPIO_Speed_2MHz     GPIO_MODE_OUT_2MHz
#define GPIO_Speed_50MHz    GPIO_MODE_OUT_50MHz

#define GPIO_CNF_IN_ANALOG      0x00
#define GPIO_CNF_IN_FLOATING    0x04
#define GPIO_CNF_IN_PUPD        0x08
#define GPIO_CNF_OUT_PP         0x00
#define GPIO_CNF_OUT_OD         0x04
#define GPIO_CNF_OUT_PP_AF      0x08
#define GPIO_CNF_OUT_OD_AF      0x0C

/* Combined pin mode for funPinMode() */
#define GPIO_CFGLR_OUT_10Mhz_PP     ((GPIO_CNF_OUT_PP) | (GPIO_Speed_10MHz))
#define GPIO_CFGLR_OUT_2Mhz_PP      ((GPIO_CNF_OUT_PP) | (GPIO_Speed_2MHz))
#define GPIO_CFGLR_OUT_50Mhz_PP     ((GPIO_CNF_OUT_PP) | (GPIO_Speed_50MHz))
#define GPIO_CFGLR_OUT_10Mhz_OD     ((GPIO_CNF_OUT_OD) | (GPIO_Speed_10MHz))
#define GPIO_CFGLR_OUT_10Mhz_AF_PP  ((GPIO_CNF_OUT_PP_AF) | (GPIO_Speed_10MHz))
#define GPIO_CFGLR_OUT_50Mhz_AF_PP  ((GPIO_CNF_OUT_PP_AF) | (GPIO_Speed_50MHz))
#define GPIO_CFGLR_IN_ANALOG        ((GPIO_CNF_IN_ANALOG) | (GPIO_MODE_IN))
#define GPIO_CFGLR_IN_FLOATING      ((GPIO_CNF_IN_FLOATING) | (GPIO_MODE_IN))
#define GPIO_CFGLR_IN_PUPD          ((GPIO_CNF_IN_PUPD) | (GPIO_MODE_IN))

/* Port and pin definitions */
#define FUNCONF_USE_DEBUGPRINTF 0
#define FUNCONF_DISABLE_SSD1306 1

/* System initialization */
static inline void SystemInit(void) {
    /* Basic system initialization - typically done in startup code */
    /* Configure system clock to 48MHz HSI (default) */
}

/* Delay functions (simple busy-wait) */
static inline void Delay_Us(uint32_t us) {
    /* Approximate delay for 48MHz: ~48 cycles per microsecond */
    volatile uint32_t count = us * 12; /* Rough calibration */
    while (count--) {
        __asm__ volatile ("nop");
    }
}

static inline void Delay_Ms(uint32_t ms) {
    while (ms--) {
        Delay_Us(1000);
    }
}

/* GPIO pin mode configuration */
static inline void funPinMode(GPIO_TypeDef *port, uint8_t pin, uint32_t mode) {
    uint32_t config = mode & 0x0F;

    if (pin < 8) {
        /* Configure low register (pins 0-7) */
        uint32_t shift = pin * 4;
        port->CFGLR = (port->CFGLR & ~(0x0F << shift)) | (config << shift);
    } else {
        /* Configure high register (pins 8-15) */
        uint32_t shift = (pin - 8) * 4;
        port->CFGHR = (port->CFGHR & ~(0x0F << shift)) | (config << shift);
    }
}

/* GPIO digital write */
static inline void funDigitalWrite(GPIO_TypeDef *port, uint8_t pin, uint8_t value) {
    if (value) {
        port->BSHR = (1 << pin);  /* Set bit */
    } else {
        port->BSHR = (1 << (pin + 16));  /* Reset bit */
    }
}

/* GPIO digital read */
static inline uint8_t funDigitalRead(GPIO_TypeDef *port, uint8_t pin) {
    return (port->INDR & (1 << pin)) ? 1 : 0;
}

/* GPIO port initialization helper */
static inline void funGpioInitAll(void) {
    /* Enable all GPIO clocks */
    RCC->APB2PCENR |= RCC_APB2PCENR_IOPAEN | RCC_APB2PCENR_IOPCEN | RCC_APB2PCENR_IOPDEN;
}

#ifdef __cplusplus
}
#endif

#endif /* __CH32FUN_H */
