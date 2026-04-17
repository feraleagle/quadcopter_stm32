# ----------------------------
# Project Settings
# ----------------------------
PROJECT = firmware
TARGET  = $(PROJECT).elf
BIN     = $(PROJECT).bin
HEX     = $(PROJECT).hex

# ----------------------------
# Toolchain
# ----------------------------
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
CXX     = $(PREFIX)g++
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# ----------------------------
# MCU Settings
# ----------------------------
MCU       = cortex-m4
FPU       = fpv4-sp-d16
FLOAT_ABI = hard

# ----------------------------
# Directories
# ----------------------------
DRIVERS_CMSIS_DIR  = drivers/cmsis
DRIVERS_ST_DIR     = drivers/stm32f4xx
NO_OS_DIR          = core/src
BUILD_DIR          = build


# ----------------------------
# Compiler Flags
# ----------------------------
CFLAGS = -mcpu=$(MCU) -mthumb -mfpu=$(FPU) -mfloat-abi=$(FLOAT_ABI) \
         -Og -g3 -Wall -fdata-sections -ffunction-sections \
         -DSTM32F411xE -D__FPU_PRESENT=1 -D__FPU_USED=1 \
         -I$(DRIVERS_ST_DIR) \
         -I$(DRIVERS_CMSIS_DIR) \
         -I$(NO_OS_DIR) \
         -Icore/inc \

LDFLAGS = -mcpu=$(MCU) -mthumb -mfpu=$(FPU) -mfloat-abi=$(FLOAT_ABI) \
          -Wl,--gc-sections -Tstartup/STM32F411CEU6_FLASH.ld \
          -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map \
          -specs=nano.specs \
          -Wl,--no-warn-rwx-segments

# ----------------------------
# Source Files
# ----------------------------
STARTUP_FILE = startup/startup_stm32f411xe.s

APP_SRC = application/main.cpp
SYSTEM_SRC = $(NO_OS_DIR)/segger_fault_handler.c \
             $(NO_OS_DIR)/syscalls.c \
             $(NO_OS_DIR)/sysmem.c \
             $(DRIVERS_ST_DIR)/system_stm32f4xx.c \
             $(STARTUP_FILE)

# ----------------------------
# Build Object Files
# ----------------------------
OBJ_APP = $(BUILD_DIR)/main.o
OBJ_SYSTEM = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SYSTEM_SRC))

OBJ_FILES = $(OBJ_APP) $(OBJ_SYSTEM)
# ----------------------------
# Build Rules
# ----------------------------
all: $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(BIN) $(BUILD_DIR)/$(HEX)

# Ensure build directory exists
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Compile C++ source
$(BUILD_DIR)/main.o: $(APP_SRC) | $(BUILD_DIR)
	@echo "Compiling C++ Source: $<"
	@$(CXX) $(CFLAGS) -c $< -o $@

# Compile system files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "Compiling System/Driver: $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# Link
$(BUILD_DIR)/$(TARGET): $(OBJ_FILES)
	@echo "Linking: $@"
	@$(CXX) $(OBJ_FILES) $(LDFLAGS) -o $@
	@$(SIZE) $@

# Binary and Hex
$(BUILD_DIR)/$(BIN): $(BUILD_DIR)/$(TARGET)
	@$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(HEX): $(BUILD_DIR)/$(TARGET)
	@$(OBJCOPY) -O ihex $< $@

# ----------------------------
# Flashing
# ----------------------------
dfu: $(BUILD_DIR)/$(BIN)
	@echo "Flashing via DFU..."
	dfu-util -a 0 -s 0x08000000:leave -D $<

openocd_flash: $(BUILD_DIR)/$(BIN)
	@echo "Flashing via OpenOCD..."
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program $< verify reset exit 0x08000000"
	@echo "✅ Flashing Done Successfully"

# ----------------------------
# Clean
# ----------------------------
clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

.PHONY: all clean dfu openocd_flash
