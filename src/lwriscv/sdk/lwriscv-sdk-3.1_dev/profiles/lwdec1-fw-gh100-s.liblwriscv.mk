# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := lwdec1-fw-gh100-s

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gh100.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/lwdec1.mk

# Use lwstatus as return codes
LWRISCV_CONFIG_USE_STATUS := lwstatus
# Enable MPU driver
LWRISCV_FEATURE_MPU := y

# Add start file and exception handler
LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := y

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# We want more printfs
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

LWRISCV_FEATURE_SSP := y
LWRISCV_FEATURE_SSP_FORCE_SW_CANARY := y
