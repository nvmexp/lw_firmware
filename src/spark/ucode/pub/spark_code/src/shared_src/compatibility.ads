-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

--  @summary
--  Defines build version at compile time. Used for SW Revocation
--
--  @description
--  Compatibility package defines typed constants for sw defined compile time
-- build version used for SW Revocation.
--

package Compatibility
with SPARK_Mode => On
is

   -- TODO : This is temp solution. Move to more robust solution in next instance.
   UCODE_TU102_UCODE_BUILD_VERSION  :  constant LwU8 := 0;
   UCODE_TU104_UCODE_BUILD_VERSION  :  constant LwU8 := 3;
   UCODE_TU106_UCODE_BUILD_VERSION  :  constant LwU8 := 0;

end Compatibility;
