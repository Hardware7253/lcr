#include <string.h>
#include "sampling.h"
#include "stm32f4xx_hal.h"
#include "system.h"
#include "blocking_delay.h"
#include "ad9833.h"

// Peripheral defines 
// The setup for the peripherals and test_frequency_conf_t function assume a lot about clock speeds involved
// But the peripherals should be able to be changed around from these so long as they are derived from the same APB clock
#define TEST_ADC                ADC1 /* ADC for measuring test waveform */
#define TEST_ADC_CLK_EN         __HAL_RCC_ADC1_CLK_ENABLE
#define TEST_ADC_CHANNEL        ADC_CHANNEL_2
#define TEST_DMA                DMA2_Stream0
#define TEST_DMA_CHANNEL        DMA_CHANNEL_0
#define TEST_DMA_IRQn           DMA2_Stream0_IRQn
#define TEST_DMA_ISR            DMA2_Stream0_IRQHandler

#define DUT_ADC                 ADC2 /* ADC for measuring DUT waveform */
#define DUT_ADC_CLK_EN          __HAL_RCC_ADC2_CLK_ENABLE
#define DUT_ADC_CHANNEL         ADC_CHANNEL_3
#define DUT_DMA                 DMA2_Stream2
#define DUT_DMA_CHANNEL         DMA_CHANNEL_1
#define DUT_DMA_IRQn            DMA2_Stream2_IRQn
#define DUT_DMA_ISR             DMA2_Stream2_IRQHandler

#define TEST_PIN                 GPIO_PIN_2
#define TEST_PIN_BUS             GPIOA
#define TEST_PIN_CLK_EN          __HAL_RCC_GPIOA_CLK_ENABLE 

#define DUT_PIN                 GPIO_PIN_3
#define DUT_PIN_BUS             GPIOA
#define DUT_PIN_CLK_EN          __HAL_RCC_GPIOA_CLK_ENABLE 

// Timer peripheral to trigger ADC conversions
#define MASTER_TIM              TIM3
#define TIM_CLK_EN              __HAL_RCC_TIM3_CLK_ENABLE
#define ADC_EXTERNALTRIG        ADC_EXTERNALTRIGCONV_T3_TRGO
#define DMA_CLK_EN              __HAL_RCC_DMA2_CLK_ENABLE

// SPI for sending data to DDS
#define DDS_SPI                 SPI1
#define SPI_BAUDRATE            SPI_BAUDRATEPRESCALER_256 // 84MHz (APB2) / 256 = 328 KHz
#define SPI_CLK_EN              __HAL_RCC_SPI1_CLK_ENABLE
#define SPI_PIN_CLK_EN          __HAL_RCC_GPIOA_CLK_ENABLE(); __HAL_RCC_GPIOB_CLK_ENABLE
#define SPI_PIN_AF              GPIO_AF5_SPI1
#define DDS_MCLK                25000000UL

#define SPI_CLK_PIN                 GPIO_PIN_3
#define SPI_CLK_PIN_BUS             GPIOB

#define SPI_MOSI_PIN                GPIO_PIN_7
#define SPI_MOSI_PIN_BUS            GPIOA

#define SPI_FSYNC_PIN               GPIO_PIN_5
#define SPI_FSYNC_PIN_BUS           GPIOB

static SPI_HandleTypeDef hspi;
static TIM_HandleTypeDef htim;

static DMA_HandleTypeDef hdma_test;
static DMA_HandleTypeDef hdma_dut;

static ADC_HandleTypeDef hadc_test;
static ADC_HandleTypeDef hadc_dut;

static bool test_buf_full = false;
static bool dut_buf_full = false;

typedef struct {
    uint16_t prescaler;
    uint16_t period;
    uint32_t sample_time;
    uint32_t test_f;
} test_frequency_conf_t;

// Spi write function for ad9833
static void spi_write_ad9833(uint16_t val) {
    HAL_GPIO_WritePin(SPI_FSYNC_PIN_BUS, SPI_FSYNC_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi, (uint8_t*)(&val), 1, 10);
    delay_us(2);
    HAL_GPIO_WritePin(SPI_FSYNC_PIN_BUS, SPI_FSYNC_PIN, GPIO_PIN_SET);
    delay_us(2);
}

#define ad9833_p ((ad9833_t*)(&ad9833))
static const ad9833_t ad9833 = {
    .spi_write = spi_write_ad9833,
    .mclk_speed = DDS_MCLK,
};

// Get test frequency configuration for DDS and ADC trigger timer
// Assumes an 84MHz timer clock and 42 MHz ADC clock (after prescaler)
static test_frequency_conf_t get_tf_conf(test_frequency_t test_f) {
    test_frequency_conf_t conf;
    switch (test_f) {
        case TF_94KHZ:
            conf.prescaler = 0;
            conf.period = 55;
            conf.sample_time = ADC_SAMPLETIME_15CYCLES;
            conf.test_f = 93750;
            break;
        case TF_10KHZ:
            conf.prescaler = 4;
            conf.period = 104;
            conf.sample_time = ADC_SAMPLETIME_84CYCLES;
            conf.test_f = 10000;
            break;
        case TF_1KHZ:
            conf.prescaler = 49;
            conf.period = 104;
            conf.sample_time = ADC_SAMPLETIME_480CYCLES;
            conf.test_f = 1000;
            break;

        #if (DEBUG_SAMPLING == 1)
        case TF_DEBUG:
            conf.prescaler = 8399;
            conf.period = 999;
            conf.sample_time = ADC_SAMPLETIME_480CYCLES;
            conf.test_f = 10;
            break;
        #endif
    }
    return conf;
}

// This function initialises an ADC peripheral and DMA stream pair
// ADC and DMA will be setup to work together
//
// Before running this the ADC and DMA handle needs some config
// The Instances need to be set
// DMA channel needs to be set
// The ADC ExternalTrigConv needs to be seet
// Peripheral clocks need to be enabled
// Interrupts need to be enabled in the NVIC
// Pins need to be configured
//
// After running the ADC channel_cfg needs to be set
static void init_adc_dma(DMA_HandleTypeDef *hdma, ADC_HandleTypeDef *hadc) {

    // Config DMA
    hdma->Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma->Init.PeriphInc = DMA_PINC_DISABLE;
    hdma->Init.MemInc = DMA_MINC_ENABLE;
    hdma->Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD;
    hdma->Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    hdma->Init.Mode = DMA_NORMAL;
    hdma->Init.Priority = DMA_PRIORITY_MEDIUM;
    hdma->Init.FIFOMode = DMA_FIFOMODE_DISABLE;

    // Config ADC
    hadc->Init.Resolution = ADC_RESOLUTION_12B;
    hadc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc->Init.NbrOfConversion = 1;
    hadc->Init.ScanConvMode = DISABLE;
    hadc->Init.ContinuousConvMode = DISABLE;
    hadc->Init.DiscontinuousConvMode = DISABLE;
    hadc->Init.DMAContinuousRequests = ENABLE;

    error_handler_msg(HAL_DMA_Init(hdma), "Failed to init DMA");
    error_handler_msg(HAL_ADC_Init(hadc), "Failed to init ADC");
    __HAL_LINKDMA(hadc, DMA_Handle, *hdma);
}



// Initialises peripherals for reading samples
// TIM3
// ADC1, ADCx
// DMA2_Stream0, DMA2_Streamx
void init_sampling(void) {
    TIM_CLK_EN();
    TEST_ADC_CLK_EN();
    DUT_ADC_CLK_EN();
    DMA_CLK_EN();

    init_blocking_delay();

    // Timer init
    {
        // TIM3 is clocked by APB1 timer clock (84MHz)
        // Keep this in mind for prescaler and frequency calculations
        htim.Instance = MASTER_TIM;
        htim.Init.CounterMode = TIM_COUNTERMODE_UP;
        htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        htim.Channel = HAL_TIM_ACTIVE_CHANNEL_1;

        TIM_MasterConfigTypeDef master_cfg = {
            .MasterOutputTrigger = TIM_TRGO_UPDATE,
            .MasterSlaveMode =  TIM_MASTERSLAVEMODE_DISABLE,
        };

        // HAL_NVIC_SetPriority(TIM3_IRQn, 5, 5);
        // HAL_NVIC_EnableIRQ(TIM3_IRQn);
        error_handler_msg(HAL_TIMEx_MasterConfigSynchronization(&htim, &master_cfg), "Failed to config TIM master mode");
    }

    // SPI init
    {
        SPI_CLK_EN();
        SPI_PIN_CLK_EN();

        hspi.Instance = DDS_SPI;
        hspi.Init.Mode = SPI_MODE_MASTER;
        hspi.Init.Direction = SPI_DIRECTION_1LINE;
        hspi.Init.DataSize = SPI_DATASIZE_16BIT;
        hspi.Init.CLKPolarity = SPI_POLARITY_HIGH;
        hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
        hspi.Init.NSS = SPI_NSS_SOFT; // NSS is a chip select output, goes low during byte transfer
        hspi.Init.BaudRatePrescaler = SPI_BAUDRATE;
        hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
        hspi.Init.TIMode = SPI_TIMODE_DISABLE;
        hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        HAL_SPI_Init(&hspi);

        GPIO_InitTypeDef pin_cfg = {
            .Pin =  SPI_CLK_PIN,
            .Mode = GPIO_MODE_AF_PP,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_MEDIUM,
            .Alternate = SPI_PIN_AF,
        };
        HAL_GPIO_Init(SPI_CLK_PIN_BUS, &pin_cfg); // CLK
        pin_cfg.Pin = SPI_MOSI_PIN;
        HAL_GPIO_Init(SPI_MOSI_PIN_BUS, &pin_cfg); // MOSI 

        // Software NSS pin
        GPIO_InitTypeDef nss_pin_cfg = {
            .Pin =  SPI_FSYNC_PIN,
            .Mode = GPIO_MODE_OUTPUT_PP, 
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_MEDIUM,
            .Alternate = 0,
        };
        HAL_GPIO_Init(SPI_FSYNC_PIN_BUS, &nss_pin_cfg); 
        HAL_GPIO_WritePin(SPI_FSYNC_PIN_BUS, SPI_FSYNC_PIN, GPIO_PIN_SET); // Idle high
    }

    // ADC and DMA config
    {
        TEST_PIN_CLK_EN();
        DUT_PIN_CLK_EN();

        GPIO_InitTypeDef pin_cfg;
        pin_cfg.Pin = TEST_PIN;
        pin_cfg.Mode = GPIO_MODE_ANALOG;
        pin_cfg.Pull = GPIO_NOPULL;

        HAL_GPIO_Init(TEST_PIN_BUS, &pin_cfg);
        pin_cfg.Pin = DUT_PIN;
        HAL_GPIO_Init(DUT_PIN_BUS, &pin_cfg);

        hadc_test.Instance = TEST_ADC;
        hadc_test.Init.ExternalTrigConv = ADC_EXTERNALTRIG;
        hadc_dut.Instance = DUT_ADC;
        hadc_dut.Init.ExternalTrigConv = ADC_EXTERNALTRIG;

        hdma_test.Instance = TEST_DMA;
        hdma_test.Init.Channel = TEST_DMA_CHANNEL;
        HAL_NVIC_SetPriority(TEST_DMA_IRQn, 4, 1);
        HAL_NVIC_EnableIRQ(TEST_DMA_IRQn);

        hdma_dut.Instance = DUT_DMA;
        hdma_dut.Init.Channel = DUT_DMA_CHANNEL;
        HAL_NVIC_SetPriority(DUT_DMA_IRQn, 4, 2);
        HAL_NVIC_EnableIRQ(DUT_DMA_IRQn);

        init_adc_dma(&hdma_test, &hadc_test);
        init_adc_dma(&hdma_dut, &hadc_dut);
    }

    delay_us(1);
    init_ad9833(ad9833_p);
}

// Start sampling test and dut waveforms
// buf_elements should be given as the number of samples to be recorded in the buffers NOT byte length
void start_sampling(test_frequency_t test_f, uint32_t *test_buf, uint32_t *dut_buf, uint32_t buf_len) {
    test_frequency_conf_t conf = get_tf_conf(test_f);
    sample_buffers_full(); // Ensure buffer state variables are rest

    // Config channel
    ADC_ChannelConfTypeDef channel_cfg = {
        .Channel = TEST_ADC_CHANNEL,
        .Rank = 1,
        .SamplingTime = conf.sample_time, 
    };
    error_handler_msg(HAL_ADC_ConfigChannel(&hadc_test, &channel_cfg), "Failed to config test ADC channel");
    channel_cfg.Channel = DUT_ADC_CHANNEL;
    error_handler_msg(HAL_ADC_ConfigChannel(&hadc_dut, &channel_cfg), "Failed to config DUT ADC channel");

    // Config prescalers
    htim.Init.Prescaler = conf.prescaler;
    htim.Init.Period = conf.period;
    error_handler_msg(HAL_TIM_Base_Init(&htim), "Failed to init TIM");

    // Start DDS
    start_ad9833(ad9833_p, SINE_OUT, (float)conf.test_f, 0);
    delay_ms(10); // Alow system to stabilise before measuring

    // Start recording data
    error_handler_msg(HAL_ADC_Start_DMA(&hadc_test, test_buf, buf_len), "Failed to start test ADC DMA");
    error_handler_msg(HAL_ADC_Start_DMA(&hadc_dut, dut_buf, buf_len), "Failed to start DUT ADC DMA");
    error_handler_msg(HAL_TIM_Base_Start(&htim), "Failed to start TIM");
}

// Returns true when the sample buffers have been filled
// This function will return true once per buffer fill
bool sample_buffers_full(void) {
    if (test_buf_full & dut_buf_full) {
        test_buf_full = false;
        dut_buf_full = false;
        return true;
    }
    return false;
}

// Set buffer states and stop trigger timer and test waveform
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance == TEST_ADC) {
        test_buf_full = true;
    } else if (hadc->Instance == DUT_ADC) {
        dut_buf_full = true;
    }

    if (test_buf_full & dut_buf_full) {
        error_handler_msg(HAL_TIM_Base_Stop(&htim), "Failed to stop TIM");
        stop_ad9833(ad9833_p);
    }
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    (void) hadc;
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc) {
    (void) hadc;
    error_handler_msg(HAL_ERROR, "ADC ERROR");
}

extern void TEST_DMA_ISR(void) {
    HAL_DMA_IRQHandler(&hdma_test);
}

extern void DUT_DMA_ISR(void) {
    HAL_DMA_IRQHandler(&hdma_dut);
}

// extern void DMA2_Stream0_IRQHandler(void) {
//     HAL_DMA_IRQHandler(&hdma_test);
// }

// extern void DMA2_Stream2_IRQHandler(void) {
//     HAL_DMA_IRQHandler(&hdma_dut);
// }


// extern void TIM3_IRQHandler() {
//     uint32_t itflag = (&htim3)->Instance->SR;
//     static uint32_t counter = 0;

//     if ((itflag & TIM_FLAG_UPDATE) == TIM_FLAG_UPDATE) {
//         // Run every time an ADC read is triggered
//         counter++;

//         if (counter == 50) { // Divide interrupt rate by 5
//             // error_handler_msg(HAL_TIM_Base_Stop(&htim3), "Failed to stop TIM");
//             // HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
//             // count = 0;
//         }

//         __HAL_TIM_CLEAR_FLAG(&htim3, TIM_FLAG_UPDATE);
//     }
// }
