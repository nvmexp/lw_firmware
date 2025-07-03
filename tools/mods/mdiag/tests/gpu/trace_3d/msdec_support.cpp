/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2020 by LWPU Corporation.  All rights reserved.  All
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
#include "class/cl86b6.h" // LW86B6_VIDEO_COMPOSITOR
#include "class/cl95b1.h" // LW95B1_VIDEO_MSVLD

UINT32 GenericTraceModule::MassageMsdecMethod(UINT32 subdev, UINT32 Method, UINT32 Data)
{
    if (((Method == LW95B1_SEMAPHORE_D)) ||
        ((Method == LW95B1_EXELWTE) &&
         (0 != DRF_VAL(95B1, _EXELWTE, _AWAKEN, Data))))
    {
        if (m_pTraceChannel->GetWfiMethod() == WFI_INTR)
        {
            WarnPrintf("This trace not compatible with -wfi_intr, forcing -wfi_notify.\n");
            m_pTraceChannel->ForceWfiMethod(WFI_NOTIFY);
        }
    }

    return Data;
}

RC TraceChannel::SetupMsdec()
{
    return OK;
}

RC TraceSubChannel::SendMsdecSemaphore(UINT64 sem_offset)
{
    MASSERT(m_WfiMethod != WFI_POLL && m_WfiMethod != WFI_SLEEP);

    RC rc;
    if (GetClass() == LW86B6_VIDEO_COMPOSITOR)
    {
        CHECK_RC(m_pSubCh->MethodWriteRC(LW86B6_VIDEO_COMPOSITOR_SET_SEMAPHORE_A,
                                         DRF_VAL(86B6_VIDEO_COMPOSITOR, _SET_SEMAPHORE_A, _CTX_DMA, 0) |
                                         DRF_VAL(86B6_VIDEO_COMPOSITOR, _SET_SEMAPHORE_A, _UPPER, 0x00)));
        CHECK_RC(m_pSubCh->MethodWriteRC(LW86B6_VIDEO_COMPOSITOR_SET_SEMAPHORE_B, 0));
        CHECK_RC(m_pSubCh->MethodWriteRC(LW86B6_VIDEO_COMPOSITOR_SET_SEMAPHORE_PAYLOAD, 0x00000000));
        if (m_WfiMethod == WFI_INTR)
        {
            CHECK_RC(m_pSubCh->MethodWriteRC(LW86B6_VIDEO_COMPOSITOR_BACKEND_SEMAPHORE,
                                             DRF_NUM(86B6_VIDEO_COMPOSITOR, _BACKEND_SEMAPHORE, _STRUCT_SIZE, 0)|
                                             DRF_NUM(86B6_VIDEO_COMPOSITOR, _BACKEND_SEMAPHORE, _AWAKEN, 1)));
        }
        else if (m_WfiMethod == WFI_NOTIFY)
        {
            CHECK_RC(m_pSubCh->MethodWriteRC(LW86B6_VIDEO_COMPOSITOR_BACKEND_SEMAPHORE,
                                             DRF_NUM(86B6_VIDEO_COMPOSITOR, _BACKEND_SEMAPHORE, _STRUCT_SIZE, 0)|
                                             DRF_NUM(86B6_VIDEO_COMPOSITOR, _BACKEND_SEMAPHORE, _AWAKEN, 0)));
        }
        else
        {
            MASSERT(!"unhandled wfiMethod");
        }
    }
    else if (VideoSubChannel())
    {
        CHECK_RC(m_pSubCh->MethodWriteRC(LW95B1_SEMAPHORE_A,
                DRF_VAL(95B1, _SEMAPHORE_A, _UPPER, LwU64_HI32(sem_offset))));
        CHECK_RC(m_pSubCh->MethodWriteRC(LW95B1_SEMAPHORE_B,
                DRF_VAL(95B1, _SEMAPHORE_B, _LOWER, LwU64_LO32(sem_offset))));
        CHECK_RC(m_pSubCh->MethodWriteRC(LW95B1_SEMAPHORE_C, 0x00000000));
        if (m_WfiMethod == WFI_INTR)
        {
            CHECK_RC(m_pSubCh->MethodWriteRC(LW95B1_SEMAPHORE_D,
                                             DRF_DEF(95B1, _SEMAPHORE_D, _OPERATION, _RELEASE)|
                                             DRF_DEF(95B1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE)|
                                             DRF_DEF(95B1, _SEMAPHORE_D, _AWAKEN_ENABLE, _TRUE)));
        }
        else if (m_WfiMethod == WFI_NOTIFY)
        {
            CHECK_RC(m_pSubCh->MethodWriteRC(LW95B1_SEMAPHORE_D,
                                             DRF_DEF(95B1, _SEMAPHORE_D, _OPERATION, _RELEASE)|
                                             DRF_DEF(95B1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE)|
                                             DRF_DEF(95B1, _SEMAPHORE_D, _AWAKEN_ENABLE, _FALSE)));
        }
        else
        {
            MASSERT(!"unhandled wfiMethod");
        }
    }
    else
    {
        MASSERT(!"Not MSDEC class!");
    }

    m_UsingSemaphoreWfi = true;
    return OK;
}
