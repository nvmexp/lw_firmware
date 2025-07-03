-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;
with Ghost_Initializer; use Ghost_Initializer;
with Csb_Access_Contracts; use Csb_Access_Contracts;
with Reg_Rd_Wr_Csb_Falc_Specific_Tu10x; use Reg_Rd_Wr_Csb_Falc_Specific_Tu10x;
with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
--  @summary
--  HW and Arch Specific HS Init Procedures
--
--  @description
--  This package contains procedure which are various steps of HS Init .

package Hs_Init_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   package Csb_Reg_Access_Csberrstat is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
     (GENERIC_REGISTER => LW_CSEC_FALCON_CSBERRSTAT_REGISTER);
   package Csb_Reg_Access_Cmembase is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
     (GENERIC_REGISTER => LW_CSEC_FALCON_CMEMBASE_REGISTER);
   package Csb_Reg_Access_Cpuctl is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
     (GENERIC_REGISTER => LW_CSEC_FALCON_CPUCTL_REGISTER);
      package Csb_Reg_Access_Hsctl is new Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
     (GENERIC_REGISTER => LW_CSEC_FALCON_HSCTL_REGISTER);

   procedure Is_Valid_Falcon_Engine(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                   Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                     Ghost_Valid_Engine_State = VALID_ENGINE),
         Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                    Status = FLCN_PRI_ERROR elsif
                      Ghost_Valid_Engine_State = ILWALID_ENGINE then
                        Status = UNEXPECTED_FALCON_ENGID else
                          Status = UCODE_ERR_CODE_NOERROR),
           Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                      In_Out => (Ghost_Valid_Engine_State,
                                 Ghost_Csb_Pri_Error_State)),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  Ghost_Valid_Engine_State => Ghost_Valid_Engine_State,
                  null => Status),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Enable_Csb_Err_Reporting( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => ( Status = UCODE_ERR_CODE_NOERROR and then
                  Ghost_Hs_Init_States_Tracker = DMEM_SCRUBBED and then
                    Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                      Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_DISBALED),
         Post => ( (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                           Status = FLCN_PRI_ERROR else
                     (Status = UCODE_ERR_CODE_NOERROR and then
                        Ghost_Hs_Init_States_Tracker = CSB_ERROR_REPORTING_ENABLED and then
                          Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED))),
       Global => (In_Out => (Ghost_Hs_Init_States_Tracker,
                             Ghost_Csb_Pri_Error_State,
                             Ghost_Csb_Err_Reporting_State)),
     Depends => ( Status => null,
                  Ghost_Csb_Err_Reporting_State => null,
                  Ghost_Hs_Init_States_Tracker => null,
                  Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  null => (Ghost_Hs_Init_States_Tracker,
                           Status,
                           Ghost_Csb_Err_Reporting_State)),
     Inline_Always;

   -- TODO : Add main block
   procedure Set_Bar0_Timeout(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => ( Status = UCODE_ERR_CODE_NOERROR and then
                  Ghost_Hs_Init_States_Tracker = RESET_PLM_PROTECTED and then
                    Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                      Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                        Ghost_Bar0_Tmout_State = BAR0_TIMOUT_NOTSET),
           Post => ( if Status = UCODE_ERR_CODE_NOERROR then
                       Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                         Ghost_Bar0_Tmout_State = BAR0_TIMOUT_SET and then
                           Ghost_Hs_Init_States_Tracker = BAR0_TMOUT_SET
                    ),
           Global => (Proof_In => (Ghost_Csb_Err_Reporting_State),
                      In_Out => (Ghost_Hs_Init_States_Tracker,
                                 Ghost_Csb_Pri_Error_State,
                                 Ghost_Bar0_Tmout_State)),
     Depends => (Status => null,
                 Ghost_Bar0_Tmout_State => Ghost_Bar0_Tmout_State,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 Ghost_Hs_Init_States_Tracker => Ghost_Hs_Init_States_Tracker,
                 null => Status),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Prevent_Halted_Cpu_From_Being_Restarted( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Hs_Init_States_Tracker = CMEMBASE_APERTURE_DISABLED and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                     Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
         Post => ( if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                     Status = FLCN_PRI_ERROR else
                       (Status = UCODE_ERR_CODE_NOERROR and then
                                Ghost_Hs_Init_States_Tracker = HS_RESTART_PREVENTED)),
           Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                      In_Out => (Ghost_Hs_Init_States_Tracker,
                                 Ghost_Csb_Pri_Error_State)),
     Depends => ( Status => null,
                  Ghost_Hs_Init_States_Tracker => Ghost_Hs_Init_States_Tracker,
                  Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  null => Status),
     Inline_Always;

   procedure Disable_Cmembase_Aperture(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Status = UCODE_ERR_CODE_NOERROR and then
                 Ghost_Hs_Init_States_Tracker = CSB_ERROR_REPORTING_ENABLED and then
                   Ghost_Csb_Err_Reporting_State = CSB_ERR_REPORTING_ENABLED and then
                     Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE),
         Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                    Status = FLCN_PRI_ERROR else
                      (Status = UCODE_ERR_CODE_NOERROR and then
                               Ghost_Hs_Init_States_Tracker = CMEMBASE_APERTURE_DISABLED)),
           Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                      In_Out => (Ghost_Hs_Init_States_Tracker,
                                 Ghost_Csb_Pri_Error_State)),
     Depends => (Status => null,
                 Ghost_Hs_Init_States_Tracker => Ghost_Hs_Init_States_Tracker,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 null => Status),
     Inline_Always;

   procedure Disable_Trace_Pc_Buffer(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Csb_Access_Pre_Contract(Status) and then
                     Ghost_Hs_Init_States_Tracker = CMEMBASE_APERTURE_DISABLED),
         Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                    Status = FLCN_PRI_ERROR else
                      (Status = UCODE_ERR_CODE_NOERROR and then
                               Ghost_Hs_Init_States_Tracker = TRACE_PC_BUFFER_DISABLED)),
           Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                      In_Out => (Ghost_Hs_Init_States_Tracker,
                                 Ghost_Csb_Pri_Error_State)),
     Depends => (Status => null,
                 Ghost_Hs_Init_States_Tracker => Ghost_Hs_Init_States_Tracker,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 null => Status),
     Inline_Always;

   procedure Callwlate_Imem_Size_In_256B(Imem_Size : out LwU32;
                                         Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Csb_Access_Pre_Contract(Status),
     Post => (if Status = FLCN_PRI_ERROR then
                Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE and then
                  Imem_Size = 0
                    elsif Status = UCODE_ERR_CODE_NOERROR then
                (Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                     Imem_Size > 0)),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => Ghost_Csb_Pri_Error_State),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 Imem_Size => null,
                 null => Status),
     Inline_Always;

end Hs_Init_Falc_Specific_Tu10x;
