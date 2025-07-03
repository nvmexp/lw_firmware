# Unique profile so (seemingly) the same configurations don't overlap
LIBLWRISCV_PROFILE := gsp-rm-proxy-gh100

# Chip family we want to compile to
include $(LIBLWRISCV_CONFIGS)/gh100.mk
# Extra configuration
include $(LIBLWRISCV_CONFIGS)/lwriscv_s.mk
# Engine we want to compile to
include $(LIBLWRISCV_CONFIGS)/gsp.mk


# we may run with fb locked, pretend FBIF is dead
LWRISCV_WAR_FBIF_IS_DEAD := y

# Enable most of the prints
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 10
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := y
LWRISCV_CONFIG_DEBUG_PRINT_USES_SWGEN := y
LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES := y

# Use lwstatus as return codes
LWRISCV_CONFIG_USE_STATUS := lwstatus

LWRISCV_APP_IMEM_LIMIT := LWRISCV_IMEM_SIZE - 0x1000
LWRISCV_APP_DMEM_LIMIT := LWRISCV_DMEM_SIZE - 0x1000

# Need dio for libccc
LWRISCV_FEATURE_DIO := y
LWRISCV_FEATURE_SSP := y
LWRISCV_FEATURE_SCP := y
LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED := y
LWRISCV_FEATURE_SHA := y
LWRISCV_FEATURE_MPU := y
