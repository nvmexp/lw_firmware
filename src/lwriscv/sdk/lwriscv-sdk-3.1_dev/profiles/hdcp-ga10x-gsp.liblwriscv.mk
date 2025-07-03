# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := hdcp-ga10x-gsp

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/ga10x.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/gsp.mk

# Don't add start file and exception handler
LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := y

# We want moar printfs
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

#LWRISCV_FEATURE_SSP := y   # requires SCP
#LWRISCV_FEATURE_SCP := y   # requires work to support
LWRISCV_FEATURE_SHA := y
LWRISCV_FEATURE_MPU := y
