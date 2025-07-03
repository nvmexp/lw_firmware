#define IN_ADA_BODY
#include "hs_init_tu10x.h"

void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_tu10x__is_valid_falcon_core_rev(ucode_post_codes__lw_ucode_unified_error_type *status) {
  dev_sec_csb_ada__lw_csec_falcon_hwcfg1_register reg_val;
  csb_reg_rd_wr_instances__csb_reg_rd32_hwcfg1(&reg_val, 19200U);
  if (reg_val.f_core_rev != 6) {
    (*status) = ucode_post_codes__ilwalid_falcon_rev;
  }
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_tu10x__is_valid_falcon_engine(ucode_post_codes__lw_ucode_unified_error_type *status) {
  dev_sec_csb_ada__lw_csec_falcon_engid_register reg_val;
  csb_reg_rd_wr_instances__csb_reg_rd32_engineid(&reg_val, 20224U);
  if (reg_val.f_familyid != 1) {
    (*status) = ucode_post_codes__unexpected_falcon_engid;
  }
  return;
}
typedef dev_master__lw_pmc_boot_42_register hs_init_tu10x__is_valid_chip__bar0_reg_rd32GP1435__generic_register;
static void hs_init_tu10x__is_valid_chip__bar0_reg_rd32(hs_init_tu10x__is_valid_chip__bar0_reg_rd32GP1435__generic_register *reg, lw_types__reg_addr_types__bar0_addr addr);
static void hs_init_tu10x__is_valid_chip__bar0_reg_rd32(hs_init_tu10x__is_valid_chip__bar0_reg_rd32GP1435__generic_register *reg, lw_types__reg_addr_types__bar0_addr addr) {
  lw_types__lwu32 val;


  reg_rd_wr_bar0_tu10x__bar0_reg_rd32_private(&val, addr);
  *((lw_types__lwu32 *)&(*reg)) = val;
  return;
}
ucode_post_codes__lw_ucode_unified_error_type
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_tu10x__is_valid_chip(void) {
  dev_master__lw_pmc_boot_42_register reg_val;
  ucode_post_codes__lw_ucode_unified_error_type status = ucode_post_codes__ucode_err_code_noerror;


  hs_init_tu10x__is_valid_chip__bar0_reg_rd32(&reg_val, 2560U);
  if (reg_val.chip_id != 356U) {
    status = ucode_post_codes__chip_id_ilwalid;
  }
  return (status);
}
void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_tu10x__check_csb_err_state(ucode_post_codes__lw_ucode_unified_error_type *status) {
  dev_sec_csb_ada__lw_csec_falcon_csberrstat_register reg_val;
  csb_reg_rd_wr_instances__csb_reg_rd32_csberrstat(&reg_val, 37120U);
  if (reg_val.f_enable == 0) {
    (*status) = ucode_post_codes__bad_csberrorstate;
  }
  return;
}
typedef dev_sec_csb_ada__lw_csec_bar0_tmout_register hs_init_tu10x__set_bar0_timeout__csb_reg_wr32GP2389__generic_register
  GNAT_ALIGN(4);
static void hs_init_tu10x__set_bar0_timeout__csb_reg_wr32(const hs_init_tu10x__set_bar0_timeout__csb_reg_wr32GP2389__generic_register *reg, lw_types__reg_addr_types__csb_addr addr);
static void hs_init_tu10x__set_bar0_timeout__csb_reg_wr32(const hs_init_tu10x__set_bar0_timeout__csb_reg_wr32GP2389__generic_register *reg, lw_types__reg_addr_types__csb_addr addr) {
  typedef lw_types__Tlwu32B hs_init_tu10x__set_bar0_timeout__csb_reg_wr32__uc_lwu32GP1900__target;

  const lw_types__lwu32 val = (*(hs_init_tu10x__set_bar0_timeout__csb_reg_wr32__uc_lwu32GP1900__target*)(&(*reg)));
  {
    const lw_types__lwu32 C8b = (lw_types__lwu32)addr;
    {
      extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

      falc_stxb_wrapper(C8b, val);
    }
    {
      lw_types__lwu32 val;
      dev_sec_csb_ada__lw_csec_falcon_csberrstat_register csberrstatval;
    {
        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

        val = falc_ldx_i_wrapper(37120U, 0U);
      }
      *((lw_types__lwu32 *)&csberrstatval) = val;
      if (csberrstatval.f_valid == 1) {
        falc_halt();
      }
    }
    return;

  }}
void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_tu10x__set_bar0_timeout(ucode_post_codes__lw_ucode_unified_error_type *status) {
  dev_sec_csb_ada__lw_csec_bar0_tmout_register reg_val;


  {
    typedef integer hs_init_tu10x__set_bar0_timeout__T6b;
    hs_init_tu10x__set_bar0_timeout__T6b unused_loop_var;

    for (unused_loop_var = 1; unused_loop_var <= 1; unused_loop_var++) {
      reg_val.f_cycles = 20000000;
      hs_init_tu10x__set_bar0_timeout__csb_reg_wr32(&reg_val, 115456U);
    }
  }
  (*status) = ucode_post_codes__ucode_err_code_noerror;
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_tu10x__prevent_halted_cpu_from_being_restarted(void) {
  dev_sec_csb_ada__lw_csec_falcon_cpuctl_register reg_val;
  csb_reg_rd_wr_instances__csb_reg_rd32_cpuctl(&reg_val, 16384U);
  reg_val.f_alias_en = 0;
  csb_reg_rd_wr_instances__csb_reg_wr32_cpuctl(&reg_val, 16384U);
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_tu10x__update_status_in_mailbox(ucode_post_codes__lw_ucode_unified_error_type status) {
  dev_sec_csb_ada__lw_csec_falcon_mailbox0_register csec_mailbox0;
  typedef ucode_post_codes__lw_ucode_unified_error_type hs_init_tu10x__update_status_in_mailbox__to_mailboxGP3403__source;

  csec_mailbox0.f_data = ((dev_sec_csb_ada__lw_csec_falcon_mailbox0_data_field)((hs_init_tu10x__update_status_in_mailbox__to_mailboxGP3403__source)status));
  csb_reg_rd_wr_instances__csb_reg_wr32_mailbox(&csec_mailbox0, 4096U);
  return;
}

