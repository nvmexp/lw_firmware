/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2019, 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Channel wrapper interface.

#include "semawrap.h"
#include <algorithm>
#include "class/cl0000.h"  // LW01_NULL_OBJECT
#include "class/cl206e.h"  // LW20_CHANNEL_DMA
#include "class/cl366e.h"  // LW36_CHANNEL_DMA
#include "class/cl406e.h"  // LW40_CHANNEL_DMA
#include "class/cl446e.h"  // LW44_CHANNEL_DMA
#include "class/cl506f.h"  // LW50_CHANNEL_GPFIFO
#include "class/cl826f.h"  // G82_CHANNEL_GPFIFO
#include "class/clc36f.h"  // VOLTA_CHANNEL_GPFIFO
#include "class/cl9097.h"  // FERMI_A
#include "class/cl90b5.h"  // GF100_DMA_COPY
#include "class/cl90b7.h"  // LW90B7_VIDEO_ENCODER
#include "class/cla0b0.h"  // LWA0B0_VIDEO_DECODER
#include "class/cl95a1.h"  // LW95A1_TSEC
#include "class/cl90c0.h"  // FERMI_COMPUTE_A
#include "class/cla297.h"  // KEPLER_C
#include "class/clc4d1.h" // LWC4D1_VIDEO_LWJPG
#include "class/clc6fa.h" // LWC6FA_VIDEO_OFA
#include "class/clc7c0.h" // AMPERE_COMPUTE_B
#include "class/clc797.h" // AMPERE_B
#include "class/clc7b5.h" // AMPERE_DMA_COPY_B
#include "class/clc7b7.h" // LWC7B7_VIDEO_ENCODER
#include "class/clc7b0.h" // LWC7B0_VIDEO_DECODER
#include "class/clc7fa.h" // LWC7FA_VIDEO_OFA

#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/utility.h"
#include "chanwmgr.h"
#include "atomwrap.h"

//! Methods below MIN_ENGINE_METHOD are host methods, methods >=
//! MIN_ENGINE_METHOD are engine methods.
//!
static const UINT32 MIN_ENGINE_METHOD = 0x100;

//! Allocators that indicate which GPU classes are supported by this
//! class.  Unless otherwise indicated, all SemaphoreChannelWrapper
//! methods support fermi on up.
//!
static ThreeDAlloc s_ThreeDClasses;
static ComputeAlloc s_ComputeClasses;
static ComputeAlloc s_ComputeClassesAmpereB;
static DmaCopyAlloc s_DmaCopyClasses;
static EncoderAlloc s_EncoderClasses;
static LwdecAlloc s_LwdecClasses;
static TsecAlloc s_TsecClasses;
static LwjpgAlloc s_LwjpgClasses;
static OfaAlloc s_OfaClasses;

static void InitStaticGrallocs()
{
    static bool initialized = false;
    if (!initialized)
    {
        s_ThreeDClasses.SetOldestSupportedClass(FERMI_A);
        s_ComputeClasses.SetOldestSupportedClass(FERMI_COMPUTE_A);
        s_ComputeClassesAmpereB.SetOldestSupportedClass(AMPERE_COMPUTE_B);
        s_DmaCopyClasses.SetOldestSupportedClass(GF100_DMA_COPY);
        s_EncoderClasses.SetOldestSupportedClass(LW90B7_VIDEO_ENCODER);
        initialized = true;
    }
}

//--------------------------------------------------------------------
//! Colwenience method used to generate the subchannel-mask used by
//! CleanupDirtySemaphoreState() and SemData::IsDirty().  Returns a
//! bit-mask of the subchannels that receive engine methods from a
//! given Subch/Method pair.  Returns 0 for host methods.
//!
static inline UINT32 GetSubchMask(UINT32 Subch, UINT32 Method)
{
    return (Method >= MIN_ENGINE_METHOD) ? (1 << Subch) : 0;
}

//--------------------------------------------------------------------
//! \brief constructor
//!
SemaphoreChannelWrapper::SemaphoreChannelWrapper
(
    Channel *pChannel
) :
    ChannelWrapper(pChannel),
    m_CleaningDirtySemaphoreState(false),
    m_CleaningDirtySubchMask(0)
{
    GpuDevice *pGpuDevice = pChannel->GetGpuDevice();
    UINT32 Put;

    MASSERT(IsSupported(pChannel));

    m_SupportsBackend = (pChannel->RmcGetClassEngineId(0, 0, 0, 0) !=
                         RC::UNSUPPORTED_FUNCTION);

    if (m_pWrappedChannel->GetGpPut(&Put) == OK)
    {
        m_ChannelType = GPFIFO_CHANNEL;
        m_SupportsHost = true;
    }
    else if (m_pWrappedChannel->GetPut(&Put) == OK)
    {
        m_ChannelType = DMA_CHANNEL;
        m_SupportsHost = true;
    }
    else
    {
        m_ChannelType = PIO_CHANNEL;
        m_SupportsHost = false;
    }

    m_AllSubdevMask = (1 << pGpuDevice->GetNumSubdevices()) - 1;

    SemState baseSemState;
    baseSemState.SetSubdevMask(m_AllSubdevMask);
    baseSemState.SetSemReleaseFlags(pChannel->GetSemaphoreReleaseFlags());
    m_SemStateStack.push(baseSemState);

    for (UINT32 ii = 0; ii < NumSubchannels; ++ii)
    {
        m_SubchObject[ii] = 0;
        m_SubchClass[ii]  = LW01_NULL_OBJECT;
        m_SubchEngine[ii] = LW2080_ENGINE_TYPE_NULL;
    }

    InitStaticGrallocs();
}

//--------------------------------------------------------------------
//! \brief Tell whether SemaphoreChannelWrapper supports the indicated channel
//!
//! If this static method returns false, you should not try to
//! construct a SemaphoreChannelWrapper around the indicated channel.
//!
bool SemaphoreChannelWrapper::IsSupported(Channel *pChannel)
{
    UINT32 Put;

    if (pChannel->GetGpPut(&Put) == OK)
    {
        // SemaphoreChannelWrapper is supported on all GpFifos
        return true;
    }
    else if (pChannel->GetPut(&Put) == OK)
    {
        // These are the only DMA channels that support semaphores.
        switch (pChannel->GetClass())
        {
            case LW20_CHANNEL_DMA:
            case LW36_CHANNEL_DMA:
            case LW40_CHANNEL_DMA:
            case LW44_CHANNEL_DMA:
                return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------
//! \brief Return the last subchannel written to, or 0 if none
//!
//! GetLwrrentSubchannel() returns the subchannel that was used most
//! recently.  A subchannel is considered to be "used" if it has a
//! non-host method written on it, or if it's used for a SetObject().
//!
UINT32 SemaphoreChannelWrapper::GetLwrrentSubchannel() const
{
    return m_SubchHistory.empty() ? 0 : m_SubchHistory[0];
}

//--------------------------------------------------------------------
//! \brief Write methods to release a backend engine semaphore
//!
//! \param gpuAddr The GPU address of the semaphore
//! \param payload The semaphore payload
//! \param withTimer If \a false, the semaphore is 32-bit. If \a true,
//!                  the semaphore is 128-bit and contains a 64-bit value
//!                  which indicates when the semaphore was released.
//! \param allowHostSemaphore If true, this method is allowed to
//!     release a host semaphore instead of a backend, if no
//!     subchannels support a backend semaphore on the current engine.
//! \param[out] pEngine On success, the engine used to write the
//!     semaphore.  May be NULL if the caller doesn't need this data.
//!
RC SemaphoreChannelWrapper::ReleaseBackendSemaphore
(
    UINT64 gpuAddr,
    UINT64 payload,
    bool allowHostSemaphore,
    UINT32 *pEngine
)
{
    static const GrClassAllocator *supportedGrallocs[] =
    {
        &s_ThreeDClasses,
        &s_ComputeClasses,
        &s_DmaCopyClasses,
        &s_EncoderClasses,
        &s_LwdecClasses,
        &s_TsecClasses,
        &s_LwjpgClasses,
        &s_OfaClasses
    };

    Channel *pCh = GetWrapper();
    UINT32 flags = pCh->GetSemaphoreReleaseFlags();
    UINT32 classID;
    UINT32 subch;
    RC rc;

    CHECK_RC(FindSupportedClass(
        supportedGrallocs,
        static_cast<UINT32>(NUMELEMS(supportedGrallocs)),
        allowHostSemaphore, &classID, &subch));
    if (s_ThreeDClasses.HasClass(classID))
    {
        if (s_ThreeDClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec797Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(C797, _REPORT_SEMAPHORE_EXELWTE,
                                                _OPERATION, _RELEASE)));
        }
        else
        {
            CHECK_RC(Write9097Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D,
                                                _OPERATION, _RELEASE)));
        }
    }
    else if (s_ComputeClasses.HasClass(classID))
    {
        if (s_ComputeClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec7c0Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(C7C0, _REPORT_SEMAPHORE_EXELWTE,
                                                _OPERATION, _RELEASE)));
        }
        else
        {
            CHECK_RC(Write90c0Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(90C0, _SET_REPORT_SEMAPHORE_D,
                                                _OPERATION, _RELEASE)));
        }
    }
    else if (s_DmaCopyClasses.HasClass(classID))
    {
        if (s_DmaCopyClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec7b5Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(C7B5, _LAUNCH_DMA,
                                                _INTERRUPT_TYPE, _NONE)));
        }
        else
        {
            CHECK_RC(Write90b5Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(90B5, _LAUNCH_DMA,
                                                _INTERRUPT_TYPE, _NONE)));
        }
    }
    else if (s_EncoderClasses.HasClass(classID))
    {
        if (s_EncoderClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec7b7Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(C7B7, _SEMAPHORE_D,
                                                _OPERATION, _RELEASE)));
        }
        else
        {
            CHECK_RC(Write90b7Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(90B7, _SEMAPHORE_D,
                                                _OPERATION, _RELEASE)));
        }
    }
    else if (s_LwdecClasses.HasClass(classID))
    {
        if (s_LwdecClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec7b0Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(C7B0, _SEMAPHORE_D,
                                                _OPERATION, _RELEASE)));
        }
        else
        {
            CHECK_RC(WriteA0b0Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(A0B0, _SEMAPHORE_D,
                                                _OPERATION, _RELEASE)));
        }
    }
    else if (s_TsecClasses.HasClass(classID))
    {
        CHECK_RC(Write95a1Semaphore(flags, subch, gpuAddr, payload,
                                    DRF_DEF(95A1, _SEMAPHORE_D,
                                            _OPERATION, _RELEASE)));
    }
    else if (s_LwjpgClasses.HasClass(classID))
    {
        CHECK_RC(Writec4d1Semaphore(flags, subch, gpuAddr, payload,
                                    DRF_DEF(C4D1, _SEMAPHORE_D,
                                            _OPERATION, _RELEASE)));
    }
    else if (s_OfaClasses.HasClass(classID))
    {
        if (s_OfaClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec7faSemaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(C7FA, _SEMAPHORE_D,
                                                _OPERATION, _RELEASE)));
        }
        else
        {
            CHECK_RC(Writec6faSemaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(C6FA, _SEMAPHORE_D,
                                                _OPERATION, _RELEASE)));
        }
    }
    else if (classID == GetClass())
    {
        // Release a host semaphore
        CHECK_RC(SetSemaphoreOffset(gpuAddr));
        CHECK_RC(SemaphoreRelease(payload));
    }
    else
    {
        MASSERT(!"Illegal class in ReleaseBackendSemaphore");
    }

    Utility::SetOptionalPtr(pEngine, GetEngine(subch));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write methods to release a backend engine semaphore and
//! raise a backend non-stalling int.
//!
//! \param gpuAddr The GPU address of the semaphore
//! \param payload The semaphore payload
//! \param allowHostSemaphore If true, this method is allowed to
//!     release a host semaphore instead of a backend, if no
//!     subchannels support a backend semaphore on the current engine.
//! \param[out] pEngine On success, the class used to write the
//!     semaphore.  May be NULL if the caller doesn't need this data.
//!
RC SemaphoreChannelWrapper::ReleaseBackendSemaphoreWithTrap
(
    UINT64 gpuAddr,
    UINT64 payload,
    bool allowHostSemaphore,
    UINT32 *pEngine
)
{
    static const GrClassAllocator *supportedGrallocs[] =
    {
        &s_ThreeDClasses,
        &s_ComputeClasses,
        &s_DmaCopyClasses,
        &s_EncoderClasses,
        &s_LwdecClasses,
        &s_TsecClasses,
        &s_LwjpgClasses,
        &s_OfaClasses,
    };

    Channel *pCh = GetWrapper();
    UINT32 flags = pCh->GetSemaphoreReleaseFlags();
    UINT32 classID;
    UINT32 subch;
    RC rc;

    CHECK_RC(FindSupportedClass(
        supportedGrallocs,
        static_cast<UINT32>(NUMELEMS(supportedGrallocs)),
        allowHostSemaphore, &classID, &subch));
    if (s_ThreeDClasses.HasClass(classID))
    {
        if (s_ThreeDClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec797Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(C797, _REPORT_SEMAPHORE_EXELWTE,
                                                _OPERATION, _RELEASE)));
            CHECK_RC(pCh->Write(subch, LWC797_REPORT_SEMAPHORE_EXELWTE,
                                DRF_DEF(C797, _REPORT_SEMAPHORE_EXELWTE,
                                        _OPERATION, _TRAP) |
                                DRF_DEF(C797, _REPORT_SEMAPHORE_EXELWTE,
                                        _TRAP_TYPE, _TRAP_UNCONDITIONAL)));
        }
        else
        {
            CHECK_RC(Write9097Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D,
                                                _OPERATION, _RELEASE)));
            CHECK_RC(pCh->Write(subch, LW9097_SET_REPORT_SEMAPHORE_D,
                                DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D,
                                        _OPERATION, _TRAP)));
        }
    }
    else if (s_ComputeClasses.HasClass(classID))
    {
        if (s_ComputeClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec7c0Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(C7C0, _REPORT_SEMAPHORE_EXELWTE,
                                                _OPERATION, _RELEASE)));
            CHECK_RC(pCh->Write(subch, LWC7C0_REPORT_SEMAPHORE_EXELWTE,
                                DRF_DEF(C7C0, _REPORT_SEMAPHORE_EXELWTE,
                                        _OPERATION, _TRAP) |
                                DRF_DEF(C7C0, _REPORT_SEMAPHORE_EXELWTE,
                                        _TRAP_TYPE, _TRAP_UNCONDITIONAL)));
        }
        else
        {
            CHECK_RC(Write90c0Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(90C0, _SET_REPORT_SEMAPHORE_D,
                                                _OPERATION, _RELEASE)));
            CHECK_RC(pCh->Write(subch, LW90C0_SET_REPORT_SEMAPHORE_D,
                                DRF_DEF(90C0, _SET_REPORT_SEMAPHORE_D,
                                        _OPERATION, _TRAP)));
        }
    }
    else if (s_DmaCopyClasses.HasClass(classID))
    {
        if (s_DmaCopyClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec7b5Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(C7B5, _LAUNCH_DMA,
                                                _INTERRUPT_TYPE, _NON_BLOCKING)));
        }
        else
        {
            CHECK_RC(Write90b5Semaphore(flags, subch, gpuAddr, payload,
                                        DRF_DEF(90B5, _LAUNCH_DMA,
                                                _INTERRUPT_TYPE, _NON_BLOCKING)));
        }
    }
    else if (s_EncoderClasses.HasClass(classID))
    {
        if (s_EncoderClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec7b7Semaphore(
                         flags, subch, gpuAddr, payload,
                         DRF_DEF(C7B7, _SEMAPHORE_D, _OPERATION, _RELEASE)));
            CHECK_RC(pCh->Write(subch, LWC7B7_SEMAPHORE_D,
                                DRF_DEF(C7B7, _SEMAPHORE_D, _OPERATION, _TRAP)));
        }
        else
        {
            CHECK_RC(Write90b7Semaphore(
                         flags, subch, gpuAddr, payload,
                         DRF_DEF(90B7, _SEMAPHORE_D, _OPERATION, _RELEASE)));
            CHECK_RC(pCh->Write(subch, LW90B7_SEMAPHORE_D,
                                DRF_DEF(90B7, _SEMAPHORE_D, _OPERATION, _TRAP)));
        }
    }
    else if (s_LwdecClasses.HasClass(classID))
    {
        if (s_LwdecClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec7b0Semaphore(
                         flags, subch, gpuAddr, payload,
                         DRF_DEF(C7B0, _SEMAPHORE_D, _OPERATION, _RELEASE)));
            CHECK_RC(pCh->Write(subch, LWC7B0_SEMAPHORE_D,
                                DRF_DEF(C7B0, _SEMAPHORE_D, _OPERATION, _TRAP) | 
                                DRF_DEF(C7B0, _SEMAPHORE_D, _TRAP_TYPE, _UNCONDITIONAL)));
        }
        else
        {
            CHECK_RC(WriteA0b0Semaphore(
                         flags, subch, gpuAddr, payload,
                         DRF_DEF(A0B0, _SEMAPHORE_D, _OPERATION, _RELEASE)));
            CHECK_RC(pCh->Write(subch, LWA0B0_SEMAPHORE_D,
                                DRF_DEF(A0B0, _SEMAPHORE_D, _OPERATION, _TRAP)));
        }
    }
    else if (s_TsecClasses.HasClass(classID))
    {
        CHECK_RC(Write95a1Semaphore(
                     flags, subch, gpuAddr, payload,
                     DRF_DEF(95A1, _SEMAPHORE_D, _OPERATION, _RELEASE)));
        CHECK_RC(pCh->Write(subch, LW95A1_SEMAPHORE_D,
                     DRF_DEF(95A1, _SEMAPHORE_D, _OPERATION, _TRAP)));
    }
    else if (s_LwjpgClasses.HasClass(classID))
    {
        CHECK_RC(Writec4d1Semaphore(
                     flags, subch, gpuAddr, payload,
                     DRF_DEF(C4D1, _SEMAPHORE_D, _OPERATION, _RELEASE)));
        CHECK_RC(pCh->Write(subch, LWC4D1_SEMAPHORE_D,
                     DRF_DEF(C4D1, _SEMAPHORE_D, _OPERATION, _TRAP)));
    }
    else if (s_OfaClasses.HasClass(classID))
    {
        if (s_OfaClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec7faSemaphore(
                         flags, subch, gpuAddr, payload,
                         DRF_DEF(C7FA, _SEMAPHORE_D, _OPERATION, _RELEASE)));
            CHECK_RC(pCh->Write(subch, LWC7FA_SEMAPHORE_D,
                         DRF_DEF(C7FA, _SEMAPHORE_D, _OPERATION, _TRAP) |
                         DRF_DEF(C7FA, _SEMAPHORE_D, _TRAP_TYPE, _UNCONDITIONAL)));
        }
        else
        {
            CHECK_RC(Writec6faSemaphore(
                         flags, subch, gpuAddr, payload,
                         DRF_DEF(C6FA, _SEMAPHORE_D, _OPERATION, _RELEASE)));
            CHECK_RC(pCh->Write(subch, LWC6FA_SEMAPHORE_D,
                         DRF_DEF(C6FA, _SEMAPHORE_D, _OPERATION, _TRAP)));
        }
    }
    else if (classID == GetClass())
    {
        // Release a host semaphore
        CHECK_RC(SetSemaphoreOffset(gpuAddr));
        CHECK_RC(SemaphoreRelease(payload));
        CHECK_RC(NonStallInterrupt());
    }
    else
    {
        MASSERT(!"Illegal class in ReleaseBackendSemaphoreWithTrap");
    }

    Utility::SetOptionalPtr(pEngine, GetEngine(subch));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write methods to acquire a backend engine semaphore
//!
//! \param gpuAddr The GPU address of the semaphore
//! \param payload The semaphore payload
//! \param allowHostSemaphore If true, this method is allowed to
//!     acquire a host semaphore instead of a backend, if no
//!     subchannels support a backend semaphore on the current engine.
//! \param[out] pEngine On success, the engine used to write the
//!     semaphore.  May be NULL if the caller doesn't need this data.
//!
RC SemaphoreChannelWrapper::AcquireBackendSemaphore
(
    UINT64 gpuAddr,
    UINT64 payload,
    bool allowHostSemaphore,
    UINT32 *pEngine
)
{
    static const GrClassAllocator *supportedGrallocs[] =
    {
        &s_ThreeDClasses,
        &s_ComputeClassesAmpereB
    };

    UINT32 classID;
    UINT32 subch;
    RC rc;

    CHECK_RC(FindSupportedClass(
        supportedGrallocs,
        static_cast<UINT32>(NUMELEMS(supportedGrallocs)),
        allowHostSemaphore, &classID, &subch));
    if (s_ThreeDClasses.HasClass(classID))
    {
        if (s_ThreeDClasses.Has64BitSemaphore(classID))
        {
            CHECK_RC(Writec797Semaphore(0, subch, gpuAddr, payload,
                                        DRF_DEF(C797, _REPORT_SEMAPHORE_EXELWTE,
                                                _OPERATION, _ACQUIRE) |
                                        DRF_DEF(C797, _REPORT_SEMAPHORE_EXELWTE,
                                                _PIPELINE_LOCATION, _ALL)));
        }
        else
        {
            CHECK_RC(Write9097Semaphore(0, subch, gpuAddr, payload,
                                        DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D,
                                                _OPERATION, _ACQUIRE) |
                                        DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D,
                                                _PIPELINE_LOCATION, _ALL)));
        }
    }
    else if (s_ComputeClassesAmpereB.HasClass(classID))
    {
        CHECK_RC(Writec7c0Semaphore(0, subch, gpuAddr, payload,
                                    DRF_DEF(C7C0, _REPORT_SEMAPHORE_EXELWTE,
                                            _OPERATION, _ACQUIRE)));
    }
    else if (classID == GetClass())
    {
        // Acquire a host semaphore
        CHECK_RC(SetSemaphoreOffset(gpuAddr));
        CHECK_RC(SemaphoreAcquire(payload));
    }
    else
    {
        MASSERT(!"Illegal class in AcquireBackendSemaphore");
    }

    Utility::SetOptionalPtr(pEngine, GetEngine(subch));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a single method into the channel.
//!
/* virtual */ RC SemaphoreChannelWrapper::Write
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Data
)
{
    RC rc;

    CHECK_RC(UpdateSemaphoreState(Subchannel, Method, Data));
    CHECK_RC(m_pWrappedChannel->Write(Subchannel, Method, Data));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write multiple methods to the channel
//!
/* virtual */ RC SemaphoreChannelWrapper::Write
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    RC rc;

    for (UINT32 i = 0; i < Count; i++)
    {
        CHECK_RC(UpdateSemaphoreState(Subchannel, Method + (i * 4), pData[i]));
    }
    CHECK_RC(m_pWrappedChannel->Write(Subchannel, Method, Count, pData));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Write multiple non-incrementing methods to the channel
//!
/* virtual */ RC SemaphoreChannelWrapper::WriteNonInc
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    RC rc;

    CHECK_RC(UpdateSemaphoreState(Subchannel, Method, pData[Count-1]));
    CHECK_RC(m_pWrappedChannel->WriteNonInc(Subchannel,
                                            Method,
                                            Count,
                                            pData));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a header without data
//!
/* virtual */ RC SemaphoreChannelWrapper::WriteHeader
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count
)
{
    RC rc;

    CHECK_RC(CleanupDirtySemaphoreState(GetSubchMask(Subchannel, Method)));
    CHECK_RC(m_pWrappedChannel->WriteHeader(Subchannel, Method, Count));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a header without data
//!
/* virtual */ RC SemaphoreChannelWrapper::WriteNonIncHeader
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count
)
{
    RC rc;

    CHECK_RC(CleanupDirtySemaphoreState(GetSubchMask(Subchannel, Method)));
    CHECK_RC(m_pWrappedChannel->WriteNonIncHeader(Subchannel, Method, Count));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write increment-once method
//!
/* virtual */ RC SemaphoreChannelWrapper::WriteIncOnce
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    RC rc;

    CHECK_RC(UpdateSemaphoreState(Subchannel, Method, pData[0]));
    if (Count > 1)
        CHECK_RC(UpdateSemaphoreState(Subchannel, Method+4, pData[Count-1]));
    CHECK_RC(m_pWrappedChannel->WriteIncOnce(Subchannel, Method, Count, pData));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write immediate-data method
//!
/* virtual */ RC SemaphoreChannelWrapper::WriteImmediate
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Data
)
{
    RC rc;
    CHECK_RC(UpdateSemaphoreState(Subchannel, Method, Data));
    CHECK_RC(m_pWrappedChannel->WriteImmediate(Subchannel, Method, Data));
    return OK;
}

//--------------------------------------------------------------------
//! \brief Execute an arbitrary block of commands as as subroutine
//!
/* virtual */ RC SemaphoreChannelWrapper::CallSubroutine
(
    UINT64          Offset,
    UINT32          Size
)
{
    RC rc;

    // We don't know what's in the subroutine, so clean up all subchannels.
    //
    CHECK_RC(CleanupDirtySemaphoreState((1 << NumSubchannels) - 1));

    CHECK_RC(m_pWrappedChannel->CallSubroutine(Offset, Size));
    return OK;
}

//--------------------------------------------------------------------
//! \brief Execute an arbitrary block of commands as as subroutine
//!
/* virtual */ RC SemaphoreChannelWrapper::InsertSubroutine
(
    const UINT32 *pBuffer,
    UINT32        count
)
{
    RC rc;

    // We don't know what's in the subroutine, so clean up all subchannels.
    //
    CHECK_RC(CleanupDirtySemaphoreState((1 << NumSubchannels) - 1));

    CHECK_RC(m_pWrappedChannel->InsertSubroutine(pBuffer, count));
    return OK;
}

//--------------------------------------------------------------------
//! \brief Write subdevice mask
//!
/* virtual */ RC SemaphoreChannelWrapper::WriteSetSubdevice
(
    UINT32 Mask
)
{
    RC rc;
    CHECK_RC(CleanupDirtySemaphoreState(0));
    m_SemStateStack.top().SetSubdevMask((Mask == Channel::AllSubdevicesMask)
                                        ? m_AllSubdevMask
                                        : Mask);
    CHECK_RC(m_pWrappedChannel->WriteSetSubdevice(Mask));
    return rc;
}

//--------------------------------------------------------------------
//! Thin wrapper around Channel::SetObject that records the class and
//! engine that each subchannel is controlling.
//!
/* virtual */ RC SemaphoreChannelWrapper::SetObject
(
    UINT32 Subchannel,
    LwRm::Handle Handle
)
{
    RC rc;

    if (m_SupportsBackend)
    {
        UINT32 ClassId;
        UINT32 EngineId;

        CHECK_RC(RmcGetClassEngineId(Handle, NULL, &ClassId, &EngineId));
        m_SubchObject[Subchannel] = Handle;
        m_SubchClass[Subchannel]  = ClassId;
        m_SubchEngine[Subchannel] = EngineId;
        SetLwrrentSubchannel(Subchannel);
    }

    CHECK_RC(m_pWrappedChannel->SetObject(Subchannel, Handle));
    return rc;
}

//--------------------------------------------------------------------
//! Thin wrapper around Channel::UnsetObject that erases the
//! information set by SetObject().
//!
/* virtual */ RC SemaphoreChannelWrapper::UnsetObject(LwRm::Handle hObject)
{
    RC rc;

    // Erase the class/engine info on the subchannel, and remove the
    // subchannel from m_SubchHistory (which only keeps track of
    // subchannels with non-host data)
    //
    for (UINT32 Subch = 0; Subch < NumSubchannels; ++Subch)
    {
        if (m_SubchObject[Subch] == hObject)
        {
            m_SubchObject[Subch] = 0;
            m_SubchClass[Subch]  = LW01_NULL_OBJECT;
            m_SubchEngine[Subch] = LW2080_ENGINE_TYPE_NULL;
            m_SubchHistory.erase(
                remove(m_SubchHistory.begin(), m_SubchHistory.end(), Subch),
                m_SubchHistory.end());
        }
    }
    // Pass the call to the wrapped channel
    //
    CHECK_RC(m_pWrappedChannel->UnsetObject(hObject));
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Set semaphore offset
//!
/* virtual */ RC SemaphoreChannelWrapper::SetSemaphoreOffset(UINT64 offset)
{
    RC rc = m_pWrappedChannel->SetSemaphoreOffset(offset);
    if (OK == rc)
        m_SemStateStack.top().SetSemOffset(offset);
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Set semaphore release flags
//!
/* virtual */ void SemaphoreChannelWrapper::SetSemaphoreReleaseFlags(UINT32 flags)
{
    m_SemStateStack.top().SetSemReleaseFlags(flags);
    m_pWrappedChannel->SetSemaphoreReleaseFlags(flags);
}

//------------------------------------------------------------------------------
//! \brief Set semaphore payload size
//!
/* virtual */ RC SemaphoreChannelWrapper::SetSemaphorePayloadSize(SemaphorePayloadSize size)
{
    RC rc = m_pWrappedChannel->SetSemaphorePayloadSize(size);
    if (OK == rc)
        m_SemStateStack.top().SetSemaphorePayloadSize(size);
    return rc;
}

//--------------------------------------------------------------------
//! \brief Save the semaphore state
//!
//! \sa EndInsertedMethods
//! \sa CancelInsertedMethods
//!
/* virtual */ RC SemaphoreChannelWrapper::BeginInsertedMethods()
{
    MASSERT(m_SemStateStack.size() > 0);
    RC rc;

    CHECK_RC(m_pWrappedChannel->BeginInsertedMethods());
    m_SemStateStack.push(m_SemStateStack.top());
    return rc;
}

//--------------------------------------------------------------------
//! \brief Restore the semaphore state saved by BeginInsertedMethods
//!
//! This function should be called at the end of a series of inserted
//! methods.  The purpose of this function is to restore the state of
//! the semaphore methods to what it was before the inserted methods
//! were inserted.  This function also restores the subdevice mask.
//!
//! \sa BeginInsertedMethods
//! \sa CancelInsertedMethods
//!
/* virtual */ RC SemaphoreChannelWrapper::EndInsertedMethods()
{
    RC rc;

    // Sanity check
    //
    if (m_SemStateStack.size() < 2)
    {
        MASSERT(!"EndInsertedMethods without an associated "
                 "BeginInsertedMethods");
        m_pWrappedChannel->CancelInsertedMethods();
        return RC::SOFTWARE_ERROR;
    }

    // Pop m_SemStateStack, and set the appropriate dirty bits in the
    // new top-of-stack
    //
    SemState InsertedSemState = m_SemStateStack.top();
    m_SemStateStack.pop();
    m_SemStateStack.top().SetDirty(&InsertedSemState);

    // Propagate the EndInsertedMethods() to the next ChannelWrapper
    // or Channel.
    //
    CHECK_RC(m_pWrappedChannel->EndInsertedMethods());
    return rc;
}

//--------------------------------------------------------------------
//! \brief Forget the semaphore state saved by BeginInsertedMethods
//!
//! \sa BeginInsertedMethods
//! \sa EndInsertedMethods
//!
/* virtual */ void SemaphoreChannelWrapper::CancelInsertedMethods()
{
    MASSERT(m_SemStateStack.size() > 1);
    m_SemStateStack.pop();
    m_pWrappedChannel->CancelInsertedMethods();
}

//--------------------------------------------------------------------
//! \brief Clear the pushbuffer data, forgetting all previous methods
//!
/* virtual */ void SemaphoreChannelWrapper::ClearPushbuffer()
{
    MASSERT(m_SemStateStack.size() > 0);

    m_pWrappedChannel->ClearPushbuffer();

    SemState baseSemState;
    baseSemState.SetSubdevMask(m_AllSubdevMask);
    baseSemState.SetSemReleaseFlags(GetSemaphoreReleaseFlags());

    size_t stackSize = m_SemStateStack.size();
    for (UINT32 ii = 0; ii < stackSize; ++ii)
        m_SemStateStack.pop();
    for (UINT32 ii = 0; ii < stackSize; ++ii)
        m_SemStateStack.push(baseSemState);
}

//--------------------------------------------------------------------
//! \brief Return this object
//!
/* virtual */
SemaphoreChannelWrapper * SemaphoreChannelWrapper::GetSemaphoreChannelWrapper()
{
    return this;
}

//--------------------------------------------------------------------
//!\brief Called prior to writing methods to parse out and identify
//! semaphore methods
//!
//! \param Subchannel is the subchannel being written to
//! \param Method is the method being written
//! \param Data is the data for the method
//!
RC SemaphoreChannelWrapper::UpdateSemaphoreState
(
    UINT32 Subch,
    UINT32 Method,
    UINT32 Data
)
{
    RC rc;
    MASSERT(m_SemStateStack.size() > 0);

    CHECK_RC(CleanupDirtySemaphoreState(GetSubchMask(Subch, Method)));

    SemState &LwrrentSemState = m_SemStateStack.top();

    if (Method >= MIN_ENGINE_METHOD)
    {
        UINT32 ClassID = m_SubchClass[Subch];
        if (s_ThreeDClasses.HasClass(ClassID))
        {
            switch (Method)
            {
                case LW9097_SET_REPORT_SEMAPHORE_A:
                case LW9097_SET_REPORT_SEMAPHORE_B:
                case LW9097_SET_REPORT_SEMAPHORE_C:
                case LWC797_SET_REPORT_SEMAPHORE_PAYLOAD_LOWER:
                case LWC797_SET_REPORT_SEMAPHORE_PAYLOAD_UPPER:
                case LWC797_SET_REPORT_SEMAPHORE_ADDRESS_LOWER:
                case LWC797_SET_REPORT_SEMAPHORE_ADDRESS_UPPER:
                    LwrrentSemState.AddSemData(Subch, Method, Data);
                    break;
            }
        }
        else if (s_ComputeClasses.HasClass(ClassID))
        {
            switch (Method)
            {
                case LW90C0_SET_REPORT_SEMAPHORE_A:
                case LW90C0_SET_REPORT_SEMAPHORE_B:
                case LW90C0_SET_REPORT_SEMAPHORE_C:
                case LWC7C0_SET_REPORT_SEMAPHORE_PAYLOAD_LOWER:
                case LWC7C0_SET_REPORT_SEMAPHORE_PAYLOAD_UPPER:
                case LWC7C0_SET_REPORT_SEMAPHORE_ADDRESS_LOWER:
                case LWC7C0_SET_REPORT_SEMAPHORE_ADDRESS_UPPER:
                    LwrrentSemState.AddSemData(Subch, Method, Data);
                    break;
            }
        }
        else if (s_DmaCopyClasses.HasClass(ClassID))
        {
            switch (Method)
            {
                case LW90B5_SET_SEMAPHORE_A:
                case LW90B5_SET_SEMAPHORE_B:
                case LW90B5_SET_SEMAPHORE_PAYLOAD:
                case LWC7B5_SET_SEMAPHORE_PAYLOAD_UPPER:
                    LwrrentSemState.AddSemData(Subch, Method, Data);
                    break;
                case LW90B5_LAUNCH_DMA:
                    LwrrentSemState.DelSemData(
                        Subch, LW90B5_SET_SEMAPHORE_A);
                    LwrrentSemState.DelSemData(
                        Subch, LW90B5_SET_SEMAPHORE_B);
                    LwrrentSemState.DelSemData(
                        Subch, LW90B5_SET_SEMAPHORE_PAYLOAD);
                    LwrrentSemState.DelSemData(
                        Subch, LWC7B5_SET_SEMAPHORE_PAYLOAD_UPPER);
                    break;
            }
        }
        else if (s_EncoderClasses.HasClass(ClassID))
        {
            switch (Method)
            {
            case LW90B7_SEMAPHORE_A:
            case LW90B7_SEMAPHORE_B:
            case LW90B7_SEMAPHORE_C:
            case LWC7B7_SET_SEMAPHORE_PAYLOAD_LOWER:
            case LWC7B7_SET_SEMAPHORE_PAYLOAD_UPPER:
                LwrrentSemState.AddSemData(Subch, Method, Data);
                break;
            case LW90B7_SEMAPHORE_D:
                LwrrentSemState.DelSemData(Subch, LW90B7_SEMAPHORE_A);
                LwrrentSemState.DelSemData(Subch, LW90B7_SEMAPHORE_B);
                LwrrentSemState.DelSemData(Subch, LW90B7_SEMAPHORE_C);
                LwrrentSemState.DelSemData(
                    Subch, LWC7B7_SET_SEMAPHORE_PAYLOAD_LOWER);
                LwrrentSemState.DelSemData(
                    Subch, LWC7B7_SET_SEMAPHORE_PAYLOAD_UPPER);
                break;
            }
        }
        else if (s_LwdecClasses.HasClass(ClassID))
        {
            switch (Method)
            {
            case LWA0B0_SEMAPHORE_A:
            case LWA0B0_SEMAPHORE_B:
            case LWA0B0_SEMAPHORE_C:
            case LWC7B0_SET_SEMAPHORE_PAYLOAD_LOWER:
            case LWC7B0_SET_SEMAPHORE_PAYLOAD_UPPER:
                LwrrentSemState.AddSemData(Subch, Method, Data);
                break;
            case LWA0B0_SEMAPHORE_D:
                LwrrentSemState.DelSemData(Subch, LWA0B0_SEMAPHORE_A);
                LwrrentSemState.DelSemData(Subch, LWA0B0_SEMAPHORE_B);
                LwrrentSemState.DelSemData(Subch, LWA0B0_SEMAPHORE_C);
                LwrrentSemState.DelSemData(
                    Subch, LWC7B0_SET_SEMAPHORE_PAYLOAD_LOWER);
                LwrrentSemState.DelSemData(
                    Subch, LWC7B0_SET_SEMAPHORE_PAYLOAD_UPPER);
                break;
            }
        }
        else if (s_TsecClasses.HasClass(ClassID))
        {
            switch (Method)
            {
            case LW95A1_SEMAPHORE_A:
            case LW95A1_SEMAPHORE_B:
            case LW95A1_SEMAPHORE_C:
                LwrrentSemState.AddSemData(Subch, Method, Data);
                break;
            case LW95A1_SEMAPHORE_D:
                LwrrentSemState.DelSemData(Subch, LW95A1_SEMAPHORE_A);
                LwrrentSemState.DelSemData(Subch, LW95A1_SEMAPHORE_B);
                LwrrentSemState.DelSemData(Subch, LW95A1_SEMAPHORE_C);
                break;
            }
        }
        else if (s_LwjpgClasses.HasClass(ClassID))
        {
            switch (Method)
            {
            case LWC4D1_SEMAPHORE_A:
            case LWC4D1_SEMAPHORE_B:
            case LWC4D1_SEMAPHORE_C:
                LwrrentSemState.AddSemData(Subch, Method, Data);
                break;
            case LWC4D1_SEMAPHORE_D:
                LwrrentSemState.DelSemData(Subch, LWC4D1_SEMAPHORE_A);
                LwrrentSemState.DelSemData(Subch, LWC4D1_SEMAPHORE_B);
                LwrrentSemState.DelSemData(Subch, LWC4D1_SEMAPHORE_C);
                break;
            }
        }
        else if (s_OfaClasses.HasClass(ClassID))
        {
            switch (Method)
            {
            case LWC6FA_SEMAPHORE_A:
            case LWC6FA_SEMAPHORE_B:
            case LWC6FA_SEMAPHORE_C:
            case LWC7FA_SET_SEMAPHORE_PAYLOAD_LOWER:
            case LWC7FA_SET_SEMAPHORE_PAYLOAD_UPPER:
                LwrrentSemState.AddSemData(Subch, Method, Data);
                break;
            case LWC6FA_SEMAPHORE_D:
                LwrrentSemState.DelSemData(Subch, LWC6FA_SEMAPHORE_A);
                LwrrentSemState.DelSemData(Subch, LWC6FA_SEMAPHORE_B);
                LwrrentSemState.DelSemData(Subch, LWC6FA_SEMAPHORE_C);
                LwrrentSemState.DelSemData(
                    Subch, LWC7FA_SET_SEMAPHORE_PAYLOAD_LOWER);
                LwrrentSemState.DelSemData(
                    Subch, LWC7FA_SET_SEMAPHORE_PAYLOAD_UPPER);
                break;
            }
        }
    }
    else if (m_SupportsHost)
    {
        switch (m_ChannelType)
        {
            case GPFIFO_CHANNEL:
                // GPFIFO channels (other than LW50) can use either LW50
                // style semaphores with a single context and offset
                // method or a 64bit offset and value methods
                //
                switch (Method)
                {
                    case LW506F_SET_CONTEXT_DMA_SEMAPHORE:
                        if (Has64bitSemaphores())
                        {
                            // VOLTA+ use 64b semaphores.
                            // LW506F_SET_CONTEXT_DMA_SEMAPHORE corresponds to
                            // LWC36F_SEM_ADDR_HI

                            // Writing LWC36F_SEM_ADDR_LO/HI ilwalidates
                            // previous writes to LW506F_SEMAPHORE_OFFSET,
                            // LW826F_SEMAPHOREA, LW826F_SEMAPHOREB
                            LwrrentSemState.DelSemData(0, LW506F_SEMAPHORE_OFFSET);
                            LwrrentSemState.DelSemData(0, LW826F_SEMAPHOREA);
                            LwrrentSemState.DelSemData(0, LW826F_SEMAPHOREB);
                        }
                        LwrrentSemState.AddSemData(0, Method, Data);
                        break;
                    case LW506F_SEMAPHORE_OFFSET:
                        if (Has64bitSemaphores())
                        {
                            // VOLTA+ use 64b semaphores.
                            // LW506F_SEMAPHORE_OFFSET corresponds to
                            // LWC36F_SEM_PAYLOAD_LO

                            // Writing LWC36F_SEM_PAYLOAD_LO/HI ilwalidates
                            // previous writes to LW826F_SEMAPHOREC,
                            LwrrentSemState.DelSemData(0, LW826F_SEMAPHOREC);
                        }
                        else
                        {
                            // Writing LW506F_SEMAPHORE_OFFSET ilwalidates
                            // previous writes to LW826F_SEMAPHOREA/B
                            LwrrentSemState.DelSemData(0, LW826F_SEMAPHOREA);
                            LwrrentSemState.DelSemData(0, LW826F_SEMAPHOREB);
                        }
                        LwrrentSemState.AddSemData(0, Method, Data);
                        break;
                    case LW826F_SEMAPHOREA:
                    case LW826F_SEMAPHOREB:
                    case LW826F_SEMAPHOREC:
                        // Writing LW826F_SEMAPHOREA/B/C ilwalidates
                        // previous writes to LW506F_SEMAPHORE_OFFSET
                        LwrrentSemState.DelSemData(0, LW506F_SEMAPHORE_OFFSET);

                        // Writing LW826F_SEMAPHOREA/B/C ilwalidates
                        // previous writes to VOLTA+ Semaphore methods
                        LwrrentSemState.DelSemData(0, LWC36F_SEM_ADDR_LO);
                        LwrrentSemState.DelSemData(0, LWC36F_SEM_ADDR_HI);
                        LwrrentSemState.DelSemData(0, LWC36F_SEM_PAYLOAD_LO);
                        LwrrentSemState.DelSemData(0, LWC36F_SEM_PAYLOAD_HI);

                        LwrrentSemState.AddSemData(0, Method, Data);
                        break;
                    case LWC36F_SEM_ADDR_LO:
                        // Writing LWC36F_SEM_ADDR_LO/HI ilwalidates
                        // previous writes to LW506F_SEMAPHORE_OFFSET,
                        // LW826F_SEMAPHOREA, LW826F_SEMAPHOREB
                        LwrrentSemState.DelSemData(0, LW506F_SEMAPHORE_OFFSET);
                        LwrrentSemState.DelSemData(0, LW826F_SEMAPHOREA);
                        LwrrentSemState.DelSemData(0, LW826F_SEMAPHOREB);
                        LwrrentSemState.AddSemData(0, Method, Data);
                        break;
                    case LWC36F_SEM_PAYLOAD_HI:
                        // Writing LWC36F_SEM_PAYLOAD_LO/HI ilwalidates
                        // previous writes to LW826F_SEMAPHOREC,
                        LwrrentSemState.DelSemData(0, LW826F_SEMAPHOREC);
                        LwrrentSemState.AddSemData(0, Method, Data);
                        break;
                    case LWC36F_SEM_EXELWTE:
                        LwrrentSemState.DelSemData(0, LWC36F_SEM_ADDR_LO);
                        LwrrentSemState.DelSemData(0, LWC36F_SEM_ADDR_HI);
                        LwrrentSemState.DelSemData(0, LWC36F_SEM_PAYLOAD_LO);
                        LwrrentSemState.DelSemData(0, LWC36F_SEM_PAYLOAD_HI);
                        break;
                    default:
                        break;
                }
                break;
            case DMA_CHANNEL:
                // For DMA channels that support semaphores, parse out the
                // context DMA handle and offset
                //
                switch (Method)
                {
                    case LW206E_SET_CONTEXT_DMA_SEMAPHORE:
                    case LW206E_SEMAPHORE_OFFSET:
                        LwrrentSemState.AddSemData(0, Method, Data);
                        break;
                    default:
                        break;
                }
                break;
            default:
                MASSERT(!"Should not get here");
                break;
        }
    }

    if (m_SupportsBackend && Method >= MIN_ENGINE_METHOD)
    {
        SetLwrrentSubchannel(Subch);
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Restore any dirty semaphore data to the correct values
//!
//! This function re-writes any semaphore methods that are "dirty" due
//! to inserted methods (i.e. methods between BeginInsertedMethods and
//! EndInsertedMethods).
//!
//! For example, in buffer-based-runlist mode, mods inserts a
//! semaphore-release at the end of each runlist entry.  But in order
//! to do so, mods must call some SET_SEMAPHORE_OFFSET method, which
//! overwrites any semaphore offset that the test has already set up.
//! The old offset must be restored in order for the test to succeed.
//!
//! In that cirlwmstance, SemState::SetDirty() marks the
//! SET_SEMAPHORE_OFFSET data "dirty", and this method re-writes the
//! old SET_SEMAPHORE_OFFSET data to the channel.  This method also
//! restores dirty subdevice masks and semaphore-release flags.
//!
//! This method should be called before any write method that could
//! potentially be affected by dirty semaphore data.
//!
//! \sa SemState::SetDirty()
//!
//! \param pInsertedSemState The semaphore state that was just popped
//!     from m_SemStateStack
//!
RC SemaphoreChannelWrapper::CleanupDirtySemaphoreState(UINT32 SubchMask)
{
    MASSERT(m_SemStateStack.size() > 0);
    Channel *pCh = GetWrapper();
    UINT32 numIterations = 0;
    RC rc;

    // If we're calling this function relwrsively, then add SubchMask
    // to the list of subchannels we're cleaning, and abort.
    //
    if (m_CleaningDirtySemaphoreState)
    {
        m_CleaningDirtySubchMask |= SubchMask;
        return rc;
    }
    Utility::TempValue<bool> tmp1(m_CleaningDirtySemaphoreState, true);

    // Set the mask of subchannels we're cleaning
    //
    MASSERT(m_CleaningDirtySubchMask == 0);
    Utility::TempValue<UINT32> tmp2(m_CleaningDirtySubchMask, SubchMask);

    // If the semaphore state is dirty, then re-write the dirty
    // methods until the state is clean.  The cleanup code is in a
    // while-loop (as opposed to an if-statement) because there's a
    // slim chance that a new semaphore will be inserted in the middle
    // of the cleanup (e.g. if we autoflush a runlist entry).  In that
    // case, we'll need a second iteration to clean up those methods
    // again.
    //
    while (m_SemStateStack.top().IsDirty(m_CleaningDirtySubchMask))
    {
        // Sanity check: This loop usually needs to run at most once,
        // or twice on rare occasions.  So something must be broken if
        // we iterate 100 times and the semaphore state is still
        // dirty.
        //
        if (+numIterations > 100)
        {
            MASSERT(!"SemaphoreChannelWrapper cannot clean dirty sem state");
            return RC::SOFTWARE_ERROR;
        }

        // Copy the desired semaphore state from m_SemStateStack.top().
        //
        SemState DesiredSemState = m_SemStateStack.top();
        vector<SemData> DesiredSemData;
        m_SemStateStack.top().GetAllSemData(&DesiredSemData);

        // Since restoring the old semaphore state counts as "Inserted
        // methods", call BeginInsertedMethods().
        //
        CHECK_RC(pCh->BeginInsertedMethods());
        Utility::CleanupOnReturn<Channel>
            CleanupInsertedMethods(pCh, &Channel::CancelInsertedMethods);

        // Restore the desired semaphore method data
        //
        for (vector<SemData>::iterator pSemData = DesiredSemData.begin();
             pSemData != DesiredSemData.end(); ++pSemData)
        {
            if (pSemData->DirtyMask == 0)
            {
                continue;
            }

            if ((pSemData->Method >= MIN_ENGINE_METHOD) &&
                (((1 << pSemData->Subch) & m_CleaningDirtySubchMask) == 0))
            {
                continue;
            }

            // Determine which data values are dirty, and need to be
            // written.
            //
            set<UINT32> DirtyDataValues; // The data values that we need to
                                         // write in order to restore pSemData
            map<UINT32, UINT32> Data2Mask; // The subdevice mask for each data
                                           // value in pSemData.

            for (map<UINT32, UINT32>::iterator iter =
                     pSemData->Mask2Data.begin();
                 iter != pSemData->Mask2Data.end(); ++iter)
            {
                UINT32 Mask = iter->first;
                UINT32 Data = iter->second;

                if (Mask & pSemData->DirtyMask)
                    DirtyDataValues.insert(Data);
                if (Data2Mask.count(Data))
                    Data2Mask[Data] |= Mask;
                else
                    Data2Mask[Data] = Mask;
            }

            // Now write every value in DirtyDataValues to the
            // appropriate subdevice mask.
            //
            // Note: the subdevice masks in Data2Mask only contain the
            // subdevices that actually exist, but we prefer to pass
            // Channel::AllSubdevicesMask to WriteSetSubdevice() when
            // we broadcast to all devices.  So if any mask is
            // m_AllSubdevMask (i.e. all subdevices), colwert it to
            // Channel::AllSubdevicesMask
            //
            for (set<UINT32>::iterator pData = DirtyDataValues.begin();
                 pData != DirtyDataValues.end(); ++pData)
            {
                UINT32 Mask = Data2Mask[*pData];
                if (Mask != m_SemStateStack.top().GetSubdevMask() ||
                    m_SemStateStack.top().IsSubdevMaskDirty())
                {
                    CHECK_RC(pCh->WriteSetSubdevice(
                                 (Mask == m_AllSubdevMask)
                                 ? Channel::AllSubdevicesMask
                                 : Mask));
                    MASSERT(Mask == m_SemStateStack.top().GetSubdevMask());
                }
                CHECK_RC(Write(pSemData->Subch, pSemData->Method, *pData));
            }
        }

        // Now that we've finished restoring the semaphore methods,
        // restore the semaphore flags and subdevice mask.
        //
        if ((DesiredSemState.GetSubdevMask() !=
                                m_SemStateStack.top().GetSubdevMask()) ||
            m_SemStateStack.top().IsSubdevMaskDirty())
        {
            UINT32 SubdevMask = (DesiredSemState.GetSubdevMask() == m_AllSubdevMask
                                 ? Channel::AllSubdevicesMask
                                 : DesiredSemState.GetSubdevMask());
            CHECK_RC(pCh->WriteSetSubdevice(SubdevMask));
        }

        if ((DesiredSemState.GetSemOffset() !=
                                 m_SemStateStack.top().GetSemOffset()) ||
            m_SemStateStack.top().IsSemOffsetDirty())
        {
            pCh->SetSemaphoreOffset(DesiredSemState.GetSemOffset());
        }

        if ((DesiredSemState.GetSemReleaseFlags() !=
                                 m_SemStateStack.top().GetSemReleaseFlags()) ||
            m_SemStateStack.top().IsSemReleaseFlagsDirty())
        {
            pCh->SetSemaphoreReleaseFlags(
                DesiredSemState.GetSemReleaseFlags());
        }

        if ((DesiredSemState.GetSemaphorePayloadSize() !=
                                 m_SemStateStack.top().GetSemaphorePayloadSize()) ||
            m_SemStateStack.top().IsSemaphorePayloadSizeDirty())
        {
            pCh->SetSemaphorePayloadSize(
                DesiredSemState.GetSemaphorePayloadSize());
        }

        // Call EndInsertedMethods().  This has the side-effect of
        // popping m_SemStateStack and calling SemState::SetDirty(),
        // which ensures that IsDirty() will return the correct value
        // in the surrounding while-loop.
        //
        CleanupInsertedMethods.Cancel();
        CHECK_RC(pCh->EndInsertedMethods());
    }

    return rc;
}

//--------------------------------------------------------------------
//!\brief Move the indicated subchannel to the head of m_SubchHistory
//!
void SemaphoreChannelWrapper::SetLwrrentSubchannel(UINT32 Subchannel)
{
    if (m_SubchHistory.empty() || m_SubchHistory[0] != Subchannel)
    {
        vector<UINT32>::iterator pSubchannel =
            find(m_SubchHistory.begin(), m_SubchHistory.end(), Subchannel);

        if (pSubchannel == m_SubchHistory.end())
            m_SubchHistory.insert(m_SubchHistory.begin(), Subchannel);
        else
            rotate(m_SubchHistory.begin(), pSubchannel, pSubchannel + 1);
    }
}

//--------------------------------------------------------------------
//!\brief Prints the current semaphore state
//!
void SemaphoreChannelWrapper::PrintSemaphoreState()
{
    MASSERT(m_SemStateStack.size() > 0);

    GpuDevice *pGpuDevice = GetGpuDevice();
    SemState &LwrrentSemState = m_SemStateStack.top();
    vector<SemData> SemDataArray;

    LwrrentSemState.GetAllSemData(&SemDataArray);

    Printf(Tee::PriHigh,
           "Semaphore State: ChHandle=0x%08x; SubdevMask=0x%08x\n",
           GetHandle(), LwrrentSemState.GetSubdevMask());
    Printf(Tee::PriHigh, "    Subch Class  Method     Data\n");
    Printf(Tee::PriHigh, "    ----- -----  ------     ----\n");

    for (vector<SemData>::iterator pSemData = SemDataArray.begin();
         pSemData != SemDataArray.end(); ++pSemData)
    {
        string ClassName;
        string Data;

        if (pSemData->Method >= MIN_ENGINE_METHOD)
        {
            ClassName =
                Utility::StrPrintf("0x%04x", m_SubchClass[pSemData->Subch]);
        }
        else
        {
            switch (m_ChannelType)
            {
                case GPFIFO_CHANNEL:
                    ClassName = "GPFIFO";
                    break;
                case DMA_CHANNEL:
                    ClassName = "DMA_CH";
                    break;
                case PIO_CHANNEL:
                    ClassName = "PIO_CH";
                    break;
                default:
                    MASSERT(!"Should never get here");
                    break;
            }
        }

        for (UINT32 Sub = 0; Sub < pGpuDevice->GetNumSubdevices(); ++Sub)
        {
            if (Sub > 0)
                Data += '/';
            if (pSemData->Mask2Data.count(1 << Sub))
                Data += Utility::StrPrintf("0x%08x",
                                           pSemData->Mask2Data[1 << Sub]);
            else
                Data += '?';
        }

        Printf(Tee::PriHigh, "    %-5d %-6s 0x%08x %s\n",
               pSemData->Subch, ClassName.c_str(),
               pSemData->Method, Data.c_str());
    }
}

//--------------------------------------------------------------------
//! \brief Find a class/subchannel that supports a backend method.
//!
//! This method is used by ReleaseBackendSemaphore() and
//! ReleaseBackendSemaphoreWithTrap() to find a class/subchannel that
//! supports the backend method.
//!
//! The basic problem is that an engine may support several classes,
//! but not all of those classes support backend semaphores.  For
//! example, the 2D class (902d), 3D class (9097), and lwca class
//! (90c0) are all on the graphics engine, but the 2D class doesn't
//! support backend semaphores.  So if you're sending methods to the
//! 2D class and need to release a backend semaphore, you need to
//! switch to a subchannel that has the 3D or lwca class on it.
//!
//! This method tries to use the subchannel that was used for the most
//! recent write.  If the current subchannel doesn't support the
//! backend method, it tries some other subchannel that uses the same
//! engine.  If no other subchannel supports the backend method for
//! the correct engine, this method tries to either create a new
//! object on an unused subchannel (if allowHostSemaphore is false),
//! or it tells the host to use a host semaphore instead (if
//! allowHostSemaphore is true).  If all of that fails, this method
//! returns a failure.
//!
//! \param ppSupportedGrallocs Array of gralloc.h objects such that all
//!     classes returned by GetClassList() support the desired backend
//!     method.
//! \param numSupportedGrallocs Size of the pSupportedGrallocs array
//! \param allowHostSemaphore If true, then this method is allowed to
//!     tell the caller that it should use a host semaphore, instead
//!     of a backend semaphore.  If false, then this method will
//!     insist on a backend semaphore no matter what, even if it means
//!     creating a new object or even generating an error.  This
//!     method recommends a host semaphore by setting *pClass =
//!     GetClass and *pSubch = 0.
//! \param[out] pClass On success, the class to send methods to.
//!     *pClass is guaranteed to either be pSupportedClasses, or be
//!     the same as GetClass() if allowHostSemaphore is set.
//! \param[out] pSubch On success, the subchannel that *pClass is on
//!
RC SemaphoreChannelWrapper::FindSupportedClass
(
    const GrClassAllocator **ppSupportedGrallocs,
    UINT32 numSupportedGrallocs,
    bool allowHostSemaphore,
    UINT32 *pClass,
    UINT32 *pSubch
)
{
    RC rc;

    // Since this method is used for backend semaphores, abort if
    // channel doesn't support backend
    //
    if (!m_SupportsBackend)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Ignore the allowHostSemaphore arg if this class doesn't support
    // host semaphores
    //
    if (!m_SupportsHost)
    {
        allowHostSemaphore = false;
    }

    // Abort if this channel doesn't have any classes or anything set up
    //
    if (m_SubchHistory.empty())
    {
        if (allowHostSemaphore)
        {
            *pClass = GetClass();
            *pSubch = 0;
            return rc;
        }
        else
        {
            Printf(Tee::PriHigh,
                   "Channel has no active engine in FindSupportedClass()\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    UINT32 lwrrentSubch = m_SubchHistory[0];
    UINT32 lwrrentClass = m_SubchClass[lwrrentSubch];
    UINT32 lwrrentEngine = m_SubchEngine[lwrrentSubch];

    // Turn ppSupportedGrallocs into an STL set (for quick searching)
    // and vector (for preserving order).
    //
    vector<UINT32> supportedClassVector;
    for (UINT32 ii = 0; ii < numSupportedGrallocs; ++ii)
    {
        const UINT32 *pClassList;
        UINT32 numClasses;
        ppSupportedGrallocs[ii]->GetClassList(&pClassList, &numClasses);
        supportedClassVector.insert(supportedClassVector.end(),
                                    pClassList, pClassList + numClasses);
    }
    set<UINT32> supportedClassSet(supportedClassVector.begin(),
                                  supportedClassVector.end());

    // If the current class is supported, then use it.
    //
    if (supportedClassSet.count(lwrrentClass) != 0)
    {
        *pClass = lwrrentClass;
        *pSubch = lwrrentSubch;
        return rc;
    }

    // Otherwise, if the class on any other subchannel is supported,
    // and it's for the same engine as the current class, then use
    // that.
    //
    for (UINT32 subch = 0; subch < NumSubchannels; ++subch)
    {
        UINT32 subchClass = m_SubchClass[subch];
        UINT32 subchEngine = m_SubchEngine[subch];
        if (subchEngine == lwrrentEngine &&
            supportedClassSet.count(subchClass) != 0)
        {
            *pClass = subchClass;
            *pSubch = subch;
            return rc;
        }
    }

    // If we get this far, none of the subchannels support a backend
    // semaphore.  Use a host semaphore, if it's allowed.
    //
    if (allowHostSemaphore)
    {
        *pClass = GetClass();
        *pSubch = 0;
        return rc;
    }

    // Otherwise, if we can find *any* supported class that exists on
    // this device and that uses the same engine as the current class,
    // *and* if there's an unused channel, then create an instance of
    // that class on the unused subchannel and use it.
    //
    // We don't take any care to clean up the objects created like
    // this.  There shouldn't be too many of them -- any object
    // created like this will automatically get reused for engine
    // semaphores unless the subchannel gets reassigned.  They need to
    // persist until the engine finishes using them, however long that
    // is.  So we tolerate the small leak until we call LwRm::Free on
    // the parent channel, which should relwrsively free all child
    // objects.
    //
    bool foundEmptySubch = false;
    UINT32 emptySubch = 0;
    for (UINT32 ii = 0; ii < NumSubchannels; ++ii)
    {
        if (m_SubchClass[ii] == LW01_NULL_OBJECT)
        {
            foundEmptySubch = true;
            emptySubch = ii;
            break;
        }
    }

    if (foundEmptySubch)
    {
        GpuDevice *pGpuDevice = GetGpuDevice();
        LwRm * pLwRm = GetRmClient();

        // Get a list of classes supported on the current engine
        //
        vector<UINT32> classList;

        LW2080_CTRL_GPU_GET_ENGINE_CLASSLIST_PARAMS params = {0};
        params.engineType = lwrrentEngine;
        CHECK_RC(pLwRm->ControlBySubdevice(
                     pGpuDevice->GetSubdevice(0),
                     LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                     &params, sizeof(params)));
        classList.resize(params.numClasses);

        memset(&params, 0, sizeof(params));
        params.engineType = lwrrentEngine;
        params.numClasses = (LwU32)classList.size();
        params.classList = LW_PTR_TO_LwP64(&classList[0]);
        CHECK_RC(pLwRm->ControlBySubdevice(
                     pGpuDevice->GetSubdevice(0),
                     LW2080_CTRL_CMD_GPU_GET_ENGINE_CLASSLIST,
                     &params, sizeof(params)));
        MASSERT(params.numClasses == classList.size());
        sort(classList.begin(), classList.end());

        // Find a class from ppSupportedGrallocs that's in the current
        // engine's class list.
        //
        for (UINT32 ii = 0; ii < supportedClassVector.size(); ++ii)
        {
            UINT32 classID = supportedClassVector[ii];
            if (binary_search(classList.begin(), classList.end(), classID))
            {
                UINT32 hObject;
                CHECK_RC(pLwRm->Alloc(GetHandle(), &hObject, classID, NULL));
                CHECK_RC(GetWrapper()->SetObject(emptySubch, hObject));
                *pClass = classID;
                *pSubch = emptySubch;
                return rc;
            }
        }
    }

    // Otherwise, give up.
    //
    return RC::UNSUPPORTED_FUNCTION;
}

//-------------------------------------------------------------------
//! \brief Checks the payload bits for 32 bit semaphore classes
//!
RC SemaphoreChannelWrapper::Check32BitPayload(UINT64 payload)
{
    if (payload >> 32)
    {
        Printf(Tee::PriDebug, "64b semaphore payload used with an unsupported class\n"); 
        return RC::UNSUPPORTED_FUNCTION;
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Write a class-9097 backend semaphore
//!
//! The last (SEMAPHORE_D) method is mostly determined by the
//! SemaphoreD arg, with updates determined by the Flags arg.  Flags
//! is essentially GetSemReleaseFlags(), but the caller must remove
//! any bits that are incompatible with SemaphoreD.  This convention
//! applies to all the private Write<class>Semaphore methods.
//!
RC SemaphoreChannelWrapper::Write9097Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;
    CHECK_RC(Check32BitPayload(Payload));

    SemaphoreD |= (  (Flags & FlagSemRelWithTime)
                   ? DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D,
                             _STRUCTURE_SIZE, _FOUR_WORDS)
                   : DRF_DEF(9097, _SET_REPORT_SEMAPHORE_D,
                             _STRUCTURE_SIZE, _ONE_WORD) );
    CHECK_RC(GetWrapper()->Write(
        Subch, LW9097_SET_REPORT_SEMAPHORE_A, 4,
        DRF_NUM(9097, _SET_REPORT_SEMAPHORE_A,
                _OFFSET_UPPER, (UINT32)(GpuAddr >> 32)),
        DRF_NUM(9097, _SET_REPORT_SEMAPHORE_B, _OFFSET_LOWER, (UINT32)GpuAddr),
        DRF_NUM(9097, _SET_REPORT_SEMAPHORE_C, _PAYLOAD, Payload),
        SemaphoreD));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-90b5 backend semaphore
//!
//! \sa Write90b5Semaphore()
//!
RC SemaphoreChannelWrapper::Write90b5Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 LaunchDma
)
{
    RC rc;
    CHECK_RC(Check32BitPayload(Payload));

    LaunchDma |= DRF_DEF(90B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NONE);
    LaunchDma |= DRF_DEF(90B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE);
    LaunchDma |= (  (Flags & FlagSemRelWithTime)
                   ? DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,
                             _RELEASE_FOUR_WORD_SEMAPHORE)
                   : DRF_DEF(90B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,
                             _RELEASE_ONE_WORD_SEMAPHORE) );
    CHECK_RC(GetWrapper()->Write(
        Subch, LW90B5_SET_SEMAPHORE_A, 3,
        DRF_NUM(90B5, _SET_SEMAPHORE_A, _UPPER, (UINT32)(GpuAddr >> 32)),
        DRF_NUM(90B5, _SET_SEMAPHORE_B, _LOWER, (UINT32)GpuAddr),
        DRF_NUM(90B5, _SET_SEMAPHORE_PAYLOAD, _PAYLOAD, Payload)));
    CHECK_RC(GetWrapper()->Write(Subch, LW90B5_LAUNCH_DMA, LaunchDma));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-90b7 backend semaphore
//!
//! \sa Write90b7Semaphore()
//!
RC SemaphoreChannelWrapper::Write90b7Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;
    CHECK_RC(Check32BitPayload(Payload));

    SemaphoreD |= (  (Flags & FlagSemRelWithTime)
                   ? DRF_DEF(90B7, _SEMAPHORE_D, _STRUCTURE_SIZE, _FOUR)
                   : DRF_DEF(90B7, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) );
    CHECK_RC(GetWrapper()->Write(
                 Subch, LW90B7_SEMAPHORE_A, 3,
                 DRF_NUM(90B7, _SEMAPHORE_A, _UPPER, (UINT32)(GpuAddr >> 32)),
                 DRF_NUM(90B7, _SEMAPHORE_B, _LOWER, (UINT32)GpuAddr),
                 DRF_NUM(90B7, _SEMAPHORE_C, _PAYLOAD, Payload)));
    CHECK_RC(GetWrapper()->Write(Subch, LW90B7_SEMAPHORE_D, SemaphoreD));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-a0b0 backend semaphore
//!
//! \sa WriteA0b0Semaphore()
//!
RC SemaphoreChannelWrapper::WriteA0b0Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;
    CHECK_RC(Check32BitPayload(Payload));

    SemaphoreD |= (  (Flags & FlagSemRelWithTime)
                   ? DRF_DEF(A0B0, _SEMAPHORE_D, _STRUCTURE_SIZE, _FOUR)
                   : DRF_DEF(A0B0, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) );
    CHECK_RC(GetWrapper()->Write(
                 Subch, LWA0B0_SEMAPHORE_A, 3,
                 DRF_NUM(A0B0, _SEMAPHORE_A, _UPPER, (UINT32)(GpuAddr >> 32)),
                 DRF_NUM(A0B0, _SEMAPHORE_B, _LOWER, (UINT32)GpuAddr),
                 DRF_NUM(A0B0, _SEMAPHORE_C, _PAYLOAD, Payload)));
    CHECK_RC(GetWrapper()->Write(Subch, LWA0B0_SEMAPHORE_D, SemaphoreD));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-95a1 backend semaphore
//!
//! \sa Write95a1Semaphore()
//!
RC SemaphoreChannelWrapper::Write95a1Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;
    CHECK_RC(Check32BitPayload(Payload));

    SemaphoreD |= (  (Flags & FlagSemRelWithTime)
                   ? DRF_DEF(95A1, _SEMAPHORE_D, _STRUCTURE_SIZE, _FOUR)
                   : DRF_DEF(95A1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) );
    CHECK_RC(GetWrapper()->Write(
                 Subch, LW95A1_SEMAPHORE_A, 3,
                 DRF_NUM(95A1, _SEMAPHORE_A, _UPPER, (UINT32)(GpuAddr >> 32)),
                 DRF_NUM(95A1, _SEMAPHORE_B, _LOWER, (UINT32)GpuAddr),
                 DRF_NUM(95A1, _SEMAPHORE_C, _PAYLOAD, Payload)));
    CHECK_RC(GetWrapper()->Write(Subch, LW95A1_SEMAPHORE_D, SemaphoreD));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-c4d1 backend semaphore
//!
//! \sa Writec4d1Semaphore()
//!
RC SemaphoreChannelWrapper::Writec4d1Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;
    CHECK_RC(Check32BitPayload(Payload));

    SemaphoreD |= (  (Flags & FlagSemRelWithTime)
                   ? DRF_DEF(C4D1, _SEMAPHORE_D, _STRUCTURE_SIZE, _FOUR)
                   : DRF_DEF(C4D1, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) );
    CHECK_RC(GetWrapper()->Write(
                 Subch, LWC4D1_SEMAPHORE_A, 3,
                 DRF_NUM(C4D1, _SEMAPHORE_A, _UPPER, (UINT32)(GpuAddr >> 32)),
                 DRF_NUM(C4D1, _SEMAPHORE_B, _LOWER, (UINT32)GpuAddr),
                 DRF_NUM(C4D1, _SEMAPHORE_C, _PAYLOAD, Payload)));
    CHECK_RC(GetWrapper()->Write(Subch, LWC4D1_SEMAPHORE_D, SemaphoreD));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-c6fa backend semaphore
//!
//! \sa Writec6faSemaphore()
//!
RC SemaphoreChannelWrapper::Writec6faSemaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;
    CHECK_RC(Check32BitPayload(Payload));

    SemaphoreD |= (  (Flags & FlagSemRelWithTime)
                   ? DRF_DEF(C6FA, _SEMAPHORE_D, _STRUCTURE_SIZE, _FOUR)
                   : DRF_DEF(C6FA, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE) );
    CHECK_RC(GetWrapper()->Write(
                 Subch, LWC6FA_SEMAPHORE_A, 3,
                 DRF_NUM(C6FA, _SEMAPHORE_A, _UPPER, (UINT32)(GpuAddr >> 32)),
                 DRF_NUM(C6FA, _SEMAPHORE_B, _LOWER, (UINT32)GpuAddr),
                 DRF_NUM(C6FA, _SEMAPHORE_C, _PAYLOAD, Payload)));
    CHECK_RC(GetWrapper()->Write(Subch, LWC6FA_SEMAPHORE_D, SemaphoreD));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-90c0 backend semaphore
//!
//! \sa Write90c0Semaphore()
//!
RC SemaphoreChannelWrapper::Write90c0Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;
    CHECK_RC(Check32BitPayload(Payload));

    SemaphoreD |= (  (Flags & FlagSemRelWithTime)
                   ? DRF_DEF(90C0, _SET_REPORT_SEMAPHORE_D,
                             _STRUCTURE_SIZE, _FOUR_WORDS)
                   : DRF_DEF(90C0, _SET_REPORT_SEMAPHORE_D,
                             _STRUCTURE_SIZE, _ONE_WORD) );
    CHECK_RC(GetWrapper()->Write(
        Subch, LW90C0_SET_REPORT_SEMAPHORE_A, 4,
        DRF_NUM(90C0, _SET_REPORT_SEMAPHORE_A,
                _OFFSET_UPPER, (UINT32)(GpuAddr >> 32)),
        DRF_NUM(90C0, _SET_REPORT_SEMAPHORE_B, _OFFSET_LOWER, (UINT32)GpuAddr),
        DRF_NUM(90C0, _SET_REPORT_SEMAPHORE_C, _PAYLOAD, Payload),
        SemaphoreD));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-c7c0 backend semaphore
//!
//! \sa Writec7c0Semaphore()
//!
RC SemaphoreChannelWrapper::Writec7c0Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;

    if (Flags & FlagSemRelWithTime)
    {
        SemaphoreD |= DRF_DEF(C7C0, _REPORT_SEMAPHORE_EXELWTE,
                             _STRUCTURE_SIZE, _SEMAPHORE_FOUR_WORDS);
    }
    else if (Payload >> 32)
    {
        SemaphoreD |= DRF_DEF(C7C0, _REPORT_SEMAPHORE_EXELWTE,
                             _STRUCTURE_SIZE, _SEMAPHORE_TWO_WORDS);
    }
    else
    {
        SemaphoreD |= DRF_DEF(C7C0, _REPORT_SEMAPHORE_EXELWTE,
                             _STRUCTURE_SIZE, _SEMAPHORE_ONE_WORD);
    }

    CHECK_RC(GetWrapper()->Write(
        Subch, LWC7C0_SET_REPORT_SEMAPHORE_PAYLOAD_LOWER,
            DRF_NUM(C7C0, _SET_REPORT_SEMAPHORE_PAYLOAD_LOWER, _PAYLOAD_LOWER, 
                (UINT32)Payload)));

    if (Payload >> 32)
    {
        SemaphoreD |= DRF_DEF(C7C0, _REPORT_SEMAPHORE_EXELWTE,
                             _PAYLOAD_SIZE64 , _TRUE);
        CHECK_RC(GetWrapper()->Write(
            Subch, LWC7C0_SET_REPORT_SEMAPHORE_PAYLOAD_UPPER,
                DRF_NUM(C7C0, _SET_REPORT_SEMAPHORE_PAYLOAD_UPPER, _PAYLOAD_UPPER, 
                    (UINT32)(Payload >> 32))));
    }

    CHECK_RC(GetWrapper()->Write(
        Subch, LWC7C0_SET_REPORT_SEMAPHORE_ADDRESS_LOWER, 3,
        DRF_NUM(C7C0, _SET_REPORT_SEMAPHORE_ADDRESS_LOWER, _LOWER, (UINT32)GpuAddr),
        DRF_NUM(C7C0, _SET_REPORT_SEMAPHORE_ADDRESS_UPPER, _UPPER, (UINT32)(GpuAddr >> 32)),
        SemaphoreD));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-c797 backend semaphore
//!
//! \sa Writec797Semaphore()
//!
RC SemaphoreChannelWrapper::Writec797Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;

    if (Flags & FlagSemRelWithTime)
    {
        SemaphoreD |= DRF_DEF(C797, _REPORT_SEMAPHORE_EXELWTE,
                             _STRUCTURE_SIZE, _SEMAPHORE_FOUR_WORDS);
    }
    else if (Payload >> 32)
    {
        SemaphoreD |= DRF_DEF(C797, _REPORT_SEMAPHORE_EXELWTE,
                             _STRUCTURE_SIZE, _SEMAPHORE_TWO_WORDS);
    }
    else
    {
        SemaphoreD |= DRF_DEF(C797, _REPORT_SEMAPHORE_EXELWTE,
                             _STRUCTURE_SIZE, _SEMAPHORE_ONE_WORD);
    }

    CHECK_RC(GetWrapper()->Write(
        Subch, LWC797_SET_REPORT_SEMAPHORE_PAYLOAD_LOWER, 
            DRF_NUM(C797, _SET_REPORT_SEMAPHORE_PAYLOAD_LOWER, _PAYLOAD_LOWER, 
                (UINT32)Payload)));

    if (Payload >> 32)
    {
        SemaphoreD |= DRF_DEF(C797, _REPORT_SEMAPHORE_EXELWTE,
                             _PAYLOAD_SIZE64 , _TRUE);
        CHECK_RC(GetWrapper()->Write( 
            Subch, LWC797_SET_REPORT_SEMAPHORE_PAYLOAD_UPPER, 
                DRF_NUM(C797, _SET_REPORT_SEMAPHORE_PAYLOAD_UPPER, _PAYLOAD_UPPER, 
                    (UINT32)(Payload >> 32))));
    }

    CHECK_RC(GetWrapper()->Write(
        Subch, LWC797_SET_REPORT_SEMAPHORE_ADDRESS_LOWER, 3,
        DRF_NUM(C797, _SET_REPORT_SEMAPHORE_ADDRESS_LOWER, _LOWER, (UINT32)GpuAddr),
        DRF_NUM(C797, _SET_REPORT_SEMAPHORE_ADDRESS_UPPER, _UPPER, (UINT32)(GpuAddr >> 32)),
        SemaphoreD));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-c7b5 backend semaphore
//!
//! \sa Writec7b5Semaphore()
//!
RC SemaphoreChannelWrapper::Writec7b5Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 LaunchDma
)
{
    RC rc;
    
    LaunchDma |= DRF_DEF(C7B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NONE);
    LaunchDma |= DRF_DEF(C7B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE);

    if (Flags & FlagSemRelWithTime)
    {
        LaunchDma |= DRF_DEF(C7B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,
                            _RELEASE_FOUR_WORD_SEMAPHORE);
    }
    else
    {
        LaunchDma |= DRF_DEF(C7B5, _LAUNCH_DMA, _SEMAPHORE_TYPE,
                            _RELEASE_ONE_WORD_SEMAPHORE);
    }

    CHECK_RC(GetWrapper()->Write(Subch, LWC7B5_SET_SEMAPHORE_A, 3,
        DRF_NUM(C7B5, _SET_SEMAPHORE_A, _UPPER, (UINT32)(GpuAddr >> 32)),
        DRF_NUM(C7B5, _SET_SEMAPHORE_B, _LOWER, (UINT32)GpuAddr),
        DRF_NUM(C7B5, _SET_SEMAPHORE_PAYLOAD, _PAYLOAD, (UINT32)Payload)));

    if (Payload >> 32)
    {
        LaunchDma |= DRF_DEF(C7B5, _LAUNCH_DMA, _SEMAPHORE_PAYLOAD_SIZE,
                            _TWO_WORD);
        CHECK_RC(GetWrapper()->Write(Subch, LWC7B5_SET_SEMAPHORE_PAYLOAD_UPPER,
            DRF_NUM(C7B5, _SET_SEMAPHORE_PAYLOAD_UPPER, _PAYLOAD, 
                (UINT32)(Payload >> 32))));
    }

    CHECK_RC(GetWrapper()->Write(Subch, LWC7B5_LAUNCH_DMA, LaunchDma));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-c7b7 backend semaphore
//!
//! \sa Writec7b7Semaphore()
//!
RC SemaphoreChannelWrapper::Writec7b7Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;

    if (Flags & FlagSemRelWithTime)
    {
        SemaphoreD |= DRF_DEF(C7B7, _SEMAPHORE_D, _STRUCTURE_SIZE, _FOUR);
    }
    else if (Payload >> 32)
    {
        SemaphoreD |= DRF_DEF(C7B7, _SEMAPHORE_D, _STRUCTURE_SIZE, _TWO);
    }
    else
    {
        SemaphoreD |= DRF_DEF(C7B7, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE);
    }

    CHECK_RC(GetWrapper()->Write(Subch, LWC7B7_SEMAPHORE_A, 2,
        DRF_NUM(C7B7, _SEMAPHORE_A, _UPPER, (UINT32)(GpuAddr >> 32)),
        DRF_NUM(C7B7, _SEMAPHORE_B, _LOWER, (UINT32)GpuAddr)));
    CHECK_RC(GetWrapper()->Write(Subch, LWC7B7_SET_SEMAPHORE_PAYLOAD_LOWER,
        DRF_NUM(C7B7, _SET_SEMAPHORE_PAYLOAD_LOWER, _PAYLOAD_LOWER, 
            (UINT32)Payload)));

    if (Payload >> 32)
    {
        SemaphoreD |= DRF_DEF(C7B7, _SEMAPHORE_D, _PAYLOAD_SIZE, _64BIT);
        CHECK_RC(GetWrapper()->Write(Subch, LWC7B7_SET_SEMAPHORE_PAYLOAD_UPPER,
            DRF_NUM(C7B7, _SET_SEMAPHORE_PAYLOAD_UPPER, _PAYLOAD_UPPER, 
                (UINT32)(Payload >> 32))));
    }

    CHECK_RC(GetWrapper()->Write(Subch, LWC7B7_SEMAPHORE_D, SemaphoreD));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-c7b0 backend semaphore
//!
//! \sa Writec7b0Semaphore()
//!
RC SemaphoreChannelWrapper::Writec7b0Semaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;
    
    if (Flags & FlagSemRelWithTime)
    {
        SemaphoreD |= DRF_DEF(C7B0, _SEMAPHORE_D, _STRUCTURE_SIZE, _FOUR);
    }
    else if (Payload >> 32)
    {
        SemaphoreD |= DRF_DEF(C7B0, _SEMAPHORE_D, _STRUCTURE_SIZE, _TWO);
    }
    else
    {
        SemaphoreD |= DRF_DEF(C7B0, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE);
    }

    CHECK_RC(GetWrapper()->Write(Subch, LWC7B0_SEMAPHORE_A, 2,
        DRF_NUM(C7B0, _SEMAPHORE_A, _UPPER, (UINT32)(GpuAddr >> 32)),
        DRF_NUM(C7B0, _SEMAPHORE_B, _LOWER, (UINT32)GpuAddr)));
    CHECK_RC(GetWrapper()->Write(Subch, LWC7B0_SET_SEMAPHORE_PAYLOAD_LOWER,
        DRF_NUM(C7B0, _SET_SEMAPHORE_PAYLOAD_LOWER, _PAYLOAD_LOWER, 
            (UINT32)Payload)));
    
    if (Payload >> 32)
    {
        SemaphoreD |= DRF_DEF(C7B0, _SEMAPHORE_D, _PAYLOAD_SIZE, _64BIT);
        CHECK_RC(GetWrapper()->Write(Subch, LWC7B0_SET_SEMAPHORE_PAYLOAD_UPPER,
            DRF_NUM(C7B0, _SET_SEMAPHORE_PAYLOAD_UPPER, _PAYLOAD_UPPER, 
                (UINT32)(Payload >> 32))));
    }

    CHECK_RC(GetWrapper()->Write(Subch, LWC7B0_SEMAPHORE_D, SemaphoreD));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Write a class-c7fa backend semaphore
//!
//! \sa Writec7faSemaphore()
//!
RC SemaphoreChannelWrapper::Writec7faSemaphore
(
    UINT32 Flags,
    UINT32 Subch,
    UINT64 GpuAddr,
    UINT64 Payload,
    UINT32 SemaphoreD
)
{
    RC rc;
    
    if (Flags & FlagSemRelWithTime)
    {
        SemaphoreD |= DRF_DEF(C7FA, _SEMAPHORE_D, _STRUCTURE_SIZE, _FOUR);
    }
    else if (Payload >> 32)
    {
        SemaphoreD |= DRF_DEF(C7FA, _SEMAPHORE_D, _STRUCTURE_SIZE, _TWO);
    }
    else
    {
        SemaphoreD |= DRF_DEF(C7FA, _SEMAPHORE_D, _STRUCTURE_SIZE, _ONE);
    }

    CHECK_RC(GetWrapper()->Write(Subch, LWC7FA_SEMAPHORE_A, 2,
        DRF_NUM(C7FA, _SEMAPHORE_A, _UPPER, (UINT32)(GpuAddr >> 32)),
        DRF_NUM(C7FA, _SEMAPHORE_B, _LOWER, (UINT32)GpuAddr)));
    CHECK_RC(GetWrapper()->Write(Subch, LWC7FA_SET_SEMAPHORE_PAYLOAD_LOWER,
        DRF_NUM(C7FA, _SET_SEMAPHORE_PAYLOAD_LOWER, _PAYLOAD_LOWER, 
            (UINT32)Payload)));
    
    if (Payload >> 32)
    {
        SemaphoreD |= DRF_DEF(C7FA, _SEMAPHORE_D, _PAYLOAD_SIZE, _64BIT);
        CHECK_RC(GetWrapper()->Write(Subch, LWC7FA_SET_SEMAPHORE_PAYLOAD_UPPER,
            DRF_NUM(C7FA, _SET_SEMAPHORE_PAYLOAD_UPPER, _PAYLOAD_UPPER, 
                (UINT32)(Payload >> 32))));
    }

    CHECK_RC(GetWrapper()->Write(Subch, LWC7FA_SEMAPHORE_D, SemaphoreD));

    return rc;
}

SemaphoreChannelWrapper::SemState::SemState() :
    m_SubdevMask(0),
    m_SemOffset(0),
    m_SemReleaseFlags(FlagSemRelDefault),
    m_SemaphorePayloadSize(SEM_PAYLOAD_SIZE_AUTO),
    m_HostDirty(false),
    m_DirtySubchannels(0),
    m_SubdevMaskDirty(false),
    m_SemOffsetDirty(false),
    m_SemReleaseFlagsDirty(false),
    m_SemaphorePayloadSizeDirty(false)
{
}

//--------------------------------------------------------------------
//! \brief Tell the SemState which subdevices are lwrrently selected.
//!
//! This method should be called on m_SemStateStack.top() when
//! WriteSetSubdevice() is called.
//!
//! \param Mask Bitmask of the subdevices selected by
//!     WriteSetSubdevice().  This isn't quite the same as the
//!     WriteSetSubdevice() arg: if Channel::AllSubdevicesMask methods
//!     is passed in, the caller should pass a mask of the subdevices
//!     that actually exist.  (Otherwise, AddSemData() will act as if
//!     subsequent data was sent to a dozen subdevices.)
//!
void SemaphoreChannelWrapper::SemState::SetSubdevMask(UINT32 Mask)
{
    m_SubdevMask = Mask;
    m_SubdevMaskDirty = false;
}

//--------------------------------------------------------------------
//! \brief Tell the SemState about the semaphore offset.
//!
//! This method should be called on m_SemStateStack.top() when
//! SetSemReleaseFlags() is called.
//!
//! \param offset will be referenced on releasing semaphore
//!
void SemaphoreChannelWrapper::SemState::SetSemOffset(UINT64 offset)
{
    m_SemOffset = offset;
    m_SemOffsetDirty = false;
}

//--------------------------------------------------------------------
//! \brief Tell the SemState about the semaphore releases flags.
//!
//! This method should be called on m_SemStateStack.top() when
//! SetSemReleaseFlags() is called.
//!
//! \param flags will be referenced on releasing semaphore
//!
void SemaphoreChannelWrapper::SemState::SetSemReleaseFlags(UINT32 flags)
{
    m_SemReleaseFlags = flags;
    m_SemReleaseFlagsDirty = false;
}

//--------------------------------------------------------------------
//! \brief Tell the SemState about the semaphore payload size.
//!
//! This method should be called on m_SemStateStack.top() when
//! SetSemaphorePayloadSize() is called.
//!
//! \param payload size will be referenced on releasing semaphore
//!
void SemaphoreChannelWrapper::SemState::SetSemaphorePayloadSize(SemaphorePayloadSize size)
{
    m_SemaphorePayloadSize = size;
    m_SemaphorePayloadSizeDirty = false;
}

//--------------------------------------------------------------------
//! \brief Add the indicated semaphore method/data to the SemState.
//!
//! This method should be called on m_SemStateStack.top() when a
//! semaphore method is called.
//!
//! \param Subch The subchannel the method was sent on, or 0 for host
//!     methods.
//! \param Method The method number.
//! \param Data The method data.  This function uses the most recent
//!     SetSubdevMask() call to determine which subdevices the Data
//!     was sent to, and modifies SemData::Mask2Data appropriately.
//!
void SemaphoreChannelWrapper::SemState::AddSemData
(
    UINT32 Subch,
    UINT32 Method,
    UINT32 Data
)
{
    UINT64 Key = GetKey(Subch, Method);

    if (m_SemData.count(Key) == 0)
    {
        m_SemData[Key] = SemData(Subch, Method);
    }
    SemData *pSemData = &m_SemData[Key];

    for (UINT32 ii = 0; ii < 32; ++ii)
    {
        UINT32 MaskBit = 1 << ii;
        if (MaskBit & m_SubdevMask)
        {
            pSemData->Mask2Data[MaskBit] = Data;
            pSemData->DirtyMask &= ~MaskBit;
        }
    }
}

//--------------------------------------------------------------------
//! \brief Delete the indicated semaphore method/data from the SemState
//!
//! This method should be called on m_SemStateStack.top() when a
//! semaphore method is ilwalidated.  Lwrrently, the only such case is
//! host semaphores: there are two sets of host-semaphore methods, and
//! sending one set of methods causes the GPU to "forget" the data
//! sent by the other set.
//!
//! This function uses the most recent SetSubdevMask() call to
//! determine which subdevices to ilwalidate the method on.
//!
//! \param Subch The subchannel the method was sent on, or 0 for host
//!     methods.
//! \param Method The method number.
//!
void SemaphoreChannelWrapper::SemState::DelSemData
(
    UINT32 Subch,
    UINT32 Method
)
{
    UINT64 Key = GetKey(Subch, Method);
    if (m_SemData.count(Key))
    {
        SemData *pSemData = &m_SemData[Key];

        for (UINT32 ii = 0; ii < 32; ++ii)
        {
            UINT32 MaskBit = 1 << ii;
            pSemData->Mask2Data.erase(MaskBit);
            pSemData->DirtyMask &= ~MaskBit;
        }
    }
}

//--------------------------------------------------------------------
//! \brief Return a vector of all SemData structs in the SemState
//!
void SemaphoreChannelWrapper::SemState::GetAllSemData
(
    vector<SemData> *pSemDataArray
) const
{
    pSemDataArray->clear();
    pSemDataArray->reserve(m_SemData.size());
    for (map<UINT64, SemData>::const_iterator pSemData = m_SemData.begin();
         pSemData != m_SemData.end(); ++pSemData)
    {
        pSemDataArray->push_back(pSemData->second);
    }
}

//--------------------------------------------------------------------
//! \brief Mark the semaphore state dirty after inserted methods
//!
//! This function helps to restore the previous semaphore state after
//! some methods are inserted between BeginInsertedMethods() and
//! EndInsertedMethods().  It is called by EndInsertedMethods().
//!
//! This function compares the semaphore state left by the inserted
//! methods (i.e. the state just popped from m_SemStateStack) with the
//! current semaphore state (i.e. the top of m_SemStateStack after the
//! pop), and marks the current state "dirty" if any information was
//! overwritten by the inserted methods.
//!
//! There are several situations that require an inserted method to be
//! the last thing in a pushbuffer or runlist entry, so we can't clean
//! up the dirty data immediately.  Instead, we clean it up just
//! before the next write (if any) by calling
//! CleanupDirtySemaphoreState().
//!
//! \sa CleanupDirtySemaphoreState()
//!
//! \param pInsertedSemState The semaphore state that was just popped
//!     from m_SemStateStack
//!
void SemaphoreChannelWrapper::SemState::SetDirty
(
    const SemState *pInsertedSemState
)
{
    MASSERT(pInsertedSemState != NULL);

    m_HostDirty = false;
    m_DirtySubchannels = 0;

    m_SubdevMaskDirty =
        (pInsertedSemState->m_SubdevMaskDirty ||
         pInsertedSemState->m_SubdevMask != m_SubdevMask);
    m_SemOffsetDirty =
        (pInsertedSemState->m_SemOffsetDirty ||
         pInsertedSemState->m_SemOffset != m_SemOffset);
    m_SemReleaseFlagsDirty =
        (pInsertedSemState->m_SemReleaseFlagsDirty ||
         pInsertedSemState->m_SemReleaseFlags != m_SemReleaseFlags);
    m_SemaphorePayloadSizeDirty =
        (pInsertedSemState->m_SemaphorePayloadSizeDirty ||
         pInsertedSemState->m_SemaphorePayloadSize != m_SemaphorePayloadSize);

    if (m_SubdevMaskDirty || m_SemReleaseFlagsDirty ||
        m_SemaphorePayloadSizeDirty || m_SemOffsetDirty)
    {
        m_HostDirty = true;
    }

    for (map<UINT64, SemData>::iterator iter = m_SemData.begin();
         iter != m_SemData.end(); ++iter)
    {
        SemData *pSemData = &iter->second;
        const SemData *pInsertedSemData = NULL;

        map<UINT64, SemData>::const_iterator insertedSemDataIter =
            pInsertedSemState->m_SemData.find(iter->first);
        if (insertedSemDataIter != pInsertedSemState->m_SemData.end())
            pInsertedSemData = &insertedSemDataIter->second;

        pSemData->DirtyMask = 0;
        for (map<UINT32, UINT32>::iterator iter2 = pSemData->Mask2Data.begin();
             iter2 != pSemData->Mask2Data.end(); ++iter2)
        {
            UINT32 Mask = iter2->first;
            UINT32 Data = iter2->second;

            if ((pInsertedSemData == NULL) ||
                (pInsertedSemData->DirtyMask & Mask))
            {
                pSemData->DirtyMask |= Mask;
            }
            else
            {
                map<UINT32, UINT32>::const_iterator insertedDataIter =
                    pInsertedSemData->Mask2Data.find(Mask);
                if (insertedDataIter == pInsertedSemData->Mask2Data.end() ||
                    insertedDataIter->second != Data)
                {
                    pSemData->DirtyMask |= Mask;
                }
            }
        }

        if (pSemData->DirtyMask != 0)
        {
            if (pSemData->Method >= MIN_ENGINE_METHOD)
                m_DirtySubchannels |= (1 << pSemData->Subch);
            else
                m_HostDirty = true;
        }
    }
}

bool SemaphoreChannelWrapper::SemState::IsDirty(UINT32 SubchMask) const
{
    return m_HostDirty || ((SubchMask & m_DirtySubchannels) != 0);
}

//--------------------------------------------------------------------
//! \brief Return the key used to index into m_SemData
//!
/* static */ UINT64 SemaphoreChannelWrapper::SemState::GetKey
(
    UINT32 Subch,
    UINT32 Method
)
{
    return (((UINT64)Subch) << 32) + Method;
}
