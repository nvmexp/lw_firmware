-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2003-2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Dev_Pwr_Pri_Ada; use Dev_Pwr_Pri_Ada;
with Pub_Pmu_Bar0_Reg_Rd_Wr_Instances; use Pub_Pmu_Bar0_Reg_Rd_Wr_Instances;


package body Pmu_Reset
with SPARK_Mode => On
is
   procedure Reset_Pmu(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Falcon_Cpuctl_Reg :LW_PPWR_FALCON_CPUCTL_REGISTER;
      Ppwr_Falcon_Sctl_Reg :LW_PPWR_FALCON_SCTL_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop

         Put_Pmu_In_Reset(Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Bring_Pmu_Out_Of_Reset(Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Bar0_Reg_Rd32_Ppwr_Falcon_Cpuctl(Reg => Ppwr_Falcon_Cpuctl_Reg,
                                          Addr => Lw_Ppwr_Falcon_Cpuctl,
                                          Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Bar0_Reg_Rd32_Ppwr_Falcon_Sctl(Reg => Ppwr_Falcon_Sctl_Reg,
                                        Addr => Lw_Ppwr_Falcon_Sctl,
                                        Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         -- Verify that PMU reset is successfull
         if Ppwr_Falcon_Cpuctl_Reg.F_Halted /= Lw_Ppwr_Falcon_Cpuctl_Halted_True or else
           Ppwr_Falcon_Sctl_Reg.F_Hsmode /= Lw_Ppwr_Falcon_Sctl_Hsmode_False
         then
            Status := UCODE_PUB_ERR_PMU_RESET;
            Ghost_Pmu_Reset_Status := PMU_NOT_RESET;

            exit Main_Block;

         end if;
         -- Ghost variable is updated with status of pmu reset
         Ghost_Pmu_Reset_Status := PMU_HAS_RESET;

      end loop Main_Block;
   end Reset_Pmu;

   procedure Put_Pmu_In_Reset (Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Falcon_Engine_Reg : LW_PPWR_FALCON_ENGINE_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         Bar0_Reg_Rd32_Ppwr_Falcon_Engine(Reg => Ppwr_Falcon_Engine_Reg,
                                          Addr => Lw_Ppwr_Falcon_Engine,
                                          Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;

         Ppwr_Falcon_Engine_Reg.F_Reset := Lw_Ppwr_Falcon_Engine_Reset_True;
         ---- Put PMU in reset mode ----


         Bar0_Reg_Wr32_Ppwr_Falcon_Engine(Reg => Ppwr_Falcon_Engine_Reg,
                                          Addr => Lw_Ppwr_Falcon_Engine,
                                          Status => Status);
      end loop Main_Block;

   end Put_Pmu_In_Reset;



   procedure Bring_Pmu_Out_Of_Reset (Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
   is
      Ppwr_Falcon_Engine_Reg :LW_PPWR_FALCON_ENGINE_REGISTER;
   begin
      Main_Block:
      for Unused_Loop_Var in 1 .. 1 loop
         Bar0_Reg_Rd32_Ppwr_Falcon_Engine(Reg => Ppwr_Falcon_Engine_Reg,
                                          Addr => Lw_Ppwr_Falcon_Engine,
                                          Status => Status);
         exit Main_Block when Status /= UCODE_ERR_CODE_NOERROR;
         ---- Bring PMU out of reset mode ----
         Ppwr_Falcon_Engine_Reg.F_Reset := Lw_Ppwr_Falcon_Engine_Reset_False;

         Bar0_Reg_Wr32_Ppwr_Falcon_Engine(Reg => Ppwr_Falcon_Engine_Reg,
                                          Addr => Lw_Ppwr_Falcon_Engine,
                                          Status => Status);
      end loop Main_Block;

   end Bring_Pmu_Out_Of_Reset;

end Pmu_Reset;
