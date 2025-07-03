# Copyright (c) 2021, LWPU CORPORATION.  All Rights Reserved.
#
# LWPU Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from LWPU Corporation
# is strictly prohibited.

include $(LIBCCC_CONFIGS)/common.mk

# TODO: Remove later on when HW gives us manuals
LIBCCC_USE_LOCAL_MANUALS = 1

CONFIG_INCLUDE := $(LIBCCC_INC)/$(LIBCCC_CHIP_FAMILY)/gh100_selite_se/

SELITE_SE_AES_GEN_SRC_FILES +=      \
            tegra_se_gen.c

SELITE_SE_AES_SRC_FILES +=          \
            tegra_se_genrnd.c       \
            tegra_se_kac.c          \
            tegra_se_kac_gcm.c

# TODO: Add other engines in SE-lite SE here

CCC_SRC_FILES += $(addprefix src/common_crypto/,$(SELITE_SE_AES_GEN_SRC_FILES))
CCC_SRC_FILES += $(addprefix src/common_crypto_private/,$(SELITE_SE_AES_SRC_FILES))
