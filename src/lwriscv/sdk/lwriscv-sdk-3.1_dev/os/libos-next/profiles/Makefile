include $(LIBOS_SOURCE)/kernel/colors.mk

ifeq (,$(wildcard /home/utils/gcc-9.3.0))
SIMULATOR_CC=gcc -static 
SIMULATOR_CXX=g++ -static 
else
SIMULATOR_CC=/home/utils/gcc-9.3.0/bin/gcc -static 
SIMULATOR_CXX=/home/utils/gcc-9.3.0/bin/g++ -static
endif

JOBS=$(shell nproc)

detect-cores:
	@$(MAKE) -j$(JOBS) all  && echo "$$ANNOUNCE_PASS" || echo "$$ANNOUNCE_FAIL"

subdirs := $(shell ls -d $$LIBOS_SOURCE/profiles/*-*)

.PHONY: all clean $(subdirs)

all:

clean:

define makesubdir
$(shell basename $(1))/libos-config.h: $(shell basename $(1))/config
$(shell basename $(1))/libos-config.mk: 
	@printf  " [ %-55s ] $(COLOR_GREEN_BRIGHT)$(COLOR_BOLD)%-14s$(COLOR_DEFAULT)$(COLOR_RESET)  %s\n" $(shell basename $(1)) " CONFIG " $<
	@mkdir -p $(shell basename $(1))
	@sed -e 's:#.*::;s/\([^=]*\)=\(.*\)/#define \1 \2/' $(shell basename $(1))/config > $(shell basename $(1))/libos-config.in.h
	@echo '#include "defaults.c"' >> $(shell basename $(1))/libos-config.in.h
	@$(SIMULATOR_CC) -I$(LIBOS_SOURCE)/profiles -dM -P -E $(shell basename $(1))/libos-config.in.h -o $(shell basename $(1))/libos-config.out.h
	@p4 edit $(shell basename $(1))/libos-config.h || true
	@echo "/** GENERATED FILE **/" >$(shell basename $(1))/libos-config.h
	@grep "#define LIBOS_" $(shell basename $(1))/libos-config.out.h >> $(shell basename $(1))/libos-config.h
	@p4 add $(shell basename $(1))/libos-config.h || true
	@p4 edit $(shell basename $(1))/libos-config.mk
	@echo "## GENERATED FILE ##" >$(shell basename $(1))/libos-config.mk
	@grep "#define LIBOS_" $(shell basename $(1))/libos-config.out.h | sed -e 's/#define \([^ ]*\) \(.*\)/\1=\2/' >> $(shell basename $(1))/libos-config.mk
	@p4 add $(shell basename $(1))/libos-config.mk || true
	@rm -f $(shell basename $(1))/libos-config.in.h $(shell basename $(1))/libos-config.out.h

all: $(shell basename $(1))/libos-config.h $(shell basename $(1))/libos-config.mk
clean-$(shell basename $(1)):
	rm -f $(shell basename $(1))/libos-config.h $(shell basename $(1))/libos-config.mk

clean: clean-$(shell basename $(1))
endef


$(foreach SUBDIR,$(subdirs),$(eval $(call makesubdir,$(SUBDIR))))



