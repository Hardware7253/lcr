// Buttons module
// Alows for buttons to be polled, and for events to be read later
// This allows buttons to be polled at a high frequency, then the events can be read at a low frequency
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "button_debounce_cfg.h"
#include "button_debouncing.h"

typedef enum {
    BUTTON_1 = 0,
    NO_BUTTONS
} button_t;

void init_buttons(void);
void poll_buttons(void);

bool is_button_pressed(button_t button);
bool is_button_just_pressed(button_t button);
bool is_button_long_pressed(button_t button);
bool is_button_just_long_pressed(button_t button);

#if ENABLE_BUTTON_COUNTING
uint8_t get_button_consecutive_presses(button_t button);
#endif
