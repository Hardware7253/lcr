#include "buttons.h"
#include "button_debouncing.h"
#include "stm32g4xx_hal.h"

typedef struct {
    button_debounce_t debounce_data;
    bool active_low;
    bool just_pressed;
    bool just_long_pressed;
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
        bool button_state = HAL_GPIO_ReadPin(this_button->bus, this_button->pin) ^ this_button->active_low; 
        this_button->just_pressed = button_pressed(&this_button->debounce_data, button_state, HAL_GetTick());
        this_button->just_long_pressed = button_long_pressed(&this_button->debounce_data, button_state, HAL_GetTick());
    }
}

// Returns true if the given button is pressed (with debouncing)
bool is_button_pressed(button_t button) {
    return buttons_array[button].debounce_data.state;
}

// Returns true if the given button state just transitioned from not pressed to pressed (with debouncing)
bool is_button_just_pressed(button_t button) {
    bool return_value = buttons_array[button].just_pressed;
    buttons_array[button].just_pressed = false;
    return return_value;
}

// Returns true if the given button is registering a long press
bool is_button_long_pressed(button_t button) {
    return buttons_array[button].debounce_data.long_press_registered;
}

// Returns true if the given button long press state just transitioned from false to true
bool is_button_just_long_pressed(button_t button) {
    bool return_value = buttons_array[button].just_long_pressed;
    buttons_array[button].just_long_pressed = false;
    return return_value;
}

#if ENABLE_BUTTON_COUNTING
// Returns the current number of consecutive presses a button has registered
uint8_t get_button_consecutive_presses(button_t button) {
    return buttons_array[button].debounce_data.consecutive_presses;
}
#endif