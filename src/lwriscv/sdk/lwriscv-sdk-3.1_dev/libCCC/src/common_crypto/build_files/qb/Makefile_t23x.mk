# Copyright (c) 2021, LWPU CORPORATION.  All Rights Reserved.
#
# LWPU Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from LWPU Corporation
# is strictly prohibited.

# Makefile fragment included by QB build system
# for building CCC library for T23X.
#
# Subsystem: Quickboot
# Platfom: T23x
# Version: 0.1
# Contact: mchourasia@lwpu.com, pkunapuli@lwpu.com
#
# This makefile fragment requires these make variables to be set on call:
#
# CCC_DIR         : Common crypto core source directory
# CCC_PRIVATE_DIR : Common crypto T23x source directory
#
# This makefile fragment sets these make variables for the calling build system:
#
# CCC_SRC_FILES   : CCC files to compile for Quickboot in T23x
# CCC_HEADER_FILES: Not required
# CCC_CFLAGS      : Additional flags (mainly for R&D or GVS issues).
#                   Production compilations should not use CCC_CFLAGS unless
#                   there is some specific reason to import CCC_CFLAGS form here.
#
LOCAL__CCC_SUBSYS_NAME := "QUICKBOOT"

ifeq (,$(CCC_DIR))
$(error "$(LOCAL__CCC_SUBSYS_NAME) CCC makefile fragment expects CCC_DIR value to be set")
endif

ifeq (,$(CCC_PRIVATE_DIR))
$(error "$(LOCAL__CCC_SUBSYS_NAME) CCC makefile fragment expects CCC_PRIVATE_DIR value to be set")
endif

# Default return values
#
CCC_CFLAGS :=
CCC_SRC_FILES :=
CCC_HEADER_FILES :=

# API layer
CCC_SRC_FILES +=  $(CCC_DIR)/crypto_acipher.c \
                  $(CCC_DIR)/crypto_asig.c \
                  $(CCC_DIR)/crypto_ae.c \
                  $(CCC_DIR)/crypto_cipher.c \
                  $(CCC_DIR)/crypto_classcall.c \
                  $(CCC_DIR)/crypto_context.c \
                  $(CCC_DIR)/crypto_derive.c  \
                  $(CCC_DIR)/crypto_mac.c   \
                  $(CCC_DIR)/crypto_md.c  \
                  $(CCC_DIR)/crypto_random.c

# Initializers, API and process layer support
CCC_SRC_FILES +=  $(CCC_DIR)/crypto_asig_proc.c  \
                  $(CCC_DIR)/crypto_akey_proc.c  \
                  $(CCC_DIR)/crypto_cipher_proc.c  \
                  $(CCC_DIR)/crypto_derive_proc.c  \
                  $(CCC_DIR)/crypto_mac_proc.c  \
                  $(CCC_DIR)/crypto_md_proc.c  \
                  $(CCC_DIR)/crypto_random_proc.c

# Process layer
CCC_SRC_FILES +=  $(CCC_DIR)/crypto_process.c   \
                  $(CCC_DIR)/crypto_process_ec.c  \
                  $(CCC_DIR)/crypto_process_ed25519.c  \
                  $(CCC_DIR)/crypto_process_rsa.c  \
                  $(CCC_DIR)/crypto_process_signature.c  \
                  $(CCC_DIR)/crypto_process_sign.c  \
                  $(CCC_DIR)/crypto_process_aes_cipher.c  \
                  $(CCC_DIR)/crypto_process_aes_mac.c  \
                  $(CCC_DIR)/crypto_process_sha.c  \
                  $(CCC_DIR)/crypto_process_random.c  \
                  $(CCC_DIR)/aes-gcm/crypto_process_gcm.c  \
                  $(CCC_DIR)/aes-ccm/crypto_process_ccm.c

# Utils, generic
CCC_SRC_FILES +=  $(CCC_DIR)/crypto_util.c     \
                  $(CCC_DIR)/crypto_hexdump.c  \
                  $(CCC_DIR)/context_mem.c     \
                  $(CCC_DIR)/crypto_measure.c

# Engine switch
CCC_SRC_FILES +=  $(CCC_DIR)/tegra_cdev.c  \
                  $(CCC_DIR)/tegra_cdev_ecall.c

# Shared ECC file (PKA1 and LWPKA share this)
#
CCC_SRC_FILES += $(CCC_DIR)/tegra_pka1_ec_gen.c

# SE engine handlers
CCC_SRC_FILES +=  $(CCC_DIR)/tegra_se_gen.c  \
                  $(CCC_DIR)/tegra_se_sha.c

# Key slot functions
CCC_SRC_FILES +=  $(CCC_DIR)/crypto_ks_pka.c \
                  $(CCC_DIR)/crypto_ks_se.c

# T23X specific
CCC_SRC_FILES +=  $(CCC_PRIVATE_DIR)/tegra_se_kac.c \
                  $(CCC_PRIVATE_DIR)/tegra_se_kac_mac.c \
                  $(CCC_PRIVATE_DIR)/tegra_se_kac_keyop.c \
                  $(CCC_PRIVATE_DIR)/tegra_se_kac_gcm.c \
                  $(CCC_PRIVATE_DIR)/tegra_se_kac_sha_kops.c \
                  $(CCC_PRIVATE_DIR)/crypto_kwuw.c \
                  $(CCC_PRIVATE_DIR)/crypto_process_keyop.c \
                  $(CCC_PRIVATE_DIR)/crypto_process_sha_kdf.c

# T23X random number generators (GENRND/LWRNG)
CCC_SRC_FILES +=  $(CCC_PRIVATE_DIR)/tegra_lwrng.c \
                  $(CCC_PRIVATE_DIR)/tegra_se_genrnd.c

#
# LWPKA engine functions
#
CCC_SRC_FILES +=  $(CCC_PRIVATE_DIR)/lwpka/tegra_lwpka_gen.c \
                  $(CCC_PRIVATE_DIR)/lwpka/tegra_lwpka_rsa.c \
                  $(CCC_PRIVATE_DIR)/lwpka/tegra_lwpka_ec.c \
                  $(CCC_PRIVATE_DIR)/lwpka/tegra_lwpka_edwards.c \
                  $(CCC_PRIVATE_DIR)/lwpka/tegra_lwpka_montgomery.c \
                  $(CCC_PRIVATE_DIR)/lwpka/tegra_lwpka_mod.c

# Unset work var
#
LOCAL__CCC_SUBSYS_NAME :=
