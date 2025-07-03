/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdastst.h"
#include "gpu/utility/chanwmgr.h"
#include "gpu/include/gpudev.h"
#include "core/include/help.h"
#include "gpu/include/testdevice.h"

//! \brief Default name for LwdaTests
//------------------------------------------------------------------------------
LwdaStreamTest::LwdaStreamTest()
{
    SetName("LwdaStreamTest");
}

//! \brief LwdaStreamTest doesn't do much, we leave that to the child classes
//------------------------------------------------------------------------------
LwdaStreamTest::~LwdaStreamTest()
{
}

//! \brief Determine whether a corresponding LWCA device has successfuly been bound
bool LwdaStreamTest::IsSupported()
{
    if (ChannelWrapperMgr::Instance()->UsesRunlistWrapper())
        return false;

    return m_LwdaDevice.IsValid();
}

/**
 * Override GpuTest's BindTestDevice, so we can figure out which LWCA
 * device instance matches this gpu device.
 */
RC LwdaStreamTest::BindTestDevice(TestDevicePtr pTestDevice, JSContext* cx, JSObject* obj)
{
    RC rc;
    CHECK_RC(GpuTest::BindTestDevice(pTestDevice, cx, obj));

    auto pSubdev = pTestDevice->GetInterface<GpuSubdevice>();
    CHECK_RC(m_Lwda.Init());
    if (pSubdev)
    {
        m_Lwda.FindDevice(*(pSubdev->GetParentDevice()), &m_LwdaDevice);
    }
    return rc;
}

Lwca::Device LwdaStreamTest::GetBoundLwdaDevice() const
{
    if (!m_LwdaDevice.IsValid())
    {
        MASSERT(!"A LWCA test should not be exelwted if LWCA device hasn't been successfuly identified!\n");
    }
    return m_LwdaDevice;
}

RC LwdaStreamTest::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());
    CHECK_RC(AllocDisplay());     // Required to enable interrupts
    CHECK_RC(m_Lwda.InitContext(m_LwdaDevice, NumSecondaryCtx(), GetDefaultActiveContext()));
    return OK;
}

RC LwdaStreamTest::Cleanup()
{
    m_Lwda.Free();
    return (GpuTest::Cleanup());
}

//-----------------------------------------------------------------------------
//! \brief Identify the type of ModsTest subclass
//!
/* virtual */ bool LwdaStreamTest::IsTestType(TestType tt)
{
    return (tt == LWDA_STREAM_TEST) || GpuTest::IsTestType(tt);
}

void LwdaStreamTest::PrintDeviceProperties(const Tee::Priority pri)
{
    if (m_LwdaDevice.IsValid())
    {
        const char * computeMode[] = {
            "Default,(Multiple contexts allowed per device)",
            "Exclusive,(Only one context can be present on this device at a time)",
            "Prohibited,(No contexts can be created on this device at this time)",
            "Unknown"
        };
        LWdevprop prop;
        int cmode = m_LwdaDevice.GetAttribute(LW_DEVICE_ATTRIBUTE_COMPUTE_MODE);
        m_LwdaDevice.GetProperties(&prop);

        Printf(pri,
           "lwdaDeviceProperties:"
           "\n\tname:                    %s"
           "\n\tsharedMemPerBlock:       %d"
           "\n\tregsPerBlock:            %d"
           "\n\twarpSize:                %d"
           "\n\tmemPitch:                %d"
           "\n\tmaxThreadsPerBlock:      %d"
           "\n\tmaxThreadsDim[3]:        %d,%d,%d"
           "\n\tmaxGridSize[3]:          %d,%d,%d"
           "\n\tclockRate:               %d"
           "\n\ttotalConstMem:           %d"
           "\n\tmajor:                   %d"
           "\n\tminor:                   %d"
           "\n\ttextureAlignment:        %d"
           "\n\tnumSMs:                  %d"
           "\n\tkernelTimeout:           %d"
           "\n\tcomputeMode:             %s"
           "\n",
           m_LwdaDevice.GetName().c_str(),
           prop.sharedMemPerBlock,
           prop.regsPerBlock,
           m_LwdaDevice.GetAttribute(LW_DEVICE_ATTRIBUTE_WARP_SIZE),
           prop.memPitch,
           prop.maxThreadsPerBlock,
           prop.maxThreadsDim[0],prop.maxThreadsDim[1],prop.maxThreadsDim[2],
           prop.maxGridSize[0], prop.maxGridSize[1],prop.maxGridSize[2],
           prop.clockRate,
           m_LwdaDevice.GetAttribute(LW_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY),
           m_LwdaDevice.GetCapability().MajorVersion(),
           m_LwdaDevice.GetCapability().MinorVersion(),
           m_LwdaDevice.GetAttribute(LW_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT),
           m_LwdaDevice.GetAttribute(LW_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT),
           m_LwdaDevice.GetAttribute(LW_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT),
           (cmode < 4) ? computeMode[cmode] : computeMode[3]);
    }
}

RC LwdaStreamTest::ReportFailingSmids(const map<UINT16, UINT64>& errorCountMap,
                                      const UINT64 failureLimit)
{
    RC rc;
    for (const auto& smPair : errorCountMap)
    {
        const auto smid = smPair.first;
        const auto miscompareCount = smPair.second;
        VerbosePrintf("SMID%d: %llu miscompares reported (limit %llu)\n",
                      smid, miscompareCount, failureLimit);
        if (miscompareCount >= failureLimit)
        {
            UINT32 hwGpcNum = -1;
            UINT32 hwTpcNum = -1;
            CHECK_RC(GetBoundGpuSubdevice()->SmidToHwGpcHwTpc(smid, &hwGpcNum, &hwTpcNum));

            // This print is parsed by SLT, so be wary of changing it
            Printf(Tee::PriError, "Failing SMID: %d GPC: %d TPC: %d SM: %d\n",
                                  smid, hwGpcNum, hwTpcNum, smid % 2);
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
// JS linkage
//------------------------------------------------------------------------------
JS_CLASS_VIRTUAL_INHERIT(LwdaStreamTest, GpuTest, "LWCA test base class");

void LwdaStreamTest::PropertyHelp(JSContext *pCtx,regex *pRegex)
{
    JSObject *pLwrJObj = JS_NewObject(pCtx,&LwdaStreamTest_class,0,0);
    JS_SetPrivate(pCtx,pLwrJObj,this);
    Help::JsPropertyHelp(pLwrJObj,pCtx,pRegex,0);
    JS_SetPrivate(pCtx,pLwrJObj,nullptr);
    GpuTest::PropertyHelp(pCtx,pRegex);
}

