/* LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file rmt_channelMemInfo.cpp
//! \brief LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO functionality test
//!
//! This test tries to retrieve the channel memory details using the
//! LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO control call.
//! The call returns the physical base address, size and aperture of
//! each of instance memory, ramfc, and method buffers of a channel.

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "ctrl/ctrl2080.h"
#include "core/include/memcheck.h"

class ChannelMemInfo: public RmTest
{
public:
    ChannelMemInfo();
    virtual ~ChannelMemInfo();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:

    LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO_PARAMS m_getChannelMemParams = {};
    RC GetChannelMemInfo();

};

//! \brief ChannelMemInfo constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
ChannelMemInfo::ChannelMemInfo()
{
    SetName("ChannelMemInfo");
}

//! \brief ChannelMemInfo destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
ChannelMemInfo::~ChannelMemInfo()
{
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
RC ChannelMemInfo::Setup()
{
    RC rc = OK;

    // Allocate a channel
    CHECK_RC(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh));

    m_getChannelMemParams.hChannel = m_hCh;

    return OK;
}

//! \brief Whether or not the test is supported in the current environment
//!
//! If supported then the test will be ilwoked.
//!
//! \return True
//!
//-----------------------------------------------------------------------------

string ChannelMemInfo::IsTestSupported()
{

    if (GetBoundGpuSubdevice()->HasFeature(Device::GPUSUB_HAS_GPFIFO_CHANNEL))
        return RUN_RMTEST_TRUE;
    else
        return "GPUSUB_HAS_GPFIFO_CHANNEL feature is not supported on current platform";
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC ChannelMemInfo::Run()
{
    RC rc;
    LW2080_CTRL_FIFO_CHANNEL_MEM_INFO *pMemInfo;

    CHECK_RC(GetChannelMemInfo());

    if(rc == OK)
    {
        UINT32 i;

        pMemInfo = &m_getChannelMemParams.chMemInfo;

        Printf(Tee::PriNormal, "Instance base : 0x%llx\n", pMemInfo->inst.base);
        Printf(Tee::PriNormal, "Instance size : 0x%llx\n", pMemInfo->inst.size);
        Printf(Tee::PriNormal, "Instance aperture : 0x%x\n", (UINT32)pMemInfo->inst.aperture);

        Printf(Tee::PriNormal, "Ramfc base : 0x%llx\n", pMemInfo->ramfc.base);
        Printf(Tee::PriNormal, "Ramfc size : 0x%llx\n", pMemInfo->ramfc.size);
        Printf(Tee::PriNormal, "Ramfc aperture : 0x%x\n", (UINT32)pMemInfo->ramfc.aperture);

        Printf(Tee::PriNormal, "Count of Method Buffers : %d\n", pMemInfo->methodBufCount);
        for (i = 0; i < pMemInfo->methodBufCount; i++)
        {
            Printf(Tee::PriNormal, "Method Buffer %d base : 0x%llx\n", i, pMemInfo->methodBuf[i].base);
            Printf(Tee::PriNormal, "Method Buffer %d size : 0x%llx\n", i, pMemInfo->methodBuf[i].size);
            Printf(Tee::PriNormal, "Method Buffer %d aperture : 0x%x\n", i, (UINT32)pMemInfo->methodBuf[i].aperture);
        }
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC ChannelMemInfo::Cleanup()
{
    RC rc;
    CHECK_RC(m_TestConfig.FreeChannel(m_pCh));
    return OK;
}

//! \brief Try to retrieve channel memory info
//-----------------------------------------------------------------------------
RC ChannelMemInfo::GetChannelMemInfo()
{
    LwRmPtr pLwRm;
    RC rc = OK;

    rc = pLwRm->Control(pLwRm->GetSubdeviceHandle(GetBoundGpuSubdevice()),
                        LW2080_CTRL_CMD_FIFO_GET_CHANNEL_MEM_INFO,
                        &m_getChannelMemParams,
                        sizeof (m_getChannelMemParams));

    return rc;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------
//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ ChannelMemInfo
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(ChannelMemInfo, RmTest,
                 "ChannelMemInfo RM test.");

