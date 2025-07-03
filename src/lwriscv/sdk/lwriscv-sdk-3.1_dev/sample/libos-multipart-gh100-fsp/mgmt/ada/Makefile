APP_ROOT = $(LWRDIR)/../..
OUT_DIR = $(APP_ROOT)/_out/mgmt/ada
LWRISCVX_SDK_ROOT = $(APP_ROOT)/../..

# Toolchain detection
msys_version = $(if $(findstring Msys, $(shell uname -o)),$(word 1, $(subst ., ,$(shell uname -r))),0)
ifeq ($(msys_version), 0)
GNAT_COMMUNITY_PATH = $(LW_TOOLS)/riscv/adacore/gnat_community/linux/target_riscv/20200818
GNAT_PRO_PATH       = $(LW_TOOLS)/adacore/gnat_pro_ada/linux/gnat_pro_ent_ada/target_riscv/20200209
SPARK_PRO_PATH      = $(LW_TOOLS)/adacore/gnat_pro_ada/linux/spark_pro/20200212
GNAT_PATH           = $(GNAT_COMMUNITY_PATH)
else
GNAT_PRO_PATH       = $(LW_TOOLS)/adacore/gnat_pro_ada/windows/gnat_pro_ent_ada/target_riscv/20200209
SPARK_PRO_PATH      = $(LW_TOOLS)/adacore/gnat_pro_ada/windows/spark_pro/20200212
GNAT_PATH           = $(GNAT_PRO_PATH)
endif

GPRBUILD = $(GNAT_PATH)/bin/gprbuild
GNATPROVE = $(SPARK_PRO_PATH)/bin/gnatprove

TARGET = $(OUT_DIR)/libtasks.a

.PHONY: default
default: $(TARGET)

.PHONY: $(TARGET)
$(TARGET):
	LWRISCVX_SDK_ROOT=$(LWRISCVX_SDK_ROOT) $(GPRBUILD) tasks.gpr

.PHONY: prove
prove:
	LWRISCVX_SDK_ROOT=$(LWRISCVX_SDK_ROOT) $(GNATPROVE) -P tasks.gpr

.PHONY: clean
clean:
	@rm -rf $(OUT_DIR)
