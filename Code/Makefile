CC = arm-none-eabi-gcc
CFLAGS  ?=  -Wall -Werror -Wshadow -Wdouble-promotion \
            -Wformat-truncation -fno-common \
            -g3 -Os -ffunction-sections -fdata-sections \
            -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -MMD -MP
LDFLAGS ?= -Tlink.ld -nostartfiles -nostdlib --specs nano.specs \
			-lc -lgcc -lm -Wl,--gc-sections -Wl,-Map=$@.map

DEFINES := \
	-DSTM32G431xx
CFLAGS += $(DEFINES)

# Add application specific user libraries here
# Sub folders within the library will be searched too
# If a library has seperate src and inc folders put both of them here
LIBRARIES := \
	main_csrc \
	submodules/embedded_c_modules \
	submodules/stm32g4xx-hal-driver/Src \
	submodules/stm32g4xx-hal-driver/Inc \
	submodules/CMSIS_6/CMSIS/Core/Include \
	submodules/cmsis-device-g4/Include \

INCLUDES := -I. \
	$(foreach lib,$(LIBRARIES),$(addprefix -I,$(lib))) \
	$(foreach lib,$(LIBRARIES),$(addprefix -I,$(wildcard ./$(lib)/*/)))
CFLAGS += $(INCLUDES)

# CMSIS sources
S_SOURCES := submodules/cmsis-device-g4/Source/Templates/gcc/startup_stm32g431xx.s
C_SOURCES := submodules/cmsis-device-g4/Source/Templates/system_stm32g4xx.c

# Add application code and library sources
C_SOURCES += $(shell find $(LIBRARIES) -type f -name '*.c')
S_SOURCES += $(shell find $(LIBRARIES) -type f -name '*.s')

# Filter out sources that aren't needed, or don't play nicely with other sources 
SOURCE_EXCLUDES := \
	submodules/stm32f4xx-hal-driver/Src/stm32fgxx_hal_msp_template.c \
	submodules/stm32f4xx-hal-driver/Src/stm32f4xx_hal_timebase_tim_template.c
C_SOURCES := $(filter-out $(SOURCE_EXCLUDES), $(C_SOURCES))

OBJECTS := $(addprefix target/,$(C_SOURCES:.c=.o))
OBJECTS += $(addprefix target/,$(S_SOURCES:.s=.o))
DEPENDS := $(OBJECTS:.o=.d)

build: target/firmware.elf

target:
	mkdir target

target/%.o: %.c | target
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

target/%.o: %.s | target
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

target/firmware.elf: $(OBJECTS) 
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) 

target/firmware.bin: target/firmware.elf
	arm-none-eabi-objcopy -O binary $< $@

flash: target/firmware.bin submodules
	st-flash --reset write $< 0x8000000

clean:
	rm -rf target 

rmsub:
	rm -rf submodules

-include $(DEPENDS)
