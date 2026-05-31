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
FREERTOS_DIR       = middlewares/freertos

# ----------------------------
# Source Files Discovery 🔍
# ----------------------------
# Automatically search for C++ and C files inside application/ and its subdirectories
APP_CXX_SRC = $(wildcard application/*.cpp) $(wildcard application/**/*.cpp)
APP_C_SRC   = $(wildcard application/*.c)   $(wildcard application/**/*.c)

STARTUP_FILE = startup/startup_stm32f411xe.s

SYSTEM_SRC = $(NO_OS_DIR)/segger_fault_handler.c \
             $(NO_OS_DIR)/syscalls.c \
             $(NO_OS_DIR)/sysmem.c \
             $(DRIVERS_ST_DIR)/system_stm32f4xx.c

FREERTOS_SRC = $(FREERTOS_DIR)/croutine.c \
               $(FREERTOS_DIR)/event_groups.c \
               $(FREERTOS_DIR)/list.c \
               $(FREERTOS_DIR)/queue.c \
               $(FREERTOS_DIR)/stream_buffer.c \
               $(FREERTOS_DIR)/tasks.c \
               $(FREERTOS_DIR)/timers.c \
               $(FREERTOS_DIR)/portable/GCC/ARM_CM4F/port.c \
               $(FREERTOS_DIR)/portable/MemMang/heap_4.c

# ----------------------------
# Compiler & Linker Flags
# ----------------------------
COMMON_FLAGS = -mcpu=$(MCU) -mthumb -mfpu=$(FPU) -mfloat-abi=$(FLOAT_ABI) \
               -Og -g3 -Wall -fdata-sections -ffunction-sections \
               -DSTM32F411xE

# Include Directories (Automatically including application subfolders!)
INCLUDES = -I$(DRIVERS_ST_DIR) \
           -I$(DRIVERS_CMSIS_DIR) \
           -I$(NO_OS_DIR) \
           -Icore/inc \
           -Iapplication \
           -Iapplication/communication \
           -Iapplication/control \
           -Iapplication/utils \
           -I$(FREERTOS_DIR)/include \
           -I$(FREERTOS_DIR)/portable/GCC/ARM_CM4F

CFLAGS   = $(COMMON_FLAGS) $(INCLUDES) -std=gnu11
CXXFLAGS = $(COMMON_FLAGS) $(INCLUDES) -std=gnu++17 -fno-exceptions -fno-rtti

LDFLAGS  = -mcpu=$(MCU) -mthumb -mfpu=$(FPU) -mfloat-abi=$(FLOAT_ABI) \
           -Wl,--gc-sections -Tstartup/STM32F411CEU6_FLASH.ld \
           -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map \
           -specs=nano.specs \
           -Wl,--no-warn-rwx-segments

# ----------------------------
# Object File Mappings 🗺️
# ----------------------------
OBJ_APP_CXX  = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(APP_CXX_SRC))
OBJ_APP_C    = $(patsubst %.c,$(BUILD_DIR)/%.o,$(APP_C_SRC))
OBJ_SYSTEM   = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SYSTEM_SRC))
OBJ_FREERTOS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(FREERTOS_SRC))
OBJ_STARTUP  = $(patsubst %.s,$(BUILD_DIR)/%.o,$(STARTUP_FILE))

OBJ_FILES = $(OBJ_APP_CXX) $(OBJ_APP_C) $(OBJ_SYSTEM) $(OBJ_FREERTOS) $(OBJ_STARTUP)

# ----------------------------
# Build Rules
# ----------------------------
all: $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(BIN) $(BUILD_DIR)/$(HEX)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Rule for C++ Files (.cpp)
$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	@echo "✨ Compiling C++ Source: $<"
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule for C Files (.c)
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "⚙️ Compiling C Source: $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

# Rule for Startup/Assembly Files (.s)
$(BUILD_DIR)/%.o: %.s | $(BUILD_DIR)
	@echo "🚀 Assembling Startup File: $<"
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@

# Link Target
$(BUILD_DIR)/$(TARGET): $(OBJ_FILES)
	@echo "🔗 Linking target: $@"
	@$(CXX) $(OBJ_FILES) $(LDFLAGS) -o $@
	@echo "📊 Binary Size Metrics:"
	@$(SIZE) $@

$(BUILD_DIR)/$(BIN): $(BUILD_DIR)/$(TARGET)
	@$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/$(HEX): $(BUILD_DIR)/$(TARGET)
	@$(OBJCOPY) -O ihex $< $@

# ----------------------------
# Flashing
# ----------------------------
dfu: $(BUILD_DIR)/$(BIN)
	@echo "📲 Flashing via DFU..."
	dfu-util -a 0 -s 0x08000000:leave -D $<

openocd_flash: $(BUILD_DIR)/$(BIN)
	@echo "⚡ Flashing via OpenOCD..."
	openocd -f interface/stlink.cfg -f target/stm32f4x.cfg -c "program $< verify reset exit 0x08000000"
	@echo "✅ Flashing Done Successfully!"

# ----------------------------
# Clean
# ----------------------------
clean:
	@echo "🧹 Cleaning build directory..."
	@rm -rf $(BUILD_DIR)

.PHONY: all clean dfu openocd_flash
