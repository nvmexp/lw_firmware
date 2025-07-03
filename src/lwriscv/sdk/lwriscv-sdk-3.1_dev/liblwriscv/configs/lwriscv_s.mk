# Configuration for s-mode liblwriscv

ifeq ($(LWRISCV_HAS_S_MODE),n)
$(error This LWRISCV doesn't support S mode.)
endif

LWRISCV_CONFIG_CPU_MODE:= 1
LWRISCV_HAS_SBI := y
