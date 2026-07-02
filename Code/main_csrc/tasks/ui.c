#include "ui.h"

#include "blocking_delay.h"
#include "lcr_math.h"
#include "math_helpers.h"

#include "dut_measure.h"
#include "display.h"

#include <stdio.h>
#include <string.h>

#include "buttons.h"

#define TOP_PAD 4
#define LEFT_PAD 4

#define FONT u8g2_font_6x10_mr
#define LINE_SPACING 

#define CHARS_PER_LINE 20
#define NO_LINES 5

static char lines[NO_LINES][CHARS_PER_LINE + 1] = {0};

static polar_t dut_z = {0};
static test_frequency_t test_frequency = TF_10KHZ;

static u8g2_t display;

// Write the lines from the lines array to the display 
static void write_lines() {
    u8g2_ClearBuffer(&display);
    u8g2_SetFont(&display, FONT);

    for (uint16_t i = 0; i < NO_LINES; i++) {
        u8g2_DrawStr(&display, LEFT_PAD, i * LINE_SPACING + TOP_PAD, lines[i]);
    }

    u8g2_SendBuffer(&display);
}

// Updates display with newest measurement data
static void update_display(void) {
    float test_frequency_hz = (float)get_test_frequency_hz(test_frequency);
    passive_component_t dut = calc_passive_component(&dut_z, test_frequency_hz);

    // Print impedance in polar form
    sprintf(lines[0], "Z = %.3f / %.3fdeg", dut_z.mag, rad_to_degrees(dut_z.angle));

    if (dut.passive_type != RESISTOR) {

        // Print inductance / capacitance
        eng_float_t reactive_component_eng_val = convert_to_eng_notation(dut.reactive_component_val);
        sprintf(lines[1], "C = %.3f%c", reactive_component_eng_val.val, reactive_component_eng_val.unit_prefix);
        if (dut.passive_type == INDUCTOR) {
            lines[1][0] = 'L';
        }

        // Print ESR / DCR and dissipation factor
        sprintf(lines[2], "R = %.3f", dut.impedance.real);
        sprintf(lines[3], "D = %.3f", 0.0); // Placeholder
    } else {
        sprintf(lines[1], "Z = %.3f + %.3fj", dut.impedance.real, dut.impedance.imaginary);
    }

    // Add test frequency to bottom line
    eng_float_t tf_eng_val = convert_to_eng_notation(test_frequency_hz);
    sprintf(lines[NO_LINES - 1], "TF=%.0f%cHz", tf_eng_val.val, tf_eng_val.unit_prefix);

    write_lines();
}

// Initiliase display and DUT measuring
void init_ui(void) {
    init_dut_measurement();

    // Wait to ensure display is reset by RC circuit before communication
    delay_ms(25);

    init_display(&display, U8G2_R2);
    u8g2_ClearBuffer(&display);
}

void run_ui(void) {

    // Measure
    if (is_button_just_pressed(BUTTON_1)) {
        dut_z = measure_dut(test_frequency);
        update_display();
    }

    // Change test frequency
    if (get_button_consecutive_presses(BUTTON_1) == 2) {
        test_frequency++;
        if (test_frequency >= NO_OF_TEST_FREQUENCIES) {
            test_frequency = 0;
        }

        // Last measurement is now invalid, so reset the display
        dut_z.angle = 0.0;
        dut_z.mag = 0.0;
        update_display();
    }
}