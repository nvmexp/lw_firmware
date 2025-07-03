-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_
with Lw_Types_Generic;
with Lw_Types_Intrinsic; use Lw_Types_Intrinsic;

--  @summary
--  Defining types
--
--  @description
--  Lw_Types package instantiates Lw_Types_Generic package to enable arithmetic operations on
--  all types.
package Lw_Types
with SPARK_Mode => On
is

   package LwU1_Types  is new Lw_Types_Generic (BASE_TYPE => LwU1_Wrap);
   package LwU2_Types  is new Lw_Types_Generic (BASE_TYPE => LwU2_Wrap);
   package LwU3_Types  is new Lw_Types_Generic (BASE_TYPE => LwU3_Wrap);
   package LwU4_Types  is new Lw_Types_Generic (BASE_TYPE => LwU4_Wrap);
   package LwU5_Types  is new Lw_Types_Generic (BASE_TYPE => LwU5_Wrap);
   package LwU6_Types  is new Lw_Types_Generic (BASE_TYPE => LwU6_Wrap);
   package LwU7_Types  is new Lw_Types_Generic (BASE_TYPE => LwU7_Wrap);
   package LwU8_Types  is new Lw_Types_Generic (BASE_TYPE => LwU8_Wrap);
   package LwU9_Types  is new Lw_Types_Generic (BASE_TYPE => LwU9_Wrap);
   package LwU10_Types is new Lw_Types_Generic (BASE_TYPE => LwU10_Wrap);
   package LwU11_Types is new Lw_Types_Generic (BASE_TYPE => LwU11_Wrap);
   package LwU12_Types is new Lw_Types_Generic (BASE_TYPE => LwU12_Wrap);
   package LwU13_Types is new Lw_Types_Generic (BASE_TYPE => LwU13_Wrap);
   package LwU14_Types is new Lw_Types_Generic (BASE_TYPE => LwU14_Wrap);
   package LwU15_Types is new Lw_Types_Generic (BASE_TYPE => LwU15_Wrap);
   package LwU16_Types is new Lw_Types_Generic (BASE_TYPE => LwU16_Wrap);
   package LwU17_Types is new Lw_Types_Generic (BASE_TYPE => LwU17_Wrap);
   package LwU18_Types is new Lw_Types_Generic (BASE_TYPE => LwU18_Wrap);
   package LwU19_Types is new Lw_Types_Generic (BASE_TYPE => LwU19_Wrap);
   package LwU20_Types is new Lw_Types_Generic (BASE_TYPE => LwU20_Wrap);
   package LwU21_Types is new Lw_Types_Generic (BASE_TYPE => LwU21_Wrap);
   package LwU22_Types is new Lw_Types_Generic (BASE_TYPE => LwU22_Wrap);
   package LwU23_Types is new Lw_Types_Generic (BASE_TYPE => LwU23_Wrap);
   package LwU24_Types is new Lw_Types_Generic (BASE_TYPE => LwU24_Wrap);
   package LwU25_Types is new Lw_Types_Generic (BASE_TYPE => LwU25_Wrap);
   package LwU26_Types is new Lw_Types_Generic (BASE_TYPE => LwU26_Wrap);
   package LwU27_Types is new Lw_Types_Generic (BASE_TYPE => LwU27_Wrap);
   package LwU28_Types is new Lw_Types_Generic (BASE_TYPE => LwU28_Wrap);
   package LwU29_Types is new Lw_Types_Generic (BASE_TYPE => LwU29_Wrap);
   package LwU30_Types is new Lw_Types_Generic (BASE_TYPE => LwU30_Wrap);
   package LwU31_Types is new Lw_Types_Generic (BASE_TYPE => LwU31_Wrap);
   package LwU32_Types is new Lw_Types_Generic (BASE_TYPE => LwU32_Wrap);
   package LwU64_Types is new Lw_Types_Generic (BASE_TYPE => LwU64_Wrap);

   type LwU1 is new LwU1_Types.N_TYPE;
   type LwU2 is new LwU2_Types.N_TYPE;
   type LwU3 is new LwU3_Types.N_TYPE;
   type LwU4 is new LwU4_Types.N_TYPE;
   type LwU5 is new LwU5_Types.N_TYPE;
   type LwU6 is new LwU6_Types.N_TYPE;
   type LwU7 is new LwU7_Types.N_TYPE;
   type LwU8 is new LwU8_Types.N_TYPE;
   type LwU9 is new LwU9_Types.N_TYPE;
   type LwU10 is new LwU10_Types.N_TYPE;
   type LwU11 is new LwU11_Types.N_TYPE;
   type LwU12 is new LwU12_Types.N_TYPE;
   type LwU13 is new LwU13_Types.N_TYPE;
   type LwU14 is new LwU14_Types.N_TYPE;
   type LwU15 is new LwU15_Types.N_TYPE;
   type LwU16 is new LwU16_Types.N_TYPE;
   type LwU17 is new LwU17_Types.N_TYPE;
   type LwU18 is new LwU18_Types.N_TYPE;
   type LwU19 is new LwU19_Types.N_TYPE;
   type LwU20 is new LwU20_Types.N_TYPE;
   type LwU21 is new LwU21_Types.N_TYPE;
   type LwU22 is new LwU22_Types.N_TYPE;
   type LwU23 is new LwU23_Types.N_TYPE;
   type LwU24 is new LwU24_Types.N_TYPE;
   type LwU25 is new LwU25_Types.N_TYPE;
   type LwU26 is new LwU26_Types.N_TYPE;
   type LwU27 is new LwU27_Types.N_TYPE;
   type LwU28 is new LwU28_Types.N_TYPE;
   type LwU29 is new LwU29_Types.N_TYPE;
   type LwU30 is new LwU30_Types.N_TYPE;
   type LwU31 is new LwU31_Types.N_TYPE;
   type LwU32 is new LwU32_Types.N_TYPE;
   type LwU64 is new LwU64_Types.N_TYPE;

   --  WARNING WARNING WARNING: Unless you also use Lw_Types.Shift_Left_Op and Lw_Types.Shift_Right_Op, shift
   --  operations will permit shift amount greater than or equal to the number of bits in the operand. Furthermore,
   --  there could be loss of bits. However, those additional constraints are being kept as a separate package to
   --  ensure that scenarios that intentionally want to violate one or more of such constraints while still using LwUXX
   --  types instead of LwUxx_Wrap types can do so without needing to create their own copy of Lw_Types. Usage of
   --  Lw_Types.Shift_Left_Op and Lw_Types.Shift_Right_Op is highly recommended.
   --  Every usage violating this must be thoroughly reviewed.
   pragma Provide_Shift_Operators (LwU8);
   pragma Provide_Shift_Operators (LwU16);
   pragma Provide_Shift_Operators (LwU32);
   pragma Provide_Shift_Operators (LwU64);

end Lw_Types;
