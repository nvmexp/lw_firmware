# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := basic-sepkern-t23x-lwdec

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t234-cpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/lwdec.mk

# Include basic-sepkern app template
include $(DIR_SDK_PROFILE)/basic-sepkern.liblwriscv-template.mk
