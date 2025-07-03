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
--  Lw_Types_Intrinsic package defines LwUX_Wrap type for each data-type.
package Lw_Types_Intrinsic
with SPARK_Mode => On
is
   pragma Pure;

   type LwU1_Wrap is mod 2 ** 1  with Size => 1;
   type LwU2_Wrap is mod 2 ** 2  with Size => 2;
   type LwU3_Wrap is mod 2 ** 3  with Size => 3;
   type LwU4_Wrap is mod 2 ** 4  with Size => 4;
   type LwU5_Wrap is mod 2 ** 5  with Size => 5;
   type LwU6_Wrap is mod 2 ** 6  with Size => 6;
   type LwU7_Wrap is mod 2 ** 7  with Size => 7;
   type LwU8_Wrap is mod 2 ** 8  with Size => 8;
   type LwU9_Wrap is mod 2 ** 9  with Size => 9;
   type LwU10_Wrap is mod 2 ** 10 with Size => 10;
   type LwU11_Wrap is mod 2 ** 11 with Size => 11;
   type LwU12_Wrap is mod 2 ** 12 with Size => 12;
   type LwU13_Wrap is mod 2 ** 13 with Size => 13;
   type LwU14_Wrap is mod 2 ** 14 with Size => 14;
   type LwU15_Wrap is mod 2 ** 15 with Size => 15;
   type LwU16_Wrap is mod 2 ** 16 with Size => 16;
   type LwU17_Wrap is mod 2 ** 17 with Size => 17;
   type LwU18_Wrap is mod 2 ** 18 with Size => 18;
   type LwU19_Wrap is mod 2 ** 19 with Size => 19;
   type LwU20_Wrap is mod 2 ** 20 with Size => 20;
   type LwU21_Wrap is mod 2 ** 21 with Size => 21;
   type LwU22_Wrap is mod 2 ** 22 with Size => 22;
   type LwU23_Wrap is mod 2 ** 23 with Size => 23;
   type LwU24_Wrap is mod 2 ** 24 with Size => 24;
   type LwU25_Wrap is mod 2 ** 25 with Size => 25;
   type LwU26_Wrap is mod 2 ** 26 with Size => 26;
   type LwU27_Wrap is mod 2 ** 27 with Size => 27;
   type LwU28_Wrap is mod 2 ** 28 with Size => 28;
   type LwU29_Wrap is mod 2 ** 29 with Size => 29;
   type LwU30_Wrap is mod 2 ** 30 with Size => 30;
   type LwU31_Wrap is mod 2 ** 31 with Size => 31;
   type LwU32_Wrap is mod 2 ** 32 with Size => 32;
   type LwU64_Wrap is mod 2 ** 64 with Size => 64;


end Lw_Types_Intrinsic;
