include $(LIBOS_SOURCE)/kernel/colors.mk
VERBOSE?=0
JOBS=$(shell nproc)

detect-cores:
	@$(MAKE) -j$(JOBS) all && echo "$$ANNOUNCE_PASS" || echo "$$ANNOUNCE_FAIL"

test_queue := $(shell ls -d *_*)

## BUGBUG: Disable GA10x profile as support for that chip isn't complete.
profiles := $(shell ls -d $$LIBOS_SOURCE/profiles/*-* | grep -v gsp-ga10x-debug)

.PHONY: all clean $(test_queue)

all:

clean:

define makesubdir
$(1)-$(2):
	@+$(MAKE) -s -C $(1) PEREGRINE_PROFILE=$(shell basename $(2)) PREFIX=$(shell basename $(1))/$(shell basename $(2))  

all: $(1)-$(2)
endef

define makesubdirclean
$(1)-$(2)-clean:
	@$(MAKE) -s -C $(1) PEREGRINE_PROFILE=$(shell basename $(2)) clean

clean: $(1)-$(2)-clean
endef

$(foreach PROFILE, $(profiles),$(foreach SUBDIR, $(shell cat $(PROFILE)/tests),$(eval $(call makesubdir,$(SUBDIR),$(PROFILE)))))
$(foreach PROFILE, $(profiles),$(foreach SUBDIR, $(shell cat $(PROFILE)/tests),$(eval $(call makesubdirclean,$(SUBDIR),$(PROFILE)))))

#$(foreach SUBDIR, $(test_queue),$(foreach PROFILE, $(profiles),$(eval $(call makesubdir,$(SUBDIR),$(PROFILE)))))
#$(foreach SUBDIR, $(test_queue),$(foreach PROFILE, $(profiles),$(eval $(call makesubdirclean,$(SUBDIR),$(PROFILE)))))
