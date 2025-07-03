# my make-fu is bad
ROOT=$(LWRDIR)/..
include ../config.mk

TARGET := riscv-loader-$(ENGINE)-$(LW_TARGET_ARCH)
SYSROOT:=$(LW_TOOLS)/linux/mods/gcc-4.9.2-x86_64

ifeq ($(LW_TARGET_ARCH), aarch64)
CC:=$(LW_TOOLS)/../mobile/tools/linux/linaro/gcc-linaro-7.3.1-2018.05-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-gcc
else
CC:=$(LW_TOOLS)/linux/mods/gcc-4.9.2-x86_64/bin/gcc
endif

ifneq ($(LW_TARGET_ARCH), aarch64)
LIBPCI_DIR:=$(LW_TOOLS)/unix/targets/Linux-x86_64/pciutils-2.2.1
LIBPCI_INC:=$(LIBPCI_DIR)/include
LIBPCI_LIB:=$(LIBPCI_DIR)/lib
endif

ifeq ($(LW_TARGET_ARCH), aarch64)
obj-y += igpu-io.o
else
obj-y += dgpu-io.o
endif
obj-y += gpu-io.o
obj-y += engine-io.o
obj-y += main.o
obj-y += misc.o
obj-y += brom.o

CFLAGS:= -DENGINE=$(ENGINE) -DENGINE_NAME='"$(ENGINE)"'
CFLAGS+= -DCHIP=$(CHIP)
CFLAGS+= -I$(INC) -I$(INC)/$(CHIP)
CFLAGS+= -g
ifeq ($(LW_TARGET_ARCH), aarch64)
CFLAGS+= -DIS_TARGET_AARCH64=1
else
CFLAGS+= -I$(LIBPCI_INC)
CFLAGS+= --sysroot=$(SYSROOT)
CFLAGS+= -DIS_TARGET_X86_64=1
endif

ifneq ($(LW_TARGET_ARCH), aarch64)
LDFLAGS:= --sysroot=$(SYSROOT) -L$(LIBPCI_LIB) -L$(LW_TOOLS)/unix/targets/Linux-x86/zlib-1.2.3/lib
# LIBS:= -L$(LIBPCI_LIB) -Wl,-Bstatic -lpci -Wl,-Bdynamic -ludev
LIBS := -lpci
endif
ASFLAGS:=
DEPFLAGS = -MMD -MF $(@F).d
OBJS:= $(patsubst %.o,%-$(ENGINE)-$(LW_TARGET_ARCH).o,$(obj-y))
DEPS:= $(addsuffix .d, $(OBJS))

install: build
	cp -f $(TARGET) $(INSTALL_DIR)/

build: $(TARGET)

$(TARGET): $(OBJS) Makefile
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $(TARGET)

%-$(ENGINE)-$(LW_TARGET_ARCH).o: %.c Makefile
	$(CC) $(DEPFLAGS) $(CFLAGS) -o $@ -c $<

%-$(ENGINE)-$(LW_TARGET_ARCH).o: %.S Makefile
	$(CC) $(DEPFLAGS) $(ASFLAGS) -o $@ -c $<

clean:
	rm -f $(OBJS) $(DEPS) $(TARGET)

-include $(DEPS)
