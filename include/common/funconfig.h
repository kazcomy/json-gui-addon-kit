/**
 * @file funconfig.h
 * @brief CH32V003 configuration and feature selection.
 *
 * This file contains configuration options for the CH32V003 microcontroller:
 * - Clock configuration (HSI, HSE, PLL options)
 * - Peripheral enable/disable flags
 * - Debug and printf configuration
 * - I2C pinout selection
 * - System-specific optimizations
 *
 * Most options are commented out by default and should be enabled
 * based on the specific hardware configuration and requirements.
 */
#ifndef _FUNCONFIG_H
#define _FUNCONFIG_H

// board definition file will already take care of this
/** Enable CH32V003 specific features. */
#define CH32V003 1
// SSD1306_128X32 is now defined in platformio.ini build flags
/** Use default I2C pinout configuration. */
#define I2C_PINOUT_DEFAULT

// Exclude built-in SSD1306 to use lexus2k library instead
/** Disable built-in SSD1306 driver in favor of custom implementation. */
#define FUNCONF_DISABLE_SSD1306 1
// #define I2C_PINOUT_ALT_1
// #define I2C_PINOUT_ALT_2

// #define FUNCONF_USE_PLL 1               // Use built-in 2x PLL
// #define FUNCONF_USE_HSI 1               // Use HSI Internal Oscillator
// #define FUNCONF_USE_HSE 0               // Use External Oscillator
// #define FUNCONF_HSITRIM 0x10            // Use factory calibration on HSI
// Trim. #define FUNCONF_SYSTEM_CORE_CLOCK 48000000  // Computed Clock in Hz
// (Default only for 003, other chips have other defaults) #define
// FUNCONF_HSE_BYPASS 0            // Use HSE Bypass feature (for oscillator
// input) #define FUNCONF_USE_CLK_SEC	1			// Use clock security system,
// enabled by default
/** Disable debug printf functionality by default.
 *  Can be overridden per-environment via build flags.
 */
#ifndef FUNCONF_USE_DEBUGPRINTF
#define FUNCONF_USE_DEBUGPRINTF 0
#endif
/** Enable UART printf for serial output at monitor_speed (default off).
 *  Can be overridden per-environment via build flags.
 */
#ifndef FUNCONF_USE_UARTPRINTF
#define FUNCONF_USE_UARTPRINTF 0
#endif
/* Optional: baud is 115200 by default; uncomment to override */
/* #define FUNCONF_UART_PRINTF_BAUD 115200 */
// #define FUNCONF_NULL_PRINTF 0           // Have printf but direct it
// "nowhere" #define FUNCONF_SYSTICK_USE_HCLK 0      // Should systick be at 48
// MHz (1) or 6MHz (0) on an '003.  Typically set to 0 to divide HCLK by 8.
// #define FUNCONF_TINYVECTOR 0            // If enabled, Does not allow normal
// interrupts. #define FUNCONF_UART_PRINTF_BAUD 115200 // Only used if
// FUNCONF_USE_UARTPRINTF is set. #define FUNCONF_DEBUGPRINTF_TIMEOUT 0x80000 //
// Arbitrary time units, this is around 120ms. #define FUNCONF_ENABLE_HPE 1 //
// Enable hardware interrupt stack.  Very good on QingKeV4, i.e. x035, v10x,
// v20x, v30x, but questionable on 003.
//                                         // If you are using that, consider
//                                         using INTERRUPT_DECORATOR as an
//                                         attribute to your interrupt handlers.
// #define FUNCONF_USE_5V_VDD 0            // Enable this if you plan to use
// your part at 5V - affects USB and PD configration on the x035. #define
// #define FUNCONF_DEBUG_HARDFAULT    1    // Log fatal errors with "printf"

/* Logging macros compile to no-ops to avoid pulling stdio/printf. */
#define LOG_DEBUG(fmt, ...) ((void) 0)
#define LOG_INFO(fmt, ...) ((void) 0)
#define LOG_ERROR(fmt, ...) ((void) 0)

#endif
