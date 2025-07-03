#
# _LWRM_COPYRIGHT_BEGIN_
#
# Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# _LWRM_COPYRIGHT_END_
#

##############################################################################
# Build a list of all possible gsp-config profiles
##############################################################################

PROFILES_ALL ?= gh100 gh100_fmodel gh100_gsprm gh100_resident gh100_resident_fmodel gh100_spdm

##############################################################################
# Profile-specific settings
##############################################################################

ifdef PROFILE
    MANUAL_PATHS          =
    UPROC_ARCH            = lwriscv
    ENGINE                = gsp
    USE_LIBCCC            = true
    USE_LIBSPDM           = false
    USE_LIBRTS            = false
    CC_IS_RESIDENT        = false
    PARTITIONS            = init acr shared attestation spdm rm
    SK_PARTITION_COUNT    = 5
    LW_MANUALS_ROOT       = $(LW_SOURCE)/drivers/common/inc/hwref
    LWRISCV_SDK_VERSION   = lwriscv-sdk-3.0_dev
    FWS_SIGN_PROFILE      = GH100_GSP_RMCC
    CC_FMODEL_BUILD       = false
    CC_GSPRM_BUILD        = false
    # Toggle that to permit printing from all partitions (not only gsp-rm-proxy)
    # Usage is allowed for debug purposes *IN LOCAL CLs* only
    # To make it work properly, remember to disable lockdown by changing 
    # Selwre_Partition_Lockdown in policies
    CC_PERMIT_DEBUG       = false

 # Shared gh100 settings
    ifneq (,$(findstring gh100, $(PROFILE)))
        CHIP                = gh100
        ARCH                = hopper
        MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/swref/hopper/gh100
        MANUAL_PATHS        += $(LW_SOURCE)/drivers/common/inc/hwref/hopper/gh100
        SIGGEN_CHIP         = gh100_rsa3k_0
        LWRISCV_SDK_PROFILE = gsp-rm-proxy-gh100
    endif

    # Shared fmodel settings
    ifneq (,$(findstring fmodel, $(PROFILE)))
        CC_FMODEL_BUILD     = true
    endif

    ifeq ("$(PROFILE)","gh100_fmodel") 
        PROJ                = gh100_fmodel
    endif

    ifeq ("$(PROFILE)","gh100")
        PROJ                = gh100
    endif

    ifeq ("$(PROFILE)","gh100_resident")
        PROJ                     = gh100_resident
        CC_IS_RESIDENT           = true
        PARTITIONS              := $(subst rm,rm_proxy,$(PARTITIONS))
        ACRLIB_PROJ_OVERRIDE     = gh100
        MANIFEST_OVERRIDE        = gh100
        POLICY_OVERRIDE          = gh100
    endif

    ifeq ("$(PROFILE)","gh100_gsprm")
        PROJ                = gh100_gsprm
        CC_GSPRM_BUILD      = true
    endif

    ifeq ("$(PROFILE)","gh100_resident_fmodel")
        PROJ                    = gh100_resident_fmodel
        CC_IS_RESIDENT          = true
        USE_LIBSPDM             = true
        PARTITIONS             := $(subst rm,rm_proxy,$(PARTITIONS))
        ACRLIB_PROJ_OVERRIDE    = gh100_fmodel
        MANIFEST_OVERRIDE       = gh100_fmodel
        POLICY_OVERRIDE         = gh100_fmodel
    endif

 #####################################################################################
 # Includes libspdm in build when USE_LIBSPDM is true.
 # For release branch, must have SWIPAT approval and have RM attributions in place.
 #####################################################################################
   ifeq ("$(PROFILE)","gh100_spdm")
      PROJ                   = gh100_spdm
      PARTITIONS             := $(subst rm,rm_proxy,$(PARTITIONS))
      USE_LIBSPDM            = true
      CC_IS_RESIDENT         = true
      ACRLIB_PROJ_OVERRIDE   = gh100
      MANIFEST_OVERRIDE      = gh100
      POLICY_OVERRIDE        = gh100
   endif

endif
