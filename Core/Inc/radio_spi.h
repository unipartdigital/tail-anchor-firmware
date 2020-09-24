/* radio_spi.h */

#ifndef _RADIO_SPI_H
#define _RADIO_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f7xx_hal.h"

void radio_spi_init(SPI_HandleTypeDef *spi_handle);
void radio_spi_start(void);
void radio_spi_stop(void);
void radio_spi_write(uint8_t value);
uint8_t radio_spi_read(void);
void radio_spi_wakeup_deassert(void);
void radio_spi_wakeup_assert(void);
void radio_spi_speed(bool fast);
void radio_spi_read_burst(uint8_t *data, int len);
void radio_spi_write_burst(uint8_t *data, int len);


#endif /* _RADIO_SPI_H */
