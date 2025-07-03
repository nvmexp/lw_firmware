#define IN_ADA_BODY
#include "exceptions_tu10x.h"

void
  GNAT_LINKER_SECTION(".imem_pub")
exceptions_tu10x__stack_underflow_installer(void) {
  dev_sec_csb_ada__lw_csec_falcon_stackcfg_register stack_cfg_reg;
  lw_types__lwu32 C1b;
  {
    lw_types__lwu32 stack_bottom;

    stack_bottom = 49152U;
    stack_bottom = (lw_types__lwu32)(49152U >> 2);
    C1b = stack_bottom;
  }
  if (!(C1b <= 16777215U))
    GNAT_LAST_CHANCE_HANDLER(__FILE__, __LINE__);
  {
    const dev_sec_csb_ada__lw_csec_falcon_stackcfg_bottom_field stack_bottom = (dev_sec_csb_ada__lw_csec_falcon_stackcfg_bottom_field)C1b;
    stack_cfg_reg.f_bottom = stack_bottom;
    stack_cfg_reg.f_spexc = 1;
    {
      const lw_types__lwu32 C6b = (*(lw_types__lwu32*)(&stack_cfg_reg));
      {
        extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

        falc_stxb_wrapper(19968U, C6b);
      }
      return;

    }
  }}

