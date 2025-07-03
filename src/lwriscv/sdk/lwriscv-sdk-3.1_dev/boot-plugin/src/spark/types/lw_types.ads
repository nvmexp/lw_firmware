-- Copyright (c) 2020 LWPU Corporation - All Rights Reserved
--
-- This source module contains confidential and proprietary information
-- of LWPU Corporation. It is not to be disclosed or used except
-- in accordance with applicable agreements. This copyright notice does
-- not evidence any actual or intended publication of such source code.

-- @summary
-- Defining LwUx types

with Lw_Types_Generic;
with Lw_Types_Intrinsic; use Lw_Types_Intrinsic;

package Lw_Types with SPARK_Mode => On is

    package LwU1_Types  is new Lw_Types_Generic (Base_Type => LwU1_Wrap);
    package LwU2_Types  is new Lw_Types_Generic (Base_Type => LwU2_Wrap);
    package LwU3_Types  is new Lw_Types_Generic (Base_Type => LwU3_Wrap);
    package LwU4_Types  is new Lw_Types_Generic (Base_Type => LwU4_Wrap);
    package LwU5_Types  is new Lw_Types_Generic (Base_Type => LwU5_Wrap);
    package LwU6_Types  is new Lw_Types_Generic (Base_Type => LwU6_Wrap);
    package LwU7_Types  is new Lw_Types_Generic (Base_Type => LwU7_Wrap);
    package LwU8_Types  is new Lw_Types_Generic (Base_Type => LwU8_Wrap);
    package LwU9_Types  is new Lw_Types_Generic (Base_Type => LwU9_Wrap);
    package LwU10_Types is new Lw_Types_Generic (Base_Type => LwU10_Wrap);
    package LwU11_Types is new Lw_Types_Generic (Base_Type => LwU11_Wrap);
    package LwU12_Types is new Lw_Types_Generic (Base_Type => LwU12_Wrap);
    package LwU13_Types is new Lw_Types_Generic (Base_Type => LwU13_Wrap);
    package LwU14_Types is new Lw_Types_Generic (Base_Type => LwU14_Wrap);
    package LwU15_Types is new Lw_Types_Generic (Base_Type => LwU15_Wrap);
    package LwU16_Types is new Lw_Types_Generic (Base_Type => LwU16_Wrap);
    package LwU17_Types is new Lw_Types_Generic (Base_Type => LwU17_Wrap);
    package LwU18_Types is new Lw_Types_Generic (Base_Type => LwU18_Wrap);
    package LwU19_Types is new Lw_Types_Generic (Base_Type => LwU19_Wrap);
    package LwU20_Types is new Lw_Types_Generic (Base_Type => LwU20_Wrap);
    package LwU21_Types is new Lw_Types_Generic (Base_Type => LwU21_Wrap);
    package LwU22_Types is new Lw_Types_Generic (Base_Type => LwU22_Wrap);
    package LwU23_Types is new Lw_Types_Generic (Base_Type => LwU23_Wrap);
    package LwU24_Types is new Lw_Types_Generic (Base_Type => LwU24_Wrap);
    package LwU25_Types is new Lw_Types_Generic (Base_Type => LwU25_Wrap);
    package LwU26_Types is new Lw_Types_Generic (Base_Type => LwU26_Wrap);
    package LwU27_Types is new Lw_Types_Generic (Base_Type => LwU27_Wrap);
    package LwU28_Types is new Lw_Types_Generic (Base_Type => LwU28_Wrap);
    package LwU29_Types is new Lw_Types_Generic (Base_Type => LwU29_Wrap);
    package LwU30_Types is new Lw_Types_Generic (Base_Type => LwU30_Wrap);
    package LwU31_Types is new Lw_Types_Generic (Base_Type => LwU31_Wrap);
    package LwU32_Types is new Lw_Types_Generic (Base_Type => LwU32_Wrap);
    package LwU33_Types is new Lw_Types_Generic (Base_Type => LwU33_Wrap);
    package LwU34_Types is new Lw_Types_Generic (Base_Type => LwU34_Wrap);
    package LwU35_Types is new Lw_Types_Generic (Base_Type => LwU35_Wrap);
    package LwU36_Types is new Lw_Types_Generic (Base_Type => LwU36_Wrap);
    package LwU37_Types is new Lw_Types_Generic (Base_Type => LwU37_Wrap);
    package LwU38_Types is new Lw_Types_Generic (Base_Type => LwU38_Wrap);
    package LwU39_Types is new Lw_Types_Generic (Base_Type => LwU39_Wrap);
    package LwU40_Types is new Lw_Types_Generic (Base_Type => LwU40_Wrap);
    package LwU41_Types is new Lw_Types_Generic (Base_Type => LwU41_Wrap);
    package LwU42_Types is new Lw_Types_Generic (Base_Type => LwU42_Wrap);
    package LwU43_Types is new Lw_Types_Generic (Base_Type => LwU43_Wrap);
    package LwU44_Types is new Lw_Types_Generic (Base_Type => LwU44_Wrap);
    package LwU45_Types is new Lw_Types_Generic (Base_Type => LwU45_Wrap);
    package LwU46_Types is new Lw_Types_Generic (Base_Type => LwU46_Wrap);
    package LwU47_Types is new Lw_Types_Generic (Base_Type => LwU47_Wrap);
    package LwU48_Types is new Lw_Types_Generic (Base_Type => LwU48_Wrap);
    package LwU49_Types is new Lw_Types_Generic (Base_Type => LwU49_Wrap);
    package LwU50_Types is new Lw_Types_Generic (Base_Type => LwU50_Wrap);
    package LwU51_Types is new Lw_Types_Generic (Base_Type => LwU51_Wrap);
    package LwU52_Types is new Lw_Types_Generic (Base_Type => LwU52_Wrap);
    package LwU53_Types is new Lw_Types_Generic (Base_Type => LwU53_Wrap);
    package LwU54_Types is new Lw_Types_Generic (Base_Type => LwU54_Wrap);
    package LwU55_Types is new Lw_Types_Generic (Base_Type => LwU55_Wrap);
    package LwU56_Types is new Lw_Types_Generic (Base_Type => LwU56_Wrap);
    package LwU57_Types is new Lw_Types_Generic (Base_Type => LwU57_Wrap);
    package LwU58_Types is new Lw_Types_Generic (Base_Type => LwU58_Wrap);
    package LwU59_Types is new Lw_Types_Generic (Base_Type => LwU59_Wrap);
    package LwU60_Types is new Lw_Types_Generic (Base_Type => LwU60_Wrap);
    package LwU61_Types is new Lw_Types_Generic (Base_Type => LwU61_Wrap);
    package LwU62_Types is new Lw_Types_Generic (Base_Type => LwU62_Wrap);
    package LwU63_Types is new Lw_Types_Generic (Base_Type => LwU63_Wrap);
    package LwU64_Types is new Lw_Types_Generic (Base_Type => LwU64_Wrap);

    type LwU1  is new LwU1_Types.NType;
    type LwU2  is new LwU2_Types.NType;
    type LwU3  is new LwU3_Types.NType;
    type LwU4  is new LwU4_Types.NType;
    type LwU5  is new LwU5_Types.NType;
    type LwU6  is new LwU6_Types.NType;
    type LwU7  is new LwU7_Types.NType;
    type LwU8  is new LwU8_Types.NType;
    pragma Provide_Shift_Operators (LwU8);

    type LwU9  is new LwU9_Types.NType;
    type LwU10 is new LwU10_Types.NType;
    type LwU11 is new LwU11_Types.NType;
    type LwU12 is new LwU12_Types.NType;
    type LwU13 is new LwU13_Types.NType;
    type LwU14 is new LwU14_Types.NType;
    type LwU15 is new LwU15_Types.NType;
    type LwU16 is new LwU16_Types.NType;
    pragma Provide_Shift_Operators (LwU16);

    type LwU17 is new LwU17_Types.NType;
    type LwU18 is new LwU18_Types.NType;
    type LwU19 is new LwU19_Types.NType;
    type LwU20 is new LwU20_Types.NType;
    type LwU21 is new LwU21_Types.NType;
    type LwU22 is new LwU22_Types.NType;
    type LwU23 is new LwU23_Types.NType;
    type LwU24 is new LwU24_Types.NType;
    type LwU25 is new LwU25_Types.NType;
    type LwU26 is new LwU26_Types.NType;
    type LwU27 is new LwU27_Types.NType;
    type LwU28 is new LwU28_Types.NType;
    type LwU29 is new LwU29_Types.NType;
    type LwU30 is new LwU30_Types.NType;
    type LwU31 is new LwU31_Types.NType;
    type LwU32 is new LwU32_Types.NType;
    pragma Provide_Shift_Operators (LwU32);

    type LwU33 is new LwU33_Types.NType;
    type LwU34 is new LwU34_Types.NType;
    type LwU35 is new LwU35_Types.NType;
    type LwU36 is new LwU36_Types.NType;
    type LwU37 is new LwU37_Types.NType;
    type LwU38 is new LwU38_Types.NType;
    type LwU39 is new LwU39_Types.NType;
    type LwU40 is new LwU40_Types.NType;
    type LwU41 is new LwU41_Types.NType;
    type LwU42 is new LwU42_Types.NType;
    type LwU43 is new LwU43_Types.NType;
    type LwU44 is new LwU44_Types.NType;
    type LwU45 is new LwU45_Types.NType;
    type LwU46 is new LwU46_Types.NType;
    type LwU47 is new LwU47_Types.NType;
    type LwU48 is new LwU48_Types.NType;
    type LwU49 is new LwU49_Types.NType;
    type LwU50 is new LwU50_Types.NType;
    type LwU51 is new LwU51_Types.NType;
    type LwU52 is new LwU52_Types.NType;
    type LwU53 is new LwU53_Types.NType;
    type LwU54 is new LwU54_Types.NType;
    type LwU55 is new LwU55_Types.NType;
    type LwU56 is new LwU56_Types.NType;
    type LwU57 is new LwU57_Types.NType;
    type LwU58 is new LwU58_Types.NType;
    type LwU59 is new LwU59_Types.NType;
    type LwU60 is new LwU60_Types.NType;
    type LwU61 is new LwU61_Types.NType;
    type LwU62 is new LwU62_Types.NType;
    type LwU63 is new LwU63_Types.NType;
    type LwU64 is new LwU64_Types.NType;
    pragma Provide_Shift_Operators (LwU64);

end Lw_Types;
