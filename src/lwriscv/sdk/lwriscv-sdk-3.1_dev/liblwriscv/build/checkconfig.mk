# Sanity check of configuration. Not sure if it'll be needed permanently.

# MK TODO: this is not very portable :(
ifneq ($(shell $(EXPR) `$(ECHO) $(MAKE_VERSION) | $(SED) -r "s/\.[0-9]+//"` \>= 4),1)
    $(error You must have a make version >= 4)
endif

ifeq ($(wildcard $(LIBLWRISCV_ROOT)),)
    $(error Set LIBLWRISCV_ROOT to where //sw/lwriscv/liblwriscv is checked out.)
endif

ifeq ($(wildcard $(LW_TOOLS)),)
    $(error Set LW_TOOLS to directory where //sw/tools is checked out.)
endif

ifeq ($(wildcard $(LW_MANUALS_ROOT)),)
    $(error Set LW_MANUALS_ROOT to where your engine manuals are (//sw/resman/manuals).)
endif

ifeq ($(wildcard $(LIBLWRISCV_PROFILE_FILE)),)
    $(error Missing profile $(LIBLWRISCV_PROFILE_FILE))
endif

# MK TODO: more checks here

ifeq ($(LWRISCV_FEATURE_SSP),y)
# SSP requires SCP driver enabled for true random stack canary, or
# LWRISCV_FEATURE_SSP_FORCE_SW_CANARY set for user programmed stack canary.
    ifeq ($(LWRISCV_FEATURE_SCP),n)
        ifeq ($(LWRISCV_FEATURE_SSP_FORCE_SW_CANARY),n)
            $(error SSP requires SCP driver enabled, or LWRISCV_FEATURE_SSP_FORCE_SW_CANARY set)
        endif
    endif
endif

ifeq ($(LWRISCV_FEATURE_SCP),y)
    ifeq ($(LWRISCV_HAS_SCP),n)
        $(error Attempting to enable the SCP driver on a Peregrine without an SCP block.)
    endif

    ifeq ($(LWRISCV_IS_CHIP_FAMILY),ampere)
        $(error Attempting to enable the SCP driver on an unsupported chip ($(LWRISCV_IS_CHIP)).)
    endif
endif

ifeq ($(LWRISCV_DRIVER_SCP_FAKE_RNG_ENABLED),y)
    ifeq ($(LWRISCV_FEATURE_SCP),n)
        $(error Attempting to enable fake-RNG support but the SCP driver is disabled.)
    endif
endif

ifeq ($(LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED),y)
    ifeq ($(LWRISCV_FEATURE_SCP),n)
        $(error Attempting to enable shortlwt-DMA support but the SCP driver is disabled.)
    endif

    ifeq ($(LWRISCV_FEATURE_DMA),n)
        $(error Attempting to enable shortlwt-DMA support but the DMA driver is disabled.)
    endif
endif

ifeq ($(LWRISCV_HAS_SBI),n)
    ifeq ($(LWRISCV_FEATURE_FBIF_TFBIF_CFG_VIA_SBI),y)
        $(error Attempting to enable FBIF/TFBIF configuration via SBIs, but SBIs are not supported.)
    endif
endif

$(info Configuration for $(LIBLWRISCV_PROFILE) checked.)
