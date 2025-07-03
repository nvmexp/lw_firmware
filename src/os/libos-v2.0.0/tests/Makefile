VERBOSE?=0
JOBS=$(shell nproc)
E=\e

define ANNOUNCE_FAIL
$E[31m                       
    #######    #    ### #		
    #         # #    #  #		
    #        #   #   #  #		
    #####   #     #  #  #		
    #       #######  #  #		
    #       #     #  #  #		
    #       #     # ### #######	

Consider running ./regolden.sh to regolden the test results

endef
export ANNOUNCE_FAIL

define ANNOUNCE_PASS
$E[32m
    ######     #     #####   #####
    #     #   # #   #     # #     #
    #     #  #   #  #       #
    ######  #     #  #####   #####
    #       #######       #       #
    #       #     # #     # #     #
    #       #     #  #####   #####

endef
export ANNOUNCE_PASS

detect-cores:
	@$(MAKE) -s -j$(JOBS) all && echo "$$ANNOUNCE_PASS" || echo "$$ANNOUNCE_FAIL"

subdirs := $(shell ls -d *_*)

.PHONY: all clean $(subdirs)

all:

clean:

define makesubdir
$(1):
	@+$(MAKE) -s -C $(1) PREFIX=$(basename $(1)) 

all: $(1)
endef

define makesubdirclean
$(1)-clean:
	@$(MAKE) -s -C $(1) clean

clean: $(1)-clean
endef

$(foreach SUBDIR,$(subdirs),$(eval $(call makesubdir,$(SUBDIR))))
$(foreach SUBDIR,$(subdirs),$(eval $(call makesubdirclean,$(SUBDIR))))
