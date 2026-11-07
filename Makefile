# SPDX-License-Identifier: MIT
# Builds openlf1.gba from the original Little Fighter (1995) ZIP.
#
#   make ZIP=/path/to/Little_Fighter_1995.zip
#
# TOOL_PREFIX selects the ARM toolchain (default: arm-none-eabi- on PATH).
# EXTRA_FLAGS can add include and library paths for a toolchain installed elsewhere.

ZIP ?= Little_Fighter_1995.zip
TOOL_PREFIX ?= arm-none-eabi-
CXX := $(TOOL_PREFIX)g++
CC := $(TOOL_PREFIX)gcc
AS := $(TOOL_PREFIX)as
LD := $(TOOL_PREFIX)g++
OBJCOPY := $(TOOL_PREFIX)objcopy
PYTHON ?= python3
BOOT_LOGO ?= build/boot_logo.bin

ARCH := -mcpu=arm7tdmi -mthumb -mthumb-interwork
COMMON := $(ARCH) -O2 -Wall -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections $(EXTRA_FLAGS)
CXXFLAGS := $(COMMON) -std=gnu++23 -Ibuild/assets -Ilogic
LDFLAGS := $(ARCH) -nostartfiles -Wl,-T,gba/linker.ld -Wl,--gc-sections $(EXTRA_LDFLAGS)
LIBS := -lstdc++ -lsupc++ -lm -lc -lgcc

LOGIC_SRC := $(wildcard logic/*.cpp)
GBA_SRC := $(filter-out gba/crt0.s,$(wildcard gba/*.cpp))
OBJS := $(patsubst %.cpp,build/obj/%.o,$(LOGIC_SRC) $(GBA_SRC)) build/obj/gba/crt0.o build/obj/assets.o

all: build/openlf1.gba

build/assets/assets.h: $(ZIP) tools/convert_assets.py
	@mkdir -p build/assets
	$(PYTHON) tools/convert_assets.py $(ZIP) build/assets

build/obj/%.o: %.cpp build/assets/assets.h
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

build/obj/gba/crt0.o: gba/crt0.s
	@mkdir -p $(dir $@)
	$(AS) $(ARCH) $< -o $@

build/obj/assets.o: build/assets/assets.h
	@mkdir -p build/obj
	$(AS) $(ARCH) -Ibuild/assets build/assets/assets.S -o $@

build/openlf1.elf: $(OBJS) gba/linker.ld
	$(LD) $(LDFLAGS) $(OBJS) $(LIBS) -o $@

build/boot_logo.bin:
	@mkdir -p build
	$(PYTHON) tools/fetch_logo.py $@

build/openlf1.gba: build/openlf1.elf $(BOOT_LOGO)
	$(OBJCOPY) -O binary $< $@
	$(PYTHON) tools/gbafix.py $@ $(BOOT_LOGO)

clean:
	rm -rf build

-include $(OBJS:.o=.d)
.PHONY: all clean
