
# Copyright (c) 2020, LWPU CORPORATION.  All Rights Reserved.
#
# LWPU Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from LWPU Corporation
# is strictly prohibited.
#
# Makefile fragment included by MB1 build system
# for building CCC library for T19x.
#
# Subsystem: MB1
# Platfom: T19x
# Version: 0.1
# Contact: kelagandula@lwpu.com, sachinj@lwpu.com, vivekk@lwpu.com
#
# This makefile fragment requires these make variables to be set on call:
#
# CCC_DIR         : Common crypto core source directory
# CCC_PRIVATE_DIR : Common crypto private source directory
#
# This makefile fragment sets these make variables for the calling build system:
#
# CCC_SRC_FILES   : CCC files to compile for MB1
# CCC_HEADER_FILES: CCC header files required by MB1
# CCC_CFLAGS      : Additional flags (mainly for R&D or GVS issues).
#                   Production compilations should not use CCC_CFLAGS unless
#                   there is some specific reason to import CCC_CFLAGS form here.
#
LOCAL__CCC_SUBSYS_NAME := "MB1"

ifeq (,$(CCC_DIR))
$(error "$(LOCAL__CCC_SUBSYS_NAME) CCC makefile fragment expects CCC_DIR value to be set")
endif

# MB1 does not use any private source
#ifeq (,$(CCC_PRIVATE_DIR))
#$(error "$(LOCAL__CCC_SUBSYS_NAME) CCC makefile fragment expects CCC_PRIVATE_DIR value to be set")
#endif

# Default return values
#
CCC_CFLAGS :=
CCC_SRC_FILES :=

CCC_HEADER_FILES := $(CCC_DIR)/crypto_ops.h \
                    $(CCC_DIR)/include/crypto_api.h

# API layer
CCC_SRC_FILES :=  $(CCC_DIR)/crypto_acipher.c \
                  $(CCC_DIR)/crypto_asig.c \
                  $(CCC_DIR)/crypto_cipher.c \
                  $(CCC_DIR)/crypto_context.c \
                  $(CCC_DIR)/crypto_classcall.c \
                  $(CCC_DIR)/crypto_derive.c \
                  $(CCC_DIR)/crypto_mac.c \
                  $(CCC_DIR)/crypto_md.c \
                  $(CCC_DIR)/crypto_random.c \
                  $(CCC_DIR)/tegra_rng1.c

# Initializers, API and process layer support
CCC_SRC_FILES +=  $(CCC_DIR)/crypto_asig_proc.c \
                  $(CCC_DIR)/crypto_akey_proc.c \
                  $(CCC_DIR)/crypto_cipher_proc.c \
                  $(CCC_DIR)/crypto_derive_proc.c \
                  $(CCC_DIR)/crypto_mac_proc.c \
                  $(CCC_DIR)/crypto_md_proc.c \
                  $(CCC_DIR)/crypto_random_proc.c

# Process layer
CCC_SRC_FILES +=  $(CCC_DIR)/crypto_process.c \
                  $(CCC_DIR)/crypto_process_aes_cipher.c \
                  $(CCC_DIR)/crypto_process_aes_mac.c \
                  $(CCC_DIR)/crypto_process_ec.c \
                  $(CCC_DIR)/crypto_process_ed25519.c \
                  $(CCC_DIR)/crypto_process_random.c \
                  $(CCC_DIR)/crypto_process_rsa.c \
                  $(CCC_DIR)/crypto_process_sha.c \
                  $(CCC_DIR)/crypto_process_sign.c \
                  $(CCC_DIR)/crypto_process_signature.c

# Utils, generic
CCC_SRC_FILES +=  $(CCC_DIR)/crypto_util.c \
                  $(CCC_DIR)/context_mem.c

# Engine switch
CCC_SRC_FILES +=  $(CCC_DIR)/tegra_cdev.c \
                  $(CCC_DIR)/tegra_cdev_ecall.c

#
# PKA engine functions
#
CCC_SRC_FILES += $(CCC_DIR)/tegra_pka1_ec.c \
                 $(CCC_DIR)/tegra_pka1_ec_gen.c \
                 $(CCC_DIR)/tegra_pka1_ed25519.c \
                 $(CCC_DIR)/tegra_pka1_gen.c \
                 $(CCC_DIR)/tegra_pka1_mod.c \
                 $(CCC_DIR)/tegra_pka1_rsa.c

# SE engine functions
CCC_SRC_FILES +=  $(CCC_DIR)/tegra_se_aes.c \
                  $(CCC_DIR)/tegra_se_gen.c \
                  $(CCC_DIR)/tegra_se_mac.c \
                  $(CCC_DIR)/tegra_se_sha.c \
                  $(CCC_DIR)/tegra_se_unwrap.c

# Key slot functions
CCC_SRC_FILES +=  $(CCC_DIR)/crypto_ks_pka.c \
                  $(CCC_DIR)/crypto_ks_se.c

# Unset work var
#
LOCAL__CCC_SUBSYS_NAME :=
