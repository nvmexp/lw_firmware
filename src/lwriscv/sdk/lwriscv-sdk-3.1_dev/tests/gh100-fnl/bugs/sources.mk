OBJS += trap.o
OBJS += irq.o
OBJS += irq_handler.o
ifneq ($(LWRISCV_IS_ENGINE),lwdec)
OBJS += msgq_200673281.o
endif
ifeq ($(LWRISCV_IS_ENGINE),lwdec)
OBJS += lwdec.o
endif
