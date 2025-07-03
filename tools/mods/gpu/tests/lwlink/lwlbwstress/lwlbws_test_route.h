/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2017,2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_LWLBWS_TEST_ROUTE_H
#define INCLUDED_LWLBWS_TEST_ROUTE_H

#include "gpu/tests/lwlink/lwldatatest/lwldt_test_route.h"
#include "gpu/tests/lwlink/lwldatatest/lwldt_priv.h"
#include "core/include/types.h"
#include "core/include/rc.h"
#include "core/include/tee.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "gpu/lwlink/lwlinkimpl.h"

#include <string>
#include <set>

class GpuSubdevice;

namespace LwLinkDataTestHelper
{
    //--------------------------------------------------------------------------
    //! \brief Class which encapsulates test route for LwLink bandwidth
    //! stress testing.  A route (just like an LwLinkRoute) represents
    //! multiple LwLinks where the source and destination device is the same.
    //!
    //! This class is mainly an informational repository and contains no actual
    //! copy functionality
    class TestRouteCe2d : public TestRoute
    {
        public:
            TestRouteCe2d
            (
                 TestDevicePtr  pLocDev
                ,LwLinkRoutePtr pRoute
                ,Tee::Priority  pri
            );
            virtual ~TestRouteCe2d() {}

            UINT32 GetLocalCeCount() { return m_LocalCeCount; }
            UINT32 GetRemoteCeCount() { return m_RemoteCeCount; }
            RC GetRequiredCes
            (
                 TransferDir transDir
                ,bool bLocalCes
                ,UINT32 *pDutInCeCount
                ,UINT32 *pDutOutCeCount
            );
            RC GetUsableCeMasks
            (
                 bool        bLocalCes
                ,UINT32 *    pDutInCeMask
                ,UINT32 *    pDutOutCeMask
            );
            virtual RC Initialize();
            void RestoreCeCopyWidth();
            void SetCeCopyWidth(UINT32 w);
        private:
            //! Data on the DUT subdevice
            UINT32              m_LocalCeCount         = 0;
            UINT32              m_LocalCeBw            = 0;
            LwLinkImpl::CeWidth m_LocalCeCopyWidthSys  = LwLinkImpl::CEW_256_BYTE;
            LwLinkImpl::CeWidth m_LocalCeCopyWidthPeer = LwLinkImpl::CEW_256_BYTE;

            //! Bit masks of CEs that are valid for use when the local subdevice reads or
            //! writes from the remote subdevice.  These masks are used to implement CE assignment
            //! on GPUs where 1:1 LCE/PCE configuration is not an option and rely on
            //! correct RM configuration of LCE/PCE so that the links can be saturated
            UINT32 m_LocalReadCeMask  = 0;
            UINT32 m_LocalWriteCeMask = 0;

            //! Data on the Remote subdevice (if transfer type is P2P only)
            UINT32              m_RemoteCeCount         = 0;
            UINT32              m_RemoteCeBw            = 0;
            LwLinkImpl::CeWidth m_RemoteCeCopyWidthPeer = LwLinkImpl::CEW_256_BYTE;

            //! Bit masks of CEs that are valid for use when the remote subdevice reads or
            //! writes from the local subdevice.
            UINT32 m_RemoteReadCeMask  = 0;
            UINT32 m_RemoteWriteCeMask = 0;
    };
};

#endif // INCLUDED_LWLBWS_TEST_ROUTE_H
