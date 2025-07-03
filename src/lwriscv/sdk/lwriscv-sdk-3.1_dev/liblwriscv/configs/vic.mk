# Engine configuration, to be included by other configs
# Based on configuration in
# https://p4viewer/get///hw/lwgpu/uproc/falcon/6.0/include/projects/stub/tlit6_vic/project.h
LWRISCV_IS_ENGINE := vic

ifeq ($(LWRISCV_IS_CHIP),t239)
LWRISCV_IMEM_SIZE := 0x4000
LWRISCV_DMEM_SIZE := 0x4000
LWRISCV_EMEM_SIZE := 0x0
endif
