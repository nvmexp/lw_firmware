# Engine configuration, to be included by other configs
LWRISCV_IS_ENGINE := minion

# MK TODO: check numbers
# MK TODO: this probably should be fetched from manuals anyway

ifeq ($(LWRISCV_IS_CHIP),gh100)
LWRISCV_IMEM_SIZE := 0x10000
LWRISCV_DMEM_SIZE := 0x10000
LWRISCV_EMEM_SIZE := 0x0
endif

ifeq ($(LWRISCV_IS_CHIP),gb100)
LWRISCV_IMEM_SIZE := 0x10000
LWRISCV_DMEM_SIZE := 0x10000
LWRISCV_EMEM_SIZE := 0x0
endif

# No FBIF for Minion
LWRISCV_HAS_FBIF := n

# Can't use FBDMA without an FBIF
LWRISCV_HAS_FBDMA := n

# Minion is not priv master
LWRISCV_HAS_PRI := n
