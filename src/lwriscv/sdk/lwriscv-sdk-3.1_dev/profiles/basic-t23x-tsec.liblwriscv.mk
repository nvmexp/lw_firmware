# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := basic-t23x-tsec

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t234-cpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/tsec.mk

# Include basic app template
include $(DIR_SDK_PROFILE)/basic.liblwriscv-template.mk
