--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types; use Lw_Types;

package body Type_Colwersion
is
    procedure Colwert_LwU64_To_LwU32( input  : LwU64;
                                      output : out LwU32;
                                      rc     : out Boolean)
    is
    begin
        rc := True;
        output := 0;

        if input > LwU64(LwU32'Last) then
            rc := False;
            return;
        end if;
        output := LwU32(input);
    end Colwert_LwU64_To_LwU32;

    procedure Colwert_LwU64_To_LwU5( input  : LwU64;
                                     output : out LwU5;
                                     rc     : out Boolean)
    is
    begin
        rc := True;
        output := 0;

        if input > LwU64(LwU5'Last) then
            rc := False;
            return;
        end if;
        output := LwU5(input);
    end Colwert_LwU64_To_LwU5;

    procedure Colwert_LwU64_To_LwU4 ( input  : LwU64;
                                      output : out LwU4;
                                      rc     : out Boolean)
    is
    begin
        rc := True;
        output := 0;

        if input > LwU64(LwU4'Last) then
            rc := False;
            return;
        end if;
        output := LwU4(input);
    end Colwert_LwU64_To_LwU4;

    procedure Colwert_LwU64_To_LwU2 ( input  : LwU64;
                                      output : out LwU2;
                                      rc     : out Boolean)
    is
    begin
        rc := True;
        output := 0;

        if input > LwU64(LwU2'Last) then
            rc := False;
            return;
        end if;
        output := LwU2(input);
    end Colwert_LwU64_To_LwU2;

    procedure Colwert_LwU64_To_LwU1 ( input  : LwU64;
                                      output : out LwU1;
                                      rc     : out Boolean)
    is
    begin
        rc := True;
        output := 0;

        if input > LwU64(LwU1'Last) then
            rc := False;
            return;
        end if;
        output := LwU1(input);
    end Colwert_LwU64_To_LwU1;

end Type_Colwersion;
