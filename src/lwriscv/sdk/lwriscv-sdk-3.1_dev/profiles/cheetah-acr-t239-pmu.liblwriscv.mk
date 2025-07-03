# Unique profile so (Seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := cheetah-acr-t239-pmu

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/t239-gpu.mk

# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_m.mk

# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/pmu.mk

# Add start file and exception handler
LWRISCV_FEATURE_START := y
LWRISCV_FEATURE_EXCEPTION := y

# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y
LWRISCV_CONFIG_DEBUG_PRINT_USES_SWGEN := y
LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES := y

LWRISCV_FEATURE_SSP := y
LWRISCV_FEATURE_SCP := y
LWRISCV_FEATURE_DIO := y
LWRISCV_FEATURE_SHA := y
