/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_TESTDEVICEMGR_H
#define INCLUDED_TESTDEVICEMGR_H

#include "gpu/include/testdevice.h"
#include "core/include/devmgr.h"
#include "core/include/mgrmgr.h"

class TestDeviceMgr : public DevMgr
{
public:
    TestDeviceMgr();
    virtual ~TestDeviceMgr() { }

    RC FindDevices() override;
    void FreeDevices() override { }

    RC GetDevice(UINT32 index, Device **device) override;
    RC GetDevice(UINT32 index, TestDevicePtr* ppTestDevice);
    RC GetDevice(const TestDevice::Id & deviceId, TestDevicePtr* ppTestDevice);
    RC GetDevice(TestDevice::Type type, UINT32 index, TestDevicePtr* ppTestDevice);
    TestDevicePtr GetFirstDevice();

    RC InitializeAll() override;
    bool IsInitialized();

    UINT32 NumDevices() override { return static_cast<UINT32>(m_Devices.size()); }
    UINT32 NumDevicesType(TestDevice::Type type) const;
    UINT32 GetDevInst(TestDevice::Type type, UINT32 idx) const;
    UINT32 GetDriverId(UINT32 devInst) const;
    void OnExit() override;
    RC ShutdownAll() override;

    // This allows us to use d_TestDeviceMgr in for-range loops to iterate
    // over TestDevices
    class Iterator
    {
        public:
            Iterator()                           = default;

            // Copyable
            Iterator(const Iterator&)            = default;
            Iterator& operator=(const Iterator&) = default;

            explicit Iterator(UINT32 deviceIndex)
                : m_DeviceIndex(deviceIndex)
            {
                UpdateDevice();
            }

            TestDevicePtr operator*() const
            {
                return m_pTestDevice;
            }

            // Get next subdevice (pre-increment)
            Iterator& operator++()
            {
                m_DeviceIndex++;
                UpdateDevice();
                return *this;
            }

            // Get next subdevice (post-increment)
            Iterator operator++(int)
            {
                Iterator prev(m_DeviceIndex);
                m_DeviceIndex++;
                UpdateDevice();
                return prev;
            }

            bool operator==(const Iterator& other) const
            {
                return m_pTestDevice.get() == other.m_pTestDevice.get();
            }

            bool operator!=(const Iterator& other) const
            {
                return m_pTestDevice.get() != other.m_pTestDevice.get();
            }

        private:
            void UpdateDevice()
            {
                MASSERT(DevMgrMgr::d_TestDeviceMgr);
                if (m_DeviceIndex < DevMgrMgr::d_TestDeviceMgr->NumDevices())
                {
                    RC rc;
                    rc = DevMgrMgr::d_TestDeviceMgr->GetDevice(m_DeviceIndex, &m_pTestDevice);
                    MASSERT(rc == RC::OK);
                }
                else
                {
                    m_pTestDevice = TestDevicePtr();
                }
            }
            UINT32        m_DeviceIndex = 0U;
            TestDevicePtr m_pTestDevice;
    };

    Iterator begin()
    {
        return Iterator(0);
    }

    static Iterator end()
    {
        MASSERT(DevMgrMgr::d_TestDeviceMgr);
        return Iterator(DevMgrMgr::d_TestDeviceMgr->NumDevices());
    }

private:
    //! Array of discovered test devices
    vector<TestDevicePtr> m_Devices;
    //! Maps LwSwitch dev id (from driver) to device instance
    vector<UINT32>        m_LwSwitchMapDriverIdToDevInst;
};

#endif
