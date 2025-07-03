# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := gfw-gh100-sec-fw

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gh100.mk
# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk
# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/sec.mk

# Don't add start file and exception handler
LWRISCV_FEATURE_START := n
LWRISCV_FEATURE_EXCEPTION := n

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# We want moar printfs
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y

LWRISCV_FEATURE_MPU := y
LWRISCV_FEATURE_DIO := y
LWRISCV_FEATURE_SHA := y
LWRISCV_FEATURE_SCP := y
LWRISCV_FEATURE_SSP := y
LWRISCV_FEATURE_SSP_ENABLE_FAIL_HOOK := y
# This WAR should be removed once you're done with migration
LWRISCV_CONFIG_SSP_RANDOM_CANARY := 17152294236893282304U
