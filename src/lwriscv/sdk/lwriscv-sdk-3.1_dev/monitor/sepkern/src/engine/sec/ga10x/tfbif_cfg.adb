--*_LWRM_COPYRIGHT_BEGIN_
--*
--* Copyright 2021 by NVIDIA Corporation.  All rights reserved.  All
--* information contained herein is proprietary and confidential to NVIDIA
--* Corporation.  Any use, reproduction, or disclosure without the written
--* permission of NVIDIA Corporation is prohibited.
--*
--*_LWRM_COPYRIGHT_END_

with Nv_Types;    use Nv_Types;

package body TFBIF_Cfg
is
    procedure TFBIF_Write_Transcfg (Region : NvU32;
                                    Swid   : NvU2)
    is
    begin
        null;
    end TFBIF_Write_Transcfg;

    procedure TFBIF_Write_Regioncfg (Region : NvU32;
                                     Vpr    : NvU1;
                                     Apert  : NvU5)
    is
    begin
        null;
    end TFBIF_Write_Regioncfg;
end TFBIF_Cfg;
