--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Nv_Types; use Nv_Types;

package FBIF_Cfg
is
    procedure FBIF_Write_Transcfg (Region : NvU32;
                                   Value  : NvU32);

    procedure FBIF_Write_Regioncfg (Region : NvU32;
                                    Value  : NvU4);

end FBIF_Cfg;
