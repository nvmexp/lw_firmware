# All known features and knobs, to be configured, those are defaults,
# MK TODO: this should be included before chip or after chip? Most likely before
# as chip may block some of those

#
# Chip / Peregrine configuration
#

LWRISCV_IS_CHIP_FAMILY := invalid
LWRISCV_IS_CHIP        := gi42

LWRISCV_HAS_PRI := n
LWRISCV_HAS_CSB_MMIO := n
LWRISCV_HAS_CSB_OVER_PRI := n
LWRISCV_HAS_CSB_OVER_PRI_SHIFT := 0
LWRISCV_HAS_LOCAL_IO := y
LWRISCV_HAS_DIO_SE := n
LWRISCV_HAS_DIO_SNIC := n
LWRISCV_HAS_DIO_FBHUB := n

LWRISCV_IS_VERSION_MAJOR := 0
LWRISCV_IS_VERSION_MINOR := 0

LWRISCV_HAS_ABI := lp64
LWRISCV_HAS_ARCH := rv64im
LWRISCV_HAS_S_MODE := n

LWRISCV_IS_ENGINE := none
LWRISCV_IMEM_SIZE := 0
LWRISCV_DMEM_SIZE := 0
LWRISCV_EMEM_SIZE := 0

LWRISCV_HAS_SBI := n
LWRISCV_HAS_FBDMA := y
LWRISCV_HAS_GDMA := n
LWRISCV_HAS_FBIF := y
LWRISCV_HAS_TFBIF := n
LWRISCV_HAS_SCP := y
LWRISCV_HAS_SHA := n

# Controls if rdtime returns PTIMER (set) or PTIMER << 5 (cleared)
# It is enabled on all Hopper+ chips.
LWRISCV_HAS_BINARY_PTIMER := y

#
# Feature configuration
#

# Select available lwstatus type. Possible types: lwrv (default), lwstatus, fsp, ...
LWRISCV_CONFIG_USE_STATUS := lwstatus

# Mode at which cpu runs (3 = M, 1 = S)
LWRISCV_CONFIG_CPU_MODE := 3

# Prints, if non-0 enables printing capability, min verbosity visible
LWRISCV_CONFIG_DEBUG_PRINT_LEVEL := 0
# If set - debug buffer uses msg/cmd queue to track buffer position
LWRISCV_CONFIG_DEBUG_PRINT_USES_QUEUES := n
# If set - embedds metadata in debug buffer
LWRISCV_CONFIG_DEBUG_PRINT_METADATA_IN_BUFFER := n
# If set, debug buffer uses interrupt to notify host of messages
LWRISCV_CONFIG_DEBUG_PRINT_USES_SWGEN := n

LWRISCV_FEATURE_START := n
LWRISCV_FEATURE_EXCEPTION := n

LWRISCV_FEATURE_SSP := n
LWRISCV_FEATURE_SSP_FORCE_SW_CANARY := n
LWRISCV_FEATURE_SSP_ENABLE_FAIL_HOOK := n

LWRISCV_FEATURE_DMA := y

# FBIF/TFBIF config will be done via SBI (off by default)
LWRISCV_FEATURE_FBIF_TFBIF_CFG_VIA_SBI := n

# Disable the MPU by default since it is not needed by bare-metal applications
# Profiles which require MPU can enable it by overriding this flag.
LWRISCV_FEATURE_MPU := n

# Disable the SCP driver and all of its extra features unless specifically
# requested.
LWRISCV_FEATURE_SCP := n
LWRISCV_DRIVER_SCP_FAKE_RNG_ENABLED := n
LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED := n

# SHA driver and features. Disabled by default.
LWRISCV_FEATURE_SHA := n

LWRISCV_FEATURE_DIO := n

# if enabled, this will build liblwriscv which is meant to be run on CMODEL
LWRISCV_PLATFORM_IS_CMOD := n

#if enabled this will build libRTS as well
LWRISCV_HAS_RTS := n

#
# Drivers configuration
#

#
# Hacks and workarounds
#

# if enabled, no rw fence is available
LWRISCV_WAR_FBIF_IS_DEAD := n

# enabled for old clients which have not switched away from the legacy API
LWRISCV_FEATURE_LEGACY_API := y
