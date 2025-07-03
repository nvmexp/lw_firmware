# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := pmu-rtos-cheetah-ga10b

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t234-gpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/pmu.mk

# Don't add start file and exception handler
LWRISCV_FEATURE_START := n
LWRISCV_FEATURE_EXCEPTION := n

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y
LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES := y
LWRISCV_CONFIG_DEBUG_PRINT_USES_SWGEN := y

LWRISCV_FEATURE_SSP := y
LWRISCV_FEATURE_SCP := y
