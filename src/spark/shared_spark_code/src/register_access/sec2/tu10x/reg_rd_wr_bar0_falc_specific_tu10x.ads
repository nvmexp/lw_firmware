-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types.Reg_Addr_Types; use Lw_Types.Reg_Addr_Types;
with Lw_Types; use Lw_Types;
with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Register_Whitelist_Shared; use Register_Whitelist_Shared;
with Register_Whitelist_Ucode_Specific; use Register_Whitelist_Ucode_Specific;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;
with Bar0_Access_Contracts; use Bar0_Access_Contracts;

--  @summary
--  BAR0 register read/write routines.
--
--  @description
--  This package contains procedures and required wrappers to
--  read and write register using BAR0 transactions.

package Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   -- TODO : Reconfirm Status dependencies after reading on PRI_ERR/BAR0_WAIT
   generic
      type GENERIC_REGISTER is private;
      --  Requirement: This procedure shall do the following:
      --  1) Accept address of BAR0 register as input.
      --  2) Read and data from BAR0 register.
      --  @param Reg  Output parameter : Data read from BAR0 register.
      --  @param Addr Input parameter  : Address of BAR0 register to read from.
   procedure Bar0_Reg_Rd32_Generic( Reg  : out GENERIC_REGISTER;
                                    Addr : BAR0_ADDR;
                                    Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => ( Bar0_Access_Pre_Contract(Status) and then
                      GENERIC_REGISTER'Size = LwU32'Size and then
                  ( Is_Valid_Shared_Bar0_Read_Address( Addr ) or else
                       Is_Valid_Ucode_Bar0_Register_Read_Address( Addr ))),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                Status = FLCN_PRI_ERROR
                  elsif Ghost_Bar0_Status = BAR0_WAITING then
                    Status = FLCN_BAR0_WAIT
                      else
                        Status = UCODE_ERR_CODE_NOERROR),
             Global => (Proof_In => (Ghost_Bar0_Tmout_State,
                                     Ghost_Csb_Err_Reporting_State),
                        In_Out => (Ghost_Csb_Pri_Error_State,
                                   Ghost_Bar0_Status)),
     Depends => ( Reg => Addr, Status => null,
                  Ghost_Csb_Pri_Error_State => null,
                  Ghost_Bar0_Status => null,
                  null => (Status,
                           Ghost_Csb_Pri_Error_State,
                           Ghost_Bar0_Status));
   generic
      type GENERIC_REGISTER is private;
      --  Requirement: This procedure shall do the following:
      --  1) Accept address of BAR0 register as input.
      --  2) Accept data to write to BAR0 register as input.
      --  3) Write data to BAR0 register.
      --  @param Reg  Input parameter : Data to write to BAR0 register.
      --  @param Addr Input parameter : Address of BAR0 register to write to.
   procedure Bar0_Reg_Wr32_Generic( Reg  : GENERIC_REGISTER;
                                    Addr : BAR0_ADDR;
                                    Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => ( Bar0_Access_Pre_Contract(Status) and then
                      GENERIC_REGISTER'Size = LwU32'Size and then
                  ( Is_Valid_Shared_Bar0_Read_Address( Addr ) or else
                       Is_Valid_Ucode_Bar0_Register_Write_Address( Addr ))),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                Status = FLCN_PRI_ERROR
                  elsif Ghost_Bar0_Status = BAR0_WAITING then
                    Status = FLCN_BAR0_WAIT
                      else
                        Status = UCODE_ERR_CODE_NOERROR),
             Global => (Proof_In => (Ghost_Bar0_Tmout_State,
                                     Ghost_Csb_Err_Reporting_State),
                        In_Out => (Ghost_Csb_Pri_Error_State,
                                   Ghost_Bar0_Status)),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => null,
                 Ghost_Bar0_Status => null,
                 null => ( Reg,
                           Addr,
                           Status,
                           Ghost_Csb_Pri_Error_State,
                           Ghost_Bar0_Status) );
   generic
      type GENERIC_REGISTER is private;
      --  Requirement: This procedure shall do the following:
      --  1) Accept address of BAR0 register as input.
      --  2) Accept data to write to BAR0 register as input.
      --  3) Write data to BAR0 register.
      --  4) Also reads back the value writte to register to
      --     make sure there were no hardware errors while writing the data.
      --  @param Reg  Input parameter : Data to write to BAR0 register.
      --  @param Addr Input parameter : Address of BAR0 register to write to.
   procedure Bar0_Reg_Wr32_Readback_Generic( Reg  : GENERIC_REGISTER;
                                    Addr : BAR0_ADDR;
                                    Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => ( Bar0_Access_Pre_Contract(Status) and then
                      GENERIC_REGISTER'Size = LwU32'Size and then
                  ( Is_Valid_Shared_Bar0_Read_Address( Addr ) or else
                       Is_Valid_Ucode_Bar0_Register_Write_Address( Addr ))),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                Status = FLCN_PRI_ERROR
                  elsif Ghost_Bar0_Status = BAR0_WAITING then
                    Status = FLCN_BAR0_WAIT
                      else
                        Status = UCODE_ERR_CODE_NOERROR),
             Global => (Proof_In => (Ghost_Bar0_Tmout_State,
                                     Ghost_Csb_Err_Reporting_State),
                        In_Out => (Ghost_Csb_Pri_Error_State,
                                   Ghost_Bar0_Status)),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => null,
                 Ghost_Bar0_Status => null,
                 null => ( Reg,
                           Addr,
                           Status,
                           Ghost_Csb_Pri_Error_State,
                           Ghost_Bar0_Status) );

   procedure Bar0_Wait_Idle(Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                Status = FLCN_PRI_ERROR
                  elsif Ghost_Bar0_Status = BAR0_WAITING then
                    Status = FLCN_BAR0_WAIT
                      else
                        Status = UCODE_ERR_CODE_NOERROR),
             Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                                     Ghost_Bar0_Tmout_State),
                        In_Out => (Ghost_Csb_Pri_Error_State,
                                   Ghost_Bar0_Status)),
     Depends => (Status => Status,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 Ghost_Bar0_Status => Ghost_Bar0_Status),
     Linker_Section => LINKER_SECTION_NAME;

private

   Lw_Csec_Bar0_Csr_Byte_Enable_Val_For_Bar0_Read :
   constant LW_CSEC_BAR0_CSR_BYTE_ENABLE_FIELD
     := 16#0000_000F#;

   --  Wrapper which issues a falcon2ext read transaction(Target HOST-Blockingi) when DMEM
   --- aperture is enabled.
   --  @param Val  Output parameter : Value read from BAR0 register.
   --  @param Addr Input parameter  : Address of BAR0 register to read from.
   procedure Bar0_Reg_Rd32_Private( Val  : out LwU32;
                                    Addr : BAR0_ADDR;
                                    Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                Status = FLCN_PRI_ERROR
                  elsif Ghost_Bar0_Status = BAR0_WAITING then
                (Status = FLCN_BAR0_WAIT and then
                     Val = 0)
                    else
                      Status = UCODE_ERR_CODE_NOERROR),
           Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                                   Ghost_Bar0_Tmout_State),
                      In_Out => (Ghost_Csb_Pri_Error_State,
                                 Ghost_Bar0_Status)),
     Linker_Section => LINKER_SECTION_NAME,
     Depends => ( (Val, Status) => Status,
                  Ghost_Csb_Pri_Error_State => (Status, Ghost_Csb_Pri_Error_State),
                  Ghost_Bar0_Status => (Status,
                                        Ghost_Bar0_Status),
                  null => Addr);

   --  Wrapper which issues a falcon2ext write transaction(Target HOST-Blocking) when DMEM
   --  aperture is enabled.
   --  @param Val  Input parameter : Data to write to BAR0 register.
   --  @param Addr Input parameter : Address of BAR0 register to write to.
   procedure Bar0_Reg_Wr32_Private( Val  : LwU32;
                                    Addr : BAR0_ADDR;
                                    Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                Status = FLCN_PRI_ERROR
                  elsif Ghost_Bar0_Status = BAR0_WAITING then
                    Status = FLCN_BAR0_WAIT
                      else
                        Status = UCODE_ERR_CODE_NOERROR),
             Global => (Proof_In => (Ghost_Csb_Err_Reporting_State,
                                     Ghost_Bar0_Tmout_State),
                        In_Out => (Ghost_Csb_Pri_Error_State,
                                   Ghost_Bar0_Status)),
     Depends => (Status => Status,
                 Ghost_Csb_Pri_Error_State => (Status, Ghost_Csb_Pri_Error_State),
                 Ghost_Bar0_Status => (Status,
                                       Ghost_Bar0_Status),
                 null => ( Val,
                           Addr) ),
     Linker_Section => LINKER_SECTION_NAME;
end Reg_Rd_Wr_Bar0_Falc_Specific_Tu10x;
