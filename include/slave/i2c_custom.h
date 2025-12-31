/**
 * @file i2c_custom.h
 * @brief Custom I2C library compatible with lib_i2c interface
 *
 * This library provides a custom implementation of I2C functions
 * that is compatible with the existing lib_i2c interface but
 * optimized for this specific project.
 */

#ifndef I2C_CUSTOM_H
#define I2C_CUSTOM_H

#include <stddef.h>

#include "ch32fun.h"

/*** Error Types *************************************************************/
typedef enum {
  I2C_OK = 0,
  I2C_ERR_BERR,
  I2C_ERR_NACK,
  I2C_ERR_TIMEOUT,
  I2C_ERR_BUSY,
} i2c_err_t;

/*** Address Types ***********************************************************/
typedef enum {
  I2C_ADDR_7BIT,
  I2C_ADDR_10BIT,
} i2c_addr_t;

/*** Device Structure ********************************************************/
typedef struct {
  uint32_t   clkr;  // Clock Rate (in Hz)
  i2c_addr_t type;  // Address Type - Determines address behaviour
  uint16_t   addr;  // Address Value. Default is WRITE in 7 and 10bit
  uint8_t    regb;  // Register Bytes 1-4 (Capped to sane range in in init())
  uint32_t   tout;  // Number of cycles before the master timesout. Useful for clock-stretch
} i2c_device_t;

/*** Predefined Clock Speeds *************************************************/
#define I2C_CLK_10KHZ 10000
#define I2C_CLK_50KHZ 50000
#define I2C_CLK_100KHZ 100000
#define I2C_CLK_400KHZ 400000

/*** Functions ***************************************************************/

/**
 * @brief Initialize the I2C Peripheral on the default pins, in Master Mode
 * @param dev device config - for clock speed, and limits values
 * @return i2c_err_t, I2C_OK On success
 */
i2c_err_t i2c_init(i2c_device_t* dev);


/**
 * @brief Write raw data to I2C device using DMA for large transfers
 * @param dev Device configuration
 * @param buf Data buffer to write
 * @param len Number of bytes to write
 * @return i2c_err_t status
 */
i2c_err_t i2c_write_raw_dma(const i2c_device_t* dev, const uint8_t* buf, const size_t len);
/**
 * @brief Query if current TX DMA transfer is still in progress (non-blocking)
 * @return 1 while DMA active, 0 when idle
 */
int i2c_tx_dma_busy(void);

#endif  // I2C_CUSTOM_H
