#include "ui.h"

#include "blocking_delay.h"
#include "lcr_math.h"
#include "math_helpers.h"
#include <math.h>

#include "dut_measure.h"
#include "display.h"
#include "custom_fonts.h"

#include "buttons.h"

#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

#include <stdio.h>
#include <string.h>


#define TOP_PAD 10
#define LEFT_PAD 4

#define FONT lcr_font_6x8 
#define LINE_SPACING 10

#define CHARS_PER_LINE 21 
#define NO_LINES 6

static char lines[NO_LINES][CHARS_PER_LINE + 1] = {0};

static polar_t dut_z = {0};
static range_status_t range_status;
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
    memset(lines, (int)' ', sizeof(lines));

    // Add test frequency to bottom line
    // Show test frequency and a warning for a bad range on the bottom line
    float test_frequency_hz = (float)get_test_frequency_hz(test_frequency);
    char warning_char = (range_status == BAD_RANGE) ? '!' : ' ';
    eng_float_t tf_eng_val = convert_to_eng_notation(test_frequency_hz);
    npf_snprintf(
        lines[NO_LINES - 1],
        CHARS_PER_LINE,
        "TF = %d %cHz   %c",
        (int)tf_eng_val.val,
        tf_eng_val.unit_prefix,
        warning_char
    );

    // Show error for out of range
    if (range_status == NO_RANGE) {
        npf_snprintf(lines[0], CHARS_PER_LINE, "Out of range,");
        npf_snprintf(lines[1], CHARS_PER_LINE, "try a different");
        npf_snprintf(lines[2], CHARS_PER_LINE, "test frequency.");
        write_lines();
        return;
    }

    // Print impedance in polar form
    passive_component_t dut = calc_passive_component(&dut_z, test_frequency_hz);
    eng_float_t dut_z_mag_eng_val = convert_to_eng_notation(dut_z.mag);
    npf_snprintf(
        lines[0],
        CHARS_PER_LINE,
        "Z = %.2f<%.2f* %c^",
        dut_z_mag_eng_val.val,
        rad_to_degrees(dut_z.angle),
        dut_z_mag_eng_val.unit_prefix
    );

    // Print impedance real part
    eng_float_t dut_r_eng_val = convert_to_eng_notation(dut.impedance.real);
    npf_snprintf(lines[1], CHARS_PER_LINE, "R = %.3f %c^", dut_r_eng_val.val, dut_r_eng_val.unit_prefix);

    if (dut.passive_type != RESISTOR) {
        char symbol = 'C';
        char unit = 'F';

        if (dut.passive_type == INDUCTOR) {
            symbol = 'L';
            unit = 'H';
        }

        // Print inductance / capacitance
        eng_float_t reactive_component_eng_val = convert_to_eng_notation(dut.reactive_component_val);
        npf_snprintf(
            lines[2],
            CHARS_PER_LINE,
            "%c = %.3f %c%c",
            symbol,
            reactive_component_eng_val.val,
            reactive_component_eng_val.unit_prefix,
            unit
        );

        // Print dissipation factor
        npf_snprintf(lines[3], CHARS_PER_LINE, "D = %.3f", dut.impedance.real / fabsf(dut.impedance.imaginary));
    } else {
        eng_float_t dut_x_eng_val = convert_to_eng_notation(dut.impedance.imaginary);
        npf_snprintf(lines[2], CHARS_PER_LINE, "X = %.3f %c^", dut_x_eng_val.val, dut_x_eng_val.unit_prefix);
    }

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

    // Test graphics
    // // range_status = NO_RANGE;
    // // range_status = BAD_RANGE;
    // range_status = GOOD_RANGE;

    // dut_z.mag = 1.668;
    // dut_z.angle = -1.2566;

    // // dut_z.mag = 511356;
    // // dut_z.angle = 1.2566;

    // // dut_z.mag = 1235;
    // // dut_z.angle = 0.0542;

    // update_display();
    // return;

    // Measure
    if (is_button_just_pressed(BUTTON_1)) {
        range_status = measure_dut(&dut_z, test_frequency);
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