# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := lwdec-fw-t234

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t234-cpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/lwdec.mk

# Don't add start file and exception handler
LWRISCV_FEATURE_START := n
LWRISCV_FEATURE_EXCEPTION := y

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

LWRISCV_FEATURE_SSP := y
LWRISCV_FEATURE_SCP := y

# Use lwstatus as return codes
LWRISCV_CONFIG_USE_STATUS := lwstatus

LWRISCV_FEATURE_MPU := y
LWRISCV_FEATURE_DIO := y
