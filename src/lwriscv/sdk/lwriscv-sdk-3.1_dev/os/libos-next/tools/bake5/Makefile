PREFIX=bake5
#define LIBOS_TOOL_BAKE           1
TOOL_INCLUDE=-I../../simulator -I. -I../../include/ -I/lwpu/build/sw/lwriscv/main/inc/lwpu-sdk -I/lwpu/build/sw/resman/manuals/ -DLIBOS_TOOL_BAKE

vpath %.cpp .
vpath %.cpp ../../simulator
vpath %.c ../../lib
OUT_DIR=out

include ../../kernel/colors.mk

OBJECTS=$(OUT_DIR)/riscv-core.o \
		$(OUT_DIR)/libelf.o \
		$(OUT_DIR)/libriscv.o \
		$(OUT_DIR)/libdwarf.o \
		$(OUT_DIR)/bake5.o \
		$(OUT_DIR)/riscv-trace.o

bake5: $(OBJECTS)
	g++ $^ $(INC) -o bake5 


DEPENDS = $(patsubst %.o,%.d, $(OBJECTS)) 
$(DEPENDS): ;
.PRECIOUS: $(DEPENDS)
-include $(DEPENDS)

$(OUT_DIR)/%.o : %.cpp 
	@mkdir -p $(OUT_DIR)
	@printf  " [ %-8s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " C++ " $<
	@g++ -c -MT $@ -MMD -MP -MF $(OUT_DIR)/$*.Td --std=c++14 -w -Og -DLIBOS_HOST -DTRACE $(TOOL_INCLUDE) -g $< -o $@
	@mv -f $(OUT_DIR)/$*.Td $(OUT_DIR)/$*.d > /dev/null

$(OUT_DIR)/%.o : %.c 
	@mkdir -p $(OUT_DIR)
	@printf  " [ %-8s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(PREFIX) " C " $<
	@gcc -c -MT $@ -MMD -MP -MF $(OUT_DIR)/$*.Td --std=c11 -w -Og -DLIBOS_HOST -DTRACE $(TOOL_INCLUDE) -g $< -o $@
	@mv -f $(OUT_DIR)/$*.Td $(OUT_DIR)/$*.d > /dev/null

test.elf: test.c
	riscv64-unknown-elf-gcc -nostdlib test.c -o test.elf

clean:
	rm -rf $(OUT_DIR)
