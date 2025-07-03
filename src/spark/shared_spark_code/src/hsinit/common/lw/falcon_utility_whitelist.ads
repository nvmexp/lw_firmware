-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_
with Lw_Types; use Lw_Types;
with Lw_Types.Falcon_Types; use Lw_Types.Falcon_Types;

--  @summary
--  Falcon Utility Whitelist Ghost Package.
--
--  @description
--  This package contains Ghost functions to model input conditions for Falcon Utility Package.
package Falcon_Utility_Whitelist
with SPARK_Mode => On,
  Ghost
is

   --  This function represents the input state that Falc_Sclrb_C procedure.
   --  Returns True if
   --  1) Bitnum represents CSW.E bit.
   function Falc_Sclrb_Bitnum_Check_Success( Bitnum : LwU32 )
                                            return Boolean is
     ( ( Bitnum = FALCON_CSW_EXCEPTION_BIT ) or else
          ( Bitnum = FALCON_CSW_INTERRUPT0_EN_BIT ) or else
          ( Bitnum = FALCON_CSW_INTERRUPT1_EN_BIT ) or else
          ( Bitnum = FALCON_CSW_INTERRUPT2_EN_BIT ) )
       with
         Global => null,
         Depends => ( Falc_Sclrb_Bitnum_Check_Success'Result => Bitnum );

   --  This function represents the input state that Falc_Ssetb_I_C procedure.
   --  Returns True if
   --  1) Bitnum represents CSW.1E1 bit.
   function Falc_Ssetb_Bitnum_Check_Success( Bitnum : LwU32 )
                                            return Boolean is
     ( Bitnum = FALCON_CSW_INTERRUPT1_EN_BIT )
     with
       Global => null,
       Depends => ( Falc_Ssetb_Bitnum_Check_Success'Result => Bitnum );

end Falcon_Utility_Whitelist;
