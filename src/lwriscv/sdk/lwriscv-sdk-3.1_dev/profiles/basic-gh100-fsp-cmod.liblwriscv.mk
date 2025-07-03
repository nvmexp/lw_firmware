# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := basic-gh100-fsp-cmod

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gh100.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/fsp.mk

# Include basic app template
include $(DIR_SDK_PROFILE)/basic.liblwriscv-template.mk

# include this after setting LIBLWRISCV_PROFILE. It uses this variable.
include $(LIBLWRISCV_CONFIGS)/cmod.mk
