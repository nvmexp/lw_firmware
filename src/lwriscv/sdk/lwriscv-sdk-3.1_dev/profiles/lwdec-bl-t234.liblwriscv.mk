# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := lwdec-bl-t234

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t234-cpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/lwdec.mk

LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 0

LWRISCV_FEATURE_START := y

LWRISCV_FEATURE_SSP := y
LWRISCV_FEATURE_SCP := y
LWRISCV_FEATURE_DIO := y
LWRISCV_FEATURE_SHA := y
