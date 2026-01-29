# Makefile for CH32V003 JSON GUI Addon Kit
# Toolchain: WCH RISC-V GCC (compatible with CH32V003/V203/V305)

# Project name
PROJECT = json-gui-addon-kit

# Target MCU
MCU = CH32V003F4P6
MCU_FAMILY = CH32V003

# Build targets
TARGET_SLAVE = gfx_slave
TARGET_MASTER = gfx_master

# Toolchain (use riscv64-unknown-elf if riscv-none-elf not available)
PREFIX ?= riscv-none-elf-
ifeq ($(shell which $(PREFIX)gcc 2>/dev/null),)
    PREFIX = riscv64-unknown-elf-
endif

CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
AR = $(PREFIX)ar
GDB = $(PREFIX)gdb

# Directories
SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
LD_DIR = ld
STARTUP_DIR = startup

# Source files
COMMON_SRC = $(wildcard $(SRC_DIR)/common/*.c)
SLAVE_SRC = $(wildcard $(SRC_DIR)/slave/*.c)
MASTER_SRC = $(wildcard $(SRC_DIR)/master/*.c)
STARTUP_SRC = $(STARTUP_DIR)/startup_ch32v00x.S

# Include paths
INC_PATHS = \
	-I$(INC_DIR) \
	-I$(INC_DIR)/common \
	-I$(INC_DIR)/slave \
	-I$(INC_DIR)/master \
	-I$(INC_DIR)/ch32v \
	-Ilib/CH32V003_lib_i2c

# MCU-specific flags
ARCH_FLAGS = \
	-march=rv32ec \
	-mabi=ilp32e \
	-msmall-data-limit=0 \
	-msave-restore \
	-mrelax

# Startup needs zicsr for CSR instructions
STARTUP_ARCH_FLAGS = \
	-march=rv32ec_zicsr \
	-mabi=ilp32e \
	-msmall-data-limit=0 \
	-msave-restore \
	-mrelax


# Picolibc paths
PICOLIBC_BASE = /usr/lib/picolibc/riscv64-unknown-elf
PICOLIBC_INC = $(PICOLIBC_BASE)/include
PICOLIBC_LIB = $(PICOLIBC_BASE)/lib/rv32emac/ilp32e

# Common C flags
COMMON_CFLAGS = \
	$(ARCH_FLAGS) \
	-Os \
	-flto \
	-flto-partition=one \
	-fmerge-all-constants \
	-fmerge-constants \
	-fno-unroll-loops \
	-fno-tree-vectorize \
	-fno-tree-slp-vectorize \
	-fno-asynchronous-unwind-tables \
	-fno-unwind-tables \
	-fno-exceptions \
	-fno-stack-protector \
	-ffunction-sections \
	-fdata-sections \
	-fomit-frame-pointer \
	-fno-inline-functions-called-once \
	-fno-jump-tables \
	-fuse-linker-plugin \
	-I$(PICOLIBC_INC) \
	-Wall \
	-Wextra \
	-s

# Preprocessor defines
COMMON_DEFS = \
	-DCH32V003 \
	-DFUNCONF_DISABLE_SSD1306=1 \
	-DUSE_PAGE_BUFFER=1 \
	-DPICOLIBC_INTEGER_PRINTF_SCANF \
	-D__PICOLIBC_CRT0_MINIMAL__ \
	-DDISABLE_DEBUG_LED=1

# Slave-specific flags
SLAVE_CFLAGS = $(COMMON_CFLAGS) $(COMMON_DEFS) $(INC_PATHS)

# Master-specific flags
MASTER_CFLAGS = $(COMMON_CFLAGS) $(COMMON_DEFS) \
	-DDEMO_JSON_SCENARIO_MULTI \
	-DFUNCONF_USE_DEBUGPRINTF=1 \
	-DMASTER_HPRE=RCC_HPRE_DIV3 \
	$(INC_PATHS)

# Linker flags (will be modified per target)
LDFLAGS_BASE = \
	$(ARCH_FLAGS) \
	-T$(LD_DIR)/ch32v003f4p6.ld \
	-Wl,--gc-sections \
	-Wl,--relax \
	-Wl,--print-memory-usage \
	-lgcc \
	-nostartfiles \
	-nodefaultlibs

# Debug build flags
DEBUG_FLAGS = -Os -g3 -ggdb3 -fno-omit-frame-pointer

# Object files
SLAVE_OBJS = $(patsubst $(SRC_DIR)/slave/%.c,$(BUILD_DIR)/slave/%.o,$(SLAVE_SRC)) \
             $(patsubst $(SRC_DIR)/common/%.c,$(BUILD_DIR)/common/%.o,$(COMMON_SRC)) \
             $(BUILD_DIR)/startup.o

MASTER_OBJS = $(patsubst $(SRC_DIR)/master/%.c,$(BUILD_DIR)/master/%.o,$(MASTER_SRC)) \
              $(patsubst $(SRC_DIR)/common/%.c,$(BUILD_DIR)/common/%.o,$(COMMON_SRC)) \
              $(BUILD_DIR)/startup.o

# Default target
all: slave

# Slave target
slave: $(BUILD_DIR)/$(TARGET_SLAVE).elf $(BUILD_DIR)/$(TARGET_SLAVE).hex $(BUILD_DIR)/$(TARGET_SLAVE).bin
	@echo "Slave build complete"

# Master target
master: $(BUILD_DIR)/$(TARGET_MASTER).elf $(BUILD_DIR)/$(TARGET_MASTER).hex $(BUILD_DIR)/$(TARGET_MASTER).bin
	@echo "Master build complete"

# Debug builds
debug-slave: COMMON_CFLAGS += $(DEBUG_FLAGS)
debug-slave: SLAVE_CFLAGS = $(COMMON_CFLAGS) $(COMMON_DEFS) $(INC_PATHS)
debug-slave: slave

debug-master: COMMON_CFLAGS += $(DEBUG_FLAGS)
debug-master: MASTER_CFLAGS = $(COMMON_CFLAGS) $(COMMON_DEFS) -DDEMO_JSON_SCENARIO_MULTI -DFUNCONF_USE_DEBUGPRINTF=1 -DMASTER_HPRE=RCC_HPRE_DIV3 $(INC_PATHS)
debug-master: master

# Create build directories
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)/slave
	@mkdir -p $(BUILD_DIR)/master
	@mkdir -p $(BUILD_DIR)/common

# Compile slave sources
$(BUILD_DIR)/slave/%.o: $(SRC_DIR)/slave/%.c | $(BUILD_DIR)
	@echo "CC $<"
	@$(CC) -c $(SLAVE_CFLAGS) $< -o $@

# Compile master sources
$(BUILD_DIR)/master/%.o: $(SRC_DIR)/master/%.c | $(BUILD_DIR)
	@echo "CC $<"
	@$(CC) -c $(MASTER_CFLAGS) $< -o $@

# Compile common sources for slave
$(BUILD_DIR)/common/%.o: $(SRC_DIR)/common/%.c | $(BUILD_DIR)
	@echo "CC $<"
	@$(CC) -c $(SLAVE_CFLAGS) $< -o $@

# Compile startup code
$(BUILD_DIR)/startup.o: $(STARTUP_SRC) | $(BUILD_DIR)
	@echo "AS $<"
	@$(AS) -c $(STARTUP_ARCH_FLAGS) $< -o $@

# Link ELF for slave
$(BUILD_DIR)/$(TARGET_SLAVE).elf: $(SLAVE_OBJS)
	@echo "LD $@"
	@$(CC) $(SLAVE_CFLAGS) $(SLAVE_OBJS) $(LDFLAGS_BASE) -Wl,-Map=$(BUILD_DIR)/$(TARGET_SLAVE).map -o $@
	@$(SZ) $@

# Link ELF for master
$(BUILD_DIR)/$(TARGET_MASTER).elf: $(MASTER_OBJS)
	@echo "LD $@"
	@$(CC) $(MASTER_OBJS) $(LDFLAGS_BASE) -Wl,-Map=$(BUILD_DIR)/$(TARGET_MASTER).map -o $@
	@$(SZ) $@

# Generate HEX
%.hex: %.elf
	@echo "HEX $@"
	@$(CP) -O ihex $< $@

# Generate BIN
%.bin: %.elf
	@echo "BIN $@"
	@$(CP) -O binary -S $< $@

# Clean
clean:
	rm -rf $(BUILD_DIR)

# Flash using wlink (to be implemented with remote programmer)
flash-slave: slave
	@echo "Flash with wlink: build/$(TARGET_SLAVE).hex"
	@echo "TODO: Integrate with debug-probe-hub or wlink directly"

flash-master: master
	@echo "Flash with wlink: build/$(TARGET_MASTER).hex"
	@echo "TODO: Integrate with debug-probe-hub or wlink directly"

# Stack usage analysis (disable LTO for accurate results)
analyze-stack: COMMON_CFLAGS := $(filter-out -flto -fuse-linker-plugin, $(COMMON_CFLAGS))
analyze-stack: COMMON_CFLAGS += -fstack-usage -fno-lto -fno-ipa-cp -fno-ipa-sra
analyze-stack: slave
	@echo "Stack usage analysis complete. Check .su files in $(BUILD_DIR)/"
	@find $(BUILD_DIR) -name "*.su" -exec cat {} \;

.PHONY: all slave master debug-slave debug-master clean flash-slave flash-master analyze-stack
