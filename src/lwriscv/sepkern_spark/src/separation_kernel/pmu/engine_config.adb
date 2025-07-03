-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with System;
with Ada.Unchecked_Colwersion;
with Lw_Types; use Lw_Types;
with Lw_Types.Shift_Left_Op; use Lw_Types.Shift_Left_Op;
with Types; use Types;
with Error_Handling; use Error_Handling;
with Riscv_Core; use Riscv_Core;

with Dev_Riscv_Csr_64; use Dev_Riscv_Csr_64;
with Dev_Riscv_Pri; use Dev_Riscv_Pri;
with Dev_Pwr_Pri; use Dev_Pwr_Pri;
with Dev_Falcon_V4; use Dev_Falcon_V4;
with Lw_Riscv_Address_Map; use Lw_Riscv_Address_Map;

package body Engine_Config is
   function Get_Monitor_Code_Size return LwU64 with Inline_Always;
   function Get_Imem_Limit return LwU64 with Inline_Always;

   procedure Configure is
      procedure Csr_Reg_Wr64 is new Riscv_Core.Rv_Csr.Wr64_Generic (Generic_Csr => LW_RISCV_CSR_MCOUNTEREN_Register);

      procedure Bar0_Reg_Wr32 is new Riscv_Core.Bar0_Reg.Wr32_Generic (Generic_Register => LW_PPWR_FALCON_LOCKPMB_REGISTER);
      procedure Riscv_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Riscv_Reg.Rd32_Generic (Generic_Register => LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_REGISTER);
      procedure Riscv_Reg_Wr32 is new Riscv_Core.Bar0_Reg.Riscv_Reg.Wr32_Generic (Generic_Register => LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_REGISTER);
      procedure Bar0_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Rd32_Generic (Generic_Register => LW_PPWR_FALCON_DEBUG1_REGISTER);
      procedure Bar0_Reg_Wr32 is new Riscv_Core.Bar0_Reg.Wr32_Generic (Generic_Register => LW_PPWR_FALCON_DEBUG1_REGISTER);

      procedure Flcn_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Flcn_Reg.Rd32_Generic (Generic_Register => LW_PFALCON_FALCON_ITFEN_REGISTER);
      procedure Flcn_Reg_Wr32 is new Riscv_Core.Bar0_Reg.Flcn_Reg.Wr32_Generic (Generic_Register => LW_PFALCON_FALCON_ITFEN_REGISTER);

      Device_Map_M_Mode :LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_REGISTER;
      Falcon_Debug1 : LW_PPWR_FALCON_DEBUG1_REGISTER;
      Falcon_Itfen : LW_PFALCON_FALCON_ITFEN_REGISTER;
   begin
      -- Release PMB lock (it is set up correctly now by IOPMP)
      Bar0_Reg_Wr32(Offset => LwU32(Lw_Ppwr_Falcon_Lockpmb),
                    Data => LW_PPWR_FALCON_LOCKPMB_REGISTER'(F_Imem => Lw_Ppwr_Falcon_Lockpmb_Imem_Lock,
                                                             F_Dmem => Lw_Ppwr_Falcon_Lockpmb_Dmem_Unlock,
                                                             F_Ext2Mem => Lw_Ppwr_Falcon_Lockpmb_Ext2Mem_Lock));

      -- Block access to PMB group
      Riscv_Reg_Rd32(Offset => LwU32(Lw_Priscv_Riscv_Devicemap_Riscvmmode_Addr(LW_PRISCV_DEVICE_MAP_GROUP_PMB / LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_SIZE_1)),
                     Data => Device_Map_M_Mode);

      Device_Map_M_Mode.F_Val(LW_PRISCV_DEVICE_MAP_GROUP_PMB mod LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_SIZE_1).Read := LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_READ_DISABLE;
      Device_Map_M_Mode.F_Val(LW_PRISCV_DEVICE_MAP_GROUP_PMB mod LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_SIZE_1).Write := LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_WRITE_DISABLE;
      Device_Map_M_Mode.F_Val(LW_PRISCV_DEVICE_MAP_GROUP_PMB mod LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_SIZE_1).Lock := LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_LOCK_LOCKED;

      Riscv_Reg_Wr32(Offset => LwU32(Lw_Priscv_Riscv_Devicemap_Riscvmmode_Addr(LW_PRISCV_DEVICE_MAP_GROUP_PMB / LW_PRISCV_RISCV_DEVICEMAP_RISCVMMODE_SIZE_1)),
                     Data => Device_Map_M_Mode);

      -- Give PMU access to performance counters
      Csr_Reg_Wr64(Addr => LW_RISCV_CSR_MCOUNTEREN_Address,
                   Data => LW_RISCV_CSR_MCOUNTEREN_Register'(Rs0 => LW_RISCV_CSR_MCOUNTEREN_RS0_RST,
                                                             Hpm => LW_RISCV_CSR_MCOUNTEREN_HPM_DISABLE,
                                                             Ir  => IR_ENABLE,
                                                             Tm  => TM_ENABLE,
                                                             Cy  => CY_ENABLE));

      -- Disable idle check during context switch - it is required so RM can do context switch.
      Bar0_Reg_Rd32(Offset => LwU32(Lw_Ppwr_Falcon_Debug1),
                    Data => Falcon_Debug1);
      Falcon_Debug1.F_Ctxsw_Mode1 := Lw_Ppwr_Falcon_Debug1_Ctxsw_Mode1_Bypass_Idle_Checks;

      Bar0_Reg_Wr32(Offset => LwU32(Lw_Ppwr_Falcon_Debug1),
                    Data => Falcon_Debug1);

      Flcn_Reg_Rd32(Offset => LwU32(Lw_Pfalcon_Falcon_Itfen),
                    Data => Falcon_Itfen);

      -- CTXEN - Indicates if context switch interface enabled. When set allows
      -- context switch state machine to react to incoming context switch requests from host.
      Falcon_Itfen.F_Ctxen := Lw_Pfalcon_Falcon_Itfen_Ctxen_Enable;
      Flcn_Reg_Wr32(Offset => LwU32(Lw_Pfalcon_Falcon_Itfen),
                    Data => Falcon_Itfen);

   end Configure;

   procedure Get_Partition_Entry_Point(Partition_Entry_Point : out LwU64; Err_Code : in out Error_Codes) is
      function Make_Code_Address (Addr_Lo : LwU32; Addr_Hi : LwU7) return Phys_Addr is
        (Phys_Addr (Lw_Shift_Left (Value  => LwU64 (Addr_Hi), Amount => 40) or
                             Lw_Shift_Left (Value  => LwU64 (Addr_Lo), Amount => 8)))
        with
          Post => (Make_Code_Address'Result mod 256 = 0),
        Inline_Always;

      function Make_4k_Aligned_Offset(Input_Address : LwU64) return Offset_4K_Aligned
        with
          Inline_Always
      is
      begin
         return Offset_4K_Aligned (((Input_Address) + (Fmc_4k_Alignment - 1)) and (not (Fmc_4k_Alignment - 1)));
      end Make_4k_Aligned_Offset;

      procedure Riscv_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Riscv_Reg.Rd32_Generic (Generic_Register => LW_PRISCV_RISCV_BCR_DMACFG_REGISTER);
      procedure Riscv_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Riscv_Reg.Rd32_Generic (Generic_Register => LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO_REGISTER);
      procedure Riscv_Reg_Rd32 is new Riscv_Core.Bar0_Reg.Riscv_Reg.Rd32_Generic (Generic_Register => LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_HI_REGISTER);

      Bcr_DmaCfg : LW_PRISCV_RISCV_BCR_DMACFG_REGISTER;
      Bcr_Dmaaddr_Fmccode_Lo : LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_LO_REGISTER;
      Bcr_Dmaaddr_Fmccode_Hi : LW_PRISCV_RISCV_BCR_DMAADDR_FMCCODE_HI_REGISTER;

      Base : Phys_Addr;
      Partition_Offset : Offset_4K_Aligned;
   begin

      Partition_Entry_Point := 0;

      Riscv_Reg_Rd32 (Offset => LwU32(Lw_Priscv_Riscv_Bcr_Dmacfg),
                      Data => Bcr_DmaCfg);
      Riscv_Reg_Rd32 (Offset => LwU32(Lw_Priscv_Riscv_Bcr_Dmaaddr_Fmccode_Lo),
                      Data => Bcr_Dmaaddr_Fmccode_Lo);
      Riscv_Reg_Rd32 (Offset => LwU32(Lw_Priscv_Riscv_Bcr_Dmaaddr_Fmccode_Hi),
                      Data => Bcr_Dmaaddr_Fmccode_Hi);

      Base := Make_Code_Address(Addr_Lo => LwU32 (Bcr_Dmaaddr_Fmccode_Lo.F_Val), Addr_Hi => LwU7 (Bcr_Dmaaddr_Fmccode_Hi.F_Val));

      -- TODO: Prover can't see IMEM_LIMIT value
      pragma Assume(Get_Imem_Limit = 16#800#);

      if Get_Monitor_Code_Size > Get_Imem_Limit then
         Err_Code := CRITICAL_ERROR;
      end if;
      if Get_Monitor_Code_Size mod 4 /= 0 then
         Err_Code := CRITICAL_ERROR;
      end if;

      if Err_Code /= OK then
         return;
      end if;

      pragma Assert(Get_Monitor_Code_Size <= Get_Imem_Limit);

      -- Per WPR requirement image needs to be 4k aligned.
      Partition_Offset := Make_4k_Aligned_Offset( LwU64(Base) + Get_Monitor_Code_Size);

      case Bcr_DmaCfg.F_Target is
         when Lw_Priscv_Riscv_Bcr_Dmacfg_Target_Local_Fb =>
            Partition_Entry_Point := LwU64(Partition_Offset) + LW_RISCV_AMAP_FBGPA_START;

         when Lw_Priscv_Riscv_Bcr_Dmacfg_Target_Coherent_Sysmem =>
            Partition_Entry_Point := LwU64(Partition_Offset) + LW_RISCV_AMAP_SYSGPA_START;

         when Lw_Priscv_Riscv_Bcr_Dmacfg_Target_Noncoherent_Sysmem =>
            Partition_Entry_Point := LwU64(Partition_Offset) + LW_RISCV_AMAP_SYSGPA_START;
      end case;

      pragma Assert(Partition_Entry_Point mod Fmc_4k_Alignment = 0);

   end Get_Partition_Entry_Point;

   function Get_Monitor_Code_Size return LwU64 with SPARK_Mode => Off
   is
      Monitor_Code_Size : LwU64 with
        Import        => True,
        Convention    => C,
        External_Name => "__monitor_code_size";

      Monitor_Code_Size_Val : aliased constant System.Address := Monitor_Code_Size'Address;

      function System_Addr_To_LwU64 is new Ada.Unchecked_Colwersion(Source => System.Address,
                                                                    Target => LwU64);
   begin
      return System_Addr_To_LwU64(Monitor_Code_Size_Val);
   end Get_Monitor_Code_Size;

   function Get_Imem_Limit return LwU64 with SPARK_Mode => Off
   is
      Imem_Limit : LwU64 with
        Import        => True,
        Convention    => C,
        External_Name => "_IMEM_LIMIT";

      Imem_Limit_Val : aliased constant System.Address := Imem_Limit'Address;

      function System_Addr_To_LwU64 is new Ada.Unchecked_Colwersion(Source => System.Address,
                                                                    Target => LwU64);
   begin
      return System_Addr_To_LwU64(Imem_Limit_Val);
   end Get_Imem_Limit;

end Engine_Config;
