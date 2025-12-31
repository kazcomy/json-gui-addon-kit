/**
 * @file ch32fun.h
 * @brief Host-side stub for memcalc/unit-test builds.
 */
#ifndef CH32FUN_H
#define CH32FUN_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint32_t STATR;
  uint32_t DATAR;
} spi_stub_t;

static spi_stub_t spi1_stub;

#define SPI1 (&spi1_stub)
#define SPI_STATR_OVR (1u << 6)

#endif /* CH32FUN_H */
