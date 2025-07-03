#define IN_ADA_BODY
#include "hs_init_common_tu104.h"

void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_common_tu104__read_fpf_fuse_pub_rev(lw_types__lwu8 *data) {
  dev_fpf__lw_fpf_opt_fuse_ucode_gfw_rev_register reg_val;
  bar0_reg_rd_wr_instances__bar0_reg_rd32_fpf_opt_fuse(&reg_val, 8520276U);
  (*data) = reg_val.data;
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_common_tu104__read_chip_id(lw_types__lwu9 *chipid) {
  dev_master__lw_pmc_boot_42_register reg_val;
  bar0_reg_rd_wr_instances__bar0_reg_rd32_boot42(&reg_val, 2560U);
  (*chipid) = reg_val.chip_id;
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_common_tu104__read_fuse_opt(lw_types__lwu32 *data) {
  dev_fuse__lw_fuse_opt_fuse_ucode_gfw_rev_register reg_val;
  bar0_reg_rd_wr_instances__bar0_reg_rd32_fuse_opt_fuse(&reg_val, 135768U);
  (*data) = (lw_types__lwu32)reg_val.data;
  return;
}

