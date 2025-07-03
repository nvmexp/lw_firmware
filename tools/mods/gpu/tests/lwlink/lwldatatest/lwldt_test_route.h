/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/rc.h"
#include "core/include/tee.h"
#include "core/include/types.h"
#include "device/interface/lwlink/lwlpowerstate.h"
#include "device/interface/lwlink/lwlpowerstatecounters.h"
#include "gpu/lwlink/lwlinkimpl.h"
#include "gpu/utility/lwlink/lwlinkdev.h"
#include "lwldt_priv.h"

#include <memory>
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
    class TestRoute
    {
    public:
        TestRoute
        (
             TestDevicePtr pLocDev
            ,LwLinkRoutePtr pRoute
            ,Tee::Priority pri
        );
        virtual ~TestRoute() { }

        auto PathsBegin(TestDevicePtr pFromEndpoint, LwLink::DataType dt) const
        {
            return m_pRoute->PathsBegin(pFromEndpoint, dt);
        }

        auto PathsEnd(TestDevicePtr pFromEndpoint, LwLink::DataType dt) const
        {
            return m_pRoute->PathsEnd(pFromEndpoint, dt);
        }

        TestDevicePtr GetLocalLwLinkDev() const { return m_pLocLwLinkDev; }
        TestDevicePtr GetRemoteLwLinkDev() const
        {
            return m_pRoute->GetRemoteEndpoint(GetLocalLwLinkDev());
        }

        virtual RC CheckLinkState();
        string GetFilenameString();
        string GetLinkString(bool bRemote);
        string GetLocalDevString();
        TestDevicePtr GetLocalLwLinkDev() { return m_pLocLwLinkDev; }
        GpuSubdevice * GetLocalSubdevice();
        UINT32 GetMaxBandwidthKiBps();
        FLOAT64 GetMaxObservableBwPct(LwLink::TransferType tt) const;
        UINT32 GetRawBandwidthKiBps();
        TestDevicePtr GetRemoteLwLinkDev();
        GpuSubdevice * GetRemoteSubdevice();
        string GetRemoteDevString();
        LwLinkRoutePtr GetRoute() { return m_pRoute; }
        string GetString(string prefix);
        UINT32 GetSublinkWidth() const;
        TransferType GetTransferType() { return m_TransferType; }
        UINT32 GetWidth();
        void Print(Tee::Priority pri, string prefix) { }
        Tee::Priority GetPrintPri() const { return m_PrintPri; }
        UINT64 GetLinkMask(bool bRemote);

        virtual RC Initialize();

    protected:
        void SetTransferType(TransferType tt) { m_TransferType = tt; }

    private:
        LwLinkRoutePtr      m_pRoute;        //!< Pointer to the LwLink route
        TestDevicePtr       m_pLocLwLinkDev; //!< Pointer to the DUT LwLink device
        TransferType        m_TransferType;  //!< Type of transfer performed on the route

        //! Data on the Remote device (if transfer type is P2P only)
        TestDevicePtr       m_pRemLwLinkDev;

        Tee::Priority       m_PrintPri;            //!< Priority to print informational message
    };
    typedef shared_ptr<TestRoute> TestRoutePtr;
};
