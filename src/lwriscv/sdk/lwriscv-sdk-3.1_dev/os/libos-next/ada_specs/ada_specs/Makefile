GNAT_COMMUNITY_PATH ?= $(LW_TOOLS)/riscv/adacore/gnat_community/linux/target_riscv/20200818
GNAT_PRO_PATH       ?= $(LW_TOOLS)/adacore/gnat_pro_ada/linux/gnat_pro_ent_ada/target_riscv/20200209
SPARK_PRO_PATH      ?= $(LW_TOOLS)/adacore/gnat_pro_ada/linux/spark_pro/20200212
GNAT_PATH           := $(GNAT_COMMUNITY_PATH)/bin

CC=$(GNAT_PATH)/riscv64-elf-gcc

LIBOS_PROFILE ?= fsp-gh100-mpu
LIBOS_CONFIG := $(LIBOS_SOURCE)/profiles/$(LIBOS_PROFILE)/config

INCLUDE_DIRS += -I$(LIBOS_SOURCE)
INCLUDE_DIRS += -I$(LIBOS_SOURCE)/include
INCLUDE_DIRS += -I$(LIBOS_SOURCE)/profiles/fsp-gh100-mpu
INCLUDE_DIRS += -I$(P4ROOT)/sw/lwriscv/main/inc/lwpu-sdk
INCLUDE_DIRS += -I$(P4ROOT)/sw/resman/manuals
INCLUDE_DIRS += -I.

LIBOS_HEADERS += $(LIBOS_SOURCE)/kernel/task.h
LIBOS_HEADERS += $(LIBOS_SOURCE)/kernel/port.h
LIBOS_HEADERS += $(LIBOS_SOURCE)/include/libos.h
LIBOS_HEADERS += $(LIBOS_SOURCE)/include/libriscv.h
LIBOS_HEADERS += $(LIBOS_SOURCE)/include/libos_xcsr.h
LIBOS_HEADERS += $(LIBOS_SOURCE)/include/libos_log.h

CFLAGS=-fdump-ada-spec -C $(INCLUDE_DIRS)

.PHONY: libos_specs
libos_specs:
	rm -f *.ads *.h
	sed -e 's:#.*::;s/\([^=]*\)=\(.*\)/#define \1 \2/' $(LIBOS_CONFIG) > libos-config.h
	$(CC) $(CFLAGS) -c $(LIBOS_HEADERS)
