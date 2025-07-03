ifeq (,$(wildcard /home/utils/gcc-9.3.0))
CC=gcc -static
CXX=g++ -static
else
CC=/home/utils/gcc-9.3.0/bin/gcc -static
CXX=/home/utils/gcc-9.3.0/bin/g++ -static
endif

LWRISCVX_SDK_ROOT = $(P4ROOT)/sw/lwriscv/main
LW_SDK_INC = $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk

mkbootfs: mkbootfs.cpp $(LIBOS_SOURCE)/lib/libelf.c $(LIBOS_SOURCE)/include/libelf.h
	$(CC) -Os -I$(LIBOS_SOURCE)/include -I$(LW_SDK_INC) $(LIBOS_SOURCE)/lib/libelf.c -c -o libelf.o
	$(CXX) -Os -I$(LIBOS_SOURCE)/include -I$(LW_SDK_INC) mkbootfs.cpp libelf.o -o $(LIBOS_SOURCE)/bin/mkboot
	strip mkbootfs
