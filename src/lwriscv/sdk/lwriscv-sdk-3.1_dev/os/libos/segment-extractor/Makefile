# Detecting LSF and to avoid the ancient gcc in its default PATH
ifeq (,$(wildcard /home/utils/gcc-9.3.0))
CXX=g++ -static
else
CXX=/home/utils/gcc-9.3.0/bin/g++ -static
endif

LWRISCVX_SDK_ROOT = $(P4ROOT)/sw/lwriscv/main
LW_SDK_INC = $(LWRISCVX_SDK_ROOT)/inc/lwpu-sdk
INCLUDE_PATHS+=-I$(LW_SDK_INC)
INCLUDE_PATHS+=-I../include
INCLUDE_PATHS+=-I../lib
CFLAGS=-O3 $(INCLUDE_PATHS) -Wall -Wextra -Werror

segment-extractor: segment-extractor.cpp ../lib/libelf.c
	$(CXX) $(CFLAGS) -o $@ $^

.PHONY: clean
clean:
	@rm -f segment-extractor
