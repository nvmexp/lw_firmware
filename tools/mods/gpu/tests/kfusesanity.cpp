/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file kfusesanity.cpp
 * @brief: checks the validity of KFuses after a full suite of MODS
 * This test should be run last
 *
 */

#include "gpu/include/gpusbdev.h"
#include "gputest.h"
#include "mods_reg_hal.h"
#include "core/include/utility.h"

#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

/**
 *  @class KFuseSanity
 *  @brief KFuse are the HDCP keys for chips > GT21x
 *  1)  Kick off the state machine
 *  2)  Read some status bits -> report failure
 *  3)  Make sure all keys are not read back as Zeroes
 *
 */
class KFuseSanity : public GpuTest
{
public:
    KFuseSanity();
    ~KFuseSanity();

    virtual RC   Setup();
    virtual RC   Run();
    virtual RC   Cleanup();
    virtual bool IsSupported();

private:

    GpuTestConfiguration       *m_pTestConfig;
    RC InnerLoop();
};
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(KFuseSanity, GpuTest, "K-Fuse Sanity");

//-----------------------------------------------------------------------------

KFuseSanity::KFuseSanity()
{
    m_pTestConfig = GetTestConfiguration();
}

KFuseSanity::~KFuseSanity()
{
}

RC KFuseSanity::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    return rc;
}

RC KFuseSanity::Run()
{
    RC rc;
    for (UINT32 i = 0; i < m_pTestConfig->Loops(); i++)
    {
        string ErrorMsg = Utility::StrPrintf(
                                    "Error: KFuseSanity failed at loop %d\n",i);
        CHECK_RC_MSG(InnerLoop(), ErrorMsg.c_str());
    }
    return rc;
}

RC KFuseSanity::Cleanup()
{
    return OK;
}

bool KFuseSanity::IsSupported()
{
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();

    if ((pGpuSubdevice->Regs().IsSupported(MODS_FUSE_KFUSE_STATE) &&
        !pGpuSubdevice->Regs().HasRWAccess(MODS_FUSE_KFUSE_STATE)) ||
        (pGpuSubdevice->Regs().IsSupported(MODS_FUSE_KFUSE_KEYADDR) &&
        !pGpuSubdevice->Regs().HasRWAccess(MODS_FUSE_KFUSE_KEYADDR)) ||
        (pGpuSubdevice->Regs().IsSupported(MODS_FUSE_KFUSE_DEBUG) &&
        !pGpuSubdevice->Regs().HasRWAccess(MODS_FUSE_KFUSE_DEBUG)))
    {
        return false;
    }

    return ((pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_KFUSES) &&
             pGpuSubdevice->IsFeatureEnabled(Device::GPUSUB_HAS_KFUSES)) ||
            (pGpuSubdevice->IsSOC() &&
             CheetAh::SocPtr()->HasFeature(Device::SOC_SUPPORTS_KFUSE_ACCESS)));
}

//-----------------------------------------------------------------------------
// Private functions
RC KFuseSanity::InnerLoop()
{
    StickyRC rc;
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();
    GpuSubdevice::KFuseStatus Status;
    CHECK_RC(pSubdev->GetKFuseStatus(&Status,
                                     m_pTestConfig->TimeoutMs()));

    if (!Status.CrcPass)
    {
        Printf(Tee::PriError, "KFuse CRC failed\n");
        rc = RC::EXCEEDED_EXPECTED_THRESHOLD;
    }

    if (rc != RC::OK || Status.FatalError > 0)
    {
        Printf(Tee::PriError, "Fatal errors detected: %d\n", Status.FatalError);
        Printf(Tee::PriError, "Error1=%d, Error2=%d, Error3=%d\n",
               Status.Error1, Status.Error2, Status.Error3);
        rc = RC::EXCEEDED_EXPECTED_THRESHOLD;
    }

    // check if keys == 0
    UINT32 TotalBits = 0;
    for (UINT32 i = 0; i < Status.Keys.size(); i++)
    {
        TotalBits += Utility::CountBits(Status.Keys[i]);
    }

    if (TotalBits == 0)
    {
        Printf(Tee::PriError, "All KFuse Keys read back as zero!\n");
        rc = RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    // Dont verify the HDCP_EN fuse on SOC
    if (pSubdev->IsSOC())
        return rc;

    // Per request from bug 733616: check for HDCP EN fuse if the Kfuses are good
    if (!pSubdev->Regs().Read32(MODS_FUSE_OPT_HDCP_EN))
    {
        Printf(Tee::PriNormal, "HDCP_EN is not blown\n");
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    return rc;
}
