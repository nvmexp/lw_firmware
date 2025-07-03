/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "trace_3d.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "core/include/gpu.h"
#include "lwmisc.h"
#include "class/cl90b8.h" // GF106_DMA_DECOMPRESS
#include "ctrl/ctrl2080.h" // Query CE number
#include "class/cl90b5.h" // GF100_DMA_COPY

UINT32 GenericTraceModule::MassageCeMethod(UINT32 subdev, UINT32 Method, UINT32 Data)
{
    Method <<= 2;

    if (Method == LW90B5_LAUNCH_DMA)
    {
        if ((LW90B5_LAUNCH_DMA_SEMAPHORE_TYPE_NONE != DRF_VAL(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, Data))
            && (LW90B5_LAUNCH_DMA_INTERRUPT_TYPE_BLOCKING == DRF_VAL(90B5, _LAUNCH_DMA, _INTERRUPT_TYPE, Data)))
        {
            if (m_pTraceChannel->GetWfiMethod() == WFI_INTR)
            {
                WarnPrintf("This trace not compatible with -wfi_intr, forcing -wfi_notify.\n");
                m_pTraceChannel->ForceWfiMethod(WFI_NOTIFY);
            }
        }
    }

    return Data;
}

/* Lwrrently Unused:
RC GenericTraceModule::SetupCe()
{
    return OK;
}
*/

RC LWGpuResource::GetSupportedCeNum
(
    GpuSubdevice* subdev,
    vector<UINT32> *supportedCE,
    LwRm* pLwRm
)
{
    SmcEngine* pSmcEngine = GetSmcEngine(pLwRm);
    vector<UINT32>& smcEngineSupportedCE = m_SupportedCopyEngines[pSmcEngine];

    if (smcEngineSupportedCE.size() > 0)
    {
        *supportedCE = smcEngineSupportedCE;
        return OK;
    }

    //
    // Amodel can support any number of copy engine once ce class is
    // supported. Avoid comparing copy engine type and add CE0, CE1
    // and CE2 to support list.
    if (Platform::GetSimulationMode() == Platform::Amodel)
    {
        smcEngineSupportedCE.push_back(CEObjCreateParam::CE0);
        smcEngineSupportedCE.push_back(CEObjCreateParam::CE1);
        smcEngineSupportedCE.push_back(CEObjCreateParam::CE2);

        if (EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
        {
            smcEngineSupportedCE.push_back(CEObjCreateParam::CE3);
            smcEngineSupportedCE.push_back(CEObjCreateParam::CE4);
            smcEngineSupportedCE.push_back(CEObjCreateParam::CE5);
        }

        if (EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_VOLTA))
        {
            smcEngineSupportedCE.push_back(CEObjCreateParam::CE6);
            smcEngineSupportedCE.push_back(CEObjCreateParam::CE7);
            smcEngineSupportedCE.push_back(CEObjCreateParam::CE8);
        }

        if (EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_AMPERE))
        {
            smcEngineSupportedCE.push_back(CEObjCreateParam::CE9);
        }

        *supportedCE = smcEngineSupportedCE;
        return OK;
    }

    // Get the number of supported engine
    RC rc = OK;
    LW2080_CTRL_GPU_GET_ENGINES_V2_PARAMS engineParams;
    LwRm::Handle handle = pLwRm->GetSubdeviceHandle(subdev);
    memset(&engineParams, 0, sizeof(engineParams));
    CHECK_RC(pLwRm->Control(handle,
        LW2080_CTRL_CMD_GPU_GET_ENGINES_V2,
        &engineParams,
        sizeof(engineParams)));

    // Get the list of supported engine
    vector<UINT32> engines(engineParams.engineCount);
    for (UINT32 i = 0; i < engineParams.engineCount; i++)
    {
        engines[i] = engineParams.engineList[i];
    }

    for (UINT32 i = 0; i < engineParams.engineCount; i++)
    {
        if (MDiagUtils::IsCopyEngine(engines[i]))
        {
            LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS classParams;
            memset(&classParams, 0, sizeof(classParams));
            classParams.engineType = engines[i];

            // Get number of classes
            CHECK_RC(pLwRm->Control(handle,
                LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                &classParams,
                sizeof(classParams)));
            vector<UINT32> classes(classParams.numClasses);
            classParams.classList = LW_PTR_TO_LwP64(&classes[0]);

            // Get class list
            CHECK_RC(pLwRm->Control(handle,
                LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                &classParams,
                sizeof(classParams)));

            // Check if current class is in support list
            for (UINT32 j = 0; j < classParams.numClasses; j++)
            {
                if (EngineClasses::IsClassType("Ce", classes[j]))
                {
                    UINT32 ceType = CEObjCreateParam::GetCeTypeByEngine(engines[i]);
                    smcEngineSupportedCE.push_back(ceType);
                    break;
                }
            }
        }
    }

    *supportedCE = smcEngineSupportedCE;
    return rc;
}

RC TraceSubChannel::SendCeSemaphore(UINT64 sem_offset)
{
    MASSERT(m_WfiMethod != WFI_POLL && m_WfiMethod != WFI_SLEEP);

    RC rc;

    if (CopySubChannel() ||
        GetClass() == GF106_DMA_DECOMPRESS)
    {

        CHECK_RC(m_pSubCh->MethodWriteRC(LW90B5_SET_SEMAPHORE_A,
                DRF_VAL(90B5, _SET_SEMAPHORE_A, _UPPER, LwU64_HI32(sem_offset))));
        CHECK_RC(m_pSubCh->MethodWriteRC(LW90B5_SET_SEMAPHORE_B,
                DRF_VAL(90B5, _SET_SEMAPHORE_B, _LOWER, LwU64_LO32(sem_offset))));
        CHECK_RC(m_pSubCh->MethodWriteRC(LW90B5_SET_SEMAPHORE_PAYLOAD, 0x00000000));
    }
    else
    {
        MASSERT(!"Not CE class!");
    }

    if (m_WfiMethod == WFI_INTR)
    {
        CHECK_RC(m_pSubCh->MethodWriteRC(LW90B5_LAUNCH_DMA,
                DRF_NUM(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, LW90B5_LAUNCH_DMA_SEMAPHORE_TYPE_RELEASE_ONE_WORD_SEMAPHORE) |
                DRF_NUM(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, LW90B5_LAUNCH_DMA_DATA_TRANSFER_TYPE_NONE) |
                DRF_NUM(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, LW90B5_LAUNCH_DMA_FLUSH_ENABLE_TRUE) |
                DRF_NUM(90B5, _LAUNCH_DMA, _INTERRUPT_TYPE,  LW90B5_LAUNCH_DMA_INTERRUPT_TYPE_BLOCKING)));
    }
    else if (m_WfiMethod == WFI_NOTIFY)
    {
        CHECK_RC(m_pSubCh->MethodWriteRC(LW90B5_LAUNCH_DMA,
                DRF_NUM(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE, LW90B5_LAUNCH_DMA_SEMAPHORE_TYPE_RELEASE_ONE_WORD_SEMAPHORE) |
                DRF_NUM(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, LW90B5_LAUNCH_DMA_DATA_TRANSFER_TYPE_NONE) |
                DRF_NUM(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, LW90B5_LAUNCH_DMA_FLUSH_ENABLE_TRUE) |
                DRF_NUM(90B5, _LAUNCH_DMA, _INTERRUPT_TYPE,  LW90B5_LAUNCH_DMA_INTERRUPT_TYPE_NONE)));
    }
    else
    {
       MASSERT(!"unhandled wfiMethod");
    }

    m_UsingSemaphoreWfi = true;
    return OK;
}

