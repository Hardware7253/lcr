#include <string.h>
#include "sampling.h"
#include "stm32g4xx_hal.h"
#include "system.h"
#include "blocking_delay.h"
#include "ad9833.h"

#define DMA_CLK_EN              __HAL_RCC_DMA1_CLK_ENABLE(); __HAL_RCC_DMAMUX1_CLK_ENABLE

// ADC for measuring test waveform
#define TEST_ADC                ADC1
#define TEST_ADC_CLK_EN         __HAL_RCC_ADC12_CLK_ENABLE
#define TEST_ADC_CHANNEL        ADC_CHANNEL_15
#define TEST_DMA_REQUEST        DMA_REQUEST_ADC1
#define TEST_DMA_INST           DMA1_Channel1
#define TEST_DMA_IRQn           DMA1_Channel1_IRQn 
#define TEST_DMA_ISR            DMA1_Channel1_IRQHandler

// ADC for measuring current waveform
#define CURR_ADC                 ADC2 
#define CURR_ADC_CLK_EN          __HAL_RCC_ADC12_CLK_ENABLE
#define CURR_ADC_CHANNEL         ADC_CHANNEL_13
#define CURR_DMA_REQUEST         DMA_REQUEST_ADC2
#define CURR_DMA_INST            DMA1_Channel2 
#define CURR_DMA_IRQn            DMA1_Channel2_IRQn
#define CURR_DMA_ISR             DMA1_Channel2_IRQHandler

#define TEST_PIN                 GPIO_PIN_0
#define TEST_PIN_BUS             GPIOB
#define TEST_PIN_CLK_EN          __HAL_RCC_GPIOB_CLK_ENABLE 

#define CURR_PIN                 GPIO_PIN_5
#define CURR_PIN_BUS             GPIOA
#define CURR_PIN_CLK_EN          __HAL_RCC_GPIOA_CLK_ENABLE 

// Timer peripheral to trigger ADC conversions
#define TIM_INST                TIM3
#define TIM_CLK_EN              __HAL_RCC_TIM3_CLK_ENABLE
#define ADC_EXTERNALTRIG        ADC_EXTERNALTRIG_T3_TRGO

// SPI for sending data to DDS
#define SPI_INST                SPI2
#define SPI_BAUDRATE            SPI_BAUDRATEPRESCALER_256
#define SPI_CLK_EN              __HAL_RCC_SPI2_CLK_ENABLE
#define SPI_PIN_CLK_EN          __HAL_RCC_GPIOB_CLK_ENABLE
#define SPI_PIN_AF              GPIO_AF5_SPI2

#define SPI_CLK_PIN             GPIO_PIN_13
#define SPI_CLK_PIN_BUS         GPIOB

#define SPI_MOSI_PIN            GPIO_PIN_15
#define SPI_MOSI_PIN_BUS        GPIOB

#define SPI_FSYNC_PIN           GPIO_PIN_12
#define SPI_FSYNC_PIN_BUS       GPIOB

#define TIM_BASE_CLK            104000000UL
#define DDS_MCLK                25000000UL

static SPI_HandleTypeDef hspi;
static TIM_HandleTypeDef htim;

static DMA_HandleTypeDef hdma_test;
static DMA_HandleTypeDef hdma_curr;

static ADC_HandleTypeDef hadc_test;
static ADC_HandleTypeDef hadc_curr;

static bool test_buf_full = false;
static bool dut_buf_full = false;

// Struct for setting up timer period for ADC read and ADC sample times
typedef struct {
    uint16_t prescaler;
    uint16_t period;
    uint32_t adc_sample_time;
    uint32_t test_f;
} test_frequency_conf_t;


// Macro for initialising test_frequency_conf_t struct
// TIM trigger rate is SAMPLES_PER_PERIOD * test_freq,
// so the provided prescaler frequency must be a multiple of SAMPLES_PER_PERIOD * test_freq
#define TEST_CONFIG(prescaler_freq, test_freq, sample_time) \
{ \
    .prescaler = __HAL_TIM_CALC_PSC(TIM_BASE_CLK, prescaler_freq), \
    .period = __HAL_TIM_CALC_PERIOD( \
        TIM_BASE_CLK, \
        __HAL_TIM_CALC_PSC(TIM_BASE_CLK, prescaler_freq), \
        (test_freq * SAMPLES_PER_PERIOD)), \
    .adc_sample_time = (sample_time), \
    .test_f = (test_freq), \
}

// Test frequency configurations for the ADC trigger timer and DDS
// Indexed by the value of a test_frequency_t enum
static const test_frequency_conf_t TEST_CONFIGS[NO_OF_TEST_FREQUENCIES] = {
    #define PRESCALER_HZ 1600000 // 1.6 MHz (easily divisible to get test_freq * 16 trigger rates)

    [TF_100KHZ] = TEST_CONFIG(20800000, 100000, ADC_SAMPLETIME_12CYCLES_5),
    [TF_10KHZ] = TEST_CONFIG(PRESCALER_HZ, 10000, ADC_SAMPLETIME_92CYCLES_5),
    [TF_1KHZ] = TEST_CONFIG(PRESCALER_HZ, 1000, ADC_SAMPLETIME_247CYCLES_5),
    [TF_100HZ] = TEST_CONFIG(PRESCALER_HZ, 100, ADC_SAMPLETIME_640CYCLES_5),
};

// Spi write function for ad9833
static void spi_write_ad9833(uint16_t val) {
    HAL_GPIO_WritePin(SPI_FSYNC_PIN_BUS, SPI_FSYNC_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi, (uint8_t*)(&val), 1, 10);
    delay_us(2);
    HAL_GPIO_WritePin(SPI_FSYNC_PIN_BUS, SPI_FSYNC_PIN, GPIO_PIN_SET);
    delay_us(2);
}

static const ad9833_t AD9833 = {
    .spi_write = spi_write_ad9833,
    .mclk_speed = DDS_MCLK,
};


// Initialises last common settings for ADC and DMA then inits and links ADC and DMA
// Assumes other required config is already done
static void init_adc_dma(DMA_HandleTypeDef *hdma, ADC_HandleTypeDef *hadc) {

    // Config DMA
    hdma->Init.Direction = DMA_PERIPH_TO_MEMORY;
    hdma->Init.PeriphInc = DMA_PINC_DISABLE;
    hdma->Init.MemInc = DMA_MINC_ENABLE;
    hdma->Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma->Init.MemDataAlignment = DMA_MDATAALIGN_WORD;
    hdma->Init.Mode = DMA_NORMAL;
    hdma->Init.Priority = DMA_PRIORITY_MEDIUM;

    // Config ADC
    hadc->Init.Resolution = ADC_RESOLUTION_12B;
    hadc->Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    hadc->Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc->Init.GainCompensation = 0;
    hadc->Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc->Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc->Init.LowPowerAutoWait = DISABLE;
    hadc->Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    hadc->Init.NbrOfConversion = 1;
    hadc->Init.ScanConvMode = DISABLE;
    hadc->Init.ContinuousConvMode = DISABLE;
    hadc->Init.DiscontinuousConvMode = DISABLE;
    hadc->Init.DMAContinuousRequests = DISABLE;
    hadc->Init.Overrun = ADC_OVR_DATA_PRESERVED; 
    hadc->Init.OversamplingMode = DISABLE;

    error_handler_msg(HAL_DMA_Init(hdma), "Failed to init DMA");
    error_handler_msg(HAL_ADC_Init(hadc), "Failed to init ADC");

    // Use ADC independent mode
    ADC_MultiModeTypeDef mm_cfg = {
        .Mode = ADC_MODE_INDEPENDENT,
    };

    // Ignore HAL_ERROR, because an error is returned for ADC2 since it would be a slave in multimode
    HAL_ADCEx_MultiModeConfigChannel(hadc, &mm_cfg); 

    __HAL_LINKDMA(hadc, DMA_Handle, *hdma);
}

// Initialises peripherals for reading samples
void init_sampling(void) {
    TIM_CLK_EN();
    TEST_ADC_CLK_EN();
    CURR_ADC_CLK_EN();
    DMA_CLK_EN();

    init_blocking_delay();

    // Timer init
    {
        // TIM3 is clocked by APB1 timer clock
        // Keep this in mind for prescaler and frequency calculations
        htim.Instance = TIM_INST;
        htim.Init.CounterMode = TIM_COUNTERMODE_UP;
        htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

        // Use any numbers for init
        htim.Init.Prescaler = 100;
        htim.Init.Period = 100;

        TIM_MasterConfigTypeDef master_cfg = {
            .MasterOutputTrigger = TIM_TRGO_UPDATE,
            .MasterSlaveMode =  TIM_MASTERSLAVEMODE_DISABLE,
        };

        TIM_ClockConfigTypeDef clk_cfg = {
            .ClockSource = TIM_CLOCKSOURCE_INTERNAL,
        };

        error_handler_msg(HAL_TIM_Base_Init(&htim), "Failed to init TIM");
        error_handler_msg(HAL_TIM_ConfigClockSource(&htim, &clk_cfg), "Failed to config TIM clock source");
        error_handler_msg(HAL_TIMEx_MasterConfigSynchronization(&htim, &master_cfg), "Failed to config TIM master mode");
    }

    // SPI init
    {
        SPI_CLK_EN();
        SPI_PIN_CLK_EN();

        hspi.Instance = SPI_INST;
        hspi.Init.Mode = SPI_MODE_MASTER;
        hspi.Init.Direction = SPI_DIRECTION_1LINE;
        hspi.Init.DataSize = SPI_DATASIZE_16BIT;
        hspi.Init.CLKPolarity = SPI_POLARITY_HIGH;
        hspi.Init.CLKPhase = SPI_PHASE_1EDGE;
        hspi.Init.NSS = SPI_NSS_SOFT; 
        hspi.Init.BaudRatePrescaler = SPI_BAUDRATE;
        hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;
        hspi.Init.TIMode = SPI_TIMODE_DISABLE;
        hspi.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
        hspi.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
        HAL_SPI_Init(&hspi);

        GPIO_InitTypeDef pin_cfg = {
            .Pin = SPI_CLK_PIN,
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
        CURR_PIN_CLK_EN();

        // Enable internal voltage reference
        __HAL_RCC_SYSCFG_CLK_ENABLE();
        HAL_SYSCFG_VREFBUF_VoltageScalingConfig(SYSCFG_VREFBUF_VOLTAGE_SCALE2);
        HAL_SYSCFG_VREFBUF_HighImpedanceConfig(SYSCFG_VREFBUF_HIGH_IMPEDANCE_DISABLE);
        error_handler_msg(HAL_SYSCFG_EnableVREFBUF(), "Failed to enable VREFBUF");

        GPIO_InitTypeDef pin_cfg = {
            .Pin =  TEST_PIN,
            .Mode = GPIO_MODE_ANALOG, 
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_MEDIUM,
            .Alternate = 0,
        };

        HAL_GPIO_Init(TEST_PIN_BUS, &pin_cfg);
        pin_cfg.Pin = CURR_PIN;
        HAL_GPIO_Init(CURR_PIN_BUS, &pin_cfg);

        hadc_test.Instance = TEST_ADC;
        hadc_test.Init.ExternalTrigConv = ADC_EXTERNALTRIG;
        hadc_curr.Instance = CURR_ADC;
        hadc_curr.Init.ExternalTrigConv = ADC_EXTERNALTRIG;

        hdma_test.Instance = TEST_DMA_INST;
        hdma_test.Init.Request = TEST_DMA_REQUEST;
        HAL_NVIC_SetPriority(TEST_DMA_IRQn, 4, 1);
        HAL_NVIC_EnableIRQ(TEST_DMA_IRQn);

        hdma_curr.Instance = CURR_DMA_INST;
        hdma_curr.Init.Request = CURR_DMA_REQUEST;
        HAL_NVIC_SetPriority(CURR_DMA_IRQn, 4, 2);
        HAL_NVIC_EnableIRQ(CURR_DMA_IRQn);

        init_adc_dma(&hdma_test, &hadc_test);
        init_adc_dma(&hdma_curr, &hadc_curr);

        error_handler_msg(HAL_ADCEx_Calibration_Start(&hadc_test, ADC_SINGLE_ENDED), "Failed to calibrate ADC");
        error_handler_msg(HAL_ADCEx_Calibration_Start(&hadc_curr, ADC_SINGLE_ENDED), "Failed to calibrate ADC");
    }

    delay_us(1);
    init_ad9833(&AD9833);
}

// Start sampling test and dut waveforms
// buf_elements should be given as the number of samples to be recorded in the buffers NOT byte length
void start_sampling(test_frequency_t test_f, uint32_t *test_buf, uint32_t *dut_buf, uint32_t buf_len) {
    const test_frequency_conf_t *conf = &TEST_CONFIGS[test_f];
    sample_buffers_full(); // Ensure buffer state variables are rest

    // Config channel
    ADC_ChannelConfTypeDef channel_cfg = {
        .Channel = TEST_ADC_CHANNEL,
        .Rank = ADC_REGULAR_RANK_1,
        .SamplingTime = conf->adc_sample_time, 
        .SingleDiff = ADC_SINGLE_ENDED,
        .OffsetNumber = ADC_OFFSET_NONE,
        .Offset = 0,
        .OffsetSign = 0,
        .OffsetSaturation = DISABLE,
    };
    error_handler_msg(HAL_ADC_ConfigChannel(&hadc_test, &channel_cfg), "Failed to config test ADC channel");
    channel_cfg.Channel = CURR_ADC_CHANNEL;
    error_handler_msg(HAL_ADC_ConfigChannel(&hadc_curr, &channel_cfg), "Failed to config current ADC channel");

    // Config prescalers
    htim.Init.Prescaler = conf->prescaler;
    htim.Init.Period = conf->period;
    error_handler_msg(HAL_TIM_Base_Init(&htim), "Failed to init TIM");

    ad9833_cfg_t dds_cfg = {
        .mode = SINE_OUT,
        .frequency = (float)conf->test_f,
        .phase = 0.0,
    };

    // Start DDS
    start_ad9833(&AD9833, &dds_cfg);
    delay_ms(10); // Alow system to stabilise before measuring

    // Start recording data
    error_handler_msg(HAL_ADC_Start_DMA(&hadc_test, test_buf, buf_len), "Failed to start test ADC DMA");
    error_handler_msg(HAL_ADC_Start_DMA(&hadc_curr, dut_buf, buf_len), "Failed to start current ADC DMA");
    error_handler_msg(HAL_TIM_Base_Start(&htim), "Failed to start TIM");
}

// Returns true when the sample buffers have been filled
// This function will return true once per buffer fill
bool sample_buffers_full(void) {
    if (test_buf_full && dut_buf_full) {
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
    } else if (hadc->Instance == CURR_ADC) {
        dut_buf_full = true;
    }

    if (test_buf_full && dut_buf_full) {
        error_handler_msg(HAL_TIM_Base_Stop(&htim), "Failed to stop TIM");
        stop_ad9833(&AD9833);
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

extern void CURR_DMA_ISR(void) {
    HAL_DMA_IRQHandler(&hdma_curr);
}