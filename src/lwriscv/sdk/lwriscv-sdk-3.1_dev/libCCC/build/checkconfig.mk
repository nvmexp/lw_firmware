# Sanity check of configuration. Not sure if it'll be needed permanently.

ifneq ($(shell expr `echo $(MAKE_VERSION) | sed -r "s/\.[0-9]+//"` \>= 4),1)
$(error You must have a make version >= 4)
endif

ifeq ($(wildcard $(LIBCCC_ROOT)),)
$(error Set LIBCCC_ROOT to where //sw/lwriscv/main/libCCC is checked out.)
endif

ifeq ($(wildcard $(LW_TOOLS)),)
$(error Set LW_TOOLS to directory where //sw/tools is checked out.)
endif

ifeq ($(wildcard $(LIBCCC_PROFILE_FILE)),)
$(error Missing profile $(LIBCCC_PROFILE_FILE))
endif

# Add more checks here if needed

$(info Configuration for $(LIBCCC_PROFILE) checked.)
