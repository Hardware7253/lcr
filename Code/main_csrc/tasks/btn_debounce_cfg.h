#pragma once

// A press won't be registered until this time has passed since the falling edge
#define BUTTON_DEBOUNCE_MS 50

// Button needs to be pressed for this long to register a long press
#define BUTTON_LONG_PRESS_MS 700

// Minimum time between button presses to register a consecutive press
#define BUTTON_CONSECUTIVE_PRESS_MS 250