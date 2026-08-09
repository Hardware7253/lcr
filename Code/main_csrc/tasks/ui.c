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

static polar_t dut_z = {0.0, 0.0};
static range_status_t range_status = GOOD_RANGE;
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

// Returns a pointer to a string containing the unit prefix
// This makes it so no blank char is inserted when the unit prefix is a space (ie base units)
static char* get_unit_prefix_string(char unit_prefix) {
    static char string[2] = {0};

    if (unit_prefix != ' ') {
        string[0] = unit_prefix;
        string[1] = '\0';
    } else {
        string[0] = '\0';
    }

    return string;
}

// Reset the lines array to all zeros
static inline void reset_lines(void) {
    memset(lines, 0, sizeof(lines));
}

// Updates display with newest measurement data
// Function parameters are used to show message screens
static void update_display(bool overwrite_message, const char* message) {
    reset_lines();

    // Add test frequency to bottom line
    // Show test frequency and a warning for a bad range on the bottom line
    float test_frequency_hz = (float)get_test_frequency_hz(test_frequency);
    char warning_char = (range_status == BAD_RANGE) ? '!' : ' ';
    eng_float_t tf_eng_val = convert_to_eng_notation(test_frequency_hz);
    npf_snprintf(
        lines[NO_LINES - 1],
        CHARS_PER_LINE,
        "TF = %d %sHz   %c",
        (int)tf_eng_val.val,
        get_unit_prefix_string(tf_eng_val.unit_prefix),
        warning_char
    );

    // Show overwrite message
    if (overwrite_message) {
        npf_snprintf(lines[0], CHARS_PER_LINE, message);
        write_lines();
        return;
    }

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
        "Z=%.3f<%.2f* %s^",
        dut_z_mag_eng_val.val,
        rad_to_degrees(dut_z.angle),
        get_unit_prefix_string(dut_z_mag_eng_val.unit_prefix)
    );

    // Print impedance real part
    eng_float_t dut_r_eng_val = convert_to_eng_notation(dut.impedance.real);
    npf_snprintf(lines[1], CHARS_PER_LINE, "R=%.2f %c^", dut_r_eng_val.val, dut_r_eng_val.unit_prefix);

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
            "%c=%.2f %s%c",
            symbol,
            reactive_component_eng_val.val,
            get_unit_prefix_string(reactive_component_eng_val.unit_prefix),
            unit
        );

        // Print dissipation factor
        npf_snprintf(lines[3], CHARS_PER_LINE, "D=%.3f", dut.impedance.real / fabsf(dut.impedance.imaginary));
    } else {

        // Show resistor stray reactance
        eng_float_t dut_x_eng_val = convert_to_eng_notation(dut.impedance.imaginary);
        npf_snprintf(
            lines[2],
            CHARS_PER_LINE,
            "X=%.3f %s^",
            dut_x_eng_val.val,
            get_unit_prefix_string(dut_x_eng_val.unit_prefix)
        );
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
    update_display(true, "No measurement");
}

void run_ui(void) {

    // Measure
    if (get_button_data(BUTTON_1, btn_just_n_released) == 2) {
        update_display(true, "Measuring...");
        delay_ms(50);
        range_status = measure_dut(&dut_z, test_frequency);
        update_display(false, "");
    }

    // Change test frequency
    if (get_button_data(BUTTON_1, btn_just_released)) {
        test_frequency++;
        if (test_frequency >= NO_OF_TEST_FREQUENCIES) {
            test_frequency = 0;
        }

        // Last measurement is now invalid, so show no measurement message
        update_display(true, "No measurement");
    }
}