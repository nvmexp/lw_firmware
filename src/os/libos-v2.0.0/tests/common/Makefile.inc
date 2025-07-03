
# Required environment variables
$(if $(value LW_TOOLS),,$(error LW_TOOLS not set))
$(if $(value LW_SOURCE),,$(error LW_SOURCE not set))
$(if $(value LIBOS_SOURCE),,$(error LIBOS_SOURCE not set))

BUILD_CFG ?= debug
PREFIX?=local

E=\e

# Pick the latest farm compiler
ifeq (,$(wildcard /home/utils/gcc-9.3.0))
CC=gcc -static
CXX=g++ -static
else
CC=/home/utils/gcc-9.3.0/bin/gcc -static
CXX=/home/utils/gcc-9.3.0/bin/g++ -static
endif

vpath %.cpp	../common
vpath %.cpp	../exelwtion-validation
vpath %.cpp ../../simulator
vpath %.c ../../lib
vpath %.c ../../debug

# Enable instruction level tracing with line decode
TRACE = -DTRACE
BUILDCFG ?= debug
OUT_DIR = _out/$(BUILD_CFG)

INCLUDE = -I$(LW_SOURCE) -I$(LW_SOURCE)/sdk/lwpu/inc -I$(LW_SOURCE)/drivers/common/inc -I$(LIBOS_SOURCE) -I$(LIBOS_SOURCE)/include -I$(OUT_DIR)
CFLAGS =  -g -gdwarf-2 -mabi=lp64 -march=rv64im -nostdlib -Os $(INCLUDE) -falign-functions=4 -falign-labels=4 -mcmodel=medlow -flto -ffunction-sections -fdata-sections -fno-common

# Debug modules
DEBUG_DIR = ../../debug
DEBUG_FILES = $(DEBUG_DIR)/lines.c $(DEBUG_DIR)/logdecode.c
DEBUG_OBJECTS = lines.o logdecode.o

CWARN = -Wall -Wno-multichar -Wno-unused-result -Werror

run: $(OUT_DIR) $(OUT_DIR)/passed.ga100 $(OUT_DIR)/passed.tu11x $(OUT_DIR)/passed.ga102 diff.ga100 diff.tu11x diff.ga102
	
.PRECIOUS: $(OUT_DIR)

$(OUT_DIR):
	@mkdir -p $(OUT_DIR)

$(OUT_DIR)/libos-config.h: libos-config.py $(LIBOS_SOURCE)/config/libos.py $(LIBOS_SOURCE)/config/Makefile.template
$(OUT_DIR)/assemble.ld: libos-config.py $(LIBOS_SOURCE)/config/libos.py $(LIBOS_SOURCE)/config/Makefile.template
$(OUT_DIR)/Makefile: libos-config.py $(LIBOS_SOURCE)/config/libos.py $(LIBOS_SOURCE)/config/Makefile.template
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " PYTHON" $(subst $(LW_SOURCE)/,,$<)
	@python2 libos-config.py $(OUT_DIR)/

FORCE: 

$(OUT_DIR)/firmware.elf: FORCE $(OUT_DIR)/libos-config.h $(OUT_DIR)/Makefile
	@$(MAKE) -C $(OUT_DIR) PREFIX=$(PREFIX) BUILD_CFG=$(BUILD_CFG) LIBOS_SOURCE=$(LIBOS_SOURCE)

$(OUT_DIR)/%.o : %.cpp
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " C" $(subst $(LW_SOURCE)/,,$<)
	@$(CXX) -c -MT $@ -MMD -MP -MF $(OUT_DIR)/$*.Td --std=c++14 -w -Os $(TRACE) $(INCLUDE) -g $< -o $@
	@mv -f $(OUT_DIR)/$*.Td $(OUT_DIR)/$*.d > /dev/null

$(OUT_DIR)/%.o : %.c
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " C" $(subst $(LW_SOURCE)/,,$<)
	$(CC) -c -MT $@ -MMD -MP -MF $(OUT_DIR)/$*.Td --std=c11 -w -Os $(TRACE) $(INCLUDE) -g $< -o $@
	@mv -f $(OUT_DIR)/$*.Td $(OUT_DIR)/$*.d > /dev/null

OBJECTS= \
	$(OUT_DIR)/test-driver.o \
	$(OUT_DIR)/common.o \
	$(OUT_DIR)/register-preservation.o \
	$(OUT_DIR)/loader.o \
	$(OUT_DIR)/elf.o \
	$(OUT_DIR)/riscv-core.o \
	$(OUT_DIR)/riscv-trace.o \
	$(OUT_DIR)/riscv-gdb.o  \
	$(OUT_DIR)/gsp.o \
	$(OUT_DIR)/gsp_tu11x.o \
	$(OUT_DIR)/gsp_ga100.o \
	$(OUT_DIR)/gsp_ga102.o \
	$(OUT_DIR)/lines.o \
	$(OUT_DIR)/logdecode.o

DEPENDS = $(patsubst %.o,%.d, $(OBJECTS))
$(DEPENDS): ;
.PRECIOUS: $(DEPENDS)
-include $(DEPENDS)

$(OUT_DIR)/test_driver: $(OBJECTS) | $(OUT_DIR)/firmware.elf
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " LD" $(subst $(LW_SOURCE)/,,$<)
	@rm -f $(OUT_DIR)/passed.*
	@$(CXX) --std=c++14 -w -Os $(TRACE) $(INCLUDE) -g $^ -o $(OUT_DIR)/test_driver

# Ensure error is passed through tee
SHELL = bash
.SHELLFLAGS = -o pipefail -c

$(OUT_DIR)/passed.tu11x: $(OUT_DIR)/test_driver  $(OUT_DIR)/firmware.elf
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " SIM-TU11x" $(subst $(LW_SOURCE)/,,$<)
	@cd $(OUT_DIR) && ./test_driver --arch=tu11x > passed.tu11x

$(OUT_DIR)/passed.ga100: $(OUT_DIR)/test_driver  $(OUT_DIR)/firmware.elf
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " SIM-GA100" $(subst $(LW_SOURCE)/,,$<)
	@cd $(OUT_DIR) && ./test_driver --arch=ga100 > passed.ga100

$(OUT_DIR)/passed.ga102: $(OUT_DIR)/test_driver  $(OUT_DIR)/firmware.elf
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " SIM-GA102" $(subst $(LW_SOURCE)/,,$<)
	@cd $(OUT_DIR) && ./test_driver --arch=ga102 > passed.ga102

diff.tu11x: $(OUT_DIR)/passed.tu11x
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " GOLDEN " $(subst $(LW_SOURCE)/,,$<)
	@diff -w $(OUT_DIR)/passed.tu11x golden.tu11x 
	@touch diff.tu11x

diff.ga100: $(OUT_DIR)/passed.ga100
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " GOLDEN " $(subst $(LW_SOURCE)/,,$<)
	@diff -w $(OUT_DIR)/passed.ga100 golden.ga100
	@touch diff.ga100

diff.ga102: $(OUT_DIR)/passed.ga102
	@printf  " [ %-28s ] $E[92m$E[1m%-14s$E[39m$E[0m  %s\n" $(PREFIX) " GOLDEN " $(subst $(LW_SOURCE)/,,$<)
	@diff -w $(OUT_DIR)/passed.ga102 golden.ga102
	@touch diff.ga102

clean:
	@rm -rf _out/$(BUILD_CFG) || true


