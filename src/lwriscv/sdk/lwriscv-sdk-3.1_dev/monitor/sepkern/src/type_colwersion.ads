--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

package Type_Colwersion
is
    procedure Colwert_LwU64_To_LwU32( input  : LwU64;
                                      output : out LwU32;
                                      rc     : out Boolean)
    with Inline_Always;

    procedure Colwert_LwU64_To_LwU5 ( input  : LwU64;
                                      output : out LwU5;
                                      rc     : out Boolean)
    with Inline_Always;

    procedure Colwert_LwU64_To_LwU4 ( input  : LwU64;
                                      output : out LwU4;
                                      rc     : out Boolean)
    with Inline_Always;

    procedure Colwert_LwU64_To_LwU2 ( input  : LwU64;
                                      output : out LwU2;
                                      rc     : out Boolean)
    with Inline_Always;

    procedure Colwert_LwU64_To_LwU1 ( input  : LwU64;
                                      output : out LwU1;
                                      rc     : out Boolean)
    with Inline_Always;

end Type_Colwersion;
