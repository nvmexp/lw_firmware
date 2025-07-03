# Engine configuration, to be included by other configs
LWRISCV_IS_ENGINE := soe

# MK TODO: this probably should be fetched from manuals anyway
# This is for GA102 and GA100, we have native CSB on GH100+
LWRISCV_HAS_CSB_OVER_PRI_SHIFT := 6

ifeq ($(LWRISCV_IS_CHIP),ls10)
LWRISCV_IMEM_SIZE := 0x30000
LWRISCV_DMEM_SIZE := 0x20000
LWRISCV_EMEM_SIZE := 0x2000
LWRISCV_HAS_GDMA := y
endif

LWRISCV_HAS_DIO_SE := y
LWRISCV_HAS_DIO_FBHUB := y
