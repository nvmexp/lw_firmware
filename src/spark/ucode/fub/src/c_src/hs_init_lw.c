#define IN_ADA_BODY
#include "hs_init_lw.h"

ucode_post_codes__lw_ucode_unified_error_type
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_lw__is_valid_ucode_rev(void) {
  lw_types__lwu32 revlockvalchipoptionfuse;
  lw_types__lwu32 revlockvalfpf;
  lw_types__lwu32 revlockvalfirmware;
  ucode_post_codes__lw_ucode_unified_error_type status = ucode_post_codes__ucode_err_code_noerror;
  {
    typedef integer hs_init_lw__is_valid_ucode_rev__T1b;
    hs_init_lw__is_valid_ucode_rev__T1b unused_loop_var;

    for (unused_loop_var = 1; unused_loop_var <= 1; unused_loop_var++) {
      hs_init_lw__get_ucode_rev_chip_option_fuse_val(&revlockvalchipoptionfuse);
      hs_init_lw__get_ucode_rev_val(&revlockvalfirmware, &status);
      if (status != ucode_post_codes__ucode_err_code_noerror) {
        break;
      }
      hs_init_lw__get_ucode_rev_fpf_val(&revlockvalfpf);
      if ((revlockvalfirmware < revlockvalchipoptionfuse) || (revlockvalfirmware < revlockvalfpf)) {
        status = ucode_post_codes__ilwalid_ucode_revision;
      }
    }
  }
  return (status);
}
void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_lw__get_ucode_rev_fpf_val(lw_types__lwu32 *rev) {
  lw_types__lwu8 data;
  (*rev) = 0U;
  hs_init_common_tu104__read_fpf_fuse_pub_rev(&data);
  {
    typedef integer hs_init_lw__get_ucode_rev_fpf_val__T3b;
    hs_init_lw__get_ucode_rev_fpf_val__T3b unused_loop_var;

    for (unused_loop_var = 1; unused_loop_var <= 8; unused_loop_var++) {
      const lw_types__lwu32_types__n_type C4b = (lw_types__lwu32_types__n_type)(*rev);
      (*rev) = (lw_types__lwu32)(lw_types__lwu32_types__n_type)((lw_types__lwu32_types__base_type)C4b + 1U);
      data = data >> 1;
      if (data == 0U) {
        break;
      }
    }
  }
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_lw__get_ucode_rev_chip_option_fuse_val(lw_types__lwu32 *revlock_fuse_val) {
  hs_init_common_tu104__read_fuse_opt(&(*revlock_fuse_val));
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
hs_init_lw__get_ucode_rev_val(lw_types__lwu32 *rev, ucode_post_codes__lw_ucode_unified_error_type *status) {
  lw_types__lwu9 chip_id;
  (*rev) = 4294967295U;
  hs_init_common_tu104__read_chip_id(&chip_id);
  {
    lw_types__lwu9 T7b = chip_id;
    if (!(((interfaces__unsigned_32)(T7b)) <= 511U))
      GNAT_LAST_CHANCE_HANDLER(__FILE__, __LINE__);
    if (T7b == 356U) {
      (*rev) = 0U;
    }
    else {
      (*status) = ucode_post_codes__chip_id_ilwalid;
    }
    return;

  }}

