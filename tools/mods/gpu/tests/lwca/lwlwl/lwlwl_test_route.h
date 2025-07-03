/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWLWL_TEST_ROUTE_H
#define INCLUDED_LWLWL_TEST_ROUTE_H

#include "gpu/tests/lwlink/lwldatatest/lwldt_test_route.h"
#include "core/include/rc.h"
#include "core/include/tee.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "gpu/tests/lwca/lwdawrap.h"

class GpuSubdevice;

namespace LwLinkDataTestHelper
{
    //--------------------------------------------------------------------------
    //! \brief Class which encapsulates test route for Lwca LwLink data transfer
    //! testing.  A route (just like an LwLinkRoute) represents
    //! multiple LwLinks where the source and destination device is the same.
    //!
    //! This class is mainly an informational repository and contains no actual
    //! copy functionality
    class TestRouteLwda : public TestRoute
    {
        public:
            enum
            {
                ALL_SMS = ~0U
            };
            TestRouteLwda
            (
                 const Lwca::Instance* pLwda
                ,TestDevicePtr pLocDev
                ,LwLinkRoutePtr pRoute
                ,Tee::Priority pri
            );
            virtual ~TestRouteLwda() {}

            Lwca::Device GetLocalLwdaDevice() { return m_LocalLwdaDev; }
            Lwca::Device GetRemoteLwdaDevice() { return m_RemoteLwdaDev; }
            UINT32 GetLocalMaxSms() { return m_LocalMaxSms; }
            UINT32 GetRemoteMaxSms() { return m_RemoteMaxSms; }
            UINT32 GetLocalMaxThreads() { return m_LocalMaxThreads; }
            UINT32 GetRemoteMaxThreads() { return m_RemoteMaxThreads; }
            RC GetRequiredSms
            (
                 TransferDir transDir
                ,UINT32 transferWidth
                ,bool bHwLocal
                ,UINT32 readBwPerSmKiBps
                ,UINT32 writeBwPerSmKiBps
                ,UINT32 minSms
                ,UINT32 maxSms
                ,UINT32 *pDutInSmsCount
                ,UINT32 *pDutOutSmsCount
            );
            virtual RC Initialize();
        private:
            const Lwca::Instance* m_pLwda;

            //! Data on the DUT subdevice
            Lwca::Device        m_LocalLwdaDev;
            UINT32              m_LocalMaxSms       = 0;
            UINT32              m_LocalMaxThreads   = 0;

            //! Data on the Remote subdevice (if transfer type is P2P only)
            Lwca::Device        m_RemoteLwdaDev;
            UINT32              m_RemoteMaxSms      = 0;
            UINT32              m_RemoteMaxThreads  = 0;

            UINT32 GetMaxBandwidthKiBps(UINT32 transferWidth);
    };
};

#endif // INCLUDED_LWLWL_TEST_ROUTE_H
