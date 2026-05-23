#pragma once

#include "stm32g4xx_hal_def.h"
#include <stdint.h>

// General prupose error and ok to be used with the error handler functions
#define GP_ERROR    HAL_ERROR
#define GP_OK       HAL_OK

void init_clocks(void);
void error_handler(HAL_StatusTypeDef status);
void error_handler_msg(HAL_StatusTypeDef status, volatile char* msg);
uint32_t get_tick_frequency(void);