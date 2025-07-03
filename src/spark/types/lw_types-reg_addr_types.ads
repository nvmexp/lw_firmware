-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

--  @summary
--  Defining types
--
--  @description
--  Lw_Types package instantiates Lw_Types_Generic package to enable arithmetic operations on
--  all types.
package Lw_Types.Reg_Addr_Types
with SPARK_Mode => On
is

   MAX_BAR0_ADDR : constant LwU32 := 2**24 - 4; -- Address space is 24 bits
   type BAR0_ADDR is new LwU32 range 0 .. MAX_BAR0_ADDR;

   --  CSB address space for TU104 PMU is 256K. -- TODO : Max was 4_000 for PMU, but Sec2 has larger range
   type CSB_ADDR is new LwU32 range 0 .. 16#0005_0000#;

end Lw_Types.Reg_Addr_Types;
