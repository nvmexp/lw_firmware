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
with Dev_Master; use Dev_Master;
with Dev_Fpf; use Dev_Fpf;
with Dev_Fuse_Ada; use Dev_Fuse_Ada;
with Dev_Sec_Csb_Ada; use Dev_Sec_Csb_Ada;

--  @summary
--  Register whitelist for Shared Spark Code
--
--  @description
--  This package contains list of registers addrs that are allowed by 
--  Register Access functions for shared code.

package Register_Whitelist_Shared 
with SPARK_Mode => On,
  Ghost
is

   function Is_Valid_Shared_Bar0_Read_Address( Addr : BAR0_ADDR )
                                              return Boolean is
      (Addr = LW_PMC_BOOT_42_ADDR or else
        Addr = LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_ADDR or else
        Addr = LW_FPF_OPT_FUSE_UCODE_ACR_HS_REV_ADDR)
     with
       Global => null,
       Depends => (Is_Valid_Shared_Bar0_Read_Address'Result => Addr);   

   function Is_Valid_Shared_Csb_Read_Address( Addr : LwU32 )
                                             return Boolean is
      (Addr = LwU32 (Lw_Csec_Falcon_Cpuctl  )   or else          
          Addr = LwU32 (Lw_Csec_Falcon_Hwcfg1   )   or else
          Addr = LwU32 (Lw_Csec_Falcon_Engid    )   or else
          Addr = LwU32 (Lw_Csec_Falcon_Ptimer0  )   or else
          Addr = LwU32 (Lw_Csec_Falcon_Doc_Ctrl )   or else
          Addr = LwU32 (Lw_Csec_Falcon_Dic_Ctrl )   or else
          Addr = LwU32 (Lw_Csec_Falcon_Csberrstat)  or else
          Addr = LwU32 (Lw_Csec_Bar0_Csr        )   or else
          Addr = LwU32 (Lw_Csec_Falcon_Vhrcfg_Base))
     with 
         Global => null,
         Depends => (Is_Valid_Shared_Csb_Read_Address'Result => Addr);

   function Is_Valid_Shared_Bar0_Write_Address( Addr : BAR0_ADDR )
                                               return Boolean is
         (Addr = LW_PMC_BOOT_42_ADDR or else
        Addr = LW_FUSE_OPT_FUSE_UCODE_ACR_HS_REV_ADDR or else
        Addr = LW_FPF_OPT_FUSE_UCODE_ACR_HS_REV_ADDR)
     with
          Global => null,
          Depends => (Is_Valid_Shared_Bar0_Write_Address'Result => Addr);
   

   function Is_Valid_Shared_Csb_Write_Address( Addr : LwU32 )
                                              return Boolean is
      ( Addr = LwU32 ( Lw_Csec_Falcon_Cpuctl  ) or else
          Addr =  LwU32 ( Lw_Csec_Falcon_Dic_Ctrl ) or else
          Addr =  LwU32 ( Lw_Csec_Falcon_Reset_Priv_Level_Mask ) or else
          Addr =  LwU32 ( Lw_Csec_Bar0_Csr )          or else
          Addr =  LwU32 ( Lw_Csec_Falcon_Mailbox0 )    or else
          Addr =  LwU32 ( Lw_Csec_Bar0_Tmout ) )
     with 
         Global => null,
         Depends => (Is_Valid_Shared_Csb_Write_Address'Result => Addr);

end Register_Whitelist_Shared;
