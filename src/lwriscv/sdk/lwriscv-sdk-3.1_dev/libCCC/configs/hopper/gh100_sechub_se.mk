# Copyright (c) 2020, LWPU CORPORATION.  All Rights Reserved.
#
# LWPU Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from LWPU Corporation
# is strictly prohibited.

include $(LIBCCC_CONFIGS)/common.mk

# TODO: Remove later on when HW gives us manuals
LIBCCC_USE_LOCAL_MANUALS = 1

CONFIG_INCLUDE := $(LIBCCC_INC)/$(LIBCCC_CHIP_FAMILY)/gh100_sechub_se/

# Add LWPKA for sechub SE
SECHUB_SE_LWPKA_GEN_SRC_FILES +=  \
            tegra_pka1_ec_gen.c   \
            crypto_asig_proc.c

SECHUB_SE_LWPKA_SRC_FILES +=      \
            tegra_lwpka_gen.c     \
            tegra_lwpka_rsa.c     \
            tegra_lwpka_ec.c      \
            tegra_lwpka_edwards.c \
            tegra_lwpka_mod.c     \
            lwpka_ec_ks_ops.c

SECHUB_SE_LWRNG_SRC_FILES +=      \
            tegra_lwrng.c         \
            tegra_lwrng_init.c

# TODO: Add other engines in sechub SE here

CCC_SRC_FILES += $(addprefix src/common_crypto/,$(SECHUB_SE_LWPKA_GEN_SRC_FILES))
CCC_SRC_FILES += $(addprefix src/common_crypto_private/lwpka/,$(SECHUB_SE_LWPKA_SRC_FILES))
CCC_SRC_FILES += $(addprefix src/common_crypto_private/,$(SECHUB_SE_LWRNG_SRC_FILES))
