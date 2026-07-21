#include "buttons.h"
#include "stm32g4xx_hal.h"

typedef struct {
    btn_debounce_t debounce_data;
    bool active_low;
    uint16_t pin;
    GPIO_TypeDef* bus;
} button_data_t;

static button_data_t buttons_array[NO_BUTTONS] = {0};

// Init the button pins and internal button data
void init_buttons(void) {
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // BUTTON_1 
    buttons_array[BUTTON_1].pin = GPIO_PIN_6;
    buttons_array[BUTTON_1].bus = GPIOB;
    buttons_array[BUTTON_1].active_low = true;
    GPIO_InitTypeDef btn_cfg = {
        .Pin = buttons_array[BUTTON_1].pin,
        .Mode = GPIO_MODE_INPUT,
        .Pull = GPIO_PULLUP,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };
    HAL_GPIO_Init(buttons_array[BUTTON_1].bus, &btn_cfg); 
}

// Poll every button for new presses
void poll_buttons(void) {
    for (uint8_t i = 0; i < NO_BUTTONS; i++) {
        button_data_t* this_button = &buttons_array[i];
        bool button_state = (bool)HAL_GPIO_ReadPin(this_button->bus, this_button->pin) ^ this_button->active_low; 
        btn_debounce_update(&this_button->debounce_data, (uint8_t)button_state, HAL_GetTick());
    }
}

// Gets button data for the given button using the getter function
uint8_t get_button_data(button_t button, btn_getter btn_getter_fn) {
    return btn_getter_fn(&buttons_array[button].debounce_data, HAL_GetTick());
}