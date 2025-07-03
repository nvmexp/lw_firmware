--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Types;              use Types;
with Lw_Types;           use Lw_Types;
with SBI_Types;          use SBI_Types;

package body SBI_Ilwalid_Extension
is
    procedure Handle_Ilwalid_Extension(arg0        : in out LwU64;
                                       arg1        : in out LwU64;
                                       extensionId : LwU64;
                                       functionId  : LwU64)
    is
    begin
        arg0 := SBI_Error_Code_To_LwU64(SBI_ERR_NOT_SUPPORTED);
        arg1 := extensionId;
    end Handle_Ilwalid_Extension;

end SBI_Ilwalid_Extension;
