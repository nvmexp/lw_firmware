include $(LIBOS_SOURCE)/kernel/colors.mk
JOBS=$(shell nproc)

detect-cores:
	@$(MAKE) -j$(JOBS) all  && echo "$$ANNOUNCE_PASS" || echo "$$ANNOUNCE_FAIL"

subdirs := $(shell ls -d $$LIBOS_SOURCE/profiles/*-*)

.PHONY: all clean $(subdirs)

all:

clean:

define makesubdir
$(1):
	@+$(MAKE) -s -f libosmc.mk PEREGRINE_PROFILE=$(shell basename $(1))

all: $(1)
endef

define makesubdirclean
$(1)-clean:
	@+$(MAKE) -s -f libosmc.mk PEREGRINE_PROFILE=$(shell basename $(1)) clean

clean: $(1)-clean
endef

$(foreach SUBDIR,$(subdirs),$(eval $(call makesubdir,$(SUBDIR))))
$(foreach SUBDIR,$(subdirs),$(eval $(call makesubdirclean,$(SUBDIR))))
