#define IN_ADA_BODY
#include "reset_plm_tu10x.h"

void
  GNAT_LINKER_SECTION(".imem_pub")
reset_plm_tu10x__update_falcon_reset_plm(lw_types__boolean__lwbool israise) {
  dev_sec_csb_ada__lw_csec_falcon_reset_priv_level_mask_register reg_val;
  {
    typedef integer reset_plm_tu10x__update_falcon_reset_plm__T1b;
    reset_plm_tu10x__update_falcon_reset_plm__T1b unused_loop_var;

    for (unused_loop_var = 1; unused_loop_var <= 1; unused_loop_var++) {
      csb_reg_rd_wr_instances__csb_reg_rd32_flcn_reset_plm(&reg_val, 61696U);
      if (israise == 1) {
        reg_val.f_write_protection = 0;
        reg_val.f_write_violation = 1;
      }
      else {
        reg_val.f_write_protection = 7;
        reg_val.f_write_violation = 1;
      }
      csb_reg_rd_wr_instances__csb_reg_wr32_flcn_reset_plm(&reg_val, 61696U);
    }
  }
  return;
}

