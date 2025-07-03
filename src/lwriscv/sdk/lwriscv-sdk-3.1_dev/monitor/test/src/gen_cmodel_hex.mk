
SEPKERN_IMAGE_ELF   := $(SEPKERN_INSTALL_DIR)/g_sepkern_$(ENGINE)_$(CHIP)_riscv.elf
SEPKERN_IMAGE_HEX   := $(BUILD_DIR)/g_sepkern_$(ENGINE)_$(CHIP)_riscv.hex
SEPKERN_CODE_HEX    := $(BUILD_DIR)/g_sepkern_$(ENGINE)_$(CHIP)_riscv.text.hex
SEPKERN_DATA_HEX    := $(BUILD_DIR)/g_sepkern_$(ENGINE)_$(CHIP)_riscv.data.hex

PARTITIONS_HEX      := $(ELF).hex
PARTITIONS_CODE_HEX := $(ELF).text.hex
PARTITIONS_DATA_HEX := $(ELF).data.hex

FINAL_TEXT_HEX := $(FMC_TEXT).hex
FINAL_DATA_HEX := $(FMC_DATA).hex

BUILD_FILES += $(FINAL_TEXT_HEX) $(FINAL_DATA_HEX)

$(SEPKERN_IMAGE_HEX): $(BUILD_DIR) $(SEPKERN_IMAGE_ELF)
	$(ECHO) "Colwerting $(SEPKERN_IMAGE_ELF) to $@"
	$(call ELF2HEX,$(SEPKERN_IMAGE_ELF),$@)

# Extract sk text and data hex
# text is limited to 256 lines per sk exec space (0x100 * 0x10 == 0x1000)
# data is also limited to 0x1000
$(SEPKERN_CODE_HEX) $(SEPKERN_DATA_HEX) &: $(SEPKERN_IMAGE_HEX)
	(head -32768 > $(SEPKERN_CODE_HEX); cat > $(SEPKERN_DATA_HEX)) < $(SEPKERN_IMAGE_HEX)
	sed -i '257,$$ d' $(SEPKERN_CODE_HEX)
	sed -i '257,$$ d' $(SEPKERN_DATA_HEX)

$(PARTITIONS_HEX): $(BUILD_DIR) $(ELF)
	$(ECHO) "Colwerting $(ELF) to $@"
	$(call ELF2HEX,$(ELF),$@)

# Extract partition text and data hex
# text is limited to 32k for max cmodel allowed
# data is also limited to 32k for max cmodel allowed
$(PARTITIONS_CODE_HEX) $(PARTITIONS_DATA_HEX) &: $(PARTITIONS_HEX)
	(head -32768 > $(PARTITIONS_CODE_HEX); cat > $(PARTITIONS_DATA_HEX)) < $(PARTITIONS_HEX)
	sed -i '2049,$$ d' $(PARTITIONS_CODE_HEX)
	sed -i '2049,$$ d' $(PARTITIONS_DATA_HEX)

$(FINAL_TEXT_HEX) $(FINAL_DATA_HEX): $(SEPKERN_CODE_HEX) $(PARTITIONS_CODE_HEX) $(SEPKERN_DATA_HEX) $(PARTITIONS_DATA_HEX)
	$(CAT) $(SEPKERN_DATA_HEX) $(PARTITIONS_DATA_HEX) > $(FINAL_DATA_HEX)
	$(CAT) $(SEPKERN_CODE_HEX) $(PARTITIONS_CODE_HEX) > $(FINAL_TEXT_HEX)

