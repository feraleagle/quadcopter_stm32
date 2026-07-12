# ============================================================
# Firmware Makefile — STM32F411 Black Pill
# ============================================================

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
CORE_SRC_DIR       = core/src
CORE_INC_DIR       = core/inc

FREERTOS_DIR       = middlewares/freertos
APP_SRC_DIR        = application/src
APP_INC_DIR        = application/inc

STARTUP_DIR        = startup
BUILD_DIR          = build

# ----------------------------
# Source Discovery (Recursive)
# ----------------------------
# Recursively find ALL .cpp and .c files under application/
APP_CXX_SRC := $(shell find $(APP_SRC_DIR) -name '*.cpp')
APP_C_SRC   := $(shell find $(APP_SRC_DIR) -name '*.c')

STARTUP_FILE := $(STARTUP_DIR)/startup_stm32f411xe.s

SYSTEM_SRC := \
    $(CORE_SRC_DIR)/segger_fault_handler.c \
    $(CORE_SRC_DIR)/syscalls.c             \
    $(CORE_SRC_DIR)/sysmem.c               \
    $(DRIVERS_ST_DIR)/system_stm32f4xx.c

FREERTOS_SRC := \
    $(FREERTOS_DIR)/croutine.c                          \
    $(FREERTOS_DIR)/event_groups.c                      \
    $(FREERTOS_DIR)/list.c                              \
    $(FREERTOS_DIR)/queue.c                             \
    $(FREERTOS_DIR)/stream_buffer.c                     \
    $(FREERTOS_DIR)/tasks.c                             \
    $(FREERTOS_DIR)/timers.c                            \
    $(FREERTOS_DIR)/portable/GCC/ARM_CM4F/port.c        \
    $(FREERTOS_DIR)/portable/MemMang/heap_4.c

ALL_C_SRC   := $(APP_C_SRC) $(SYSTEM_SRC) $(FREERTOS_SRC)
ALL_CXX_SRC := $(APP_CXX_SRC)


INCLUDES := \
    -I$(DRIVERS_ST_DIR)              \
    -I$(DRIVERS_CMSIS_DIR)           \
    -I$(CORE_SRC_DIR)                \
    -I$(CORE_INC_DIR)                \
    -I$(APP_INC_DIR)                \
    -I$(FREERTOS_DIR)/include        \
    -I$(FREERTOS_DIR)/portable/GCC/ARM_CM4F

# ----------------------------
# Compiler & Linker Flags
# ----------------------------
ARCH_FLAGS = \
    -mcpu=$(MCU)           \
    -mthumb                \
    -mfpu=$(FPU)           \
    -mfloat-abi=$(FLOAT_ABI)

COMMON_FLAGS = $(ARCH_FLAGS)        \
    -Og -g3                         \
    -Wall -Wextra                   \
    -fdata-sections                 \
    -ffunction-sections             \
    -DSTM32F411xE

CFLAGS   = $(COMMON_FLAGS) $(INCLUDES) -std=gnu11
CXXFLAGS = $(COMMON_FLAGS) $(INCLUDES) -std=gnu++17 -fno-exceptions -fno-rtti

LDFLAGS = $(ARCH_FLAGS)                              \
    -T$(STARTUP_DIR)/STM32F411CEU6_FLASH.ld          \
    -Wl,--gc-sections                                \
    -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map             \
    -specs=nano.specs                                \
    -Wl,--no-warn-rwx-segments

# ----------------------------
# Object File Mappings
# ----------------------------
OBJ_APP_CXX  = $(patsubst %.cpp, $(BUILD_DIR)/%.o, $(ALL_CXX_SRC))
OBJ_APP_C    = $(patsubst %.c,   $(BUILD_DIR)/%.o, $(ALL_C_SRC))
OBJ_STARTUP  = $(patsubst %.s,   $(BUILD_DIR)/%.o, $(STARTUP_FILE))

OBJ_FILES = $(OBJ_APP_CXX) $(OBJ_APP_C) $(OBJ_STARTUP)

# ----------------------------
# Dependency Files (auto-generated)
# Auto-recompile when headers change — critical for embedded dev
# ----------------------------
DEP_FILES = $(OBJ_FILES:.o=.d)
-include $(DEP_FILES)

# ----------------------------
# Build Rules
# ----------------------------
.PHONY: all clean dfu openocd_flash compile_commands size

all: $(BUILD_DIR)/$(TARGET) $(BUILD_DIR)/$(BIN) $(BUILD_DIR)/$(HEX)

# Link
$(BUILD_DIR)/$(TARGET): $(OBJ_FILES)
	@echo "🔗 Linking → $@"
	@$(CXX) $(OBJ_FILES) $(LDFLAGS) -o $@
	@echo "📊 Section sizes:"
	@$(SIZE) $@

# .bin
$(BUILD_DIR)/$(BIN): $(BUILD_DIR)/$(TARGET)
	@$(OBJCOPY) -O binary $< $@
	@echo "✅ BIN → $@"

# .hex
$(BUILD_DIR)/$(HEX): $(BUILD_DIR)/$(TARGET)
	@$(OBJCOPY) -O ihex $< $@
	@echo "✅ HEX → $@"

# Compile C++ — with -MMD for dependency tracking
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "✨ [CXX] $<"
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

# Compile C — with -MMD for dependency tracking
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "⚙️  [CC]  $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Assemble startup file
$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "🚀 [AS]  $<"
	@$(CC) $(CFLAGS) -x assembler-with-cpp -c $< -o $@

# ----------------------------
# compile_commands.json
# For clangd LSP in Neovim — generated without bear
# ----------------------------
compile_commands: clean
	@echo "🧠 Generating compile_commands.json..."
	@$(MAKE) all \
	    CC="$(CC) -MJ /tmp/cc_entry.json" \
	    CXX="$(CXX) -MJ /tmp/cxx_entry.json" \
	    2>/dev/null; true
	@compiledb make 2>/dev/null || \
	    bear -- $(MAKE) all 2>/dev/null || \
	    echo "⚠️  Install 'compiledb' (pip) or 'bear' for compile_commands.json"

# ----------------------------
# Size Report
# ----------------------------
size: $(BUILD_DIR)/$(TARGET)
	@echo "📐 Full size report:"
	@$(SIZE) -A $

# ----------------------------
# Flashing
# ----------------------------
dfu: $(BUILD_DIR)/$(BIN)
	@echo "📲 Flashing via DFU..."
	dfu-util -a 0 -s 0x08000000:leave -D $

openocd_flash: $(BUILD_DIR)/$(BIN)
	@echo "⚡ Flashing via OpenOCD (ST-Link V2)..."
	openocd \
	    -f interface/stlink.cfg \
	    -f target/stm32f4x.cfg  \
	    -c "program $< verify reset exit 0x08000000"
	@echo "✅ Flash complete!"

# ----------------------------
# Clean
# ----------------------------
clean:
	@echo "🧹 Cleaning build/..."
	@rm -rf $(BUILD_DIR)
