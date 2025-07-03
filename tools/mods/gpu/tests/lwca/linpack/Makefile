#
# LWIDIA_COPYRIGHT_BEGIN
#
# Copyright 2012-2018,2021 by LWPU Corporation.  All rights reserved.  All
# information contained herein is proprietary and confidential to LWPU
# Corporation.  Any use, reproduction, or disclosure without the written
# permission of LWPU Corporation is prohibited.
#
# LWIDIA_COPYRIGHT_END
#

# List kernels by SM and then include lwdaarchs.inc to include the kernels 
# supported in the current branch in the "kernels" rule
#
# Note that this list must be established before including lwdaarchs.inc
MODS_DIR ?= ../../../..

LWBINS_DIR ?= ../../traces

sm_50_kernels := $(LWBINS_DIR)/dpstrs50.lwbin \
                 $(LWBINS_DIR)/linpack50.lwbin \
                 $(LWBINS_DIR)/arch_dgemm_nt50.lwbin \
                 $(LWBINS_DIR)/arch_sgemm_nt50.lwbin

sm_52_kernels := $(LWBINS_DIR)/dpstrs52.lwbin \
                 $(LWBINS_DIR)/linpack52.lwbin

sm_53_kernels := $(LWBINS_DIR)/dpstrs53.lwbin \
                 $(LWBINS_DIR)/linpack53.lwbin \
                 $(LWBINS_DIR)/arch_hgemm_nt53.lwbin \
                 $(LWBINS_DIR)/arch_cliff_A10_nt53.lwbin \
                 $(LWBINS_DIR)/arch_cliff_A30_nt53.lwbin

sm_60_kernels := $(LWBINS_DIR)/dpstrs60.lwbin \
                 $(LWBINS_DIR)/linpack60.lwbin \
                 $(LWBINS_DIR)/arch_dgemm_nt60.lwbin \
                 $(LWBINS_DIR)/arch_sgemm_nt60.lwbin \
                 $(LWBINS_DIR)/arch_hgemm_nt60.lwbin \
                 $(LWBINS_DIR)/arch_cliff_A10_nt60.lwbin \
                 $(LWBINS_DIR)/arch_cliff_A30_nt60.lwbin

sm_61_kernels := $(LWBINS_DIR)/linpack61.lwbin \
                 $(LWBINS_DIR)/arch_igemm_nt61.lwbin

sm_62_kernels := $(LWBINS_DIR)/dpstrs62.lwbin  \
                 $(LWBINS_DIR)/linpack62.lwbin

sm_70_kernels := $(LWBINS_DIR)/dpstrs70.lwbin \
                 $(LWBINS_DIR)/linpack70.lwbin \
                 $(LWBINS_DIR)/arch_dgemm_nt70.lwbin \
                 $(LWBINS_DIR)/arch_sgemm_nt70.lwbin \
                 $(LWBINS_DIR)/arch_hgemm_nt70.lwbin \
                 $(LWBINS_DIR)/arch_igemm_nt70.lwbin \
                 $(LWBINS_DIR)/arch_cliff_A30_nt70.lwbin \
                 $(LWBINS_DIR)/arch_h884gemm_256x128_nt70.lwbin \
                 $(LWBINS_DIR)/arch_h884gemm_128x128_nt70.lwbin \
                 $(LWBINS_DIR)/arch_fp16_s884gemm_fp16_256x128_nt70.lwbin \
                 $(LWBINS_DIR)/arch_fp16_s884gemm_fp16_128x128_nt70.lwbin

sm_72_kernels := $(LWBINS_DIR)/dpstrs72.lwbin \
                 $(LWBINS_DIR)/linpack72.lwbin \
                 $(LWBINS_DIR)/arch_int8_i8816gemm_256x128_nt72.lwbin \
                 $(LWBINS_DIR)/arch_int8_i8816gemm_128x128_nt72.lwbin \
                 $(LWBINS_DIR)/arch_int32_i8816gemm_256x128_nt72.lwbin \
                 $(LWBINS_DIR)/arch_int32_i8816gemm_128x128_nt72.lwbin

sm_75_kernels := $(LWBINS_DIR)/dpstrs75.lwbin \
                 $(LWBINS_DIR)/linpack75.lwbin \
                 $(LWBINS_DIR)/arch_dgemm_nt75.lwbin \
                 $(LWBINS_DIR)/arch_sgemm_nt75.lwbin \
                 $(LWBINS_DIR)/arch_hgemm_nt75.lwbin \
                 $(LWBINS_DIR)/arch_igemm_nt75.lwbin \
                 $(LWBINS_DIR)/arch_int8_i8816gemm_256x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_int8_i8816gemm_128x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_int32_i8816gemm_256x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_int32_i8816gemm_128x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_h884gemm_256x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_h884gemm_128x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_h1688gemm_256x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_h1688gemm_128x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_fp16_s884gemm_fp16_256x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_fp16_s884gemm_fp16_128x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_fp16_s1688gemm_fp16_256x128_nt75.lwbin \
                 $(LWBINS_DIR)/arch_fp16_s1688gemm_fp16_128x128_nt75.lwbin

sm_80_kernels := $(LWBINS_DIR)/dpstrs80.lwbin \
                 $(LWBINS_DIR)/linpack80.lwbin \
                 $(LWBINS_DIR)/arch_dgemm_nt80.lwbin \
                 $(LWBINS_DIR)/arch_sgemm_nt80.lwbin \
                 $(LWBINS_DIR)/arch_hgemm_nt80.lwbin \
                 $(LWBINS_DIR)/arch_igemm_nt80.lwbin \
                 $(LWBINS_DIR)/arch_int8_i16832gemm_256x128_nt80.lwbin \
                 $(LWBINS_DIR)/arch_int8_i16832gemm_128x128_nt80.lwbin \
                 $(LWBINS_DIR)/arch_int32_i16832gemm_256x128_nt80.lwbin \
                 $(LWBINS_DIR)/arch_int32_i16832gemm_128x128_nt80.lwbin \
                 $(LWBINS_DIR)/arch_h16816gemm_256x128_nt80.lwbin \
                 $(LWBINS_DIR)/arch_h16816gemm_128x128_nt80.lwbin \
                 $(LWBINS_DIR)/arch_fp16_s16816gemm_fp16_256x128_nt80.lwbin \
                 $(LWBINS_DIR)/arch_fp16_s16816gemm_fp16_128x128_nt80.lwbin

sm_86_kernels := $(LWBINS_DIR)/dpstrs86.lwbin \
                 $(LWBINS_DIR)/linpack86.lwbin

sm_87_kernels := $(LWBINS_DIR)/dpstrs87.lwbin \
                 $(LWBINS_DIR)/linpack87.lwbin

sm_89_kernels := $(LWBINS_DIR)/dpstrs89.lwbin \
                 $(LWBINS_DIR)/linpack89.lwbin

sm_90_kernels := $(LWBINS_DIR)/dpstrs90.lwbin \
                 $(LWBINS_DIR)/linpack90.lwbin

# Set LWDA_KERNEL_COMPILE to true for extra functionality from lwdaarchs.inc
LWDA_KERNEL_COMPILE := true

# Including lwdaarchs.inc creates default rules for building and copying lwbins
# as well as a default call for the make kernel bin command
#
# Variables used by lwdaarchs.inc that can be lwstomized by setting explicit 
# values for them before including lwdaarchs.inc:
#
#     LWBINS_DIR: Directory where lwbin files are copied to
#                 (default = ../../traces)
#     make_dir  : Directory where make_kernel_bin.py resides
#                 (default = ../)
#     mach      : Default machine type used in compiles (32 or 64), can be 
#                 overridden on a per-target basis
#                 (default = 64)
#     make_opts : Default make options used when calling make_kernel_bin.py
#                 (default = "")

include ../lwdaarchs.inc

all : kernels
ifneq "$(LWBINS_DIR)" "../../traces"
	$(Q)$(copy) -u ../../traces/arch_?gemm_*.lwbin "$(LWBINS_DIR)"
endif

$(LWBIN_ARTIFACTS_DIR)/dpstrs%.lwbin: dpstress.lw 
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l dpstress -s dpstrs
$(LWBIN_ARTIFACTS_DIR)/dpstrs%.lwbin: make_opts := --lwcc-options="-maxrregcount 24"

# Linpack lwbin with helper functions
$(LWBIN_ARTIFACTS_DIR)/linpack%.lwbin: linpack.lw linpack.h
	$(Q)echo Building $(@F)
	$(Q)$(make_kernel_bin) -l linpack -s linpack
$(LWBIN_ARTIFACTS_DIR)/linpack%.lwbin:  make_opts := --disasm-options=-ndf
$(LWBIN_ARTIFACTS_DIR)/linpack53.lwbin: make_opts := --disasm-options=-ndf --lwcc-options="-maxrregcount 32"
$(LWBIN_ARTIFACTS_DIR)/linpack62.lwbin: make_opts := --disasm-options=-ndf --lwcc-options="-maxrregcount 32"
$(LWBIN_ARTIFACTS_DIR)/linpack72.lwbin: make_opts := --disasm-options=-ndf --lwcc-options="-maxrregcount 32"

################################
# Arch GEMM Kernel Definitions #
################################

# DGEMM
$(LWBIN_ARTIFACTS_DIR)/arch_dgemm_nt50.lwbin: maxwell_dgemm_128x64_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_dgemm_nt60.lwbin: maxwell_dgemm_128x64_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_dgemm_nt70.lwbin:   volta_dgemm_128x64_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_dgemm_nt75.lwbin:  turing_dgemm_128x64_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_dgemm_nt80.lwbin:  ampere_dgemm_128x64_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_dgemm_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# SGEMM
$(LWBIN_ARTIFACTS_DIR)/arch_sgemm_nt50.lwbin: maxwell_sgemm_128x128_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_sgemm_nt60.lwbin: maxwell_sgemm_128x128_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_sgemm_nt70.lwbin:   volta_sgemm_128x128_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_sgemm_nt75.lwbin:  turing_sgemm_128x128_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_sgemm_nt80.lwbin:  ampere_sgemm_128x128_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_sgemm_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# HGEMM
$(LWBIN_ARTIFACTS_DIR)/arch_hgemm_nt53.lwbin: maxwell_hgemm_256x128_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_hgemm_nt60.lwbin: maxwell_hgemm_256x128_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_hgemm_nt70.lwbin:   volta_hgemm_256x128_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_hgemm_nt75.lwbin:  turing_hgemm_256x128_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_hgemm_nt80.lwbin:  ampere_hgemm_256x128_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_hgemm_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# IGEMM
$(LWBIN_ARTIFACTS_DIR)/arch_igemm_nt61.lwbin: maxwell_igemm_int8_128x128_ldg4_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_igemm_nt70.lwbin:   volta_igemm_int8_128x128_ldg4_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_igemm_nt75.lwbin:  turing_igemm_int8_128x128_ldg4_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_igemm_nt80.lwbin:  ampere_igemm_int8_128x128_ldg4_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_igemm_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# i8816_256 INT8 output
$(LWBIN_ARTIFACTS_DIR)/arch_int8_i8816gemm_256x128_nt72.lwbin:  volta_int8_i8816gemm_int8_256x128_ldg16_relu_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int8_i8816gemm_256x128_nt75.lwbin: turing_int8_i8816gemm_int8_256x128_ldg16_relu_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int8_i8816gemm_256x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# i8816_128 INT8 output
$(LWBIN_ARTIFACTS_DIR)/arch_int8_i8816gemm_128x128_nt72.lwbin:  volta_int8_i8816gemm_int8_128x128_ldg16_relu_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int8_i8816gemm_128x128_nt75.lwbin: turing_int8_i8816gemm_int8_128x128_ldg16_relu_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int8_i8816gemm_128x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# i8816_256 INT32 output
$(LWBIN_ARTIFACTS_DIR)/arch_int32_i8816gemm_256x128_nt72.lwbin:  volta_int32_i8816gemm_int8_256x128_ldg16_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int32_i8816gemm_256x128_nt75.lwbin: turing_int32_i8816gemm_int8_256x128_ldg16_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int32_i8816gemm_256x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# i8816_128 INT32 output
$(LWBIN_ARTIFACTS_DIR)/arch_int32_i8816gemm_128x128_nt72.lwbin:  volta_int32_i8816gemm_int8_128x128_ldg16_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int32_i8816gemm_128x128_nt75.lwbin: turing_int32_i8816gemm_int8_128x128_ldg16_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int32_i8816gemm_128x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# i16832_256 INT8 output
$(LWBIN_ARTIFACTS_DIR)/arch_int8_i16832gemm_256x128_nt80.lwbin: ampere_int8_i16832gemm_int8_256x128_ldg16_relu_mods_stages_64x3_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int8_i16832gemm_256x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# i16832_128 INT8 output
$(LWBIN_ARTIFACTS_DIR)/arch_int8_i16832gemm_128x128_nt80.lwbin: ampere_int8_i16832gemm_int8_128x128_ldg16_relu_mods_stages_64x3_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int8_i16832gemm_128x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# i16832_256 INT32 output
$(LWBIN_ARTIFACTS_DIR)/arch_int32_i16832gemm_256x128_nt80.lwbin: ampere_int32_i16832gemm_int8_256x128_ldg16_mods_stages_64x3_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int32_i16832gemm_256x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# i16832_128 INT32 output
$(LWBIN_ARTIFACTS_DIR)/arch_int32_i16832gemm_128x128_nt80.lwbin: ampere_int32_i16832gemm_int8_128x128_ldg16_mods_stages_64x3_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_int32_i16832gemm_128x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# Cliff A10
$(LWBIN_ARTIFACTS_DIR)/arch_cliff_A10_nt53.lwbin: maxwell_sgemm_64x64_voltageSpike256_nt_nosync.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_cliff_A10_nt60.lwbin: maxwell_sgemm_64x64_voltageSpike256_nt_nosync.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_cliff_A10_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# Cliff A30
$(LWBIN_ARTIFACTS_DIR)/arch_cliff_A30_nt53.lwbin: maxwell_sgemm_128x128_voltageSpike472_0_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_cliff_A30_nt60.lwbin: maxwell_sgemm_128x128_voltageSpike472_0_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_cliff_A30_nt70.lwbin: volta_sgemm_128x128_voltageSpike224_32_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_cliff_A30_nt%.lwbin: 
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# h884_256
$(LWBIN_ARTIFACTS_DIR)/arch_h884gemm_256x128_nt70.lwbin: volta_h884gemm_256x128_ldg8_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_h884gemm_256x128_nt75.lwbin: turing_h884gemm_256x128_ldg8_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_h884gemm_256x128_nt%.lwbin: 
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# h884_128
$(LWBIN_ARTIFACTS_DIR)/arch_h884gemm_128x128_nt70.lwbin: volta_h884gemm_128x128_ldg8_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_h884gemm_128x128_nt75.lwbin: turing_h884gemm_128x128_ldg8_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_h884gemm_128x128_nt%.lwbin: 
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# s884_256
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s884gemm_fp16_256x128_nt70.lwbin:  volta_fp16_s884gemm_fp16_256x128_ldg8_f2f_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s884gemm_fp16_256x128_nt75.lwbin: turing_fp16_s884gemm_fp16_256x128_ldg8_f2f_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s884gemm_fp16_256x128_nt%.lwbin: 
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# s884_128
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s884gemm_fp16_128x128_nt70.lwbin:  volta_fp16_s884gemm_fp16_128x128_ldg8_f2f_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s884gemm_fp16_128x128_nt75.lwbin: turing_fp16_s884gemm_fp16_128x128_ldg8_f2f_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s884gemm_fp16_128x128_nt%.lwbin: 
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# h1688_256
$(LWBIN_ARTIFACTS_DIR)/arch_h1688gemm_256x128_nt75.lwbin: turing_h1688gemm_256x128_ldg8_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_h1688gemm_256x128_nt%.lwbin: 
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# h1688_128
$(LWBIN_ARTIFACTS_DIR)/arch_h1688gemm_128x128_nt75.lwbin: turing_h1688gemm_128x128_ldg8_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_h1688gemm_128x128_nt%.lwbin: 
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# s1688_256
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s1688gemm_fp16_256x128_nt75.lwbin: turing_fp16_s1688gemm_fp16_256x128_ldg8_f2f_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s1688gemm_fp16_256x128_nt%.lwbin: 
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# s1688_128
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s1688gemm_fp16_128x128_nt75.lwbin: turing_fp16_s1688gemm_fp16_128x128_ldg8_f2f_mods_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s1688gemm_fp16_128x128_nt%.lwbin: 
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# h16816_256
$(LWBIN_ARTIFACTS_DIR)/arch_h16816gemm_256x128_nt80.lwbin: ampere_h16816gemm_256x128_ldg8_mods_stages_64x3_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_h16816gemm_256x128_nt%.lwbin: 
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# h16816_128
$(LWBIN_ARTIFACTS_DIR)/arch_h16816gemm_128x128_nt80.lwbin: ampere_h16816gemm_128x128_ldg8_mods_stages_64x3_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_h16816gemm_128x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# s16816_256
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s16816gemm_fp16_256x128_nt80.lwbin: ampere_fp16_s16816gemm_fp16_256x128_ldg8_f2f_mods_stages_64x3_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s16816gemm_fp16_256x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

# s16816_128
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s16816gemm_fp16_128x128_nt80.lwbin: ampere_fp16_s16816gemm_fp16_128x128_ldg8_f2f_mods_stages_64x3_nt.lwasm
$(LWBIN_ARTIFACTS_DIR)/arch_fp16_s16816gemm_fp16_128x128_nt%.lwbin:
	$(Q)echo Building $(@F)
	$(make_kernel_bin) --lwasm-options="-novliw -gen-opext=no" -l $(<:.lwasm=) -s $(@F:$(*F).lwbin=)

clean:
	$(Q)echo Clean
	$(Q)! test -d $(LWBIN_ARTIFACTS_DIR) || rm -f $(LWBIN_ARTIFACTS_DIR)/*.{lwbin,ptx,sass,report}

.PHONY: clean
