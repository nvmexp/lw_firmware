# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := minion-fw-gh100

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gh100.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/minion.mk

LWRISCV_FEATURE_START := n
LWRISCV_FEATURE_EXCEPTION := n

# No FB access
LWRISCV_WAR_FBIF_IS_DEAD := y

# Enable most of the prints
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

LWRISCV_FEATURE_SSP := y
LWRISCV_FEATURE_SCP := y
