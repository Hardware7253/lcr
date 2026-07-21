// Buttons module
// Alows for buttons to be polled, and for events to be read later
// This allows buttons to be polled at a high frequency, then the events can be read at a low frequency
#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "btn_debounce.h"

typedef enum {
    BUTTON_1 = 0,
    NO_BUTTONS
} button_t;

void init_buttons(void);
void poll_buttons(void);
uint8_t get_button_data(button_t button, btn_getter btn_getter);