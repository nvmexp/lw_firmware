#define IN_ADA_BODY
#include "sec_entry.h"

void
  GNAT_LINKER_SECTION(".imem_pub")
sec_entry__hs_entry_noninline(ucode_post_codes__lw_ucode_unified_error_type *status) {
  {
    typedef integer sec_entry__hs_entry_noninline__T166b;
    sec_entry__hs_entry_noninline__T166b unused_loop_var;

    for (unused_loop_var = 1; unused_loop_var <= 1; unused_loop_var++) {
      reset_plm_tu10x__update_falcon_reset_plm(1);
      if ((*status) != ucode_post_codes__ucode_err_code_noerror) {{

          while (true) {
            {}
          }
        }
      }
      hs_init_tu10x__set_bar0_timeout(&(*status));
      if ((*status) != ucode_post_codes__ucode_err_code_noerror) {
        goto main_block;
      }
      (*status) = hs_init_tu10x__is_valid_chip();
      if ((*status) != ucode_post_codes__ucode_err_code_noerror) {
        goto main_block;
      }
      (*status) = hs_init_lw__is_valid_ucode_rev();
    }
  }
  main_block: ;
  return;
}
void sec_entry__dummy_wrapper_hsentry(void) {
  ucode_post_codes__lw_ucode_unified_error_type status = ucode_post_codes__ucode_err_code_noerror;
  {
    typedef integer sec_entry__dummy_wrapper_hsentry__T171b;
    sec_entry__dummy_wrapper_hsentry__T171b unused_loop_var;

    for (unused_loop_var = 1; unused_loop_var <= 1; unused_loop_var++) {{
        lw_types__lwu32 dmem_addr;

        {
          typedef integer sec_entry__dummy_wrapper_hsentry__T174b;
          sec_entry__dummy_wrapper_hsentry__T174b unused_loop_var;

          for (unused_loop_var = 1; unused_loop_var <= 1; unused_loop_var++) {
            dmem_addr = 256U << 8;
            Hs_Entry_Set_Sp(dmem_addr);
            falc_scp_forget_sig();
            falc_scp_xor(0U, 0U);
            falc_scp_xor(1U, 1U);
            falc_scp_xor(2U, 2U);
            falc_scp_xor(3U, 3U);
            falc_scp_xor(4U, 4U);
            falc_scp_xor(5U, 5U);
            falc_scp_xor(6U, 6U);
            falc_scp_xor(7U, 7U);
            clearFalconGprs();
            {
              lw_types__lwu32 C184b;
              {
              {
                  lw_types__lwu32 C187b;
                  {
                    extern lw_types__lwu32 falc_rspr_sp_spr_wrapper(void);

                    C187b = falc_rspr_sp_spr_wrapper();
                  }
                  C184b = C187b;

                }}
              {
                lw_types__lwu32 addr = dmem_addr;

                while (addr < C184b) {
                  const lw_types__lwu32 C191b = addr;
                  {
                    const void *_val_address = ((system__address)(C191b));
#define val (*(volatile lw_types__lwu32* const)_val_address)

                    val = 0U;
                  }
                  #undef val
                  {
                    const lw_types__lwu32_types__n_type C193b = (lw_types__lwu32_types__n_type)addr;
                    addr = (lw_types__lwu32)(lw_types__lwu32_types__n_type)((lw_types__lwu32_types__base_type)C193b +
                      4U);

                  }}
              }
              hs_init_tu10x__check_csb_err_state(&status);
              if (status != ucode_post_codes__ucode_err_code_noerror) {{

                  while (true) {
                    {}
                  }
                }
              }
              hs_init_tu10x__prevent_halted_cpu_from_being_restarted();
              {
                dev_falc_spr__lw_falc_sec_spr_register sec_spr;
              {
                  extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

                  falc_stxb_wrapper(9728U, 0U);
                }
                {
                  extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

                  falc_stxb_wrapper(9984U, 0U);
                }
                {
                  extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

                  falc_stxb_wrapper(11264U, 0U);
                }
                {
                  extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

                  falc_stxb_wrapper(11520U, 0U);
                }
                {
                  extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

                  falc_stxb_wrapper(11776U, 0U);
                }
                {
                  extern void falc_sclrb(lw_types__lwu32 bitnum);

                  falc_sclrb(16U);
                }
                {
                  extern void falc_sclrb(lw_types__lwu32 bitnum);

                  falc_sclrb(17U);
                }
                {
                  extern void falc_sclrb(lw_types__lwu32 bitnum);

                  falc_sclrb(18U);
                }
                {
                  extern void falc_sclrb(lw_types__lwu32 bitnum);

                  falc_sclrb(24U);
                }
                install_exception_handler_hang_to_ev();
                {
                  lw_types__lwu32 C220b;
                  {

                    C220b = falc_rspr_sec_spr_wrapper();
                  }
                  *((lw_types__lwu32 *)&sec_spr) = C220b;
                  sec_spr.disable_exceptions = 0;
                  {
                    const lw_types__lwu32 C224b = (*(lw_types__lwu32*)(&sec_spr));
                    falc_wspr_sec_spr_wrapper(C224b);

                  }
                }}
              exceptions_tu10x__stack_underflow_installer();
              {
                const lw_types__lwu32 C227b = ((lw_types__lwu32)((system__address)&sec_entry__rand_num16_byte[0U]));
                {
                  lw_types__lwu32 val;
                  dev_sec_csb_ada__lw_csec_scp_rndctl1_register scp_rndctl1_reg;
                  dev_sec_csb_ada__lw_csec_scp_rndctl11_register scp_rndctl11_reg;
                  dev_sec_csb_ada__lw_csec_scp_ctl1_register scp_ctl1_reg;
                {
                    extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

                    falc_stxb_wrapper(81920U, 32767U);
                  }
                  {
                    extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

                    val = falc_ldx_i_wrapper(82176U, 0U);
                  }
                  *((lw_types__lwu32 *)&scp_rndctl1_reg) = val;
                  scp_rndctl1_reg.f_holdoff_intra_mask = 1023U;
                  {
                    const lw_types__lwu32 C235b = (*(lw_types__lwu32*)(&scp_rndctl1_reg));
                    {
                      extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

                      falc_stxb_wrapper(82176U, C235b);
                    }
                    {
                      extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

                      val = falc_ldx_i_wrapper(84736U, 0U);
                    }
                    *((lw_types__lwu32 *)&scp_rndctl11_reg) = val;
                    scp_rndctl11_reg.f_autocal_static_tap_a = 15U;
                    scp_rndctl11_reg.f_autocal_static_tap_b = 15U;
                    {
                      const lw_types__lwu32 C241b = (*(lw_types__lwu32*)(&scp_rndctl11_reg));
                      {
                        extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

                        falc_stxb_wrapper(84736U, C241b);
                      }
                      {
                        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

                        val = falc_ldx_i_wrapper(65792U, 0U);
                      }
                      *((lw_types__lwu32 *)&scp_ctl1_reg) = val;
                      scp_ctl1_reg.f_rng_en = 1;
                      {
                        const lw_types__lwu32 C247b = (*(lw_types__lwu32*)(&scp_ctl1_reg));
                        {
                          extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

                          falc_stxb_wrapper(65792U, C247b);
                        }

                      }
                    }
                  }}
                falc_scp_trap(31U);
                {
                  lw_types__lwu32 C251b;
                  {

                    C251b = falc_sethi_i(C227b, 5U);
                  }
                  falc_trapped_dmwrite(C251b);
                  falc_scp_rand(4U);
                  falc_scp_rand(3U);
                  falc_dmwait();
                  falc_scp_xor(5U, 4U);
                  falc_scp_key(4U);
                  falc_scp_encrypt(3U, 3U);
                  {
                    lw_types__lwu32 C261b;
                    {

                      C261b = falc_sethi_i(C227b, 3U);
                    }
                    falc_trapped_dmread(C261b);
                    falc_dmwait();
                    falc_scp_trap(0U);
                    {
                      lw_types__lwu32 val;
                      dev_sec_csb_ada__lw_csec_scp_ctl1_register scp_ctl1_reg;
                    {
                        extern lw_types__lwu32 falc_ldx_i_wrapper(lw_types__lwu32 addr, lw_types__lwu32 bitnum);

                        val = falc_ldx_i_wrapper(65792U, 0U);
                      }
                      *((lw_types__lwu32 *)&scp_ctl1_reg) = val;
                      scp_ctl1_reg.f_rng_en = 0;
                      {
                        const lw_types__lwu32 C271b = (*(lw_types__lwu32*)(&scp_ctl1_reg));
                        {
                          extern void falc_stxb_wrapper(lw_types__lwu32 addr, lw_types__lwu32 val);

                          falc_stxb_wrapper(65792U, C271b);
                        }

                      }}
                    __stack_chk_guard = ((system__address)((utility__uc_lwu32_to_addressGP1422__source)sec_entry__rand_num16_byte[
                      0U]));

                  }
                }
              }
            }}
        }
        main_blockL274b: ;
      }
      if (status != ucode_post_codes__ucode_err_code_noerror) {
        goto main_block;
      }
      sec_entry__hs_entry_noninline(&status);
    }
  }
  main_block: ;
  hs_init_tu10x__update_status_in_mailbox(status);
  cleanup__ucode_cleanup();
  falc_halt();
  return;
}

