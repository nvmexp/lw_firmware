include $(LIBOS_SOURCE)/kernel/colors.mk
VERBOSE?=0
JOBS=$(shell nproc)

detect-cores:
	@$(MAKE) -j$(JOBS) all && echo "$$ANNOUNCE_PASS" || echo "$$ANNOUNCE_FAIL"

profiles := $(shell ls -d $$LIBOS_SOURCE/profiles/*-* | grep -v gsp-ga10x-debug)

.PHONY: all clean

all:

clean:

define makesubdir
$(1)-all:
	@+$(MAKE) -s -f kernel.mk PEREGRINE_PROFILE=$(shell basename $(1)) PREFIX=kernel-$(shell basename $(1)) -j firmware

all: $(1)-all
endef

define makesubdirclean
$(1)-clean:
	@$(MAKE) -s -f kernel.mk PEREGRINE_PROFILE=$(shell basename $(1)) clean

clean: $(1)-clean
endef
$(foreach PROFILE,$(profiles),$(eval $(call makesubdir,$(PROFILE))))
$(foreach PROFILE,$(profiles),$(eval $(call makesubdirclean,$(PROFILE))))

