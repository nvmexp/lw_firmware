# Generic rules

# Don't remove intermediate files ever
.SECONDARY:


$(BUILD_DIR)/%.o: %.c $(LIBLWRISCV_CONFIG_H) $(BUILD_DIR)
	$(CC) $(CFLAGS) -include $(LIBLWRISCV_CONFIG_H) -c $< -o $@

$(BUILD_DIR)/%.o: %.S $(LIBLWRISCV_CONFIG_H) $(BUILD_DIR)
	$(CC) $(CFLAGS) -include $(LIBLWRISCV_CONFIG_H) -c $< -o $@

$(BUILD_DIR):
	$(MKDIR) -p $(BUILD_DIR)

$(BUILD_DIR)/%.ld: %.ld.in $(BUILD_DIR)
	$(CPP) -x c -P -o $@ $(DEFINES) $(INCLUDES) -include $(LIBLWRISCV_CONFIG_H) $<

%.objdump.src: %
	$(OBJDUMP) -S $< > $@

%.readelf: %
	$(READELF) --all $< > $@

%.hex: %
	$(XXD) $< $@

$(TARGET): $(OBJS) $(LDSCRIPT) $(LIBLWRISCV_DIR)/lib/liblwriscv.a $(BUILD_DIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(TARGET) $(LDFLAGS)
