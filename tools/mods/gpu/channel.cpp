/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Channel interface.

#include "lwtypes.h"
#include "core/include/channel.h"
#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "gpu/utility/rchelper.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "lwerror.h"
#include "lwos.h"
#include "Lwcm.h"
#include "core/include/version.h"
#include "gpu/include/userdalloc.h"
#include "gpu/utility/atomwrap.h"
#include "lwgputypes.h"

#include "maxwell/gm200/dev_ram.h"

#include "class/cl906f.h" // GF100_CHANNEL_GPFIFO
#include "class/cla06f.h" // KEPLER_CHANNEL_GPFIFO_A
#include "class/cla16f.h" // KEPLER_CHANNEL_GPFIFO_B
#include "class/cla26f.h" // KEPLER_CHANNEL_GPFIFO_C / LWA26F_MEM_OP_B
#include "class/clb06f.h" // LWB06F_MEM_OP_D / MAXWELL_CHANNEL_GPFIFO_A
#include "class/clc06f.h" // PASCAL_CHANNEL_GPFIFO_A
#include "class/clc36f.h" // VOLTA_CHANNEL_GPFIFO_A
#include "class/clc46f.h" // TURING_CHANNEL_GPFIFO_A
#include "class/clc56f.h" // AMPERE_CHANNEL_GPFIFO_A
#include "class/clc86f.h" // HOPPER_CHANNEL_GPFIFO_A

#include "class/cl2080.h"
#include "ctrl/ctrl2080.h"

#include <cstdio>
#include <cstdarg>
#include <algorithm>

/* static */ const UINT32 BaseChannel::s_SemaphoreDwords = (sizeof(LwGpuSemaphoreRec) / sizeof(UINT32));

/* static */ const vector<UINT32> Channel::FifoClasses =
{
    HOPPER_CHANNEL_GPFIFO_A,
    AMPERE_CHANNEL_GPFIFO_A,
    TURING_CHANNEL_GPFIFO_A,
    VOLTA_CHANNEL_GPFIFO_A,
    PASCAL_CHANNEL_GPFIFO_A,
    MAXWELL_CHANNEL_GPFIFO_A,
    KEPLER_CHANNEL_GPFIFO_C,
    KEPLER_CHANNEL_GPFIFO_B,
    KEPLER_CHANNEL_GPFIFO_A,
    GF100_CHANNEL_GPFIFO
};

// These match the defines in sdk/lwpu/inc/lwerror.h.
static const RC ChanErrToRC[] =
{
    OK,                                                         //  0
    RC::RM_RCH_FIFO_ERROR_FIFO_METHOD,                          //  1
    RC::RM_RCH_FIFO_ERROR_SW_METHOD,                            //  2
    RC::RM_RCH_FIFO_ERROR_UNK_METHOD,                           //  3
    RC::RM_RCH_FIFO_ERROR_CHANNEL_BUSY,                         //  4
    RC::RM_RCH_FIFO_ERROR_RUNOUT_OVERFLOW,                      //  5
    RC::RM_RCH_FIFO_ERROR_PARSE_ERR,                            //  6
    RC::RM_RCH_FIFO_ERROR_PTE_ERR,                              //  7
    RC::RM_RCH_FIFO_ERROR_IDLE_TIMEOUT,                         //  8
    RC::RM_RCH_GR_ERROR_INSTANCE,                               //  9
    RC::RM_RCH_GR_ERROR_SINGLE_STEP,                            // 10
    RC::RM_RCH_GR_ERROR_MISSING_HW,                             // 11
    RC::RM_RCH_GR_ERROR_SW_METHOD,                              // 12
    RC::RM_RCH_GR_ERROR_SW_NOTIFY,                              // 13
    RC::RM_RCH_FAKE_ERROR,                                      // 14
    RC::RM_RCH_SCANLINE_TIMEOUT,                                // 15
    RC::RM_RCH_VBLANK_CALLBACK_TIMEOUT,                         // 16
    RC::RM_RCH_PARAMETER_ERROR,                                 // 17
    RC::RM_RCH_BUS_MASTER_TIMEOUT_ERROR,                        // 18
    RC::RM_RCH_DISP_MISSED_NOTIFIER,                            // 19
    RC::RM_RCH_MPEG_ERROR_SW_METHOD,                            // 20
    RC::RM_RCH_ME_ERROR_SW_METHOD,                              // 21
    RC::RM_RCH_VP_ERROR_SW_METHOD,                              // 22
    RC::RM_RCH_RC_LOGGING_ENABLED,                              // 23
    RC::RM_RCH_GR_SEMAPHORE_TIMEOUT,                            // 24
    RC::RM_RCH_GR_ILLEGAL_NOTIFY,                               // 25
    RC::RM_RCH_FIFO_ERROR_FBISTATE_TIMEOUT,                     // 26
    RC::RM_RCH_VP_ERROR,                                        // 27
    RC::RM_RCH_VP2_ERROR,                                       // 28
    RC::RM_RCH_BSP_ERROR,                                       // 29
    RC::RM_RCH_BAD_ADDR_ACCESS,                                 // 30
    RC::RM_RCH_FIFO_ERROR_MMU_ERR_FLT,                          // 31
    RC::RM_RCH_PBDMA_ERROR,                                     // 32
    RC::RM_RCH_SEC_ERROR,                                       // 33
    RC::RM_RCH_MSVLD_ERROR,                                     // 34
    RC::RM_RCH_MSPDEC_ERROR,                                    // 35
    RC::RM_RCH_MSPPP_ERROR,                                     // 36
    RC::RM_RCH_FECS_ERR_UNIMP_FIRMWARE_METHOD,                  // 37
    RC::RM_RCH_FECS_ERR_WATCHDOG_TIMEOUT,                       // 38
    RC::RM_RCH_CE0_ERROR,                                       // 39
    RC::RM_RCH_CE1_ERROR,                                       // 40
    RC::RM_RCH_CE2_ERROR,                                       // 41
    RC::RM_RCH_VIC_ERROR,                                       // 42
    RC::RM_RCH_RESETCHANNEL_VERIF_ERROR,                        // 43
    RC::RM_RCH_GR_FAULT_DURING_CTXSW,                           // 44
    RC::RM_RCH_PREEMPTIVE_REMOVAL,                              // 45
    RC::RM_RCH_GPU_TIMEOUT_ERROR,                               // 46
    RC::RM_RCH_MSENC_ERROR,                                     // 47
    RC::RM_RCH_GPU_ECC_UNCORRECTABLE,                           // 48
    RC::RM_RCH_SILENT_RUNNING_CONSTANT_LEVEL_SET_BY_REGISTRY,   // 49
    RC::RM_RCH_SILENT_RUNNING_LEVEL_TRANSITION_DUE_TO_RC_ERROR, // 50
    RC::RM_RCH_SILENT_RUNNING_STRESS_TEST_FAILURE,              // 51
    RC::RM_RCH_SILENT_RUNNING_LEVEL_TRANS_DUE_TO_TEMP_RISE,     // 52
    RC::RM_RCH_SILENT_RUNNING_TEMP_REDUCED_CLOCKING,            // 53
    RC::RM_RCH_SILENT_RUNNING_PWR_REDUCED_CLOCKING,             // 54
    RC::RM_RCH_SILENT_RUNNING_TEMPERATURE_READ_ERROR,           // 55
    RC::RM_RCH_DISPLAY_CHANNEL_EXCEPTION,                       // 56
    RC::RM_RCH_FB_LINK_TRAINING_FAILURE_ERROR,                  // 57
    RC::RM_RCH_FB_MEMORY_ERROR,                                 // 58
    RC::RM_RCH_PMU_ERROR,                                       // 59
    RC::RM_RCH_SEC2_ERROR,                                      // 60
    RC::RM_RCH_PMU_BREAKPOINT,                                  // 61
    RC::RM_RCH_PMU_HALT_ERROR,                                  // 62
    RC::RM_RCH_INFOROM_DPR_EVENT,                               // 63
    RC::RM_RCH_INFOROM_DPR_FAILURE,                             // 64
    RC::RM_RCH_LWENC1_ERROR,                                    // 65
    RC::RM_RCH_FECS_ERR_FIRMWARE_REG_ACCESS_VIOLATION,          // 66
    RC::RM_RCH_FECS_ERR_FIRMWARE_VERIF_VIOLATION,               // 67
    RC::RM_RCH_LWDEC0_ERROR,                                    // 68
    RC::RM_RCH_GR_CLASS_ERROR,                                  // 69
    RC::RM_RCH_CE3_ERROR,                                       // 70
    RC::RM_RCH_CE4_ERROR,                                       // 71
    RC::RM_RCH_CE5_ERROR,                                       // 72
    RC::RM_RCH_LWENC2_ERROR,                                    // 73
    RC::RM_RCH_LWLINK_ERROR,                                    // 74
    RC::RM_RCH_CE6_ERROR,                                       // 75
    RC::RM_RCH_CE7_ERROR,                                       // 76
    RC::RM_RCH_CE8_ERROR,                                       // 77
    RC::RM_RCH_VGPU_ERROR,                                      // 78
    RC::RM_RCH_GPU_HAS_FALLEN_OFF_THE_BUS,                      // 79
    RC::RM_RCH_PUSHBUFFER_CRC_MISMATCH,                         // 80
    RC::RM_RCH_VGA_SUBSYSTEM_ERROR,                             // 81
    RC::RM_RCH_LWJPG0_ERROR,                                    // 82
    RC::RM_RCH_LWDEC1_ERROR,                                    // 83
    RC::RM_RCH_LWDEC2_ERROR,                                    // 84
    RC::RM_RCH_CE9_ERROR,                                       // 85
    RC::RM_RCH_OFA0_ERROR,                                      // 86
    RC::RM_LWT_DRIVER_REPORT,                                   // 87
    RC::RM_RCH_LWDEC3_ERROR,                                    // 88
    RC::RM_RCH_LWDEC4_ERROR,                                    // 89
    RC::RM_RCH_LTC_ERROR,                                       // 90
    RC::RM_RCH_RESERVED,                                        // 91
    RC::RM_RCH_EXCESSIVE_SBE_INTERRUPTS,                        // 92
    RC::RM_RCH_INFOROM_ERASE_LIMIT_EXCEEDED,                    // 93
    RC::RM_RCH_CONTAINED_ERROR,                                 // 94
    RC::RM_RCH_UNCONTAINED_ERROR,                               // 95
    RC::RM_RCH_LWDEC5_ERROR,                                    // 96
    RC::RM_RCH_LWDEC6_ERROR,                                    // 97
    RC::RM_RCH_LWDEC7_ERROR,                                    // 98
    RC::RM_RCH_LWJPG1_ERROR,                                    // 99
    RC::RM_RCH_LWJPG2_ERROR,                                    // 100
    RC::RM_RCH_LWJPG3_ERROR,                                    // 101
    RC::RM_RCH_LWJPG4_ERROR,                                    // 102
    RC::RM_RCH_LWJPG5_ERROR,                                    // 103
    RC::RM_RCH_LWJPG6_ERROR,                                    // 104
    RC::RM_RCH_LWJPG7_ERROR,                                    // 105
    RC::RM_SMBPBI_TEST_MESSAGE,                                 // 106
    RC::RM_SMBPBI_TEST_MESSAGE_SILENT,                          // 107
    RC::RM_DESTINATION_FLA_TRANSLATION_ERROR,                   // 108
    RC::RM_RCH_CTXSW_TIMEOUT_ERROR,                             // 109
    RC::RM_SEC_FAULT_ERROR,                                     // 110
    RC::RM_BUNDLE_ERROR_EVENT,                                  // 111
    RC::RM_DISP_SUPERVISOR_ERROR,                               // 112
    RC::RM_DP_LT_FAILURE,                                       // 113
    RC::RM_HEAD_RG_UNDERFLOW,                                   // 114
    RC::RM_CORE_CHANNEL_REGS,                                   // 115
    RC::RM_WINDOW_CHANNEL_REGS,                                 // 116
    RC::RM_LWRSOR_CHANNEL_REGS,                                 // 117
    RC::RM_HEAD_REGS,                                           // 118
    RC::RM_GSP_RPC_TIMEOUT,                                     // 119
    RC::RM_GSP_ERROR,                                           // 120
    RC::RM_C2C_ERROR,                                           // 121
};

// If you get this error while compiling, first fix the above
// ChanErrToRC[] array to match sdk/lwpu/inc/lwerror.h.  Then change
// the #if statement to remove the compile error.
//
#if ROBUST_CHANNEL_LAST_ERROR != 121
#error "sdk/lwpu/inc/lwerror.h and diag/mods/gpu/channel.cpp are out of sync"
#endif

//------------------------------------------------------------------------------
RC Channel::GetHostEngineId(GpuDevice * pGpuDev, LwRm *pLwRm, UINT32 engineId, UINT32 *pHostEng)
{
    *pHostEng = engineId;

    // For all other engines but the copy engine the host and actual engine are the same
    // For copy engines if the CE is a GRCE then the host channel is actually graphics
    if (!LW2080_ENGINE_TYPE_IS_COPY(engineId))
        return OK;

    RC rc;
    LW2080_CTRL_CE_GET_CAPS_V2_PARAMS capsParams = {};

    capsParams.ceEngineType = engineId;
    CHECK_RC(pLwRm->ControlBySubdevice(pGpuDev->GetSubdevice(0),
                                       LW2080_CTRL_CMD_CE_GET_CAPS_V2,
                                       &capsParams,
                                       sizeof(capsParams)));

    if (LW2080_CTRL_CE_GET_CAP(capsParams.capsTbl, LW2080_CTRL_CE_CAPS_CE_GRCE))
        *pHostEng = LW2080_ENGINE_TYPE_GRAPHICS;

    return rc;
}

//------------------------------------------------------------------------------
// Class static function for translating robust channels codes into mods RC.
//
RC Channel::RobustChannelsCodeToModsRC(UINT32 rcrc)
{
    if ((rcrc >= sizeof(ChanErrToRC)/sizeof(RC)) || (((sizeof(ChanErrToRC)/sizeof(RC))-1)!=ROBUST_CHANNEL_LAST_ERROR))
    {
        Printf(Tee::PriError, "RM RC error %d has no or has incorrect mods RC code.\n", rcrc);
        MASSERT(!"channel.cpp ChanErrToRC table is out-of-date vs. lwerror.h");
        return RC::SOFTWARE_ERROR;
    }

    return ChanErrToRC[ rcrc ];
}

//------------------------------------------------------------------------------
RC Channel::WriteL2FlushDirty()
{
    switch (GetClass())
    {
        case GF100_CHANNEL_GPFIFO:
        case KEPLER_CHANNEL_GPFIFO_A:
        case KEPLER_CHANNEL_GPFIFO_B:
        case KEPLER_CHANNEL_GPFIFO_C:
            return Write(0, LWA26F_MEM_OP_B,
                         DRF_DEF(A26F, _MEM_OP_B, _OPERATION, _L2_FLUSH_DIRTY));

        // MEM_OP_D replaced MEM_OP_B in GM20x (but not GM20B)
        default:
            return Write(0, LWB06F_MEM_OP_D,
                         DRF_DEF(B06F, _MEM_OP_D, _OPERATION, _L2_FLUSH_DIRTY));
    }
}

//------------------------------------------------------------------------------

RC NullChannel::SetAutoFlush(bool AutoFlushEnable, UINT32 AutoFlushThreshold)
{
    return OK;
}
bool NullChannel::GetAutoFlushEnable() const
{
    return false;
}
RC NullChannel::SetAutoGpEntry
(
    bool AutoGpEntryEnable,
    UINT32 AutoGpEntryThreshold
)
{
    return OK;
}

RC NullChannel::WriteExternal
(
    UINT32 **ppExtBuf,
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    return OK;
}
RC NullChannel::WriteExternalNonInc
(
    UINT32 **ppExtBuf,
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    return OK;
}
RC NullChannel::WriteExternalIncOnce
(
    UINT32 **ppExtBuf,
    UINT32 subchannel,
    UINT32 method,
    UINT32 count,
    const UINT32 *pData
)
{
    return OK;
}
RC NullChannel::WriteExternalImmediate
(
    UINT32 **ppExtBuf,
    UINT32 subchannel,
    UINT32 method,
    UINT32 data
)
{
    return OK;
}

RC NullChannel::Write(UINT32 Subchannel, UINT32 Method, UINT32 Data)
{
    return OK;
}
RC NullChannel::Write
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Count,
    UINT32 Data,
    ...
)
{
    return OK;
}
RC NullChannel::Write
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Count,
    const UINT32 *pData
)
{
    return OK;
}
RC NullChannel::WriteNonInc
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Count,
    const UINT32 *pData
)
{
    return OK;
}
RC NullChannel::WriteHeader(UINT32 Subchannel, UINT32 Method, UINT32 Count)
{
    return OK;
}
RC NullChannel::WriteNonIncHeader
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Count
)
{
    return OK;
}
RC NullChannel::WriteIncOnce
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Count,
    const UINT32 *pData
)
{
    return OK;
}
RC NullChannel::WriteImmediate(UINT32 Subchannel, UINT32 Method, UINT32 Data)
{
    return OK;
}
RC NullChannel::WriteWithSurface
(
    UINT32           subchannel,
    UINT32           method,
    const Surface2D& surface,
    UINT32           offset,
    UINT32           offsetShift,
    MappingType      mappingType
)
{
    return OK;
}
RC NullChannel::WriteWithSurfaceHandle
(
    UINT32      subchannel,
    UINT32      method,
    UINT32      hSurface,
    UINT32      offset,
    UINT32      offsetShift,
    MappingType mappingType
)
{
    return OK;
}
RC NullChannel::WriteNop()
{
    return OK;
}
RC NullChannel::WriteSetSubdevice(UINT32 mask)
{
    return OK;
}
RC NullChannel::WriteCrcChkMethod()
{
    return OK;
}
RC NullChannel::WriteGpCrcChkGpEntry()
{
    return OK;
}
RC NullChannel::WriteAcquireMutex(UINT32 subchannel, UINT32 mutexIdx)
{
    return OK;
}
RC NullChannel::WriteReleaseMutex(UINT32 subchannel, UINT32 mutexIdx)
{
    return OK;
}
RC NullChannel::SetClass(UINT32 subchannel, UINT32 classId)
{
    return OK;
}
RC NullChannel::SetClassAndWriteMask
(
    UINT32 subchannel,
    UINT32 classId,
    UINT32 method,
    UINT32 mask,
    const UINT32* pData
)
{
    return OK;
}
RC NullChannel::WriteMask
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 mask,
    const UINT32* pData
)
{
    return OK;
}
RC NullChannel::WriteDone()
{
    return OK;
}
RC NullChannel::CallSubroutine(UINT64 Offset, UINT32 Size)
{
    return OK;
}
RC NullChannel::InsertSubroutine(const UINT32 *pBuffer, UINT32 count)
{
    return OK;
}
RC NullChannel::SetHwCrcCheckMode(UINT32 Mode)
{
    return OK;
}
RC NullChannel::SetPrivEnable(bool Priv)
{
    return OK;
}
RC NullChannel::SetSyncEnable(bool Sync)
{
    return OK;
}
RC NullChannel::ScheduleChannel(bool Enable)
{
    return OK;
}
RC NullChannel::SetObject(UINT32 Subchannel, LwRm::Handle Handle)
{
    return OK;
}
RC NullChannel::UnsetObject(LwRm::Handle Handle)
{
    return OK;
}
RC NullChannel::SetReference(UINT32 Value)
{
    return OK;
}
RC NullChannel::SetContextDmaSemaphore(LwRm::Handle hCtxDma)
{
    return OK;
}
RC NullChannel::SetSemaphoreOffset(UINT64 Offset)
{
    return OK;
}
void NullChannel::SetSemaphoreReleaseFlags(UINT32 flags)
{
}
UINT32 NullChannel::GetSemaphoreReleaseFlags()
{
    return 0;
}
RC NullChannel::SetSemaphorePayloadSize(SemaphorePayloadSize size)
{
    return OK;
}
Channel::SemaphorePayloadSize NullChannel::GetSemaphorePayloadSize()
{
    return SEM_PAYLOAD_SIZE_DEFAULT;
}
RC NullChannel::SemaphoreAcquire(UINT64 Data)
{
    return OK;
}
RC NullChannel::SemaphoreAcquireGE(UINT64 Data)
{
    return OK;
}
RC NullChannel::SemaphoreRelease(UINT64 Data)
{
    return OK;
}
RC NullChannel::SemaphoreReduction(Reduction Op, ReductionType redType, UINT64 Data)
{
    return OK;
}
RC NullChannel::NonStallInterrupt()
{
    return OK;
}
RC NullChannel::Yield(UINT32 Subchannel)
{
    return OK;
}
RC NullChannel::BeginInsertedMethods()
{
    return OK;
}
RC NullChannel::EndInsertedMethods()
{
    return OK;
}
void NullChannel::CancelInsertedMethods()
{
}
RC NullChannel::GetSyncPoint(UINT32* pSyncPointId)
{
    return OK;
}
UINT32 NullChannel::ReadSyncPoint(UINT32 syncPointId)
{
    return ~0U;
}
void NullChannel::IncrementSyncPoint(UINT32 syncPointId)
{
}
RC NullChannel::WriteSyncPointIncrement(UINT32 syncPointId)
{
    return OK;
}
RC NullChannel::WriteSyncPointWait(UINT32 syncPointId, UINT32 value)
{
    return OK;
}
RC NullChannel::Flush()
{
    return OK;
}
RC NullChannel::WaitForDmaPush(FLOAT64 TimeoutMs)
{
    return OK;
}
RC NullChannel::WaitIdle(FLOAT64 TimeoutMs)
{
    return OK;
}
RC NullChannel::SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(bool Enable)
{
    return OK;
}
bool NullChannel::GetLegacyWaitIdleWithRiskyIntermittentDeadlocking() const
{
    return false;
}
void NullChannel::SetDefaultTimeoutMs(FLOAT64 TimeoutMs)
{
}
FLOAT64 NullChannel::GetDefaultTimeoutMs()
{
    return 0;
}
UINT32 NullChannel::GetClass()
{
    return 0;
}
LwRm::Handle NullChannel::GetHandle()
{
    return 0;
}
RC NullChannel::CheckError()
{
    return OK;
}
void NullChannel::SetError(RC Error)
{
}
void NullChannel::ClearError()
{
}
bool NullChannel::DetectNewRobustChannelError() const
{
    return false;
}
bool NullChannel::DetectError() const
{
    return false;
}
void NullChannel::ClearPushbuffer()
{
}
RC NullChannel::GetCachedPut(UINT32 *PutPtr)
{
    return OK;
}
RC NullChannel::SetCachedPut(UINT32 PutPtr)
{
    return OK;
}
RC NullChannel::GetPut(UINT32 *PutPtr)
{
    return OK;
}
RC NullChannel::SetPut(UINT32 PutPtr)
{
    return OK;
}
RC NullChannel::GetGet(UINT32 *GetPtr)
{
    return OK;
}
RC NullChannel::GetPushbufferSpace(UINT32 *PbSpacePtr)
{
    return OK;
}
RC NullChannel::GetCachedGpPut(UINT32 *GpPutPtr)
{
    return OK;
}
RC NullChannel::GetGpPut(UINT32 *GpPutPtr)
{
    return OK;
}
RC NullChannel::GetGpGet(UINT32 *GpGetPtr)
{
    return OK;
}
RC NullChannel::GetGpEntries(UINT32 *GpEntriesPtr)
{
    return OK;
}
RC NullChannel::FinishOpenGpFifoEntry(UINT64* pEnd)
{
    return OK;
}
RC NullChannel::GetReference(UINT32 *pRefCnt)
{
    return OK;
}
RC NullChannel::SetExternalGPPutAdvanceMode(bool enable)
{
    return OK;
}
UINT32 NullChannel::GetChannelId() const
{
    return 0;
}
void NullChannel::EnableLogging(bool b)
{
}
bool NullChannel::GetEnableLogging() const
{
    return false;
}
void NullChannel::SetIdleChannelsRetries(UINT32 retries)
{
}
UINT32 NullChannel::GetIdleChannelsRetries()
{
    return 0;
}
GpuDevice *NullChannel::GetGpuDevice() const
{
    return NULL;
}
void NullChannel::SetWrapper(Channel *pWrapper)
{
}
Channel *NullChannel::GetWrapper() const
{
    return NULL;
}
AtomChannelWrapper *NullChannel::GetAtomChannelWrapper()
{
    return NULL;
}
PmChannelWrapper *NullChannel::GetPmChannelWrapper()
{
    return NULL;
}
RunlistChannelWrapper *NullChannel::GetRunlistChannelWrapper()
{
    return NULL;
}
SemaphoreChannelWrapper *NullChannel::GetSemaphoreChannelWrapper()
{
    return NULL;
}
RC NullChannel::RobustChannelCallback
(
    UINT32 errorLevel,
    UINT32 errorType,
    void *pData,
    void *pRecoveryCallback
)
{
    return OK;
}
RC NullChannel::RecoverFromRobustChannelError()
{
    return OK;
}
LwRm * NullChannel::GetRmClient()
{
    return LwRmPtr(0).Get();
}
RC NullChannel::RmcGetClassEngineId(LwRm::Handle, UINT32*, UINT32*, UINT32*)
{
    return OK;
}
RC NullChannel::RmcResetChannel(UINT32 EngineId)
{
    return OK;
}
RC NullChannel::RmcGpfifoSchedule(bool Enable)
{
    return OK;
}
RC NullChannel::SetRandomAutoFlushThreshold(const MinMaxSeed &minmaxseed)
{
    return OK;
}
RC NullChannel::SetRandomAutoFlushThresholdGpFifoEntries(const MinMaxSeed &minmaxseed)
{
    return OK;
}
RC NullChannel::SetRandomAutoGpEntryThreshold(const MinMaxSeed &minmaxseed)
{
    return OK;
}
RC NullChannel::SetRandomPauseBeforeGPPutWrite(const MinMaxSeed &minmaxseed)
{
    return OK;
}
RC NullChannel::SetRandomPauseAfterGPPutWrite(const MinMaxSeed &minmaxseed)
{
    return OK;
}
RC NullChannel::SetAutoSchedule(bool bAutoSched)
{
    return OK;
}
RC NullChannel::Initialize()
{
    return OK;
}
LwRm::Handle NullChannel::GetVASpace() const
{
    return 0U;
}

bool NullChannel::GetDoorbellRingEnable() const
{
    return false;
}

RC NullChannel::SetDoorbellRingEnable(bool enable)
{
    return RC::UNSUPPORTED_FUNCTION;
}

bool NullChannel::GetUseBar1Doorbell() const
{
    return false;
}

//------------------------------------------------------------------------------
BaseChannel::BaseChannel
(
    void *       pErrorNotifierMemory,
    LwRm::Handle hChannel,
    UINT32       ChID,
    UINT32       Class,
    GpuDevice *  pGrDev,
    LwRm *       pLwRm
)
{
    MASSERT(pGrDev);
    if (!pLwRm)
      pLwRm = LwRmPtr(0).Get();

    m_pGrDev = pGrDev;
    m_pLwRm = pLwRm;

    m_pRcHelper.reset(new RcHelper(pLwRm, pGrDev, hChannel,
                                   pErrorNotifierMemory,
                                   NULL));
    m_hChannel = hChannel;
    m_ChID = ChID;
    m_DevInst = pGrDev->GetDeviceInst();
    m_Class = Class;
    m_TimeoutMs = 1000;

    m_EnableLogging = false;
    m_IdleChannelsRetries = 0;

    m_pOutermostWrapper = this;

    switch (Surface2D::GetLocationOverride())
    {
        case Memory::Fb:
            m_PushBufferInFb = true;
            break;
        default:
            m_PushBufferInFb = false;
            break;
    }
}

//------------------------------------------------------------------------------
BaseChannel::~BaseChannel()
{
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::ScheduleChannel(bool Enable)
{
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::SetObject
(
    UINT32 Subchannel,
    LwRm::Handle Handle
)
{
    return Write(Subchannel, 0, Handle);
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::UnsetObject(LwRm::Handle Handle)
{
    return OK;
}

//------------------------------------------------------------------------------
RC BaseChannel::SetReference(UINT32 Value)
{
    return Write(0, LW906F_SET_REFERENCE, Value);
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::SetContextDmaSemaphore(LwRm::Handle hCtxDma)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::SetSemaphoreOffset(UINT64 Offset)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ void BaseChannel::SetSemaphoreReleaseFlags(UINT32 flags)
{
}

//------------------------------------------------------------------------------
/* virtual */ UINT32 BaseChannel::GetSemaphoreReleaseFlags()
{
    return FlagSemRelDefault;
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::SetSemaphorePayloadSize(SemaphorePayloadSize size)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ Channel::SemaphorePayloadSize BaseChannel::GetSemaphorePayloadSize()
{
    return SEM_PAYLOAD_SIZE_DEFAULT;
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::SemaphoreAcquire(UINT64 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::SemaphoreAcquireGE(UINT64 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::SemaphoreRelease(UINT64 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::SemaphoreReduction(Reduction Op, ReductionType redType, UINT64 Data)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::NonStallInterrupt()
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::Yield(UINT32 Subchannel)
{
    return Write(Subchannel, LW906F_YIELD, 0);
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::BeginInsertedMethods()
{
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ RC BaseChannel::EndInsertedMethods()
{
    return OK;
}

//------------------------------------------------------------------------------
/* virtual */ void BaseChannel::CancelInsertedMethods()
{
}

RC BaseChannel::GetSyncPoint(UINT32* pSyncPointId)
{
    return RC::UNSUPPORTED_FUNCTION;
}

UINT32 BaseChannel::ReadSyncPoint(UINT32 syncPointId)
{
    return ~0U;
}

void BaseChannel::IncrementSyncPoint(UINT32 syncPointId)
{
}

RC BaseChannel::WriteSyncPointIncrement(UINT32 syncPointId)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC BaseChannel::WriteSyncPointWait(UINT32 syncPointId, UINT32 value)
{
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
// next two are supported on Fermi+ classes only
RC BaseChannel::WriteIncOnce
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    return RC::UNSUPPORTED_FUNCTION;
}
RC BaseChannel::WriteImmediate
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Data
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC BaseChannel::WriteAcquireMutex
(
    UINT32 subchannel,
    UINT32 mutexIdx
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC BaseChannel::WriteReleaseMutex
(
    UINT32 subchannel,
    UINT32 mutexIdx
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC BaseChannel::SetClass
(
    UINT32 subchannel,
    UINT32 classId
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC BaseChannel::SetClassAndWriteMask
(
    UINT32 subchannel,
    UINT32 classId,
    UINT32 method,
    UINT32 mask,
    const UINT32* pData
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC BaseChannel::WriteMask
(
    UINT32 subchannel,
    UINT32 method,
    UINT32 mask,
    const UINT32* pData
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC BaseChannel::WriteDone()
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC BaseChannel::WaitIdle
(
    FLOAT64 TimeoutMs
)
{
    RC rc;

    // First do a WaitForDmaPush so that we spend less time inside the RM
    // (beneficial because of RM mutexing)
    Printf(Tee::PriHigh, "WARNING: Do not use WaitIdle, Use either notifier or gpu::poll\n");
    CHECK_RC(WaitForDmaPush(TimeoutMs));

    UINT32 retries = 0;
    do
    {
        if (retries != 0)
        {
            Tasker::Yield();
        }

        rc.Clear();
        rc = GetRmClient()->IdleChannels(
            m_hChannel,
            1,          // numChannels
            (UINT32*)0, // phClients
            (UINT32*)0, // phDevices
            (UINT32*)0, // phChannels
            DRF_DEF(OS30, _FLAGS, _BEHAVIOR, _SLEEP) |
            DRF_DEF(OS30, _FLAGS, _IDLE, _CACHE1) |
            DRF_DEF(OS30, _FLAGS, _IDLE, _ALL_ENGINES) |
            DRF_DEF(OS30, _FLAGS, _CHANNEL, _SINGLE),
            static_cast<UINT32>(TimeoutMs * 1000),
            m_DevInst);    // Colwert to us for RM
    } while ((++retries < m_IdleChannelsRetries) &&
             (rc == RC::LWRM_MORE_PROCESSING_REQUIRED));

    if (OK == rc)
    {
        rc = CheckError();
    }
    else
    {
        Printf(Tee::PriHigh, "%#010x channel could not be idled.\n", m_hChannel);
        JavaScriptPtr pJs;
        pJs->CallMethod(pJs->GetGlobalObject(), "ChannelWaitIdleErrorCallback");
        Tee::FlushToDisk();
    }

    // Informational escape write
    if (Platform::GetSimulationMode() != Platform::Hardware)
        m_pGrDev->EscapeWrite("WaitForIdle", 0, 4, 0);

    return rc;
}
RC BaseChannel::SetLegacyWaitIdleWithRiskyIntermittentDeadlocking(bool Enable)
{
    // Most older channel classes only support one type of WaitIdle.
    // Technically, they support legacy WaitIdle, but we'll pretend
    // it's non-legacy just to discourage people from setting this
    // option.  (Hey, it wasn't legacy at the time.)
    //
    return Enable ? RC::UNSUPPORTED_FUNCTION : OK;
}
bool BaseChannel::GetLegacyWaitIdleWithRiskyIntermittentDeadlocking() const
{
    return false;
}
void BaseChannel::SetDefaultTimeoutMs(FLOAT64 TimeoutMs)
{
    m_TimeoutMs = TimeoutMs;
}

FLOAT64 BaseChannel::GetDefaultTimeoutMs()
{
    return m_TimeoutMs;
}

UINT32 BaseChannel::GetClass()
{
   return m_Class;
}

LwRm::Handle BaseChannel::GetHandle()
{
   return m_hChannel;
}

RC BaseChannel::CheckError()
{
    GpuDevice *pGpuDevice = GetGpuDevice();
    RcHelper *pRcHelper = GetRcHelper();
    RcHelper::RecoveryHolder recoveryHolder;

    if ((DetectNewRobustChannelError() || pGpuDevice->GetResetInProgress()) &&
        recoveryHolder.TryAcquire(pRcHelper))
    {
        StickyRC firstRc;
        firstRc = GetWrapper()->RecoverFromRobustChannelError();
        if (pGpuDevice->GetResetInProgress())
            firstRc = RC::RESET_IN_PROGRESS;
        if (firstRc != OK)
            SetError(firstRc);
    }
    return m_Error;
}

void BaseChannel::SetError(RC Error)
{
    // Flush the error queue
    //
    CheckError();

    // Don't overwrite an existing channel error with a different
    // error.
    //
    if (Error != OK && m_Error != OK)
        return;

    m_Error = Error;
    if (Error == RC::RM_RCH_FIFO_ERROR_RUNOUT_OVERFLOW)
    {
        Printf(Tee::PriHigh,
               "Channel overflow!  Please increase channel size, Flush"
               " more often, or use AutoFlush.\n");
    }
}

void BaseChannel::ClearError()
{
    // Flush the error queue
    //
    CheckError();

    m_Error.Clear();
}

bool BaseChannel::DetectNewRobustChannelError() const
{
    return GetRcHelper()->DetectNewRobustChannelError();
}

bool BaseChannel::DetectError() const
{
    return (
        (m_Error != OK) ||                      // Old errors
        DetectNewRobustChannelError() ||        // New errors
        GetGpuDevice()->GetResetInProgress());  // Full-chip reset
}

UINT32 BaseChannel::GetChannelId() const
{
    return m_ChID;
}

void BaseChannel::EnableLogging (bool b)
{
    m_EnableLogging = b;
}

bool BaseChannel::GetEnableLogging() const
{
    return m_EnableLogging;
}

void BaseChannel::SetIdleChannelsRetries(UINT32 retries)
{
    m_IdleChannelsRetries = retries;
}

UINT32 BaseChannel::GetIdleChannelsRetries()
{
    return m_IdleChannelsRetries;
}

/* virtual */ RC BaseChannel::RobustChannelCallback
(
    UINT32 errorLevel,
    UINT32 errorType,
    void *pData,
    void *pRecoveryCallback
)
{
    return GetRcHelper()->RobustChannelCallback(errorLevel, errorType,
                                                pData, pRecoveryCallback);
}

/* virtual */ RC BaseChannel::RecoverFromRobustChannelError()
{
    RcHelper *pRcHelper = GetRcHelper();
    RC rc;

    CHECK_RC(pRcHelper->FlushIncomingErrors(GetDefaultTimeoutMs()));
    m_Error = pRcHelper->GetLastFlushedError();
    return rc;
}
/* virtual */ LwRm * BaseChannel::GetRmClient()
{
    return m_pLwRm;
}
/* virtual */ RC BaseChannel::RmcGetClassEngineId
(
    LwRm::Handle hObject,
    UINT32 *pClassEngineId,
    UINT32 *pClassId,
    UINT32 *pEngineId
)
{
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ RC BaseChannel::RmcResetChannel(UINT32 EngineId)
{
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ RC BaseChannel::RmcGpfifoSchedule(bool Enable)
{
    return RC::UNSUPPORTED_FUNCTION;
}

void BaseChannel::LogData(UINT32 n, UINT32 * pData)
{
    // We could put fancy channel-parsing features here someday...
    const UINT32 nCols = 4;
    UINT32 i;
    for (i = 0; i < n; i++)
    {
        if (0 == i % nCols)
        {
            if (i)
                Printf(Tee::PriNormal, "\n");
            Printf(Tee::PriNormal, "DEV_%x_CH_%x", m_DevInst, m_ChID);
        }
        Printf(Tee::PriNormal, " %08x", MEM_RD32(pData + i));
    }
    Printf(Tee::PriNormal, "\n");
}
RC BaseChannel::SetRandomAutoFlushThreshold(const MinMaxSeed &minmaxseed)
{
    return RC::UNSUPPORTED_FUNCTION;
}
RC BaseChannel::SetRandomAutoFlushThresholdGpFifoEntries(const MinMaxSeed &minmaxseed)
{
    return RC::UNSUPPORTED_FUNCTION;
}
RC BaseChannel::SetRandomAutoGpEntryThreshold(const MinMaxSeed &minmaxseed)
{
    return RC::UNSUPPORTED_FUNCTION;
}
RC BaseChannel::SetRandomPauseBeforeGPPutWrite(const MinMaxSeed &minmaxseed)
{
    return RC::UNSUPPORTED_FUNCTION;
}
RC BaseChannel::SetRandomPauseAfterGPPutWrite(const MinMaxSeed &minmaxseed)
{
    return RC::UNSUPPORTED_FUNCTION;
}
RC BaseChannel::SetAutoSchedule(bool bAutoSched)
{
    return RC::UNSUPPORTED_FUNCTION;
}
RC BaseChannel::Initialize()
{
    return OK;
}
RcHelper *BaseChannel::GetRcHelper() const
{
    return m_pRcHelper.get();
}

bool BaseChannel::GetDoorbellRingEnable() const
{
    return false;
}

RC BaseChannel::SetDoorbellRingEnable(bool enable)
{
    return RC::UNSUPPORTED_FUNCTION;
}

bool BaseChannel::GetUseBar1Doorbell() const
{
    return false;
}

//------------------------------------------------------------------------------
// GpFifoChannel
//

// Return true (exit polling loop) when Get == Put for all subdevices.
bool GpFifoChannel::PollGetEqPut
(
    void *pVoidPollArgs
)
{
    PollArgs *pPollArgs = (PollArgs *)pVoidPollArgs;
    GpFifoChannel *pThis = pPollArgs->pThis;
    UINT32 i;

    if (pThis->DetectError())
        return true;

    if (pThis->m_PbSizeInLastWrittenGpEntry == 0)
    {
        // No mthods are kicked off, return true directly
        // e.g. those tests doesn't write method to pushbuffer
        return true;
    }

    // Polled Put should inside the pushbuffer of last written GpEntry
    if ((pPollArgs->Put >= pThis->m_PbBaseInLastWrittenGpEntry) &&
        (pPollArgs->Put <= pThis->m_PbBaseInLastWrittenGpEntry
                           + pThis->m_PbSizeInLastWrittenGpEntry))
    {
        for (i = 0; i <= pPollArgs->LastGetIdx; i++)
        {
            const UINT64 NewGet = pThis->ReadGet(i);

            // Only update our cached get if the new Get pointer is
            // inside the legacy pushbuffer. m_CachedGet[] is not
            // for subroutine pushbuffer.
            if ((NewGet >= pThis->m_PushBufferOffset) &&
                (NewGet <= pThis->m_PushBufferOffset + pThis->m_PushBufferSize))
            {
                pThis->m_CachedGet[i] = NewGet - pThis->m_PushBufferOffset;
            }

            if (NewGet != pPollArgs->Put)
                return false;
        }
    }
    else
    {
        // Suppose this will not happen.
        // If the polled put is not in pushbuffer segment of last written
        // Gpentry, return true directly because it's assumed o have happened
        return true;
    }
    return true;
}

// Return true (exit polling loop) when GpGet == GpPut for all subdevices.
bool GpFifoChannel::PollGpGetEqGpPut
(
    void *pVoidPollArgs
)
{
    PollArgs *pPollArgs = (PollArgs *)pVoidPollArgs;
    GpFifoChannel *pThis = pPollArgs->pThis;
    UINT32 i;

    if (pThis->DetectError())
        return true;

    pThis->UpdateCachedGpGets();
    for (i = 0; i <= pPollArgs->LastGetIdx; i++)
    {
        if (pPollArgs->pCachedGpGet[i] != pPollArgs->GpPut)
            return false;
    }
    return true;
}

// Wait until the Get pointers are outside the range from beginPut to
// endPut.  This should be called before writing, to make sure that we
// don't overwrite any data that the GPU hasn't read yet.  The write
// starts at beginPut and ends at endPut-1.
//
// The pushbuffer is cirlwlar, so it's legal to hav endPut < beginPut.
// In that case, the caller wants to start writing from beginPut, wrap
// around to the start of the pushbuffer, and write up to endPut.
//
// beginPut and endPut should be from 0 to m_PushBufferSize-1, but to
// make the caller's math easier, this method allows a value of
// m_PushBufferSize to be treated as an alias for 0.
//
RC GpFifoChannel::WaitForRoom(UINT64 beginPut, UINT64 endPut)
{
    MASSERT(beginPut <= m_PushBufferSize);
    MASSERT(endPut   <= m_PushBufferSize);

    const UINT32 numSubdevices = GetGpuDevice()->GetNumSubdevices();
    RC rc;

    // To avoid dealing with funky cirlwlar-pushbuffer math, translate
    // all the pushbuffer pointers into modulo offsets from 0 to
    // m_PushBufferSize-1, such that endPut is at the highest offset.
    // The results *should* be in the following order, except that we
    // might have to wait while modCachedGet > modBeginPut until the
    // GPU advances the get pointer:
    // cachedGet <= cachedPut <= gpEntryPut <= beginPut <= endPut
    //
    const UINT64 base = (2 * m_PushBufferSize - endPut - 1) % m_PushBufferSize;
    const UINT64 modCachedPut  = (m_CachedPut + base) % m_PushBufferSize;
    const UINT64 modBeginPut   = (beginPut    + base) % m_PushBufferSize;

    // Check whether we're trying to overwrite the last flushed Put
    // pointer.  If so, then the test has tried to write more data
    // than the pushbuffer can hold, without flushing.  It must either
    // enable autoflush or add calls to Flush somewhere.
    //
    if (modCachedPut > modBeginPut)
    {
        // Fake an RM robust-channels error, so that this Channel will
        // persistently fail from now on.
        SetError(RC::RM_RCH_FIFO_ERROR_RUNOUT_OVERFLOW);
        return RC::PUSHBUFFER_TOO_SMALL;
    }

    // Make sure that the other pointers are in the correct order
    //
#ifdef DEBUG
    const UINT64 modGpEntryPut = (m_GpEntryPut + base) % m_PushBufferSize;
    MASSERT(modCachedPut <= modGpEntryPut);
    MASSERT(modGpEntryPut <= modBeginPut);
#endif

    // Check whether we have enough space based on the last known
    // position of Get.  If so, we can return OK without polling.
    //
    bool success = true;
    for (UINT32 ii = 0; ii < numSubdevices; ii++)
    {
        const UINT64 modCachedGet = (m_CachedGet[ii] + base) % m_PushBufferSize;
        if (modCachedGet > modBeginPut)
        {
            success = false;
            break;
        }
        // TODO: enable this MASSERT(modCachedGet <= modCachedPut);
    }
    if (success)
    {
        return OK;
    }

    // Poll until the Get pointer is outside of the range
    // beginPut < Get <= endPut
    //
    CHECK_RC(Tasker::PollHw(m_TimeoutMs, [&]()->bool
    {
        if (DetectError())
        {
            return true;
        }
        UpdateCachedGets();
        for (UINT32 ii = 0; ii < numSubdevices; ii++)
        {
            const UINT64 modCachedGet = ((m_CachedGet[ii] + base) %
                                         m_PushBufferSize);
            if (modCachedGet > modBeginPut)
            {
                return false;
            }
            // TODO: enable this MASSERT(modCachedGet <= modCachedPut);
        }
        return true;
    }, __FUNCTION__));

    CHECK_RC(CheckError());
    return rc;
}

// Return true (exit polling loop) when all subdevices have read past the
// gpfifo entry we are about to overwrite.
bool GpFifoChannel::PollGpFifoFull
(
    void * pVoidPollArgs
)
{
    PollArgs *pPollArgs = (PollArgs *)pVoidPollArgs;
    GpFifoChannel *pThis = pPollArgs->pThis;
    UINT32 i;

    if (pThis->DetectError())
        return true;

    pThis->UpdateCachedGpGets();
    for (i = 0; i <= pPollArgs->LastGetIdx; i++)
    {
        if (pPollArgs->pCachedGpGet[i] == pPollArgs->GpPut)
            return false;
    }
    return true;
}

void GpFifoChannel::UpdateCachedGets()
{
    UINT32 i;
    for (i = 0; i <= m_PollArgs.LastGetIdx; i++)
    {
        const UINT64 NewGet = ReadGet(i);

        // Only update our cached get if the new Get pointer is inside the pushbuffer
        // This handles both the subroutine case and the initial case where Get is
        // unconditionally zero.
        if ((NewGet >= m_PushBufferOffset) &&
            (NewGet <= m_PushBufferOffset + m_PushBufferSize))
        {
            m_CachedGet[i] = NewGet - m_PushBufferOffset;
        }
    }
}

UINT64 GpFifoChannel::ReadGet(UINT32 i) const
{
    if (m_bUseGetSemaphore)
    {
        const UINT32* ptr = static_cast<UINT32*>(m_GetSemaphore.GetAddress());
        const UINT32  get = MEM_RD32(ptr + i * s_SemaphoreDwords);
        return get + m_PushBufferOffset;
    }
    else
    {
        const UINT32 get   = MEM_RD32(m_pGet[i]);
        const UINT32 getHi = MEM_RD32(m_pGetHi[i]);
        const UINT64 newGet = (get | (UINT64(getHi) << 32));
        return newGet;
    }
}

void GpFifoChannel::UpdateCachedGpGets()
{
    for (UINT32 i = 0; i <= m_PollArgs.LastGetIdx; i++)
    {
        UINT32 gpGet;
        if (m_bUseGpGetSemaphore)
        {
            const UINT32* ptr = static_cast<UINT32*>(m_GpGetSemaphore.GetAddress());
            gpGet = MEM_RD32(ptr + i * s_SemaphoreDwords);
        }
        else
        {
            gpGet = MEM_RD32(m_pGpGet[i]);
        }
        m_CachedGpGet[i] = gpGet;
    }
}

GpFifoChannel::GpFifoChannel
(
    UserdAllocPtr pUserdAlloc,
    void *        pPushBufferBase,
    UINT32        PushBufferSize,
    void *        pErrorNotifierMemory,
    LwRm::Handle  hChannel,
    UINT32        ChID,
    UINT32        Class,
    UINT64        PushBufferOffset,
    void *        pGpFifoBase,
    UINT32        GpFifoEntries,
    LwRm::Handle  hVASpace,
    GpuDevice *   pGrDev,
    LwRm *        pLwRm
) :
    BaseChannel(pErrorNotifierMemory, hChannel, ChID, Class, pGrDev, pLwRm)
    , m_hVASpace(hVASpace)
    , m_bUseGetSemaphore(false)
    , m_bUseGpGetSemaphore(false)
    , m_pUserdAlloc(pUserdAlloc)
    , m_pSavedLwrrent(nullptr)
{
    MASSERT(Class >= GF100_CHANNEL_GPFIFO);

    MASSERT(pGrDev);

    m_PrivEnable = false;
    m_SyncWait =   false;

    m_pPushBufferBase = (UINT08 *)pPushBufferBase;
    m_pGpFifoBase = (UINT08 *)pGpFifoBase;
    m_PushBufferOffset = PushBufferOffset;
    m_PushBufferSize = PushBufferSize;
    m_GpFifoEntries = GpFifoEntries;

    m_PbBaseInLastWrittenGpEntry = m_PushBufferOffset;
    m_PbSizeInLastWrittenGpEntry = 0;

    // Start writing at the base of the pushbuffer plus the specified offset
    m_pLwrrent = (UINT32 *)m_pPushBufferBase;
    m_LwrrentGpPut = 0;
    m_CachedPut = 0;
    m_CachedGpPut = 0;
    m_GpEntryPut = 0;

    // Autoflush is on by default, as it makes some tests run faster,
    // and might "fix" a badly written test.
    // @@@ 256 bytes is awfully low, 4096 would be better for CPU efficiency.
    m_AutoFlushThresholdMin = 0; // Prevent using unitialized variable in the next call
    m_AutoFlushThresholdMax = 0; // Prevent using unitialized variable in the next call
    m_pAutoFlushThreshold = NULL;
    m_pAutoFlushThresholdGpFifoEntries = NULL;
    m_pAutoGpEntryThreshold = NULL;
    SetAutoGpEntry(false, 0); // Call after setting m_pLwrrent as it is used
    SetAutoFlush(true, 256);

    m_pPauseBeforeGPPutWrite = NULL;
    m_PauseBeforeGPPutWriteMin = 0;
    m_PauseBeforeGPPutWriteMax = 0;

    m_pPauseAfterGPPutWrite = NULL;
    m_PauseAfterGPPutWriteMin = 0;
    m_PauseAfterGPPutWriteMax = 0;

    m_PollArgs.pThis = this;
    m_PollArgs.pCachedGet = m_CachedGet;
    m_PollArgs.pCachedGpGet = m_CachedGpGet;
    m_PollArgs.LastGetIdx = pGrDev->GetNumSubdevices() - 1;

    // XXX50 TopLevelGet isn't working right in fmodel yet
    UINT32 i;
    for (i = 0; i <= m_PollArgs.LastGetIdx; i++)
    {
        if (!CanUseGetGpGetSemaphore())
        {
            void *pvUserd = m_pUserdAlloc->GetAddress(i);
            m_pGet[i] = (volatile UINT32 *)
                &((GF100ControlGPFifo *)(pvUserd))->Get;
            m_pGetHi[i] = (volatile UINT32 *)
                &((GF100ControlGPFifo *)(pvUserd))->GetHi;
            m_pGpGet[i] = (volatile UINT32 *)
                &((GF100ControlGPFifo *)(pvUserd))->GPGet;
        }
        else
        {
            m_pGet[i] = 0;
            m_pGetHi[i] = 0;
            m_pGpGet[i] = 0;
        }
        m_CachedGet[i] = 0;
        m_CachedGpGet[i] = 0;
    }
    m_ExternalGPPutAdvanceMode = false;
}

GpFifoChannel::~GpFifoChannel()
{
    m_pUserdAlloc->Free();
    delete m_pAutoFlushThreshold;
    delete m_pAutoFlushThresholdGpFifoEntries;
    delete m_pAutoGpEntryThreshold;
    delete m_pPauseBeforeGPPutWrite;
    delete m_pPauseAfterGPPutWrite;
}

/* virtual */ RC GpFifoChannel::Initialize()
{
    RC rc;
    if (CanUseGetGpGetSemaphore() && 
        GetGpuDevice()->GetNumSubdevices() == 1) // Lwrrently this breaks SLI, so disabling it
    {
        rc = SetupGpGetSemaphore();
        if (rc == RC::UNSUPPORTED_FUNCTION)
        {
            Printf(Tee::PriLow, "GpGet semaphore not supported on current channel\n");
            rc.Clear();
        }
        CHECK_RC_MSG(rc, "Unable to setup gpget semaphore");
        CHECK_RC_MSG(SetupGetSemaphore(), "Unable to setup get semaphore");
    }
    return BaseChannel::Initialize();
}

/* virtual */ RC GpFifoChannel::SetAutoFlush
(
    bool AutoFlushEnable,
    UINT32 AutoFlushThreshold
)
{
    m_AutoFlushEnable    = AutoFlushEnable;
    m_AutoFlushThresholdMin = AutoFlushThreshold;
    m_AutoFlushThresholdMax = AutoFlushThreshold;

    if (m_AutoFlushEnable)
    {
        // If autoflush is enabled, make sure
        // 1 <= threshold < buffer_size && threshold <= GP_ENTRY1_LENGTH limit
        m_AutoFlushThresholdMax = max(m_AutoFlushThresholdMax, (UINT32)1);
        m_AutoFlushThresholdMax = min(m_AutoFlushThresholdMax, m_PushBufferSize - 1);
        m_AutoFlushThresholdMax =
            min(m_AutoFlushThresholdMax, (GetGpEntry1LengthMask() - 1) << 2);
        m_AutoFlushThresholdMin = m_AutoFlushThresholdMax;
    }

    m_pNextAutoFlush = (UINT32 *)((UINT08 *)m_pLwrrent + m_AutoFlushThresholdMax);

    m_AutoFlushThresholdGpFifoEntriesMax = CalcAutoFlushThresholdGpFifoEntries();
    m_AutoFlushThresholdGpFifoEntriesMin = m_AutoFlushThresholdGpFifoEntriesMax;

    Printf(Tee::PriDebug, "DEV %x_CH_%x autoflush ", m_DevInst, m_ChID);
    if (m_AutoFlushEnable)
    {
        Printf(Tee::PriDebug,
               "every %d pushbuffer bytes, every %d gpentries\n",
               m_AutoFlushThresholdMax, m_AutoFlushThresholdGpFifoEntriesMax);
    }
    else
    {
        Printf(Tee::PriDebug, "disabled\n");
    }

    return OK;
}

/* virtual */ bool GpFifoChannel::GetAutoFlushEnable() const
{
    return m_AutoFlushEnable;
}

/* virtual */ RC GpFifoChannel::SetAutoGpEntry
(
    bool AutoGpEntryEnable,
    UINT32 AutoGpEntryThreshold
)
{
    m_AutoGpEntryEnable = AutoGpEntryEnable;
    m_AutoGpEntryThresholdMax = AutoGpEntryThreshold;
    m_AutoGpEntryThresholdMin = AutoGpEntryThreshold;

    if (m_AutoGpEntryEnable)
    {
        // If AutoGpEntry is enabled, make sure 1 <= threshold < buffer_size
        m_AutoGpEntryThresholdMax = max(m_AutoGpEntryThresholdMax, (UINT32)1);
        m_AutoGpEntryThresholdMax = min(m_AutoGpEntryThresholdMax,
                                     m_PushBufferSize - 1);
        m_AutoGpEntryThresholdMin = m_AutoGpEntryThresholdMax;
    }

    m_pNextAutoGpEntry = (UINT32 *)((UINT08 *)m_pLwrrent +
                                    m_AutoGpEntryThresholdMax);

    m_AutoFlushThresholdGpFifoEntriesMax = CalcAutoFlushThresholdGpFifoEntries();
    m_AutoFlushThresholdGpFifoEntriesMin = m_AutoFlushThresholdGpFifoEntriesMax;

    Printf(Tee::PriDebug, "DEV %x_CH_%x auto_gp_entry ", m_DevInst, m_ChID);
    if (m_AutoGpEntryEnable)
        Printf(Tee::PriDebug, "every %d pushbuffer bytes\n",
               m_AutoGpEntryThresholdMax);
    else
        Printf(Tee::PriDebug, "disabled\n");

    return OK;
}

/* virtual */ RC GpFifoChannel::WriteWithSurface
(
    UINT32           subchannel,
    UINT32           method,
    const Surface2D& surface,
    UINT32           offset,
    UINT32           offsetShift,
    MappingType      mappingType
)
{
    // TODO: Add MASSERT(offsetShift <= 32);
    const UINT64 va = surface.GetCtxDmaOffsetGpu() + offset;
    // Check if target VA can be shifted right without loss.
    // In case of offsetShift==32 we're writing top 32 bits of the VA.
    const UINT64 mask = (1ULL << offsetShift) - 1ULL;
    if ((va & mask) != 0U && offsetShift < 32)
    {
        MASSERT(!"Invalid surface offset!");
        return RC::SOFTWARE_ERROR;
    }
    const UINT64 data64 = va >> offsetShift;
    const UINT32 data = UNSIGNED_CAST(UINT32, data64);
    return WriteNonInc(subchannel, method, 1, &data);
}

/* virtual */ RC GpFifoChannel::WriteWithSurfaceHandle
(
    UINT32      subchannel,
    UINT32      method,
    UINT32      hSurface,
    UINT32      offset,
    UINT32      offsetShift,
    MappingType mappingType
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpFifoChannel::CallSubroutine(UINT64 Offset, UINT32 Size)
{
    RC rc;

    CHECK_RC(FinishOpenGpFifoEntry(nullptr));

    // Write a GpEntry for this subroutine
    // Unroll as required.
    const UINT32 MaxBytes = GetGpEntry1LengthMask() << 2;

    for (UINT32 BytesLeft = Size; BytesLeft > 0; /**/)
    {
        UINT32 BytesThisLoop = BytesLeft;
        if (BytesThisLoop > MaxBytes)
            BytesThisLoop = MaxBytes;

        CHECK_RC(WriteGpEntry(Offset, BytesThisLoop, true));

        Offset += BytesThisLoop;
        BytesLeft -= BytesThisLoop;
    }

    return OK;
}

/* virtual */ RC GpFifoChannel::SetPrivEnable(bool Priv)
{
    RC rc;

    if (m_PrivEnable != Priv)
    {
        // Priv levels are set in the gpfifo entry, so we must write a
        // new entry if we change priv level.
        //
        CHECK_RC(FinishOpenGpFifoEntry(nullptr));

        m_PrivEnable = Priv;
    }
    return OK;
}

/* virtual */ RC GpFifoChannel::WaitForDmaPush
(
    FLOAT64 TimeoutMs
)
{
    RC rc;

    // Informational escape write
    if (Platform::GetSimulationMode() != Platform::Hardware)
        Platform::EscapeWrite("poll_pb_empty", 0, 4, GetChannelId());

    // Wait for [Gp]Get to equal [Gp]Put
    m_PollArgs.Put = m_PbBaseInLastWrittenGpEntry + m_PbSizeInLastWrittenGpEntry;
    m_PollArgs.GpPut = m_CachedGpPut;
    CHECK_RC(Tasker::PollHw(TimeoutMs, [&]()->bool
    {
        return PollGpGetEqGpPut(&m_PollArgs);
    }, __FUNCTION__));
    CHECK_RC(Tasker::PollHw(TimeoutMs, [&]()->bool
    {
        return PollGetEqPut(&m_PollArgs);
    }, __FUNCTION__));
    CHECK_RC(CheckError());

    return OK;
}

/* virtual */ void GpFifoChannel::ClearPushbuffer()
{
    m_pLwrrent = (UINT32 *)m_pPushBufferBase;
    m_LwrrentGpPut = 0;
    m_GpEntryPut = 0;
    m_CachedPut = 0;
    m_CachedGpPut = 0;
    m_pNextAutoFlush = (UINT32 *)((UINT08 *)m_pLwrrent + GetAutoFlushThreshold());
    m_pNextAutoGpEntry = (UINT32 *)((UINT08 *)m_pLwrrent +
                                    GetAutoGpEntryThreshold());
    for (UINT32 ii = 0; ii < Gpu::MaxNumSubdevices; ++ii)
    {
        m_CachedGet[ii] = 0;
        m_CachedGpGet[ii] = 0;
    }
}

/* virtual */ RC GpFifoChannel::GetCachedPut(UINT32 *PutPtr)
{
    MASSERT(m_CachedPut <= 0xffffffff);
    *PutPtr = (UINT32)m_CachedPut;
    return OK;
}

/* virtual */ RC GpFifoChannel::SetCachedPut(UINT32 PutPtr)
{
    // This isn't legal in a GPFIFO channel, at least not yet
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpFifoChannel::GetPut(UINT32 *PutPtr)
{
    UINT64 Put = (UINT64)((UINT08 *)m_pLwrrent - m_pPushBufferBase);
    MASSERT(Put <= 0xffffffff);
    *PutPtr = (UINT32)Put;
    return OK;
}

/* virtual */ RC GpFifoChannel::SetPut(UINT32 PutPtr)
{
    // This isn't legal in a GPFIFO channel, at least not yet
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpFifoChannel::GetGet(UINT32 *GetPtr)
{
    // This isn't legal in a GPFIFO channel, at least not yet
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpFifoChannel::GetPushbufferSpace(UINT32 *PbSpacePtr)
{
    UINT32 NewPut = (UINT32)((UINT08 *)m_pLwrrent - m_pPushBufferBase);

    *PbSpacePtr = m_PushBufferSize - NewPut;
    if (*PbSpacePtr > 0)
    {
        // Modify the amount of space remaining based on the last known
        // position of Get
        for (UINT32 i = 0; i <= m_PollArgs.LastGetIdx; i++)
        {
            if (m_CachedGet[i] > NewPut)
            {
                // Since (Get > NewPut) and all increments are in sizeof(UINT32),
                // there must be enough space to pull out an extra method.
                // Since (Put == Get) means that the buffer is empty, the amount
                // of avialable space is always one method less than the get
                *PbSpacePtr = min(*PbSpacePtr,
                                  (UINT32)(m_CachedGet[i] - NewPut - sizeof(UINT32)));
            }
        }
    }

    // Return the number of methods (rather than bytes in the buffer)
    *PbSpacePtr = (*PbSpacePtr >> 2);
    return OK;
}

/* virtual */ RC GpFifoChannel::GetCachedGpPut(UINT32 *GpPutPtr)
{
    *GpPutPtr = m_CachedGpPut;
    return OK;
}

/* virtual */ RC GpFifoChannel::GetGpPut(UINT32 *GpPutPtr)
{
    *GpPutPtr = m_LwrrentGpPut;
    return OK;
}

/* virtual */ RC GpFifoChannel::GetGpGet(UINT32 *GpGetPtr)
{
    UpdateCachedGpGets();
    *GpGetPtr = m_CachedGpGet[0];

    // Note we are returning data from first subdevice of potentially a
    // multi-gpu SLI device.  Should we look at the lwrrently-selected subdev?
    return OK;
}

/* virtual */ RC GpFifoChannel::GetGpEntries(UINT32 *GpEntriesPtr)
{
    UINT32 NextGpPut;

    // We assume m_GpFifoEntries is a power of two, as required by HW.
    NextGpPut = (m_LwrrentGpPut + 1) & (m_GpFifoEntries - 1);

    if (m_CachedGpPut > NextGpPut)
    {
        *GpEntriesPtr = m_CachedGpPut - NextGpPut;
    }
    else
    {
        *GpEntriesPtr = m_GpFifoEntries - NextGpPut + m_CachedGpPut;
    }

    // Don't overwrite an unflushed-but-previously-used gpfifo entry.
    if (*GpEntriesPtr != 0)
    {
        // Are any gpus still reading the entry that would be overwritten if
        // a new GPEntry were to be created
        UINT32 i;
        for (i = 0; i <= m_PollArgs.LastGetIdx; i++)
        {
            if (m_CachedGpGet[i] == NextGpPut)
            {
                *GpEntriesPtr = 0;
                break;
            }
        }
    }

    return OK;
}

/* virtual */ RC GpFifoChannel::GetReference(UINT32 *pRefCnt)
{
    MASSERT(pRefCnt);
    *pRefCnt = MEM_RD32(&((GF100ControlGPFifo*)(m_pUserdAlloc->GetAddress(0)))->Reference);

    // Note we are returning data from first subdevice of potentially a
    // multi-gpu SLI device.  Should we look at the lwrrently-selected subdev?
    return OK;
}

/* virtual */ RC GpFifoChannel::NonStallInterrupt()
{
    return Write(0, LW906F_NON_STALL_INTERRUPT, 0);
}

/* virtual */ RC GpFifoChannel::Flush()
{
    RC rc;
    UINT64 end;
    CHECK_RC(FinishOpenGpFifoEntry(&end));

    m_CachedPut = end;
    m_pNextAutoFlush = (UINT32 *)((UINT08 *)m_pLwrrent + GetAutoFlushThreshold());

    CHECK_RC(WriteGpPut());

    return CheckError();
}

// private
RC GpFifoChannel::WriteGpPut()
{
    RC rc;

    if (m_LwrrentGpPut != m_CachedGpPut && !m_ExternalGPPutAdvanceMode)
    {
        // Flush WC before writing GpPut.
        rc = Platform::FlushCpuWriteCombineBuffer();

        // WAR for 388771
        if (m_pGrDev->HasBug(388771) &&
            m_PushBufferInFb)
        {
            GpuDevice* pGpuDev = m_pGrDev;
            for (UINT32 i=0; i<pGpuDev->GetNumSubdevices(); i++)
            {
                GpuSubdevice *pSubdev = pGpuDev->GetSubdevice(i);
                CHECK_RC(pSubdev->FbFlush(m_TimeoutMs));
            }
        }

        // Flush the GpPut pointer.
        for (UINT32 ii = 0; ii < GetGpuDevice()->GetNumSubdevices(); ++ii)
        {
            if (m_EnableLogging &&
                (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
            {
                Printf(Tee::PriNormal,
                    "DEV_%x_CH_%x Flush subdev %d: GpPut was 0x%x now 0x%x\n",
                    m_DevInst, m_ChID, ii, m_CachedGpPut, m_LwrrentGpPut);
            }

            // Check for RC errors before updating GP_PUT (sending a GP
            // entry down after an RC error will likely cause hangs since
            // the channel cannot continue to process data after an RC
            // error)
            CHECK_RC(CheckError());

            PauseBeforeGpPutUpdate();

            CHECK_RC(CommitGpPut(m_pUserdAlloc->GetAddress(ii), m_pGpFifoBase,
                        m_CachedGpPut, m_LwrrentGpPut, &m_LwrrentGpPut, ii));

            PauseAfterGpPutUpdate();
        }
        m_CachedGpPut = m_LwrrentGpPut;
    }
    return rc;
}

/* virtual */ RC GpFifoChannel::CommitGpPut(void* controlRegs,
        UINT08* pGpFifoBase, UINT32 startGpPut, UINT32 endGpPut,
        UINT32* pOutGpPut, UINT32 subdev)
{
    MEM_WR32(&((GF100ControlGPFifo*)(controlRegs))->GPPut,
                                    endGpPut);
    return OK;
}

// private
RC GpFifoChannel::WriteGpEntry(UINT64 Get, UINT32 Length, bool Subroutine)
{
    RC rc;

    // Length cannot exceed gpEntry1Length field.
    // Higher level code should unroll into chunks
    // of supported size if needed.  Also, Get must be 4-byte aligned.
    MASSERT((Length >> 2) <= GetGpEntry1LengthMask());
    MASSERT(!(Get & 3));

    // Don't overflow the pushbuffer
    if (Get >= m_PushBufferOffset &&
        Get < m_PushBufferOffset + m_PushBufferSize)
    {
        MASSERT(Length <= m_PushBufferOffset + m_PushBufferSize - Get);
    }

    if (m_EnableLogging &&
        (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
    {
        Printf(Tee::PriNormal, "DEV_%x_CH_%x GpEntry: 0x%02x%08x length 0x%x:\n",
                    m_DevInst, m_ChID, UINT32(Get>>32), UINT32(Get),
                    Length);

        if ((Get >= m_PushBufferOffset) &&
            (Get < m_PushBufferOffset + m_PushBufferSize))
        {
            // This is within our pushbuffer, we know the vaddr for the data.
            LogData(Length>>2,
                    (UINT32*)(m_pPushBufferBase + Get - m_PushBufferOffset));
        }
    }

    UINT32 GpEntry0, GpEntry1;
    CHECK_RC(ConstructGpEntryData(Get, Length, Subroutine,
        &GpEntry0, &GpEntry1));
    CHECK_RC(WriteGpEntryData(GpEntry0, GpEntry1));

    m_PbBaseInLastWrittenGpEntry = Get;
    m_PbSizeInLastWrittenGpEntry = Length;

    UINT64 entryData = GpEntry0 | (((UINT64)GpEntry1 & 0xFFFFFFFF) << 32);
    CHECK_RC(WritePbCrcCheckGpEntry(&entryData, sizeof(UINT64)));

    return OK;
}

/* virtual */ UINT32 GpFifoChannel::GetHostSemaphoreSize() const
{
    MASSERT(0);
    return 0;
}

/* virtual */ RC GpFifoChannel::FinishOpenGpFifoEntry(UINT64* pEnd)
{
    const UINT32 flushSize = GetFlushSize();
    RC rc;

    // Wait until we have enough room to write the remainder of the entry
    const UINT64 lwrrentPut = (reinterpret_cast<UINT08*>(m_pLwrrent) -
                               m_pPushBufferBase);
    CHECK_RC(WaitForRoom(lwrrentPut, lwrrentPut + flushSize));

    // Don't write the get/gpget semaphores if they just been written.
    // The semaphore is used to track the get/gpget pointers instead of reading USERD,
    // so it's pointless to advance them without any meaningful methods in the
    // pushbuffer.
    // Also we would overrun the pushbuffer, because we're not calling MakeRoom here.
    AtomChannelWrapper *pAtomWrap = GetWrapper()->GetAtomChannelWrapper();
    bool isAtomExelwtion = pAtomWrap == NULL ? false : pAtomWrap->IsAtomExelwtion();
    if ((!isAtomExelwtion && (m_bUseGetSemaphore || m_bUseGpGetSemaphore)) && 
        (m_pSavedLwrrent != m_pLwrrent))
    {
        MASSERT(m_GetSemaphore.IsMapped() || m_GpGetSemaphore.IsMapped());

        const UINT32 numSubdev = GetGpuDevice()->GetNumSubdevices();
        Channel*     pWrapper  = GetWrapper();

        CHECK_RC(pWrapper->BeginInsertedMethods());
        Utility::CleanupOnReturn<Channel> cleanupInsertedMethods(
                pWrapper, &Channel::CancelInsertedMethods);

        for (UINT32 isub=0; isub < numSubdev; isub++)
        {
            if (numSubdev > 1)
                CHECK_RC(WriteSetSubdevice(1 << isub));
            if (m_bUseGpGetSemaphore)
            {
                MASSERT(m_GpGetSemaphore.IsMapped());
                CHECK_RC(WriteGpGetSemaphore(m_GpGetSemaphore.GetCtxDmaOffsetGpu() +
                                             isub * s_SemaphoreDwords * sizeof(UINT32),
                                             m_GpFifoEntries));
            }
            if (m_bUseGetSemaphore)
            {
                MASSERT(m_GetSemaphore.IsMapped());
                CHECK_RC(WriteGetSemaphore(m_GetSemaphore.GetCtxDmaOffsetGpu() +
                                           isub * s_SemaphoreDwords * sizeof(UINT32),
                                           m_pPushBufferBase));
            }
        }

        cleanupInsertedMethods.Cancel();
        CHECK_RC(pWrapper->EndInsertedMethods());

        // Save the get pointer so we can detect if the semaphore is
        // being written back to back.
        m_pSavedLwrrent = m_pLwrrent;
    }

    const UINT64 end = (reinterpret_cast<UINT08*>(m_pLwrrent) -
                        m_pPushBufferBase);
    MASSERT(end - m_GpEntryPut <= 0xffffffff);
    const UINT32 count = static_cast<UINT32>(end - m_GpEntryPut);

    // Write the open GPFIFO entry without flushing.
    if (count)
    {
        CHECK_RC(WriteGpEntry(m_GpEntryPut + m_PushBufferOffset, count, false));

        // Start a new GPFIFO entry.
        m_GpEntryPut = end;
    }

    // Wrap if there isn't enough room to write another GPFIFO entry
    // in the pushbuffer
    if (end + flushSize > m_PushBufferSize)
    {
        CHECK_RC(Wrap(0));
    }

    if (pEnd)
    {
        *pEnd = end;
    }
    return OK;
}

//! Wrap back to the start of the pushbuffer
//! \param writeSize Num bytes the caller will write at the pushbuffer's start
//!
RC GpFifoChannel::Wrap(UINT32 writeSize)
{
    const UINT64 lwrrentPut = (reinterpret_cast<UINT08*>(m_pLwrrent) -
                               m_pPushBufferBase);
    RC rc;

    CHECK_RC(WaitForRoom(lwrrentPut, writeSize));
    m_pLwrrent = reinterpret_cast<UINT32*>(m_pPushBufferBase);
    m_GpEntryPut = 0;

    if (m_pNextAutoFlush >= reinterpret_cast<UINT32*>(m_pPushBufferBase +
                                                      lwrrentPut))
    {
        m_pNextAutoFlush -= lwrrentPut / sizeof(UINT32);
    }
    else
    {
        m_pNextAutoFlush = reinterpret_cast<UINT32*>(m_pPushBufferBase);
    }
    return OK;
}

/* virtual */ RC GpFifoChannel::SetExternalGPPutAdvanceMode(bool enable)
{
    bool wasEnabled = m_ExternalGPPutAdvanceMode;
    RC rc;

    if (wasEnabled && enable)
    {
        MASSERT(!"Two modules are both trying to control GPPUT");
        return RC::SOFTWARE_ERROR;
    }

    m_ExternalGPPutAdvanceMode = enable;

    if (wasEnabled && !enable)
    {
        // When we re-enable GPPUT, catch up on any GPPUT updates we missed
        CHECK_RC(WriteGpPut());
    }
    return OK;
}

/* virtual */ RC GpFifoChannel::RmcResetChannel(UINT32 EngineId)
{
    return RC::UNSUPPORTED_FUNCTION;
}

RC GpFifoChannel::MakeRoom(UINT32 count)
{
    RC rc;
    const UINT32 writeSize  = count * sizeof(UINT32);
    const UINT32 flushSize  = GetFlushSize();
    UINT64       lwrrentPut = (reinterpret_cast<UINT08*>(m_pLwrrent) -
                               m_pPushBufferBase);

    // Determine whether we need to flush or wrap the pushbuffer in
    // order to get the needed room.
    //
    bool mustFlush = false; // Must call Flush() or FinishOpenGpFifoEntry()
    bool mustWrap  = false; // Must wrap around to the start of the pushbuffer

    if (lwrrentPut - m_GpEntryPut + writeSize + flushSize >
        GetGpEntry1LengthMask() * sizeof(UINT32))
    {
        // Don't accumulate gpentries that are longer than HW allows.
        mustFlush = true;
    }

    if (m_AutoFlushEnable &&
        lwrrentPut < m_CachedPut &&
        lwrrentPut + writeSize + flushSize >= m_CachedPut)
    {
        // If we're about to fill the pushbuffer, then flush so that
        // the Get pointer can start advancing.
        mustFlush = true;
    }

    if (lwrrentPut + writeSize + flushSize + (mustFlush ? flushSize : 0) >
        m_PushBufferSize)
    {
        // Wrap if there isn't enough room at the end of the
        // pushbuffer.  We usually need writeSize+flushSize bytes, but
        // if we're about to flush for some unrelated reason, then
        // leave space for two flushes so that we can handle the
        // sequence flush-write-flush.
        // Note that mustWrap implies mustFlush.
        mustWrap  = true;
        mustFlush = true;
    }

    // Flush the pushbuffer if needed
    //
    if (mustFlush)
    {
        // bug 200008872: The last GPPut pointer might be in ACE PB
        // side, so set m_pSaveLwrrent=NULL to make sure the following
        // Flush() or FinishOpenGpFifoEntry() updates the get/gpget
        // semaphores.
        //
        m_pSavedLwrrent = nullptr;

        // Flush if possible, to guarantee that space will become
        // available eventually.
        //
        if (m_AutoFlushEnable)
        {
            CHECK_RC(Flush());
        }
        else
        {
            CHECK_RC(FinishOpenGpFifoEntry(nullptr));
        }
    }

    // Wrap if needed
    //
    if (mustWrap)
    {
        // Check whether Flush() or FinishOpenGpFifoEntry() already wrapped
        const UINT64 oldPut = lwrrentPut;
        lwrrentPut = reinterpret_cast<UINT08*>(m_pLwrrent) - m_pPushBufferBase;
        if (lwrrentPut >= oldPut)
        {
            CHECK_RC(Wrap(writeSize));
        }
    }
    lwrrentPut = reinterpret_cast<UINT08*>(m_pLwrrent) - m_pPushBufferBase;

    // Wait for enough room for put to not overrun get.
    //
    CHECK_RC(WaitForRoom(lwrrentPut, lwrrentPut + writeSize));
    return OK;
}

// Return the number of extra bytes written to the pushbuffer when we
// write a new GpFifo entry (e.g. during a Flush)
//
UINT32 GpFifoChannel::GetFlushSize() const
{
    // Keep room for NOP to update the GPGet pointer
    //
    UINT32 flushSize = sizeof(UINT32);

    // Add space for the get and/or gpget semaphores
    //
    AtomChannelWrapper *pAtomWrap = GetWrapper()->GetAtomChannelWrapper();
    bool isAtomExelwtion = pAtomWrap == NULL ? false : pAtomWrap->IsAtomExelwtion();
    if (m_bUseGetSemaphore && !isAtomExelwtion)
    {
        flushSize += GetHostSemaphoreSize();
    }
    if (m_bUseGpGetSemaphore && !isAtomExelwtion)
    {
        flushSize += GetHostSemaphoreSize();
    }

    return flushSize;
}

RC GpFifoChannel::AutoFlushIfNeeded()
{
    RC rc;

    if (m_AutoFlushEnable && (m_pLwrrent >= m_pNextAutoFlush))
    {
        CHECK_RC(GetWrapper()->Flush());
    }
    else if (m_AutoGpEntryEnable && (m_pLwrrent >= m_pNextAutoGpEntry))
    {
        CHECK_RC(FinishOpenGpFifoEntry(nullptr));
    }
    return rc;
}

bool GpFifoChannel::GetPrivEnable() const
{
    return m_PrivEnable;
}

RC GpFifoChannel::WriteGpEntryData(UINT32 GpEntry0, UINT32 GpEntry1)
{
    RC rc;

    UINT32 NextGpPut;

    // We assume m_GpFifoEntries is a power of two, as required by HW.
    NextGpPut = (m_LwrrentGpPut + 1) & (m_GpFifoEntries - 1);

    // Don't overwrite an unflushed-but-previously-used gpfifo entry.
    if (NextGpPut == m_CachedGpPut)
    {
        // The test has used all the entries in the GPFIFO since the last flush.
        // It must either enable autoflush, increase gpfifo size, or add calls
        // to Flush() more often in its code.

        // If the caller isn't checking every return code from ::Write,
        // we might drop methods silently, leading to very confusing failures.
        // Fake an RM robust-channels error here, so that this Channel will
        // persistently fail from now on.
        SetError(RC::RM_RCH_FIFO_ERROR_RUNOUT_OVERFLOW);

        return RC::PUSHBUFFER_TOO_SMALL;
    }

    // Are any gpus still reading the entry we are about to overwrite?
    bool anyGpFifoEntriesAvailable = true;
    UINT32 i;
    for (i = 0; i <= m_PollArgs.LastGetIdx; i++)
    {
        if (m_CachedGpGet[i] == NextGpPut)
            anyGpFifoEntriesAvailable = false;
    }
    if (!anyGpFifoEntriesAvailable)
    {
        if (m_AutoFlushEnable)
        {
            // Make sure that the GPU is processing the gpfifo buffer, so the
            // wait below isn't infinite.
            CHECK_RC(WriteGpPut());
        }
        // Wait for all gpus to advance their GpGet past the entry.
        m_PollArgs.GpPut = NextGpPut;
        CHECK_RC(POLLWRAP_HW(&PollGpFifoFull, &m_PollArgs, m_TimeoutMs));
        CHECK_RC(CheckError());
    }

    // Write a new entry to the GPFIFO
    if (m_EnableLogging &&
        (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK))
    {
        Printf(Tee::PriNormal, "DEV_%x_CH_%x GpEntryData: 0x%08x 0x%08x\n",
                    m_DevInst, m_ChID, GpEntry0, GpEntry1);
    }
    UINT32 *pGpLwrrent = (UINT32 *)(m_pGpFifoBase +
        m_LwrrentGpPut*LW906F_GP_ENTRY__SIZE);
    MEM_WR32(&pGpLwrrent[0], GpEntry0);
    MEM_WR32(&pGpLwrrent[1], GpEntry1);

    // Advance the sw GPFIFO pointer
    m_LwrrentGpPut = NextGpPut;

    m_pNextAutoGpEntry = (UINT32 *)((UINT08 *)m_pLwrrent +
                                    GetAutoGpEntryThreshold());

    if (m_AutoFlushEnable)
    {
        //  (1) if (hwPut == swPut), numToFlush is 0 (*)
        //  (2) if (hwPut  < swPut), numToFlush is        swPut-hwPut.
        //  (3) if (hwPut  > swPut), numToFlush is gpSize+swPut-hwPut.
        //
        // So, (gpSize+swPut-hwPut)%gpSize works in all cases.
        //
        // (*) Can't get here, we just incremented m_LwrrentGpPut.
        //
        UINT32 numToFlush = (m_GpFifoEntries + m_LwrrentGpPut - m_CachedGpPut)
                            % m_GpFifoEntries;

        if (numToFlush >= GetAutoFlushThresholdGpFifoEntries())
        {
            CHECK_RC(WriteGpPut());
        }
    }

    return OK;
}

/* virtual */ RC GpFifoChannel::WritePbCrcCheckGpEntry
(
    const void *pGpEntryData,
    UINT32 Length
)
{
    // Nothing need to be done for GpFifo channel
    return OK;
}

/* virtual */ RC GpFifoChannel::ConstructGpEntryData
(
    UINT64 Get,
    UINT32 Length,
    bool Subroutine,
    UINT32* pGpEntry0,
    UINT32* pGpEntry1
)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ UINT32 GpFifoChannel::GetGpEntry1LengthMask() const
{
    return DRF_MASK(LW906F_GP_ENTRY1_LENGTH);
}

void GpFifoChannel::PauseBeforeGpPutUpdate()
{
    if (m_pPauseBeforeGPPutWrite != NULL)
    {
        UINT32 PauseBeforeWrite =
            m_pPauseBeforeGPPutWrite->GetRandom(
                m_PauseBeforeGPPutWriteMin,
                m_PauseBeforeGPPutWriteMax);
        for (UINT32 PauseIdx = 0; PauseIdx < PauseBeforeWrite;
             PauseIdx++)
             Platform::Pause();
    }
}

void GpFifoChannel::PauseAfterGpPutUpdate()
{
    if (m_pPauseAfterGPPutWrite != NULL)
    {
        UINT32 PauseAfterWrite =
            m_pPauseAfterGPPutWrite->GetRandom(
            m_PauseAfterGPPutWriteMin, m_PauseAfterGPPutWriteMax);
        for (UINT32 PauseIdx = 0; PauseIdx < PauseAfterWrite;
             PauseIdx++)
             Platform::Pause();
    }
}

//--------------------------------------------------------------------
//! Callwlate a value for m_AutoFlushThresholdGpFifoEntries, which
//! determines how many GPFIFO entries should be written before we
//! automatically flush.  The value is based on the AutoFlush and
//! AutoGpEntry values.
//!
//! Called by SetAutoFlushEnable() and SetAutoGpEntryEnable()
//!
UINT32 GpFifoChannel::CalcAutoFlushThresholdGpFifoEntries()
{
    UINT32 Threshold = 0;

    if (m_AutoGpEntryEnable)
    {
        // If AutoGpEntry is enabled, then set the threshold for
        // GPFIFO entries proportionately to the ratio of AutoFlush /
        // AutoGpEntry.
        //
        Threshold = (GetAutoFlushThreshold() / GetAutoGpEntryThreshold()) + 1;
    }
    else
    {
        // Otherwise, set the autoflush threshold for GPFIFO entries
        // proportionately to the threshold for the pushbuffer itself.
        //
        double f = double(GetAutoFlushThreshold()) / double(m_PushBufferSize);
        Threshold = (UINT32)(0.5 + m_GpFifoEntries * f);
    }

    // Make sure that 1 <= threshold < buffer_size
    //
    Threshold = max(Threshold, (UINT32)1);
    Threshold = min(Threshold, m_GpFifoEntries - 1);

    return Threshold;
}

//--------------------------------------------------------------------
//! Randomize after how many bytes written to pushbuffer it is
//! autoflushed.
//!
//! Units: bytes
//!
RC GpFifoChannel::SetRandomAutoFlushThreshold
(
    const MinMaxSeed &minmaxseed
)
{
    if (m_pAutoFlushThreshold == NULL)
        m_pAutoFlushThreshold = new Random();

    m_pAutoFlushThreshold->SeedRandom(minmaxseed.Seed);
    m_AutoFlushThresholdMin = minmaxseed.Min;
    m_AutoFlushThresholdMax = minmaxseed.Max;

    return OK;
}

//--------------------------------------------------------------------
//! Randomize after how GPFifoEntries are written to pushbuffer it is
//! autoflushed.
//!
//! Units: number of GpFifoEntries
//!
RC GpFifoChannel::SetRandomAutoFlushThresholdGpFifoEntries
(
    const MinMaxSeed &minmaxseed
)
{
    if (m_pAutoFlushThresholdGpFifoEntries == NULL)
        m_pAutoFlushThresholdGpFifoEntries = new Random();

    m_pAutoFlushThresholdGpFifoEntries->SeedRandom(minmaxseed.Seed);
    m_AutoFlushThresholdGpFifoEntriesMin = minmaxseed.Min;
    m_AutoFlushThresholdGpFifoEntriesMax = minmaxseed.Max;

    return OK;
}

//--------------------------------------------------------------------
//! Randomize after how many bytes written to pushbuffer current
//! GPFifoEntry is closed.
//!
//! Units: bytes
//!
RC GpFifoChannel::SetRandomAutoGpEntryThreshold
(
    const MinMaxSeed &minmaxseed
)
{
    if (m_pAutoGpEntryThreshold == NULL)
        m_pAutoGpEntryThreshold = new Random();

    m_pAutoGpEntryThreshold->SeedRandom(minmaxseed.Seed);
    m_AutoGpEntryThresholdMin = minmaxseed.Min;
    m_AutoGpEntryThresholdMax = minmaxseed.Max;

    return OK;
}

//--------------------------------------------------------------------
//! Add random delay just before writing GPPut value to hw.
//!
//! Units: number of Platform::Pause calls
//!
RC GpFifoChannel::SetRandomPauseBeforeGPPutWrite
(
    const MinMaxSeed &minmaxseed
)
{
    if (m_pPauseBeforeGPPutWrite == NULL)
        m_pPauseBeforeGPPutWrite = new Random();

    m_pPauseBeforeGPPutWrite->SeedRandom(minmaxseed.Seed);
    m_PauseBeforeGPPutWriteMin = minmaxseed.Min;
    m_PauseBeforeGPPutWriteMax = minmaxseed.Max;

    return OK;
}

//--------------------------------------------------------------------
//! Add random delay just after writing GPPut value to hw.
//!
//! Units: number of Platform::Pause calls
//!
RC GpFifoChannel::SetRandomPauseAfterGPPutWrite
(
    const MinMaxSeed &minmaxseed
)
{
    if (m_pPauseAfterGPPutWrite == NULL)
        m_pPauseAfterGPPutWrite = new Random();

    m_pPauseAfterGPPutWrite->SeedRandom(minmaxseed.Seed);
    m_PauseAfterGPPutWriteMin = minmaxseed.Min;
    m_PauseAfterGPPutWriteMax = minmaxseed.Max;

    return OK;
}

UINT32 GpFifoChannel::GetAutoFlushThreshold()
{
    if (m_pAutoFlushThreshold == NULL)
        return m_AutoFlushThresholdMax;

    return m_pAutoFlushThreshold->GetRandom
        (
            m_AutoFlushThresholdMin,
            m_AutoFlushThresholdMax
        );
}

UINT32 GpFifoChannel::GetAutoFlushThresholdGpFifoEntries()
{
    if (m_pAutoFlushThresholdGpFifoEntries == NULL)
        return m_AutoFlushThresholdGpFifoEntriesMax;

    return m_pAutoFlushThresholdGpFifoEntries->GetRandom
        (
            m_AutoFlushThresholdGpFifoEntriesMin,
            m_AutoFlushThresholdGpFifoEntriesMax
        );
}

UINT32 GpFifoChannel::GetAutoGpEntryThreshold()
{
    if (m_pAutoGpEntryThreshold == NULL)
        return m_AutoGpEntryThresholdMax;

    return m_pAutoGpEntryThreshold->GetRandom
        (
            m_AutoGpEntryThresholdMin,
            m_AutoGpEntryThresholdMax
        );
}

RC GpFifoChannel::RecoverFromRobustChannelError()
{
    ClearPushbuffer();
    return BaseChannel::RecoverFromRobustChannelError();
}

RC GpFifoChannel::SetupGetSemaphore()
{
    if (m_bUseGetSemaphore)
    {
        return OK;
    }

    m_GetSemaphore.SetWidth(s_SemaphoreDwords * GetGpuDevice()->GetNumSubdevices());
    m_GetSemaphore.SetHeight(1);
    m_GetSemaphore.SetPageSize(4);
    m_GetSemaphore.SetColorFormat(ColorUtils::VOID32);
    // In CCMode all driver internal allocations need to be in FB.
    if (Platform::IsCCMode())
    {
        m_GetSemaphore.SetLocation(Memory::Fb);
    }
    else
    {
        m_GetSemaphore.SetLocation(Memory::NonCoherent);
    }
    m_GetSemaphore.SetGpuVASpace(m_hVASpace);
    m_GetSemaphore.SetVASpace(Surface2D::GPUVASpace);
    m_GetSemaphore.SetVaReverse(((GpuDevMgr*) DevMgrMgr::d_GraphDevMgr)->GetVasReverse());

    RC rc;
    CHECK_RC(m_GetSemaphore.Alloc(GetGpuDevice(), GetRmClient()));
    CHECK_RC(m_GetSemaphore.BindGpuChannel(m_hChannel, GetRmClient()));
    CHECK_RC(m_GetSemaphore.Map());
    CHECK_RC(m_GetSemaphore.Fill(0));

    m_bUseGetSemaphore = true;

    return OK;
}

RC GpFifoChannel::SetupGpGetSemaphore()
{
    if (m_bUseGpGetSemaphore)
    {
        return OK;
    }

    RC rc;
    m_GpGetSemaphore.SetWidth(s_SemaphoreDwords * GetGpuDevice()->GetNumSubdevices());
    m_GpGetSemaphore.SetHeight(1);
    m_GpGetSemaphore.SetColorFormat(ColorUtils::VOID32);
    // In CCMode all driver internal allocations need to be in FB.
    if (Platform::IsCCMode())
    {
        m_GpGetSemaphore.SetLocation(Memory::Fb);
    }
    else
    {
        m_GpGetSemaphore.SetLocation(Memory::NonCoherent);
    }
    m_GpGetSemaphore.SetGpuVASpace(m_hVASpace);
    m_GpGetSemaphore.SetVaReverse(((GpuDevMgr*) DevMgrMgr::d_GraphDevMgr)->GetVasReverse());
    CHECK_RC(m_GpGetSemaphore.Alloc(GetGpuDevice(), GetRmClient()));
    CHECK_RC(m_GpGetSemaphore.BindGpuChannel(m_hChannel, GetRmClient()));
    CHECK_RC(m_GpGetSemaphore.Map());
    CHECK_RC(m_GpGetSemaphore.Fill(0));

    m_bUseGpGetSemaphore = true;

    return rc;
}

bool GpFifoChannel::CanUseGetGpGetSemaphore() const
{
    // Skip using GpGet/Get Semaphore for simMods
    if (Xp::GetOperatingSystem() == Xp::OS_LINUXSIM ||
        Xp::GetOperatingSystem() == Xp::OS_WINSIM)
    {
        return false;
    }
    return true;
}
