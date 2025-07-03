pragma Warnings (Off);
pragma Ada_95;
pragma Restrictions (No_Exception_Propagation);
with System;
package ada_main is

   gnat_exit_status : Integer := 0;

   GNAT_Version : constant String :=
                    "GNAT Version: Pro 20.0w (20190611-83)" & ASCII.NUL;
   pragma Export (C, GNAT_Version, "__gnat_version");

   Ada_Main_Program_Name : constant String := "_ada_fub_main" & ASCII.NUL;
   pragma Export (C, Ada_Main_Program_Name, "__gnat_ada_main_program_name");

   procedure adainit;
   pragma Export (C, adainit, "adainit");

   function main return Integer;
   pragma Export (C, main, "main");

   --  BEGIN ELABORATION ORDER
   --  ada%s
   --  interfaces%s
   --  system%s
   --  system.unsigned_types%s
   --  lw_types_intrinsic%s
   --  compatibility%s
   --  fub_post_codes%s
   --  lw_types_generic%s
   --  lw_types%s
   --  dev_falc_spr%s
   --  lw_types.array_types%s
   --  lw_types.boolean%s
   --  lw_types.falcon_types%s
   --  lw_types.reg_addr_types%s
   --  dev_fpf%s
   --  dev_fuse%s
   --  dev_master%s
   --  dev_sec_csb_ada%s
   --  lw_types.shift_right_op%s
   --  scp_cci_intrinsics%s
   --  scp_cci_intrinsics%b
   --  tu10x%s
   --  ucode_post_codes%s
   --  se_vhr_dgpu%s
   --  utility%s
   --  utility%b
   --  exceptions_lw%s
   --  exceptions_tu10x%s
   --  exceptions_tu10x%b
   --  exceptions_lw%b
   --  pri_err_handling%s
   --  pri_err_handling%b
   --  reg_rd_wr_csb_tu10x%s
   --  reg_rd_wr_csb_tu10x%b
   --  csb_reg_rd_wr_instances%s
   --  reg_rd_wr_bar0_tu10x%s
   --  reg_rd_wr_bar0_tu10x%b
   --  bar0_reg_rd_wr_instances%s
   --  hs_init_common_tu104%s
   --  hs_init_common_tu104%b
   --  hs_init_lw%s
   --  hs_init_lw%b
   --  hs_init_tu10x%s
   --  hs_init_tu10x%b
   --  reset_plm_tu10x%s
   --  reset_plm_tu10x%b
   --  scp_rand_tu10x%s
   --  scp_rand_tu10x%b
   --  scp_rand_common_lw%s
   --  scp_rand_common_lw%b
   --  select_functions_board_specific%s
   --  cleanup%s
   --  cleanup%b
   --  sec_entry%s
   --  sec_entry%b
   --  tu10x.hal%s
   --  tu10x.hal%b
   --  fub_entry%s
   --  fub_entry%b
   --  fub_main%b
   --  END ELABORATION ORDER

end ada_main;
