# Makefile – AUTOSAR Classic Diagnostic Demo (STM32F103)
# Build: make
# Clean: make clean

TARGET_TX = node_tx
TARGET_RX = node_rx

# Default target
all: examples

CC      = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE    = arm-none-eabi-size

# STM32F103C8 (Medium-density)
CFLAGS  = -mcpu=cortex-m3 -mthumb -O0 -g3 -Wall -Wextra
CFLAGS += -ffreestanding -ffunction-sections -fdata-sections
CFLAGS += -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER -DRUN_ON_RENODE
LDFLAGS = -T stm32f103.ld --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections

# ===== Include paths =====
INC  = -IBSW/Common
INC += -IBSW/Services/Com
INC += -IBSW/Services/Dcm
INC += -IBSW/Services/PduR
INC += -IBSW/EcuAbstraction/CanIf
INC += -IBSW/EcuAbstraction/CanTp
INC += -IBSW/EcuAbstraction/LinIf
INC += -IBSW/Mcal/Can
INC += -IBSW/Mcal/Lin
INC += -IConfig
INC += -IPlatform/Spl/inc
INC += -IPlatform/Bsp/cmsis

# ===== Source files =====
# Application
SRCS  =

# AUTOSAR COM stack
SRCS += BSW/Services/Com/Com.c
SRCS += BSW/Services/PduR/PduR.c
SRCS += BSW/EcuAbstraction/CanIf/CanIf.c
SRCS += BSW/EcuAbstraction/CanTp/CanTp.c
SRCS += BSW/EcuAbstraction/LinIf/LinIf.c
SRCS += BSW/Mcal/Can/Can.c
SRCS += BSW/Mcal/Lin/Lin.c
SRCS += BSW/Services/Dcm/Dcm.c

# Config
SRCS += Config/Com_Cfg.c
SRCS += Config/CanIf_Cfg.c
SRCS += Config/PduR_Cfg.c
SRCS += Config/CanTp_Cfg.c
SRCS += Config/Diag_Cfg.c

# SPL (chỉ compile các module cần thiết)
SRCS += Platform/Spl/src/stm32f10x_rcc.c
SRCS += Platform/Spl/src/stm32f10x_gpio.c
SRCS += Platform/Spl/src/stm32f10x_can.c
SRCS += Platform/Spl/src/stm32f10x_usart.c
SRCS += Platform/Spl/src/stm32f10x_adc.c
SRCS += Platform/Spl/src/stm32f10x_dma.c
SRCS += Platform/Spl/src/stm32f10x_tim.c
SRCS += Platform/Spl/src/stm32f10x_exti.c
SRCS += Platform/Spl/src/stm32f10x_spi.c
SRCS += Platform/Spl/src/stm32f10x_i2c.c
SRCS += Platform/Spl/src/misc.c
SRCS += Platform/Spl/src/system_stm32f10x.c

# Startup
ASM_SRCS = startup_stm32f103.s

# ===== Object files (chung) =====
BUILD_DIR = build
APP_SRCS = $(wildcard main/*.c)

define MAKE_APP_TEMPLATE
APP_BASENAME_$(1) = $$(basename $$(notdir $(1)))
APP_TARGET_$(1) = $$(BUILD_DIR)/example/$$(APP_BASENAME_$(1))
APP_NODE_$(1) = $$(if $$(findstring evcu_com_ecu,$$(APP_BASENAME_$(1))),-DEVCU_COM_ECU -DNODE_TX,$$(if $$(findstring evcu_diag_ecu,$$(APP_BASENAME_$(1))),-DEVCU_DIAG_ECU -DNODE_TX,$$(if $$(findstring rx,$$(APP_BASENAME_$(1))),-DNODE_RX,-DNODE_TX)))

OBJS_$(1) = $$(patsubst %.c,$$(BUILD_DIR)/$$(APP_BASENAME_$(1))/%.o,$(1)) \
            $$(patsubst %.c,$$(BUILD_DIR)/$$(APP_BASENAME_$(1))/%.o,$$(SRCS)) \
            $$(patsubst %.s,$$(BUILD_DIR)/$$(APP_BASENAME_$(1))/%.o,$$(ASM_SRCS))

$$(APP_TARGET_$(1)).elf: $$(OBJS_$(1))
	@if not exist "$$(subst /,\,$$(dir $$@))" mkdir "$$(subst /,\,$$(dir $$@))"
	$$(CC) $$(CFLAGS) $$^ $$(LDFLAGS) -o $$@

$$(APP_TARGET_$(1)).bin: $$(APP_TARGET_$(1)).elf
	$$(OBJCOPY) -O binary $$< $$@

$$(APP_TARGET_$(1)).hex: $$(APP_TARGET_$(1)).elf
	$$(OBJCOPY) -O ihex $$< $$@

$$(BUILD_DIR)/$$(APP_BASENAME_$(1))/%.o: %.c
	@if not exist "$$(subst /,\,$$(dir $$@))" mkdir "$$(subst /,\,$$(dir $$@))"
	$$(CC) $$(CFLAGS) $$(APP_NODE_$(1)) $$(INC) -c $$< -o $$@

$$(BUILD_DIR)/$$(APP_BASENAME_$(1))/%.o: %.s
	@if not exist "$$(subst /,\,$$(dir $$@))" mkdir "$$(subst /,\,$$(dir $$@))"
	$$(CC) $$(CFLAGS) -c $$< -o $$@

ALL_TARGETS += $$(APP_TARGET_$(1)).elf $$(APP_TARGET_$(1)).bin $$(APP_TARGET_$(1)).hex
endef

$(foreach app,$(APP_SRCS),$(eval $(call MAKE_APP_TEMPLATE,$(app))))

# ===== Demo build rules =====
COM_DEMO_OBJS = $(patsubst %.c,$(BUILD_DIR)/DiagECU/%.o,main.c) \
                $(patsubst %.c,$(BUILD_DIR)/DiagECU/%.o,$(SRCS)) \
                $(patsubst %.s,$(BUILD_DIR)/DiagECU/%.o,$(ASM_SRCS))

DiagECU.elf: $(COM_DEMO_OBJS)
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@
	@echo ""
	@echo "╔══════════════════════════════════════════════════════════════╗"
	@echo "║    BUILD THANH CONG DiagECU.elf (STM32F103)                  ║"
	@echo "╚══════════════════════════════════════════════════════════════╝"
	$(SIZE) $@
	@echo ""

$(BUILD_DIR)/DiagECU/%.o: %.c
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(BUILD_DIR)/DiagECU/%.o: %.s
	@if not exist "$(subst /,\,$(dir $@))" mkdir "$(subst /,\,$(dir $@))"
	$(CC) $(CPUFLAGS) -c $< -o $@

examples: $(ALL_TARGETS)
	@echo "All examples built successfully."

debug: DiagECU.elf
	@echo "Stopping any running Renode/GDB processes..."
	@pkill -9 -i renode || true; pkill -9 -i mono || true; lsof -ti:3333 | xargs kill -9 2>/dev/null || true
	@echo "Starting Renode simulation in background..."
	@renode --disable-xwt -e "include @Script/stm32_evcu_training.resc" > renode_system.log 2>&1 & \
	RENODE_PID=$$! ; \
	echo "Waiting for GDB server to start on port 3333..." ; \
	for i in {1..10}; do \
		if lsof -i :3333 >/dev/null 2>&1; then \
			break; \
		fi; \
		sleep 0.5; \
	done; \
	echo "Launching GDB..." ; \
	arm-none-eabi-gdb DiagECU.elf -ex "target extended-remote 127.0.0.1:3333" -ex "break main" -ex "continue" ; \
	echo "Stopping Renode (PID $$RENODE_PID)..." ; \
	kill -9 $$RENODE_PID 2>/dev/null || true

clean:
	@echo "Cleaning target: $(TARGET_NAME)..."
	@if exist "$(subst /,\,$(TARGET)).elf" del /f "$(subst /,\,$(TARGET)).elf"
	@if exist "$(subst /,\,$(TARGET)).bin" del /f "$(subst /,\,$(TARGET)).bin"
	@if exist "$(subst /,\,$(TARGET)).hex" del /f "$(subst /,\,$(TARGET)).hex"
	@if exist "$(subst /,\,$(TARGET)).map" del /f "$(subst /,\,$(TARGET)).map"
	@if exist "$(subst /,\,$(TARGET)).list" del /f "$(subst /,\,$(TARGET)).list"
	@echo "Done."

.PHONY: all examples debug clean print_targets
print_targets:
	@echo $(ALL_TARGETS)