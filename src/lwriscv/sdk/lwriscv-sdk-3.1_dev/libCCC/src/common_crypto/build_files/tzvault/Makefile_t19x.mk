# Copyright (c) 2020, LWPU CORPORATION.  All Rights Reserved.
#
# LWPU Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from LWPU Corporation
# is strictly prohibited.

# Makefile fragment included by tzvault build system for building CCC library
#
# Subsystem: tzvault
# Platfom:
# Version: 0.1
# Contact:
#
# This makefile fragment requires these make variables to be set on call:
#
# CCC_DIR         : Common crypto core source directory
# CCC_PRIVATE_DIR : Common crypto T23x source directory
#
# This makefile fragment sets these make variables for the calling build system:
#
# CCC_SRC_FILES   : CCC files to compile for tzvault
# CCC_HEADER_FILES: Not required by PSC FW lwrrently, will be set later (!!!)
# CCC_CFLAGS      : Additional flags (mainly for R&D or GVS issues).
#                   Production compilations should not use CCC_CFLAGS unless
#                   there is some specific reason to import CCC_CFLAGS form here.
#
LOCAL__CCC_SUBSYS_NAME := "TZVAULT"

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

CCC_SRC_FILES +=  $(CCC_DIR)/crypto_acipher.c \
                  $(CCC_DIR)/crypto_asig.c \
                  $(CCC_DIR)/crypto_cipher.c \
                  $(CCC_DIR)/crypto_context.c \
                  $(CCC_DIR)/crypto_derive.c \
                  $(CCC_DIR)/crypto_mac.c \
                  $(CCC_DIR)/crypto_md.c \
                  $(CCC_DIR)/crypto_process.c \
                  $(CCC_DIR)/crypto_process_ec.c \
                  $(CCC_DIR)/crypto_process_rsa.c \
                  $(CCC_DIR)/crypto_process_signature.c \
                  $(CCC_DIR)/crypto_process_sign.c \
                  $(CCC_DIR)/crypto_random.c \
                  $(CCC_DIR)/crypto_util.c \
                  $(CCC_DIR)/tegra_cdev.c \
                  $(CCC_DIR)/tegra_pka1_ec.c \
                  $(CCC_DIR)/tegra_pka1_ec_gen.c \
                  $(CCC_DIR)/tegra_pka1_gen.c \
                  $(CCC_DIR)/tegra_pka1_mod.c \
                  $(CCC_DIR)/tegra_pka1_rsa.c \
                  $(CCC_DIR)/aes-ccm/crypto_process_ccm.c \
                  $(CCC_DIR)/crypto_ae.c \
                  $(CCC_DIR)/tegra_se.c \
                  $(CCC_DIR)/crypto_process_ed25519.c \
                  $(CCC_DIR)/tegra_pka1_ed25519.c \
                  $(CCC_DIR)/tegra_rng1.c \
                  $(CCC_DIR)/tegra_pka1_ks_rw.c

# Unset work var
LOCAL__CCC_SUBSYS_NAME :=
