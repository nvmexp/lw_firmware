-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Riscv_Core; use Riscv_Core;
with Dev_Fuse; use Dev_Fuse;

package body Fusing is

   procedure Check_Fusing(Err_Code : in out Error_Codes) is
      procedure Bar0_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Rd32_Generic (Generic_Register => LW_FUSE_OPT_PRIV_SEC_EN_REGISTER);
      procedure Bar0_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Rd32_Generic (Generic_Register => LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_HALT_REGISTER);
      procedure Bar0_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Rd32_Generic (Generic_Register => LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_ASSERT_REGISTER);
      procedure Bar0_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Rd32_Generic (Generic_Register => LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_INTERRUPT_REGISTER);
      procedure Bar0_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Rd32_Generic (Generic_Register => LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS_REGISTER);
      procedure Bar0_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Rd32_Generic (Generic_Register => LW_FUSE_OPT_PMU_RISCV_DEV_DIS_REGISTER);


      Pmu_Debug_Dis : LW_FUSE_OPT_SELWRE_PMU_DEBUG_DIS_REGISTER;

      Priv_Sec : LW_FUSE_OPT_PRIV_SEC_EN_REGISTER;
      Dcls_Sec_Action_Halt : LW_FUSE_OPT_RISCV_DCLS_SELWRITY_ACTION_HALT_REGISTER;
      Pmu_Dev_Dis : LW_FUSE_OPT_PMU_RISCV_DEV_DIS_REGISTER;
   begin

      -- If we're in debug mode, just exit and continue with no error
      Bar0_Reg_Rd32 (Offset => LwU32(Lw_Fuse_Opt_Selwre_Pmu_Debug_Dis),
                       Data => Pmu_Debug_Dis);
      if Pmu_Debug_Dis.F_Data /= Lw_Fuse_Opt_Selwre_Pmu_Debug_Dis_Data_Yes then
         return;
      end if;

      -- Check if privsec is on
      Bar0_Reg_Rd32 (Offset => LwU32(Lw_Fuse_Opt_Priv_Sec_En),
                      Data => Priv_Sec);
      if Priv_Sec.F_Data /= Lw_Fuse_Opt_Priv_Sec_En_Data_Yes then
         Err_Code := CRITICAL_ERROR;
         return;
      end if;

      -- Check RISCV is NOT in devmode
      Bar0_Reg_Rd32 (Offset => LwU32(Lw_Fuse_Opt_Pmu_Riscv_Dev_Dis),
                     Data => Pmu_Dev_Dis);
      if Pmu_Dev_Dis.F_Data /= Lw_Fuse_Opt_Pmu_Riscv_Dev_Dis_Data_Yes then
         Err_Code := CRITICAL_ERROR;
         return;
      end if;

      -- Check if DCLS is on with action Halt enabled
      Bar0_Reg_Rd32 (Offset => LwU32(Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Halt),
                     Data => Dcls_Sec_Action_Halt);
      if Dcls_Sec_Action_Halt.F_Data /= Lw_Fuse_Opt_Riscv_Dcls_Selwrity_Action_Halt_Data_Enable then
         Err_Code := CRITICAL_ERROR;
         return;
      end if;

   end Check_Fusing;

end Fusing;
