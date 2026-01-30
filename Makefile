# Makefile for CH32V003 JSON GUI Addon Kit
# Optimized build system based on analysis of PlatformIO configuration
# See BINARY_SIZE_ANALYSIS.md for design rationale

# Project name
PROJECT = json-gui-addon-kit

# Target MCU
MCU = CH32V003F4P6
MCU_FAMILY = CH32V003

# Build targets
TARGET_SLAVE = gfx_slave
TARGET_MASTER = gfx_master

# WCH Toolchain Configuration
WCH_TOOLCHAIN ?= /opt/wch-toolchain

# Toolchain prefix - WCH official toolchain ONLY (no fallbacks)
# Try both possible GCC locations in MounRiverStudio structure
ifneq ($(wildcard $(WCH_TOOLCHAIN)/toolchain/RISC-V-Embedded-GCC12/bin/riscv-wch-elf-gcc),)
    PREFIX = $(WCH_TOOLCHAIN)/toolchain/RISC-V-Embedded-GCC12/bin/riscv-wch-elf-
else ifneq ($(wildcard $(WCH_TOOLCHAIN)/toolchain/RISC-V-Embedded-GCC12/riscv-wch-elf/bin/riscv-wch-elf-gcc),)
    PREFIX = $(WCH_TOOLCHAIN)/toolchain/RISC-V-Embedded-GCC12/riscv-wch-elf/bin/riscv-wch-elf-
else ifneq ($(wildcard $(WCH_TOOLCHAIN)/Toolchain/RISC-V\ Embedded\ GCC12/bin/riscv-wch-elf-gcc),)
    PREFIX = $(WCH_TOOLCHAIN)/Toolchain/RISC-V\ Embedded\ GCC12/bin/riscv-wch-elf-
else ifneq ($(wildcard $(WCH_TOOLCHAIN)/RISC-V\ Embedded\ GCC12/bin/riscv-wch-elf-gcc),)
    PREFIX = $(WCH_TOOLCHAIN)/RISC-V\ Embedded\ GCC12/bin/riscv-wch-elf-
else
    $(error WCH toolchain not found at $(WCH_TOOLCHAIN). Please run ./tool/setup_wch_toolchain.sh or set WCH_TOOLCHAIN environment variable)
endif

CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
AR = $(PREFIX)ar
GDB = $(PREFIX)gdb

# OpenOCD configuration
ifneq ($(wildcard $(WCH_TOOLCHAIN)/OpenOCD/OpenOCD/bin/openocd),)
    OPENOCD ?= $(WCH_TOOLCHAIN)/OpenOCD/OpenOCD/bin/openocd
    OPENOCD_SCRIPTS ?= $(WCH_TOOLCHAIN)/OpenOCD/OpenOCD/scripts
else
    OPENOCD ?= $(WCH_TOOLCHAIN)/OpenOCD/bin/openocd
    OPENOCD_SCRIPTS ?= $(WCH_TOOLCHAIN)/OpenOCD/scripts
endif
OPENOCD_CFG ?= $(OPENOCD_SCRIPTS)/wch-riscv.cfg

# debug-probe-hub configuration
DEBUG_PROBE_HUB_TARGET ?= ch32v003
DEBUG_PROBE_HUB_PROBE ?= 4

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
SYSTEM_SRC = $(STARTUP_DIR)/system_ch32v00x.c

# Include paths
INC_PATHS = \
	-I$(INC_DIR) \
	-I$(INC_DIR)/common \
	-I$(INC_DIR)/slave \
	-I$(INC_DIR)/master \
	-I$(INC_DIR)/ch32v \
	-Ilib/CH32V003_lib_i2c

# MCU-specific flags - Matching PlatformIO's proven configuration
# xw = custom WCH instructions for better code density
ARCH_FLAGS = \
	-march=rv32ecxw \
	-mabi=ilp32e \
	-msmall-data-limit=0 \
	-msave-restore \
	-mrelax

# Startup assembly needs zicsr extension for CSR instructions
STARTUP_ARCH_FLAGS = \
	-march=rv32ec_zicsr \
	-mabi=ilp32e \
	-msmall-data-limit=0 \
	-msave-restore \
	-mrelax

# Common C flags - Optimized for size, matching PlatformIO exactly
COMMON_CFLAGS = \
	-std=gnu11 \
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
	-fsigned-char \
	-fno-common \
	-fmessage-length=0 \
	-Wunused \
	-Wuninitialized \
	-Wno-comment \
	-Wall

# Preprocessor defines
COMMON_DEFS = \
	-DCH32V003 \
	-DFUNCONF_DISABLE_SSD1306=1 \
	-DUSE_PAGE_BUFFER=1

# Slave-specific flags
SLAVE_CFLAGS = $(COMMON_CFLAGS) $(COMMON_DEFS) $(INC_PATHS)

# Master-specific flags
MASTER_CFLAGS = $(COMMON_CFLAGS) $(COMMON_DEFS) \
	-DDEMO_JSON_SCENARIO_MULTI \
	-DFUNCONF_USE_DEBUGPRINTF=1 \
	-DMASTER_HPRE=RCC_HPRE_DIV3 \
	$(INC_PATHS)

# Linker flags - Use WCH toolchain's standard libgcc
LDFLAGS_BASE = \
	-T$(LD_DIR)/ch32v003f4p6.ld \
	-fmerge-all-constants \
	-Wl,--gc-sections,--relax,--print-memory-usage \
	-Os \
	-g \
	-march=rv32ecxw \
	-mabi=ilp32e \
	-ffunction-sections \
	-fdata-sections \
	-Wl,-gc-sections \
	--specs=nano.specs \
	--specs=nosys.specs \
	-nostartfiles \
	-flto \
	-static-libgcc \
	-Wl,--start-group \
	-lc \
	-lm \
	-lgcc \
	-Wl,--end-group

# Debug build flags
DEBUG_FLAGS = -Os -g3 -ggdb3 -fno-omit-frame-pointer

# Object files - Use WCH startup code
SLAVE_OBJS = $(patsubst $(SRC_DIR)/slave/%.c,$(BUILD_DIR)/slave/%.o,$(SLAVE_SRC)) \
             $(patsubst $(SRC_DIR)/common/%.c,$(BUILD_DIR)/common/%.o,$(COMMON_SRC)) \
             $(BUILD_DIR)/startup.o \
             $(BUILD_DIR)/system_ch32v00x.o

MASTER_OBJS = $(patsubst $(SRC_DIR)/master/%.c,$(BUILD_DIR)/master/%.o,$(MASTER_SRC)) \
              $(patsubst $(SRC_DIR)/common/%.c,$(BUILD_DIR)/common/%.o,$(COMMON_SRC)) \
              $(BUILD_DIR)/startup.o \
              $(BUILD_DIR)/system_ch32v00x.o

# Default target
all: slave

# Slave target
slave: $(BUILD_DIR)/$(TARGET_SLAVE).elf $(BUILD_DIR)/$(TARGET_SLAVE).hex $(BUILD_DIR)/$(TARGET_SLAVE).bin
	@echo "✓ Slave build complete"
	@echo "  FLASH usage:"
	@$(SZ) $<

# Master target
master: $(BUILD_DIR)/$(TARGET_MASTER).elf $(BUILD_DIR)/$(TARGET_MASTER).hex $(BUILD_DIR)/$(TARGET_MASTER).bin
	@echo "✓ Master build complete"
	@echo "  FLASH usage:"
	@$(SZ) $<

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
	@echo "  CC  $<"
	@$(CC) -c $(SLAVE_CFLAGS) $< -o $@

# Compile master sources
$(BUILD_DIR)/master/%.o: $(SRC_DIR)/master/%.c | $(BUILD_DIR)
	@echo "  CC  $<"
	@$(CC) -c $(MASTER_CFLAGS) $< -o $@

# Compile common sources (using slave flags by default)
$(BUILD_DIR)/common/%.o: $(SRC_DIR)/common/%.c | $(BUILD_DIR)
	@echo "  CC  $<"
	@$(CC) -c $(SLAVE_CFLAGS) $< -o $@

# Compile startup code
$(BUILD_DIR)/startup.o: $(STARTUP_SRC) | $(BUILD_DIR)
	@echo "  AS  $<"
	@$(AS) -c $(STARTUP_ARCH_FLAGS) $(INC_PATHS) $< -o $@

# Compile system initialization
$(BUILD_DIR)/system_ch32v00x.o: $(SYSTEM_SRC) | $(BUILD_DIR)
	@echo "  CC  $<"
	@$(CC) -c $(COMMON_CFLAGS) $(COMMON_DEFS) $(INC_PATHS) $< -o $@

# Link ELF for slave
$(BUILD_DIR)/$(TARGET_SLAVE).elf: $(SLAVE_OBJS)
	@echo "  LD  $@"
	@$(CC) $(SLAVE_CFLAGS) $(SLAVE_OBJS) $(LDFLAGS_BASE) -Wl,-Map=$(BUILD_DIR)/$(TARGET_SLAVE).map -o $@

# Link ELF for master
$(BUILD_DIR)/$(TARGET_MASTER).elf: $(MASTER_OBJS)
	@echo "  LD  $@"
	@$(CC) $(MASTER_OBJS) $(LDFLAGS_BASE) -Wl,-Map=$(BUILD_DIR)/$(TARGET_MASTER).map -o $@

# Generate HEX
%.hex: %.elf
	@echo "  HEX $@"
	@$(CP) -O ihex $< $@

# Generate BIN
%.bin: %.elf
	@echo "  BIN $@"
	@$(CP) -O binary -S $< $@

# Clean
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)

# Size analysis - show top symbols
size-analysis: slave
	@echo ""
	@echo "=== Binary Size Analysis ==="
	@$(PREFIX)nm --print-size --size-sort $(BUILD_DIR)/$(TARGET_SLAVE).elf | tail -30
	@echo ""
	@echo "=== Section Sizes ==="
	@$(PREFIX)size -A $(BUILD_DIR)/$(TARGET_SLAVE).elf

# Flash using OpenOCD
flash-slave: slave
	@echo "Flashing $(BUILD_DIR)/$(TARGET_SLAVE).hex via OpenOCD"
	$(OPENOCD) -f $(OPENOCD_CFG) \
		-c "program $(BUILD_DIR)/$(TARGET_SLAVE).hex verify reset exit"

flash-master: master
	@echo "Flashing $(BUILD_DIR)/$(TARGET_MASTER).hex via OpenOCD"
	$(OPENOCD) -f $(OPENOCD_CFG) \
		-c "program $(BUILD_DIR)/$(TARGET_MASTER).hex verify reset exit"

# Start OpenOCD debug server for GDB
debug-slave-server:
	$(OPENOCD) -f $(OPENOCD_CFG) -c "gdb_port 3333"

debug-master-server:
	$(OPENOCD) -f $(OPENOCD_CFG) -c "gdb_port 3333"

# Flash via debug-probe-hub (remote)
flash-slave-remote: slave
	@echo "Flashing $(BUILD_DIR)/$(TARGET_SLAVE).bin via debug-probe-hub"
	python3 tool/debug-probe-hub-client/flash.py \
		--target $(DEBUG_PROBE_HUB_TARGET) \
		--probe $(DEBUG_PROBE_HUB_PROBE) \
		--firmware $(BUILD_DIR)/$(TARGET_SLAVE).bin

flash-master-remote: master
	@echo "Flashing $(BUILD_DIR)/$(TARGET_MASTER).bin via debug-probe-hub"
	python3 tool/debug-probe-hub-client/flash.py \
		--target $(DEBUG_PROBE_HUB_TARGET) \
		--probe $(DEBUG_PROBE_HUB_PROBE) \
		--firmware $(BUILD_DIR)/$(TARGET_MASTER).bin

# Stack usage analysis (disable LTO for accurate results)
analyze-stack: COMMON_CFLAGS := $(filter-out -flto -fuse-linker-plugin, $(COMMON_CFLAGS))
analyze-stack: COMMON_CFLAGS += -fstack-usage -fno-lto -fno-ipa-cp -fno-ipa-sra
analyze-stack: slave
	@echo "Stack usage analysis complete. Check .su files:"
	@find $(BUILD_DIR) -name "*.su" -exec echo "  {}" \; -exec cat {} \;

# Help
help:
	@echo "CH32V003 JSON GUI Addon Kit - Build System"
	@echo ""
	@echo "Targets:"
	@echo "  make slave               - Build slave firmware (default)"
	@echo "  make master              - Build master firmware"
	@echo "  make debug-slave         - Build slave with debug symbols"
	@echo "  make debug-master        - Build master with debug symbols"
	@echo "  make clean               - Remove build artifacts"
	@echo "  make flash-slave         - Flash slave via local OpenOCD"
	@echo "  make flash-master        - Flash master via local OpenOCD"
	@echo "  make flash-slave-remote  - Flash slave via debug-probe-hub"
	@echo "  make flash-master-remote - Flash master via debug-probe-hub"
	@echo "  make size-analysis       - Show binary size breakdown"
	@echo "  make analyze-stack       - Generate stack usage reports"
	@echo ""
	@echo "Configuration:"
	@echo "  Toolchain: $(CC)"
	@echo "  Target MCU: $(MCU)"
	@echo "  Build directory: $(BUILD_DIR)/"

.PHONY: all slave master debug-slave debug-master clean flash-slave flash-master flash-slave-remote flash-master-remote debug-slave-server debug-master-server analyze-stack size-analysis help
