 /*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "trace_3d.h"
#include "class/clcd97.h" // BLACKWELL_A

UINT32 GenericTraceModule::MassageBlackwellMethod(UINT32 subdev, UINT32 method, UINT32 data)
{
    UINT32 realMethod = method << 2;

    switch (realMethod)
    {
        case LWCD97_SET_ZT_FORMAT:
            if (params->ParamPresent("-separate_zs"))
            {
                data = FLD_SET_DRF(CD97, _SET_ZT_FORMAT, _STENCIL_IS_SEPARATE, _TRUE,
                        data);
            }
            break;
        default:
            return MassageMaxwellBMethod(subdev, method, data);
            break;
    }

    return data;
}

