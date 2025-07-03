-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Ucode_Post_Codes; use Ucode_Post_Codes;
with Compatibility; use Compatibility;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;
with Bar0_Access_Contracts; use Bar0_Access_Contracts;
with Lw_Types.Falcon_Types; use Lw_Types.Falcon_Types;
with Lw_Types.Boolean; use Lw_Types.Boolean;

--  @summary
--  Tu10x specific, HW Independent HS Init procedures
--
--  @description
--  This package contains procedures required for SW Revocation

package Hs_Init_Tu10x
with SPARK_Mode => On
is

   procedure Read_Fpf_Fuse_Pub_Rev(Data : out LwU8; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                (Status = FLCN_PRI_ERROR and then
                     Data = 0)
                    elsif Ghost_Bar0_Status = BAR0_WAITING then
                      Status = FLCN_BAR0_WAIT
                        else
                          Status = UCODE_ERR_CODE_NOERROR),
               Global => (Proof_In => (Ghost_Bar0_Tmout_State,
                                       Ghost_Csb_Err_Reporting_State),
                          In_Out => (Ghost_Csb_Pri_Error_State,
                                     Ghost_Bar0_Status)),
     Depends => (Status => null,
                 Data => null,
                 Ghost_Csb_Pri_Error_State => null,
                 Ghost_Bar0_Status => null,
                 null => (Status,
                          Ghost_Csb_Pri_Error_State,
                          Ghost_Bar0_Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Read_Chip_Id(Chip_Id : out LwU9; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                (Status = FLCN_PRI_ERROR and then
                     Chip_Id = 0)
                    elsif Ghost_Bar0_Status = BAR0_WAITING then
                      Status = FLCN_BAR0_WAIT
                        else
                          Status = UCODE_ERR_CODE_NOERROR),
               Global => (Proof_In => (Ghost_Bar0_Tmout_State,
                                       Ghost_Csb_Err_Reporting_State),
                          In_Out => (Ghost_Csb_Pri_Error_State,
                                     Ghost_Bar0_Status)),
     Depends => (Status => null,
                 Chip_Id => null,
                 Ghost_Csb_Pri_Error_State => null,
                 Ghost_Bar0_Status => null,
                 null => (Status,
                          Ghost_Csb_Pri_Error_State,
                          Ghost_Bar0_Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Read_Fuse_Opt(Data : out LwU8; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => Bar0_Access_Pre_Contract(Status),
     Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                (Status = FLCN_PRI_ERROR and then
                     Data = 0)
                    elsif Ghost_Bar0_Status = BAR0_WAITING then
                      Status = FLCN_BAR0_WAIT
                        else
                (Status = UCODE_ERR_CODE_NOERROR and then
                     (Data >= 0 and then Data <= 255))),
     Global => (Proof_In => (Ghost_Bar0_Tmout_State,
                             Ghost_Csb_Err_Reporting_State),
                In_Out => (Ghost_Csb_Pri_Error_State,
                           Ghost_Bar0_Status)),
     Depends => (Status => null,
                 Data => null,
                 Ghost_Csb_Pri_Error_State => null,
                 Ghost_Bar0_Status => null,
                 null => (Status,
                          Ghost_Csb_Pri_Error_State,
                          Ghost_Bar0_Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Get_Ucode_Rev_Val(Rev    : out LwU8;
                               Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Bar0_Access_Pre_Contract(Status) and then
                     Ghost_Sw_Revocation_Status = SW_REVOCATION_NOT_VERIFIED),
         Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                    (Status = FLCN_PRI_ERROR and then Rev = LwU8'Last)
                          elsif Ghost_Bar0_Status = BAR0_WAITING then
                            Status = FLCN_BAR0_WAIT
                              elsif Rev /= UCODE_TU102_UCODE_BUILD_VERSION and then
                                Rev /= UCODE_TU104_UCODE_BUILD_VERSION and then
                                  Rev /= UCODE_TU106_UCODE_BUILD_VERSION and then
                                    Ghost_Sw_Revocation_Status = SW_REVOCATION_ILWALID then
                                      Status = CHIP_ID_ILWALID
                                        else
                    (Status = UCODE_ERR_CODE_NOERROR and then
                             Ghost_Sw_Revocation_Status = SW_REVOCATION_VALID)),
           Global => (Proof_In => (Ghost_Bar0_Tmout_State,
                                   Ghost_Csb_Err_Reporting_State),
                      In_Out => (Ghost_Csb_Pri_Error_State,
                                 Ghost_Bar0_Status,
                                 Ghost_Sw_Revocation_Status)),
     Depends => (Status => null,
                 Rev => null,
                 Ghost_Csb_Pri_Error_State => null,
                 Ghost_Bar0_Status => null,
                 Ghost_Sw_Revocation_Status => Ghost_Sw_Revocation_Status,
                 null => (Status,
                          Ghost_Csb_Pri_Error_State,
                          Ghost_Bar0_Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Check_Imem_Blk_Validity (Val : LwU32;
                                      Ilwalidate : out LwBool)
     with
       Global => null,
       Depends => (Ilwalidate => Val),
     Inline_Always;
private
   function Imblk_Is_Valid( Imblk_Status : LW_FLCN_IMTAG_BLK_REGISTER ) return Boolean is
     ( Imblk_Status.F_Blk_Valid = 1 and then
      Imblk_Status.F_Blk_Pending = 0 )
     with
       Global => null,
       Depends => ( Imblk_Is_Valid'Result => Imblk_Status ),
     Inline_Always;
end Hs_Init_Tu10x;
