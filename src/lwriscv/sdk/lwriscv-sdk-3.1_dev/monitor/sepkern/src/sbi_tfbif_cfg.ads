--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by LWPU Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to LWPU
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of LWPU Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Lw_Types;          use Lw_Types;
with SBI_Types;         use SBI_Types;

package SBI_TFBIF_Cfg
is

    procedure TFBIF_Transcfg (Region : LwU64;
                              Value  : LwU64;
                              SBI_RC : out SBI_Return_Type);

    procedure TFBIF_Regioncfg (Region : LwU64;
                               Value1 : LwU64;
                               Value2 : LwU64;
                               SBI_RC : out SBI_Return_Type);

end SBI_TFBIF_Cfg;
