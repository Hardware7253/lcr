#include "display.h"
#include "stm32g4xx_hal.h"
#include "blocking_delay.h"
#include "system.h"

#include <stdbool.h>

#define SPI_INSTANCE            SPI1
#define SPI_BAUDRATE            SPI_BAUDRATEPRESCALER_128 

#define SPI_CLK_EN              __HAL_RCC_SPI1_CLK_ENABLE
#define SPI_PIN_CLK_EN          __HAL_RCC_GPIOB_CLK_ENABLE(); __HAL_RCC_GPIOA_CLK_ENABLE
#define SPI_PIN_AF              GPIO_AF5_SPI1 

#define SCK_PIN                 GPIO_PIN_3
#define SCK_PIN_BUS             GPIOB

#define MOSI_PIN                GPIO_PIN_5
#define MOSI_PIN_BUS            GPIOB

#define CS_PIN                  GPIO_PIN_15
#define CS_PIN_BUS              GPIOA

#define DC_PIN                  GPIO_PIN_4
#define DC_PIN_BUS              GPIOB

static SPI_HandleTypeDef hspi;

// Implement SPI transfers from u8g2 to the display
static uint8_t u8x8_byte_hw_spi(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    static bool dc_state = 0;
    switch(msg) {
        case U8X8_MSG_BYTE_SEND:

            // Add DC bit to display data (for 3W SPI)
            uint8_t* src = arg_ptr;
            static uint16_t txbuf[256];
            for (uint8_t i = 0; i < arg_int; i++) {
                txbuf[i] = ((uint16_t)dc_state << 8) | src[i];
            }

            error_handler_msg(HAL_SPI_Transmit(&hspi, (uint8_t*)txbuf, arg_int, 10), "Failed SPI transfer");
            break;

        case U8X8_MSG_BYTE_INIT:
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
            break;

        case U8X8_MSG_BYTE_SET_DC:
            // u8x8_gpio_SetDC(u8x8, arg_int); // For 4W SPI 
            dc_state = (bool)arg_int;
            break;

        case U8X8_MSG_BYTE_START_TRANSFER:
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_enable_level);  
            u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->post_chip_enable_wait_ns, NULL);
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:      
            u8x8->gpio_and_delay_cb(u8x8, U8X8_MSG_DELAY_NANO, u8x8->display_info->pre_chip_disable_wait_ns, NULL);
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
            break;

        default:
            return 0;
    }  
    return 1;
}

// Allow u8g2 to setup and use SPI, GPIO, and delays
static uint8_t u8x8_gpio_and_delay(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
    (void) arg_ptr;
    switch(msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            SPI_PIN_CLK_EN();
            init_blocking_delay();
            delay_ms(1);

            GPIO_InitTypeDef spi_pin_cfg = {
                .Pin       = SCK_PIN,
                .Mode      = GPIO_MODE_AF_PP,
                .Pull      = GPIO_NOPULL,
                .Speed     = GPIO_SPEED_FREQ_MEDIUM,
                .Alternate = SPI_PIN_AF,
            };
            HAL_GPIO_Init(SCK_PIN_BUS, &spi_pin_cfg); // CLK
            spi_pin_cfg.Pin = MOSI_PIN;
            HAL_GPIO_Init(MOSI_PIN_BUS, &spi_pin_cfg); // MOSI

            GPIO_InitTypeDef software_pin_cfg = {
                .Pin       = CS_PIN,
                .Mode      = GPIO_MODE_OUTPUT_PP,
                .Pull      = GPIO_NOPULL,
                .Speed     = GPIO_SPEED_FREQ_MEDIUM,
                .Alternate = 0,
            };
            HAL_GPIO_Init(CS_PIN_BUS, &software_pin_cfg); // CS
            software_pin_cfg.Pin = DC_PIN;
            HAL_GPIO_Init(DC_PIN_BUS, &software_pin_cfg); // DC

            // Set DC pin low for 3W SPI communication
            HAL_GPIO_WritePin(DC_PIN_BUS, DC_PIN, GPIO_PIN_RESET);

            // Setup SPI
            SPI_CLK_EN();
            hspi.Instance = SPI_INSTANCE;
            hspi.Init.Mode = SPI_MODE_MASTER;
            hspi.Init.Direction = SPI_DIRECTION_2LINES;
            hspi.Init.DataSize = SPI_DATASIZE_9BIT;
            hspi.Init.NSS = SPI_NSS_SOFT; 
            hspi.Init.BaudRatePrescaler = SPI_BAUDRATE;
            hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
            hspi.Init.TIMode = SPI_TIMODE_DISABLE;
            hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
            hspi.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;

            // Use mode 3 
            hspi.Init.CLKPolarity = SPI_POLARITY_HIGH;
            hspi.Init.CLKPhase = SPI_PHASE_2EDGE;

            // Send garbage bytes over spi so clock idles properly
            u8x8_gpio_SetCS(u8x8, u8x8->display_info->chip_disable_level);
            error_handler_msg(HAL_SPI_Init(&hspi), "Failed to init SPI");
            error_handler_msg(HAL_SPI_Transmit(&hspi, (uint8_t[]){0, 0}, 1, 10), "Failed SPI transfer");
            delay_us(1);

            break;

        case U8X8_MSG_DELAY_100NANO:
            if (arg_int < 10) delay_us(1);
            else delay_us(((uint32_t)arg_int + 5) / 10);
            break;

        case U8X8_MSG_DELAY_10MICRO:
            delay_us((uint32_t)arg_int * 10);
            break;

        case U8X8_MSG_DELAY_MILLI:
            delay_ms((uint32_t)arg_int);
            break;

        case U8X8_MSG_GPIO_CS:
            HAL_GPIO_WritePin(CS_PIN_BUS, CS_PIN, arg_int);
            break;

        case U8X8_MSG_GPIO_DC:
            HAL_GPIO_WritePin(DC_PIN_BUS, DC_PIN, arg_int);
            break;

        default:
            u8x8_SetGPIOResult(u8x8, 1);			// default return value
            break;
    }
    return 1;
}

// Initialises u8g2
void init_display(u8g2_t* u8g2, const u8g2_cb_t *rotation) {
    u8g2_Setup_ssd1315_128x64_noname_f(u8g2, rotation, u8x8_byte_hw_spi, u8x8_gpio_and_delay);
    u8g2_InitDisplay(u8g2); 
    u8g2_SetPowerSave(u8g2, 0);
}