/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/rmtest.h"
#include "uprocrtos.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/lwrm.h"
#include "lwRmApi.h"
#include "core/include/platform.h"
#include "uprocifcmn.h"
#include "lwmisc.h"

//! \brief Constructor
//!
UprocRtos::UprocRtos(GpuSubdevice *pParent) :
    m_UprocId(0),
    m_pRt(nullptr),
    m_UprocTest(nullptr),
    m_Initialized(false),
    m_Handle((LwRm::Handle)0),
    m_Subdevice(pParent)
{
}

//! \brief Set RM Test handle. This is not specific to uproc instance,
//!        so not a virtual function.
//!
void UprocRtos::SetRmTestHandle(RmTest *pRmTestHandle)
{
    if (pRmTestHandle != nullptr)
        m_pRt = pRmTestHandle;
    return;
}

//-----------------------------------------------------------------------------
//! \brief Dump the header from the Uproc command, event or message
//!
//! \param pHdr    : Pointer to the header to dump
//! \param hdrType : Type of header to dump
//!
void UprocRtos::DumpHeader(void *pHdr, UprocHeaderType hdrType)
{
    if (!Tee::WillPrint(Tee::PriDebug))
        return;

    RM_FLCN_QUEUE_HDR hdr;
    Platform::MemCopy(&hdr, pHdr, RM_FLCN_QUEUE_HDR_SIZE);
    Platform::FlushCpuWriteCombineBuffer();

    if (hdrType == COMMAND_HDR)
    {
        Printf(Tee::PriDebug, "UprocRtos Command : ");
    }
    else if (hdrType == EVENT_HDR)
    {
        Printf(Tee::PriDebug, "UprocRtos Event : ");
    }
    else if (hdrType == MESSAGE_HDR)
    {
        Printf(Tee::PriDebug, "UprocRtos Message : ");
    }
    Printf(Tee::PriDebug,
           "unitId=0x%02x, size=0x%02x, "
           "ctrlFlags=0x%02x, seqNumId=0x%02x\n",
           hdr.unitId, hdr.size, hdr.ctrlFlags, hdr.seqNumId);
}

//! \brief Allocate and map a surface for use with the SEC2
//!
//! \param pSurface : Pointer to the surface to allocate
//! \param Size     : Size in bytes for the surface
//!
//! \return OK if successful
RC UprocRtos::CreateSurface(Surface2D *pSurface, LwU32 size)
{

    LwU32 bytesPerPixel = ColorUtils::PixelBytes(ColorUtils::VOID32);
    LwU32 width;
    RC rc;

    size  = lwNextPow2_U32(size);
    width = (size + bytesPerPixel - 1) / bytesPerPixel;
    size  = width * bytesPerPixel;

    pSurface->SetWidth(width);
    pSurface->SetPitch(size);
    pSurface->SetHeight(1);
    pSurface->SetColorFormat(ColorUtils::VOID32);
    pSurface->SetLocation(Memory::Coherent);
    pSurface->SetAddressModel(Memory::Paging);
    pSurface->SetKernelMapping(true);

    CHECK_RC(pSurface->Alloc(m_Subdevice->GetParentDevice()));
    Utility::CleanupOnReturn<Surface2D,void>
        FreeSurface(pSurface,&Surface2D::Free);
    CHECK_RC(pSurface->Fill(0, m_Subdevice->GetSubdeviceInst()));
    CHECK_RC(pSurface->Map(m_Subdevice->GetSubdeviceInst()));
    FreeSurface.Cancel();

    return rc;
}

//!
//! @brief      Run supported uproc tests on this object
//!
//! @param[in]  test  The test to be run
//!
//! @return     OK if no test errors, otherwise corresponding RC
//!
RC UprocRtos::UprocRtosRunTests(LwU32 test, ...)
{
    RC rc;
    va_list args;
    va_start(args, test);
    m_UprocTest = new UprocTest(this);
    rc = m_UprocTest->RunTest(test, &args);
    va_end(args);
    return rc;
}

//!
//! \brief Reset the Uproc instance
//!
void UprocRtos::Reset()
{
    m_Handle = 0;
    m_pRt = nullptr;
    m_Initialized = false;
}
