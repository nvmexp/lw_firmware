/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_ctrl0000gpugetuuidinfo.cpp
//! \brief This test verifies the functionality of LW0000_GPU_GET_UUID_INFO
//!

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include "ctrl/ctrl0000.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl2080.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "core/include/platform.h"
#include "core/include/memcheck.h"

#define DEVICE_HANDLE      (0xbb008000)
#define SUBDEVICE_HANDLE   (0xbb208000)

class Ctrl0000GpuGetUuidInfoTest : public RmTest
{
public:
    Ctrl0000GpuGetUuidInfoTest();
    virtual ~Ctrl0000GpuGetUuidInfoTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    LwRm::Handle m_hClient;
    LwU32        m_gpuIds[LW0000_CTRL_GPU_MAX_ATTACHED_GPUS];
};

//------------------------------------------------------------------------------
Ctrl0000GpuGetUuidInfoTest::Ctrl0000GpuGetUuidInfoTest()
{
    SetName("Ctrl0000GpuGetUuidInfoTest");
}

//------------------------------------------------------------------------------
Ctrl0000GpuGetUuidInfoTest::~Ctrl0000GpuGetUuidInfoTest()
{

}

//! \brief This test is always valid
//------------------------------------------------------------------------------
string Ctrl0000GpuGetUuidInfoTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Allocate a client and get all attached GPU IDs
//------------------------------------------------------------------------------
RC Ctrl0000GpuGetUuidInfoTest::Setup()
{
    RC rc;
    LwU32 status;
    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS gpuAttachedIdsParams = {{0}};
    LW0000_CTRL_GPU_ATTACH_IDS_PARAMS gpuAttachIdsParams = {{0}};

    //
    // Allocate a client.
    //
    status = (LwU32)LwRmAllocRoot((LwU32*)&m_hClient);
    CHECK_RC(RmApiStatusToModsRC(status));

    // attach all GPUs
    gpuAttachIdsParams.gpuIds[0] = LW0000_CTRL_GPU_ATTACH_ALL_PROBED_IDS;
    gpuAttachIdsParams.gpuIds[1] = LW0000_CTRL_GPU_ILWALID_ID;
    CHECK_RC(LwRmControl(m_hClient,
                         m_hClient,
                         LW0000_CTRL_CMD_GPU_ATTACH_IDS,
                         &gpuAttachIdsParams,
                         sizeof(gpuAttachIdsParams)));

    // get attached GPU IDs
    CHECK_RC(LwRmControl(m_hClient,
                         m_hClient,
                         LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
                         &gpuAttachedIdsParams,
                         sizeof(gpuAttachedIdsParams)));
    memcpy(m_gpuIds, gpuAttachedIdsParams.gpuIds, sizeof(m_gpuIds));

    return rc;
}

//! \brief Test LW2080_CTRL_CMD_GPU_GET_UUID_INFO functionality
//!
//! Logic:
//! 1) get the device and subdevice instance given an attached GPU ID
//! 2) allocate a device/subdevice based on returned device/subdevice instance
//! 3) get the UUID of the GPU referred to by the subdevice
//! 4) query the GPU ID, device instance, and subdevice instance given the UUID,
//!    the returned GPU ID, device instance, and subdevice instance should match
//!    the original GPU ID, device instance, and subdevice instance used to
//!    instantiate the device/subdevice
//------------------------------------------------------------------------------
RC Ctrl0000GpuGetUuidInfoTest::Run()
{
    RC rc;
    LwU32 status;
    LwU32 gpuIdIndex;
    LwU32 uuidFormat;
    LW0000_CTRL_GPU_GET_ID_INFO_PARAMS gpuIdInfoParams = {0};
    LW0080_ALLOC_PARAMETERS lw0080Params = {0};
    LW2080_ALLOC_PARAMETERS lw2080Params = {0};
    LW2080_CTRL_GPU_GET_GID_INFO_PARAMS getGidInfoParams = {0};
    LW0000_CTRL_GPU_GET_UUID_INFO_PARAMS getUuidInfoParams = {{0}};

    for (gpuIdIndex = 0; gpuIdIndex < LW0000_CTRL_GPU_MAX_ATTACHED_GPUS; gpuIdIndex++)
    {
        // skip any invalid GPU IDs
        if ( LW0000_CTRL_GPU_ILWALID_ID == m_gpuIds[gpuIdIndex] )
        {
            continue;
        }

        // get GPU instance info
        gpuIdInfoParams.gpuId = m_gpuIds[gpuIdIndex];
        status = LwRmControl(m_hClient,
                             m_hClient,
                             LW0000_CTRL_CMD_GPU_GET_ID_INFO,
                             &gpuIdInfoParams,
                             sizeof(gpuIdInfoParams));
        CHECK_RC(RmApiStatusToModsRC(status));

        // allocate a device / subdevice
        lw0080Params.deviceId = gpuIdInfoParams.deviceInstance;
        lw0080Params.hClientShare = 0;
        status = LwRmAlloc(m_hClient,
                           m_hClient,
                           DEVICE_HANDLE,
                           LW01_DEVICE_0,
                           &lw0080Params);
        CHECK_RC(RmApiStatusToModsRC(status));

        lw2080Params.subDeviceId = gpuIdInfoParams.subDeviceInstance;
        status = LwRmAlloc(m_hClient,
                           DEVICE_HANDLE,
                           SUBDEVICE_HANDLE,
                           LW20_SUBDEVICE_0,
                           &lw2080Params);
        CHECK_RC(RmApiStatusToModsRC(status));

        for (uuidFormat = 0; uuidFormat < 2; uuidFormat++)
        {
            const LwBool bBinary = uuidFormat == 0;

            memset(&getUuidInfoParams, 0, sizeof(getUuidInfoParams));

            // get the UUID
            getGidInfoParams.flags =
                bBinary ?
                    DRF_DEF(2080_GPU_CMD, _GPU_GET_GID_FLAGS, _FORMAT, _BINARY) :
                    DRF_DEF(2080_GPU_CMD, _GPU_GET_GID_FLAGS, _FORMAT, _ASCII);

            status = LwRmControl(m_hClient,
                                 SUBDEVICE_HANDLE,
                                 LW2080_CTRL_CMD_GPU_GET_GID_INFO,
                                 &getGidInfoParams,
                                 sizeof(getGidInfoParams));
            CHECK_RC(RmApiStatusToModsRC(status));

            //
            // get the GPU ID, device instance, and subdevice instance.
            // we must tell LW0000_CTRL_CMD_GPU_GET_UUID_INFO what format the
            // UUID we are passing in is in (binary, ASCII, etc.). since we are
            // directly plugging in the UUID returned by
            // LW2080_CTRL_CMD_GPU_GET_GID_INFO.
            //
            getUuidInfoParams.flags =
                bBinary ?
                    DRF_DEF(0000_CTRL_CMD, _GPU_GET_UUID_INFO_FLAGS, _FORMAT, _BINARY) :
                    DRF_DEF(0000_CTRL_CMD, _GPU_GET_UUID_INFO_FLAGS, _FORMAT, _ASCII);

            memcpy(getUuidInfoParams.gpuUuid,
                   getGidInfoParams.data,
                   sizeof(getUuidInfoParams.gpuUuid));
            status = LwRmControl(m_hClient,
                                 m_hClient,
                                 LW0000_CTRL_CMD_GPU_GET_UUID_INFO,
                                 &getUuidInfoParams,
                                 sizeof(getUuidInfoParams));
            CHECK_RC(RmApiStatusToModsRC(status));

            // check if the GPU ID, device instnace, and subdevice instance match up
            if (getUuidInfoParams.gpuId != gpuIdInfoParams.gpuId ||
                getUuidInfoParams.deviceInstance != gpuIdInfoParams.deviceInstance ||
                getUuidInfoParams.subdeviceInstance != gpuIdInfoParams.subDeviceInstance)
            {
                Printf(Tee::PriHigh,
                       "ERROR: Mismatch of POBJGPU used to allocate "
                       "device/subdevice and those returned by querying the "
                       "UUID!\n"
                       "\tExpected GPU ID: 0x%X\tActual: 0x%X\n"
                       "\tExpected device instance: 0x%X\tActual: 0x%X\n"
                       "\tExpected subdevice instance: 0x%X\tActual: 0x%X\n",
                       getUuidInfoParams.gpuId, gpuIdInfoParams.gpuId,
                       getUuidInfoParams.deviceInstance,
                       gpuIdInfoParams.deviceInstance,
                       getUuidInfoParams.subdeviceInstance,
                       gpuIdInfoParams.subDeviceInstance);
                return RC::SOFTWARE_ERROR;
            }

            // do a negative test
            memset(&getUuidInfoParams, 0, sizeof(getUuidInfoParams));
            memset(&(getUuidInfoParams.gpuUuid), 0xdeadbeef, sizeof(getUuidInfoParams.gpuUuid));
            getUuidInfoParams.flags =
                bBinary ?
                    DRF_DEF(0000_CTRL_CMD, _GPU_GET_UUID_INFO_FLAGS, _FORMAT, _BINARY) :
                    DRF_DEF(0000_CTRL_CMD, _GPU_GET_UUID_INFO_FLAGS, _FORMAT, _ASCII);

            status = LwRmControl(m_hClient,
                                 m_hClient,
                                 LW0000_CTRL_CMD_GPU_GET_UUID_INFO,
                                 &getUuidInfoParams,
                                 sizeof(getUuidInfoParams));
            if (RC::LWRM_OBJECT_NOT_FOUND != RmApiStatusToModsRC(status))
                return RC::SOFTWARE_ERROR;
        }

        // free the device
        LwRmFree(m_hClient, m_hClient, DEVICE_HANDLE);
    }

    return OK;
}

//! \brief Clean up the allocated client
//------------------------------------------------------------------------------
RC Ctrl0000GpuGetUuidInfoTest::Cleanup()
{
    LwRmFree(m_hClient, m_hClient, m_hClient);
    return OK;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.  Don't worry about the details
//! here, you probably don't care.  :-)
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Ctrl0000GpuGetUuidInfoTest, RmTest,
    "Framework to test LW0000_GPU_CTRL_CMD_GET_UUID_INFO");

