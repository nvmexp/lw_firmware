-- _LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;           use Lw_Types;

package SBI_Ilwalid_Extension
is
    pragma Warnings (GNATprove, Off, "unused initial value of ""arg*""", Reason => "To support test extensions");
    pragma Warnings (GNATprove, Off, "unused variable ""functionId""", Reason => "To support test extensions");
    procedure Handle_Ilwalid_Extension(arg0        : in out LwU64;
                                       arg1        : in out LwU64;
                                       extensionId : LwU64;
                                       functionId  : LwU64)
    with
        Inline_Always;

end SBI_Ilwalid_Extension;
