/* radio_spi.c */

#include "stm32f7xx.h"
#include "stm32f7xx_hal_conf.h"

#include "radio_spi.h"
#include "main.h"

#include <stdio.h>

#define RADIO_USE_WAKEUP

#define SPI_PRESCALER_FAST SPI_BAUDRATEPRESCALER_8
#define SPI_PRESCALER_SLOW SPI_BAUDRATEPRESCALER_64

static SPI_HandleTypeDef *spi;

void radio_spi_init(SPI_HandleTypeDef *spi_handle)
{
    spi = spi_handle;
    // Shouldn't be needed here, but for testing...

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);

    /*Configure GPIO pins : PE4 */
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

void radio_spi_speed(bool fast)
{
    spi->Init.BaudRatePrescaler = fast?SPI_PRESCALER_FAST:SPI_PRESCALER_SLOW;
    if (HAL_SPI_Init(spi) != HAL_OK)
    {
        Error_Handler();
    }

    // Shouldn't be needed here, but for testing...

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);

    /*Configure GPIO pins : PE4 */
    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

void radio_spi_start(void)
{
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_RESET);
}

void radio_spi_stop(void)
{
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, GPIO_PIN_SET);
}

#ifdef RADIO_USE_WAKEUP
void radio_spi_wakeup_deassert(void)
{
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_RESET);
}

void radio_spi_wakeup_assert(void)
{
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_8, GPIO_PIN_SET);
}
#endif

#if 0
static inline uint8_t radio_spi_transfer(uint8_t data)
{
  while (!(USART0->STATUS & USART_STATUS_TXBL))
    ;
  USART0->TXDATA = (uint32_t)data;
  while (!(USART0->STATUS & USART_STATUS_TXC))
    ;
  return (uint8_t)USART0->RXDATA;
}
#endif

#if 0
void radio_spi_write(uint8_t data)
{
    USART_Tx(USART0, data);
    (void) USART_Rx(USART0);
}
uint8_t radio_spi_read(void)
{
    USART_Tx(USART0, 0);
    return USART_Rx(USART0);
}
#endif

#if 0
void radio_spi_write(uint8_t data)
{
    (void) USART_SpiTransfer(USART0, data);
}
uint8_t radio_spi_read(void)
{
    return USART_SpiTransfer(USART0, 0);
}
#endif

void radio_spi_read_burst(uint8_t *data, int len)
{
//    printf("read:");
    if (HAL_SPI_Receive(spi, data, len, HAL_MAX_DELAY) != HAL_OK)
        Error_Handler();
//    for (int i = 0; i < len; i++)
//        printf(" %02x", data[i]);
//    printf("\n");
}

void radio_spi_write_burst(uint8_t *data, int len)
{
//    printf("write:");
//    for (int i = 0; i < len; i++)
//        printf(" %02x", data[i]);
//    printf("\n");
    if (HAL_SPI_Transmit(spi, data, len, HAL_MAX_DELAY) != HAL_OK)
        Error_Handler();
}

