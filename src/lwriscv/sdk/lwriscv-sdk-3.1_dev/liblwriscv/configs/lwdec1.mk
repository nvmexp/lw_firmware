# Engine configuration, to be included by other configs
LWRISCV_IS_ENGINE := lwdec

ifeq ($(LWRISCV_IS_CHIP),gh100)
LWRISCV_IMEM_SIZE := 0xC000
LWRISCV_DMEM_SIZE := 0xA000
LWRISCV_EMEM_SIZE := 0x0
endif
