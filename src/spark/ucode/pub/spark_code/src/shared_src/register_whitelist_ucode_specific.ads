-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Lw_Types.Reg_Addr_Types; use Lw_Types.Reg_Addr_Types;
with Dev_Graphics_Nobundle_Ada; use Dev_Graphics_Nobundle_Ada;
with Dev_Pwr_Pri_Ada; use Dev_Pwr_Pri_Ada;
with Dev_Therm_Ada; use Dev_Therm_Ada;

--  @summary
--  Register whitelist for Ucode specific Spark Code
--
--  @description
--  This package contains list of registers addrs that are allowed by 
--  Register Access functions for pub code.

package Register_Whitelist_Ucode_Specific
with SPARK_Mode => On,
  Ghost
is

   function Is_Valid_Ucode_Bar0_Register_Read_Address (Addr : BAR0_ADDR)
                                                       return Boolean is
      (Addr = Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask                              or else
        Addr = Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask                          or else
        Addr = Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask                   or else
        Addr = Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask                    or else
        Addr = Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask             or else
        Addr = Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask                       or else
        Addr = Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask                      or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask                        or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask                or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask                    or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask                 or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask             or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask              or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask       or else
        Addr = Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask          or else
        Addr = Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask               or else
        Addr = Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask  or else
        Addr = Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask                              or else
        Addr = Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask           or else
        Addr = Lw_Ppwr_Falcon_Exe_Priv_Level_Mask                                  or else
        Addr = Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask                               or else
        Addr = Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask                                 or else
        Addr = Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask                                 or else
        Addr = Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask                                or else
        Addr = Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask                              or else
        Addr = Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask                               or else
        Addr = Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask                               or else
        Addr = Lw_Therm_Smartfan_Priv_Level_Mask_1                                 or else
        Addr = Lw_Therm_Vidctrl_Priv_Level_Mask                                    or else
        Addr = Lw_Ppwr_Falcon_Exe_Priv_Level_Mask                                  or else
        Addr = Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask                               or else
        Addr = Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask                                 or else
        Addr = Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask                                 or else
        Addr = Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask                                or else
        Addr = Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask                              or else
        Addr = Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask                               or else
        Addr = Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask                               or else
        Addr = Lw_Ppwr_Falcon_Cpuctl                                               or else
        Addr = Lw_Ppwr_Falcon_Sctl                                                 or else
        Addr = Lw_Ppwr_Falcon_Engine                                               or else
        Addr = Lw_Therm_Smartfan_Priv_Level_Mask_1                                 or else
        Addr = Lw_Therm_Vidctrl_Priv_Level_Mask)
     with
       Global => null,
       Depends => (Is_Valid_Ucode_Bar0_Register_Read_Address'Result => Addr);

   function Is_Valid_Ucode_Bar0_Register_Write_Address (Addr : BAR0_ADDR)
                                                        return Boolean is
      (Addr = Lw_Pgraph_Pri_Fecs_Pvs_Priv_Level_Mask                              or else
        Addr = Lw_Pgraph_Pri_Fecs_Arb_Wpr_Priv_Level_Mask                          or else
        Addr = Lw_Pgraph_Pri_Fecs_Falcon_Mthdctx_Priv_Level_Mask                   or else
        Addr = Lw_Pgraph_Pri_Fecs_Falcon_Irqtmr_Priv_Level_Mask                    or else
        Addr = Lw_Pgraph_Pri_Fecs_Arb_Falcon_Regioncfg_Priv_Level_Mask             or else
        Addr = Lw_Pgraph_Pri_Fecs_Falcon_Exe_Priv_Level_Mask                       or else
        Addr = Lw_Pgraph_Pri_Fecs_Rc_Lane_Cmd_Priv_Level_Mask                      or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Pvs_Priv_Level_Mask                        or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Rc_Lane_Cmd_Priv_Level_Mask                or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Wpr_Priv_Level_Mask                    or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Exe_Priv_Level_Mask                 or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Mthdctx_Priv_Level_Mask             or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Falcon_Irqtmr_Priv_Level_Mask              or else
        Addr = Lw_Pgraph_Pri_Gpcs_Gpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask       or else
        Addr = Lw_Pgraph_Pri_Gpcs_Tpcs_Sm_Private_Control_Priv_Level_Mask          or else
        Addr = Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Wpr_Priv_Level_Mask               or else
        Addr = Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Arb_Falcon_Regioncfg_Priv_Level_Mask  or else
        Addr = Lw_Pgraph_Pri_Gpcs_Mmu_Priv_Level_Mask                              or else
        Addr = Lw_Pgraph_Pri_Gpcs_Tpcs_Tpccs_Rc_Lane_Cmd_Priv_Level_Mask           or else
        Addr = Lw_Ppwr_Falcon_Exe_Priv_Level_Mask                                  or else
        Addr = Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask                               or else
        Addr = Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask                                 or else
        Addr = Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask                                 or else
        Addr = Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask                                or else
        Addr = Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask                              or else
        Addr = Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask                               or else
        Addr = Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask                               or else
        Addr = Lw_Therm_Smartfan_Priv_Level_Mask_1                                 or else
        Addr = Lw_Therm_Vidctrl_Priv_Level_Mask                                    or else
        Addr = Lw_Ppwr_Falcon_Exe_Priv_Level_Mask                                  or else
        Addr = Lw_Ppwr_Falcon_Irqtmr_Priv_Level_Mask                               or else
        Addr = Lw_Ppwr_Falcon_Dmem_Priv_Level_Mask                                 or else
        Addr = Lw_Ppwr_Falcon_Sctl_Priv_Level_Mask                                 or else
        Addr = Lw_Ppwr_Falcon_Wdtmr_Priv_Level_Mask                                or else
        Addr = Lw_Ppwr_Fbif_Regioncfg_Priv_Level_Mask                              or else
        Addr = Lw_Ppwr_Pmu_Msgq_Head_Priv_Level_Mask                               or else
        Addr = Lw_Ppwr_Pmu_Msgq_Tail_Priv_Level_Mask                               or else
        Addr = Lw_Ppwr_Falcon_Engine                                               or else           
        Addr = Lw_Therm_Smartfan_Priv_Level_Mask_1                                 or else
        Addr = Lw_Therm_Vidctrl_Priv_Level_Mask)
     with
       Global => null,
       Depends => (Is_Valid_Ucode_Bar0_Register_Write_Address'Result => Addr);

   function Is_Valid_Ucode_Csb_Register_Read_Address
                                                      return Boolean is
     ( True )
     with
       Global => null;

   function Is_Valid_Ucode_Csb_Register_Write_Address
                                                      return Boolean is
     ( True )
     with
       Global => null;

   
end Register_Whitelist_Ucode_Specific;
