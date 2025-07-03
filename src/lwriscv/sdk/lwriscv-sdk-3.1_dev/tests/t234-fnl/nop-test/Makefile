# my make-fu is bad
ROOT=$(LWRDIR)/../../..
COMMON_DIR:=$(LWRDIR)/../common
HWREF=$(LWRDIR)/../hwref

include $(COMMON_DIR)/config.mk

OBJS :=
OBJS +=start.o

# Mangle objects to be per-engine
OBJS := $(patsubst %.o,%-$(ENGINE).o,$(OBJS))

MANIFEST_SRC?=manifest.c

ELF:=testapp-$(ENGINE)
FMC_TEXT:=$(ELF).text
FMC_DATA:=$(ELF).data
MANIFEST:=$(ELF).manifest.o
MANIFEST_BIN:=$(ELF).manifest

FINAL_FMC_TEXT:=$(FMC_TEXT).encrypt.bin
FINAL_FMC_DATA:=$(FMC_DATA).encrypt.bin
FINAL_MANIFEST:=$(MANIFEST_BIN).encrypt.bin.out.bin

BUILD_FILES:=$(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST)

# TODO: this probably should be done other way
INTERMEDIATE_FILES:=

LDSCRIPT_IN:=ldscript.in.ld
LDSCRIPT:=ldscript-$(ENGINE).ld

INCFLAGS := $(COMMON_INC)
DEFINES += $(COMMON_DEFINES)
DEPS:= $(addsuffix .d, $(OBJS))
CFLAGS += $(COMMON_CFLAGS) $(INCFLAGS) $(addprefix -D,$(DEFINES))

ASFLAGS:=$(CFLAGS)
LDFLAGS+= $(COMMON_LDFLAGS) $(LIBS) -T$(LDSCRIPT)

LD_DEFINES += -D__LINKER__
LD_INCLUDES += $(INCFLAGS)

.PHONY: build sign

build: $(BUILD_FILES)

$(FINAL_FMC_TEXT) $(FINAL_FMC_DATA) $(FINAL_MANIFEST): sign

sign: $(MANIFEST_BIN) $(FMC_TEXT) $(FMC_DATA)
	@echo "Signing files..."
	$(SIGGEN) -manifest $(MANIFEST_BIN) -code $(FMC_TEXT) -data $(FMC_DATA) -chip ga10b_rsa3k_0
INTERMEDIATE_FILES+= $(MANIFEST_BIN).encrypt.bin $(MANIFEST_BIN).encrypt.bin.sig.bin

$(MANIFEST_BIN): $(MANIFEST)
	$(OBJCOPY) -O binary -j .rodata $(MANIFEST) $(MANIFEST_BIN)
INTERMEDIATE_FILES += $(MANIFEST_BIN)

$(MANIFEST): $(FMC_TEXT) $(FMC_DATA) $(MANIFEST_SRC)
	$(CC) $(MANIFEST_SRC) $(CFLAGS) -DLWRISCV_APP_IMEM_LIMIT=0x10000 -DLWRISCV_APP_DMEM_LIMIT=0x10000 -c -o $@
INTERMEDIATE_FILES += $(MANIFEST)

$(LDSCRIPT): $(LDSCRIPT_IN)
	$(CPP) -x c -P -o $@ $(LD_DEFINES) $(LD_INCLUDES) $^
INTERMEDIATE_FILES += $(LDSCRIPT)

$(FMC_TEXT): $(ELF)
	$(OBJCOPY) -O binary -j .monitor_code $(ELF) $(FMC_TEXT)
INTERMEDIATE_FILES += $(FMC_TEXT)

$(FMC_DATA): $(ELF)
	$(OBJCOPY) -O binary -j .monitor_data $(ELF) $(FMC_DATA)
INTERMEDIATE_FILES += $(FMC_DATA)

$(ELF): $(OBJS) $(LDSCRIPT)
	$(CC) $(CFLAGS) $(OBJS) -o $(ELF) $(LDFLAGS)
	$(OBJDUMP) -S -d $(ELF) > $(ELF).objdump
INTERMEDIATE_FILES += $(ELF)

%-$(ENGINE).o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%-$(ENGINE).o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(INTERMEDIATE_FILES) $(BUILD_FILES) $(OBJS) $(DEPS)

-include $(DEPS)
