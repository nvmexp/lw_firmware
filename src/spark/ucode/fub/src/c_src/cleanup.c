#define IN_ADA_BODY
#include "cleanup.h"

void
  GNAT_LINKER_SECTION(".imem_pub")
cleanup__scrub_dmem(void) {
  lw_types__lwu32 start_addr;
  lw_types__lwu32 end_addr;
  {
    lw_types__lwu32 dmem_addr;
    extern lw_types__lwu32 _stack;

    start_addr = ((lw_types__lwu32)((system__address)&dmem_addr));
    end_addr = ((lw_types__lwu32)((system__address)&_stack));
  }
  while (true) {
    if (!(start_addr < end_addr)) {
      break;
    }
    {
      const lw_types__lwu32 C5b = start_addr;
      {
        const void *_val_address = ((system__address)(C5b));
#define val (*(volatile lw_types__lwu32* const)_val_address)

        val = 0U;
      }
      #undef val
      {
        const lw_types__lwu32_types__n_type C7b = (lw_types__lwu32_types__n_type)start_addr;
        start_addr = (lw_types__lwu32)(lw_types__lwu32_types__n_type)((lw_types__lwu32_types__base_type)C7b + 4U);

      }
    }}
  return;
}
void
  GNAT_LINKER_SECTION(".imem_pub")
cleanup__ucode_cleanup(void) {
  dev_falc_spr__lw_falc_sec_spr_register sec_spr;




  falc_scp_xor(0U, 0U);
  falc_scp_xor(1U, 1U);
  falc_scp_xor(2U, 2U);
  falc_scp_xor(3U, 3U);
  falc_scp_xor(4U, 4U);
  falc_scp_xor(5U, 5U);
  falc_scp_xor(6U, 6U);
  falc_scp_xor(7U, 7U);
  reset_plm_tu10x__update_falcon_reset_plm(0);
  {
    lw_types__lwu32 C20b;
    {

      C20b = falc_rspr_sec_spr_wrapper();
    }
    *((lw_types__lwu32 *)&sec_spr) = C20b;
    sec_spr.disable_exceptions = 1;
    {
      const lw_types__lwu32 C24b = (*(lw_types__lwu32*)(&sec_spr));
      falc_wspr_sec_spr_wrapper(C24b);
      clearFalconGprs();
      return;

    }
  }}

