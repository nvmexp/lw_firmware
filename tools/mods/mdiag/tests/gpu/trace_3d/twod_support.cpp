/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2015 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"

#include "core/include/gpu.h"
#include "lwmisc.h"
#include "Lwcm.h"
#include "class/cl0002.h"
#include "class/cl0030.h" // LW01_NULL
#include "class/cl902d.h" // FERMI_TWOD_A

#include "fermi/gf100/dev_ram.h"
#include "gpu/include/gpumgr.h"
#include "core/include/mgrmgr.h"
#include "core/include/channel.h"
#include "teegroups.h"

#define MSGID() T3D_MSGID(Massage)

UINT32 GenericTraceModule::MassageTwodMethod(UINT32 subdev, UINT32 Method, UINT32 Data)
{
    DebugPrintf(MSGID(), "MassageTwodMethod 0x%04x : 0x%08x\n", Method, Data);

    if(m_NoPbMassage)
    {
        return Data;
    }

    switch (Method & DRF_SHIFTMASK(LW_FIFO_DMA_METHOD_ADDRESS))
    {
        default:
            // leave it alone by default
            break;

        case LW902D_NOTIFY:
            // tests that use notify typically write to Color Target 0
            m_Test->GetSurfaceMgr()->SetSurfaceUsed(ColorSurfaceTypes[0], true,
                                                    subdev);
            if (params->ParamPresent("-notifyAwaken"))
            {
                Data = FLD_SET_DRF(902D, _NOTIFY, _TYPE, _WRITE_THEN_AWAKEN, Data);
                InfoPrintf("Patching NOTIFY for AWAKEN\n");
            }
            break;
    }

    return Data;
}
