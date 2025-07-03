# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := hdcp-t234-tsec

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t234-cpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/tsec.mk

# Add start file and exception handler
LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := y

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

LWRISCV_FEATURE_SSP := y
LWRISCV_FEATURE_SCP := y
LWRISCV_FEATURE_SHA := y
LWRISCV_FEATURE_MPU := y
