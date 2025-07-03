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
with Register_Whitelist_Shared; use Register_Whitelist_Shared;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Register_Whitelist_Ucode_Specific; use Register_Whitelist_Ucode_Specific;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;
with Ada.Unchecked_Colwersion;
with Csb_Access_Contracts; use Csb_Access_Contracts;

--  @summary
--  CSB register read/write routines.
--
--  @description
--  This package contains procedures and required wrappers to read and write register using CSB transactions.
package Reg_Rd_Wr_Csb_Falc_Specific_Tu10x
with SPARK_Mode => On
is

   generic
      type GENERIC_REGISTER is private;

      --  Requirement: This procedure shall do the following:
      --  1) Accept address of CSB register as input.
      --  2) Read and data from CSB register.
      --  @param Reg  Output parameter : Data read from CSB register.
      --  @param Addr Input parameter  : Address of CSB register to read from.
   procedure Csb_Reg_Rd32_Generic (Reg  : out GENERIC_REGISTER;
                                   Addr : CSB_ADDR;
                                   Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Csb_Access_Pre_Contract(Status) and then
                     GENERIC_REGISTER'Size = LwU32'Size and then
                 ( Is_Valid_Shared_Csb_Read_Address( LwU32(Addr) ) or else
                    Is_Valid_Ucode_Csb_Register_Read_Address )),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                Status = FLCN_PRI_ERROR else
                  Status = UCODE_ERR_CODE_NOERROR),
       Global => (Proof_In => (Ghost_Csb_Err_Reporting_State),
                  In_Out => Ghost_Csb_Pri_Error_State),
     Depends => ( Reg => Addr,
                  Status => null,
                  Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  null => Status);

   generic
      type GENERIC_REGISTER is private;

      --  Requirement: This procedure shall do the following:
      --  1) Accept address of CSB register as input.
      --  2) Accept data to write to CSB register as input.
      --  3) Write data to CSB register.
      --  @param Reg  Input parameter : Data to write to CSB register.
      --  @param Addr Input parameter : Address of CSB register to write to.
   procedure Csb_Reg_Wr32_Generic (Reg  : GENERIC_REGISTER;
                                   Addr : CSB_ADDR;
                                   Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => ( Csb_Access_Pre_Contract(Status) and then
                      GENERIC_REGISTER'Size = LwU32'Size and then
                  ( Is_Valid_Shared_Csb_Write_Address(LwU32(Addr) ) or else
                     Is_Valid_Ucode_Csb_Register_Write_Address ) ),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                Status = FLCN_PRI_ERROR else
                  Status = UCODE_ERR_CODE_NOERROR),
       Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                  In_Out => Ghost_Csb_Pri_Error_State),
     Depends => ( Status => null,
                  Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                  null => ( Reg, Addr, Status));

   generic
      type GENERIC_REGISTER is private;

   package Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x
   with SPARK_Mode => On
   is

      --  Requirement: This procedure shall do the following:
      --  1) Accept address of CSB register as input.
      --  2) Read and data from CSB register.
      --  @param Reg  Output parameter : Data read from CSB register.
      --  @param Addr Input parameter  : Address of CSB register to read from.
      procedure Csb_Reg_Rd32_Generic_Inline (Reg  : out GENERIC_REGISTER;
                                      Addr : CSB_ADDR;
                                      Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
        with
          Pre => (Csb_Access_Pre_Contract(Status) and then
                        GENERIC_REGISTER'Size = LwU32'Size and then
                    ( Is_Valid_Shared_Csb_Read_Address( LwU32(Addr) ) or else
                       Is_Valid_Ucode_Csb_Register_Read_Address )),
        Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                   Status = FLCN_PRI_ERROR else
                     Status = UCODE_ERR_CODE_NOERROR),
          Global => (Proof_In => (Ghost_Csb_Err_Reporting_State),
                     In_Out => Ghost_Csb_Pri_Error_State),
        Depends => ( Reg => Addr,
                     Status => null,
                     Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                     null => Status),
        Inline_Always;

      --  Requirement: This procedure shall do the following:
      --  1) Accept address of CSB register as input.
      --  2) Accept data to write to CSB register as input.
      --  3) Write data to CSB register.
      --  @param Reg  Input parameter : Data to write to CSB register.
      --  @param Addr Input parameter : Address of CSB register to write to.
      procedure Csb_Reg_Wr32_Generic_Inline (Reg  : GENERIC_REGISTER;
                                      Addr : CSB_ADDR;
                                      Status :  in out LW_UCODE_UNIFIED_ERROR_TYPE)
        with
          Pre => ( Csb_Access_Pre_Contract(Status) and then
                         GENERIC_REGISTER'Size = LwU32'Size and then
                     ( Is_Valid_Shared_Csb_Write_Address(LwU32(Addr) ) or else
                        Is_Valid_Ucode_Csb_Register_Write_Address ) ),
        Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                   Status = FLCN_PRI_ERROR else
                     Status = UCODE_ERR_CODE_NOERROR),
          Global => (Proof_In => Ghost_Csb_Err_Reporting_State,
                     In_Out => Ghost_Csb_Pri_Error_State),
        Depends => ( Status => null,
                     Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                     null => ( Reg, Addr, Status)),
        Inline_Always;


      --  Requirement: This procedure shall do the following:
      --  1) Accept address of CSB register as input.
      --  2) Accept data to write to CSB register as input.
      --  3) Write data to CSB register.
      --  @param Reg  Input parameter : Data to write to CSB register.
      --  @param Addr Input parameter : Address of CSB register to write to.
      procedure Csb_Reg_Wr32_Generic_No_Peh (Reg  : GENERIC_REGISTER;
                                             Addr : CSB_ADDR)
        with
          Pre => ( GENERIC_REGISTER'Size = LwU32'Size and then
                     ( Is_Valid_Shared_Csb_Write_Address(LwU32(Addr) ) or else
                        Is_Valid_Ucode_Csb_Register_Write_Address ) ),
        Depends => ( null => ( Reg, Addr) ),
        Inline_Always;

   end Inlined_Rd_Wr_Csb_Falc_Specific_Tu10x;

end Reg_Rd_Wr_Csb_Falc_Specific_Tu10x;
