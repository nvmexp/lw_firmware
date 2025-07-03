-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Ucode_Post_Codes; use Ucode_Post_Codes;
with Lw_Types; use Lw_Types;
with Ucode_Specific_Constants; use Ucode_Specific_Constants;
with Ghost_Initializer; use Ghost_Initializer;
with Ghost_Initializer.Variables; use Ghost_Initializer.Variables;
with Bar0_Access_Contracts; use Bar0_Access_Contracts;
with Csb_Access_Contracts; use Csb_Access_Contracts;

--  @summary
--  Non Inline Hs Init Sequence procedures
--
--  @description
--  This package contains procedures part of non inline HS Init Sequence

package Hs_Init
with SPARK_Mode => on
is

   -- This procedure implements SW revocation. Passes if Rev value in SW is >= Rev value in Fpf fuse.
   -- @param Status Status of the procedure. Reports any error else UCODE_ERR_CODE_NOERROR. It is output.
   -- TODO: Add main block
   procedure Is_Valid_Ucode_Rev ( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Bar0_Access_Pre_Contract(Status) and then
                     Ghost_Hs_Init_States_Tracker = BOOT42_CHECKED and then
                       Ghost_Sw_Revocation_Status = SW_REVOCATION_NOT_VERIFIED),
           Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                      Status = FLCN_PRI_ERROR
                        elsif Ghost_Bar0_Status = BAR0_WAITING then
                          Status = FLCN_BAR0_WAIT
                            elsif Ghost_Sw_Revocation_Status = SW_REVOCATION_ILWALID then
                      (Status = ILWALID_UCODE_REVISION or else
                                 Status = CHIP_ID_ILWALID)
                                else
                      (Status = UCODE_ERR_CODE_NOERROR and then
                                 Ghost_Hs_Init_States_Tracker = SW_REVOCATION_CHECKED and then
                                   Ghost_Sw_Revocation_Status = SW_REVOCATION_VALID)),
               Global => (In_Out => (Ghost_Hs_Init_States_Tracker,
                                     Ghost_Csb_Pri_Error_State,
                                     Ghost_Sw_Revocation_Status,
                                     Ghost_Bar0_Status),
                          Proof_In => (Ghost_Bar0_Tmout_State,
                                       Ghost_Sw_Revocation_Rev_Fpf_Val,
                                       Ghost_Csb_Err_Reporting_State)),
     Depends => ((Status,
                 Ghost_Csb_Pri_Error_State,
                 Ghost_Bar0_Status) => null,
                 Ghost_Hs_Init_States_Tracker => Ghost_Hs_Init_States_Tracker,
                 Ghost_Sw_Revocation_Status => Ghost_Sw_Revocation_Status,
                 null => (Status,
                          Ghost_Csb_Pri_Error_State,
                          Ghost_Bar0_Status)),
     Linker_Section => LINKER_SECTION_NAME;

   -- This procedure checks if the binary is run on the correct chip.
   -- @param Status Status of the procedure. Reports any error else UCODE_ERR_CODE_NOERROR. It is output.
   -- TODO : ADd main block
   procedure Is_Valid_Chip (Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Bar0_Access_Pre_Contract(Status) and then
                     Ghost_Chip_Validity_Status = CHIP_VALIDITY_NOT_VERIFIED),
         Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                    Status = FLCN_PRI_ERROR
                      elsif Ghost_Bar0_Status = BAR0_WAITING then
                        Status = FLCN_BAR0_WAIT
                          elsif Ghost_Chip_Validity_Status = CHIP_ILWALID then
                            Status = CHIP_ID_ILWALID
                              else
                    (Status = UCODE_ERR_CODE_NOERROR and then
                             Ghost_Hs_Init_States_Tracker = BOOT42_CHECKED and then
                               Ghost_Chip_Validity_Status = CHIP_VALID)),
             Global => (In_Out => (Ghost_Hs_Init_States_Tracker,
                                   Ghost_Csb_Pri_Error_State,
                                   Ghost_Chip_Validity_Status,
                                   Ghost_Bar0_Status),
                        Proof_In => (Ghost_Bar0_Tmout_State,
                                     Ghost_Csb_Err_Reporting_State)),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => null,
                 Ghost_Chip_Validity_Status => Ghost_Chip_Validity_Status,
                 Ghost_Bar0_Status => null,
                 Ghost_Hs_Init_States_Tracker => Ghost_Hs_Init_States_Tracker,
                 null => (Status,
                          Ghost_Csb_Pri_Error_State,
                          Ghost_Bar0_Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Ilwalidate_Ns_Blocks( Status : in out LW_UCODE_UNIFIED_ERROR_TYPE )
     with
       Pre => Csb_Access_Pre_Contract(Status) and then
     Ghost_Hs_Init_States_Tracker = SSP_ENABLED,
     Post => (if Status = FLCN_PRI_ERROR then
                Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE
                  elsif Status = UCODE_ERR_CODE_NOERROR then
                    Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_FALSE and then
                      Ghost_Hs_Init_States_Tracker = NS_BLOCKS_ILWALIDATED),
           Global => (In_Out => (Ghost_Hs_Init_States_Tracker,
                                 Ghost_Csb_Pri_Error_State),
                      Proof_In => Ghost_Csb_Err_Reporting_State),
     Depends => (Status => null,
                 Ghost_Hs_Init_States_Tracker => Ghost_Hs_Init_States_Tracker,
                 Ghost_Csb_Pri_Error_State => Ghost_Csb_Pri_Error_State,
                 null => Status),
     Linker_Section => LINKER_SECTION_NAME;

private
   ILWALID_FPF_REV_VAL : constant LwU8 := 9;

   -- This procedure retrieves revision value corresponding fuse value burnt in fpf fuse.
   -- @param Rev Revision value corresponding to fuse value burnt in fpf fuse.
   -- @param Status Status of the procedure. Reports any error else UCODE_ERR_CODE_NOERROR. It is output.
   procedure Get_Ucode_Rev_Fpf_Val(Rev    : out LwU8; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Bar0_Access_Pre_Contract(Status) and then
                     Ghost_Sw_Revocation_Status = SW_REVOCATION_VALID),
         Post => ((if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                    (Status = FLCN_PRI_ERROR and then
                       Rev = 0)
                      elsif Ghost_Bar0_Status = BAR0_WAITING then
                        Status = FLCN_BAR0_WAIT
                          else
                    (Status = UCODE_ERR_CODE_NOERROR)) and then
                    (case Ghost_Sw_Revocation_Rev_Fpf_Val is
                             when 0 => Rev = 0,
                               when 1 => Rev = 1,
                                 when 3 => Rev = 2,
                                   when 7 => Rev = 3,
                                     when 15 => Rev = 4,
                                       when 31 => Rev = 5,
                                         when 63 => Rev = 6,
                                           when 127 => Rev = 7,
                                             when 255 => Rev = 8,
                                               when others => Rev = ILWALID_FPF_REV_VAL)),
                             Global => (Proof_In => (Ghost_Bar0_Tmout_State,
                                                     Ghost_Sw_Revocation_Rev_Fpf_Val,
                                                     Ghost_Csb_Err_Reporting_State,
                                                     Ghost_Sw_Revocation_Status),
                                        In_Out => (Ghost_Csb_Pri_Error_State,
                                                   Ghost_Bar0_Status)),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => null,
                 Rev => null,
                 Ghost_Bar0_Status => null,
                 null => (Status,
                          Ghost_Csb_Pri_Error_State,
                          Ghost_Bar0_Status)),
     Linker_Section => LINKER_SECTION_NAME;

   procedure Get_Ucode_Rev_Chip_Option_Fuse_Val(Revlock_Fuse_Val : out LwU8; Status : in out LW_UCODE_UNIFIED_ERROR_TYPE)
     with
       Pre => (Bar0_Access_Pre_Contract(Status) and then
                     Ghost_Sw_Revocation_Status = SW_REVOCATION_NOT_VERIFIED),
         Post => (if Ghost_Csb_Pri_Error_State = CSB_PRI_ERROR_TRUE then
                    (Status = FLCN_PRI_ERROR and then
                             Revlock_Fuse_Val = 0)
                            elsif Ghost_Bar0_Status = BAR0_WAITING then
                              Status = FLCN_BAR0_WAIT
                                else
                                  Status = UCODE_ERR_CODE_NOERROR),
                   Global => (Proof_In => (Ghost_Bar0_Tmout_State,
                                           Ghost_Csb_Err_Reporting_State,
                                           Ghost_Sw_Revocation_Status),
                              In_Out => (Ghost_Csb_Pri_Error_State,
                                         Ghost_Bar0_Status)),
     Depends => (Status => null,
                 Ghost_Csb_Pri_Error_State => null,
                 Revlock_Fuse_Val => null,
                 Ghost_Bar0_Status => null,
                 null => (Status,
                          Ghost_Csb_Pri_Error_State,
                          Ghost_Bar0_Status)),
     Linker_Section => LINKER_SECTION_NAME;


end Hs_Init;
