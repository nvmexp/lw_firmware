/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2008-2012,2014,2016-2017,2019 by LWPU Corporation.  All rights reserved.
* All information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_sliapi.cpp
//! \brief Verify the Revised SLI API for describing topology.
//!
//! One purpose of this test is to describe the SLI/DR (distributed rendering)
//! topology from the perspective of each GPU.  Since the topology should agree
//! regardless of the GPU it is bound to, it must iterate over all GPUs.
//! After iterating, it cross-references the results to make sure that all GPUs
//! agree.
//!
//! This test differs from other RmTest classes in that it iterates over all
//! GPUs.  As such, it ignores the GPU device bound by the calling JavaScript
//! and the -dev option has no meaning.
//!
//! Note that this test iterates over GPUs by iterating over devices and
//! subdevices.  Therefore, this test is useful whether or not SLI is enabled.
//!
//! When SLI is enabled, mutiple GPUs share a single device, but are distinct
//! as multiple subdevices.  When SLI is disabled, there is one device (each
//! with only one subdevice) for each GPU. Note that is is also possible to
//! have a mixed configuration in which some GPUs are SLI-enabled and others are
//! not.
//!
//! \note This reports but cannot verify the following information reported
//! from the API: scanlock pin, fliplock pin, IOR capabilities.
//!
//------------------------------------------------------------------------------

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif

using namespace std;
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>
#include <iostream>
#include <sstream>

#include "gpu/include/gpudev.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"

#include "gpu/tests/rmtest.h"
#include "core/include/channel.h"
#include "lwos.h"
#include "core/include/display.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "ctrl/ctrl5070.h"
#include "class/cl5070.h"
#include "class/cl8270.h"
#include "class/cl8370.h"
#include "class/cl8870.h"
#include "class/cl8570.h"
#include "ctrl/ctrl5070/ctrl5070chnc.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/memcheck.h"
#include "gpu/include/testdevicemgr.h"

#define UNINITIALIZED 0xdeadbeef                // Uninitialized values here
#define MAX_GPU_COUNT        32                 // Fail if GPU ID meets or exceeds this
#define MAX_PINSET_COUNT      8                 // Fail if pinset count exceeds this (per GPU)
#define MAX_IOR_COUNT        32                 // Fail if IOR (I/O Resource) count exceeds this

#if MAX_GPU_COUNT< LW0080_MAX_DEVICES           // MAX_GPU_COUNT must be at least LW0080_MAX_DEVICES
#undef MAX_GPU_COUNT
#define MAX_GPU_COUNT LW0080_MAX_DEVICES
#endif

#define CHECK_RC_BUT_CONTINUE(f)    \
do                                  \
{   RC rc0= (f);                    \
    if( rc0!= OK)                   \
    {   rc= rc0;                    \
        Printf(Tee::PriHigh, "RevisedSliApiTest: ERROR: %s\n", rc.Message());   \
    }                               \
} while( 0)

#define NOP                         // No operation

class RevisedSliApiTest : public RmTest {
public:
    RevisedSliApiTest();
    virtual ~RevisedSliApiTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:
    struct s_Test
    {
        GpuSubdevice *m_Subdevice;      //< Not NULL only if test should be run on this GPU
        LwU32 m_DisplayClass;           //< Compatible display class -- Zero if display class is disabled
        bool m_Tested;                  //< false if this device was never tested (because it doesn't exist?)
        UINT32 m_PinsetCount;           //< Number of pinsets on this GPU

        struct
        {   UINT32 m_Gpu;               //< Peer GPU connected to this GPU on this pinset
            UINT32 m_Pinset;            //< Pinset of the peer GPU used to connect to this GPU on this pinset
        } m_Peer[ MAX_PINSET_COUNT];    //< Peer data as a function of the pinset ID

    } m_Test[ MAX_GPU_COUNT];           //< Test and its results as a function of the GPU ID

    UINT32 m_MaxGpu;                    //< Largest ID plus one of a GPU that was inserted into the m_Test array

    bool SetupTestMatrix();
    void Reset();
    RC RebindDeviceAndSubdevice( UINT32 gpuId);
    RC TestLw5070CtrlCmdGetPinsetCount( s_Test &);
    RC TestLw5070CtrlCmdGetPinsetPeer( s_Test &);
    bool CrossReferenceLw5070CtrlCmdGetPinsetPeer();
    RC TestLw5070CtrlCmdGetPinsetLockpins( s_Test &);

    LwRm::Handle m_hClient;
    LwRm::Handle m_hDevice;
    LwRm::Handle m_hDisplayHandle;

    string ValueUnlessNone( UINT32 n, UINT32 none);
};

//! \brief RevisedSliApiTest constructor.
//----------------------------------------------------------------------------
RevisedSliApiTest::RevisedSliApiTest()
{
    SetName("RevisedSliApiTest");

    m_hDisplayHandle= 0;        // Reset assumes that a nonzero value here must be freed
    Reset();                    // The easy way to initialize structures
}

//! \brief Destructor does nothing other than call subclass destructors.
//----------------------------------------------------------------------------
RevisedSliApiTest::~RevisedSliApiTest()
{   }

//! \brief Is test supported in the current environment?
//----------------------------------------------------------------------------
string RevisedSliApiTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test for the first time.
//!
//! Setup is used to initialize internal data structures.  Other tests use this
//! call to reserve required resources, but this test must do so for every GPU
//! iteration.
//!
//! \pre m_hDisplayHandle must be zero unless it points to a handle that should be freed.
//!
//! \see Reset
//!
//! \return OK if the test passed
//!         Other values as defined by various functions
//----------------------------------------------------------------------------
RC RevisedSliApiTest::Setup()
{
    RC rc;

    Printf( Tee::PriLow,"\n");
    Printf( Tee::PriLow, "RevisedSliApiTest::Setup: start\n");

    Reset();                            // Make sure that everything is as clean as the day it was born

//  CHECK_RC( InitFromJs());            // Not needed since we don't access parameters, etc.
    CHECK_RC( GpuTest::Setup());        // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC( SetupTestMatrix()? RC::SOFTWARE_ERROR: OK);       // Initialize the test matrix

    return rc;
}

//! \brief Initialize the test array for the first time.
//!
//! \return false if all is okay
//!         true if the GPU configuration from MODS doesn't make sense.
//----------------------------------------------------------------------------
bool RevisedSliApiTest::SetupTestMatrix()
{
    GpuDevice *dev;
    GpuSubdevice *subdev;
    UINT32 subdevIndex, gpuId;
    UINT32 gpuCount= 0;

    MASSERT( !m_MaxGpu);                                            // Data structures must be initialized (via Reset())

    for( dev= ((GpuDevMgr *)DevMgrMgr::d_GraphDevMgr)->GetFirstGpuDevice();    // Iterate over devices
         dev; dev= ((GpuDevMgr *)DevMgrMgr::d_GraphDevMgr)->GetNextGpuDevice( dev))
    {
        subdevIndex= dev->GetNumSubdevices();                       // Iterate through the subdevices
        while( subdevIndex--)
        {
            subdev= dev->GetSubdevice( subdevIndex);                // Get the subdevice object which represents a GPU
            MASSERT( subdev);
            gpuId= subdev->GetGpuInst();                            // The GPU ID is the same as the GPU instance
            Printf( Tee::PriLow, "RevisedSliApiTest::SetupTestMatrix: gpu #%u:  dev.subdev=%u.%u  addresses=%p.%p\n",
                (unsigned) gpuId, (unsigned) dev->GetDeviceInst(), (unsigned) subdev->GetSubdeviceInst(), dev, subdev);

            if( gpuId>= MAX_GPU_COUNT)                              // Make sure we don't overflow the array
            {                                                       // MAX_GPU_COUNT is set so that IDs higher than this would be unreasonable
                Printf( Tee::PriHigh, "RevisedSliApiTest::SetupTestMatrix: gpu #%u: ERROR: GPU ID must not exceed %u.\n", (unsigned) gpuId, (unsigned) MAX_GPU_COUNT);
                return true;                                        // and we proabaly have some kind of failure with MODS.
            }

            m_Test[ gpuId].m_Subdevice= subdev;                     // Save off the subdevice
            if( gpuId>= m_MaxGpu) m_MaxGpu= gpuId+ 1;               // Count the GPUs by getting the largest number
            gpuCount++;                                             // Count the GPUs by simply counting
        }
    }

    Printf( Tee::PriLow, "RevisedSliApiTest::SetupTestMatrix: GPU count=%u range=%u\n", (unsigned) gpuCount, (unsigned) m_MaxGpu);

    if( !gpuCount)                                                  // Isn't it odd that there are no GPUs?
    {                                                               // I thought you would agree  :)
        Printf( Tee::PriHigh, "RevisedSliApiTest::SetupTestMatrix: ERROR: no GPUs are being reported by MODS.\n");
        return true;                                                // Indicate error to caller
    }

    if( gpuCount> m_MaxGpu)                                         // Isn't it odd that there are more GPUs than IDs allocated for them?
    {                                                               // I thought you would agree  :)
        Printf( Tee::PriHigh, "RevisedSliApiTest::SetupTestMatrix: ERROR: GPU IDs reported by MODS are not uniquely assigned.\n");
        return true;                                                // Indicate error to caller
    }

    if( gpuCount< m_MaxGpu)                                         // There are gaps in the GPU ID assignments
    {                                                               // This in itself isn't an error, but it's worth noting
        Printf( Tee::PriLow, "RevisedSliApiTest::SetupTestMatrix: Gaps exist in the GPU IDs reported by MODS.\n");
    }

    return false;                                                   // NSo error
}

//! \brief Check results from mutiple iterations of BindDevice/Run and free any resources and initialize structures.
//!
//! Cross-reference the test results to see if the results are self-consistent.
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails.
//! Data structures are initialized to constructed values
//!
//! \pre m_hDisplayHandle must be zero unless it points to a handle that should be freed.
//!
//! \see Run
//! \see BindDevice
//!
//! \return OK if the test passed
//!         SOFTWARE_ERROR if we didn't get the expected results
//!         Other values as defined by various functions
//----------------------------------------------------------------------------
RC RevisedSliApiTest::Cleanup()
{
    RC rc;

    // Deallocate stuff
    Reset();

    // Let the base class have a crack at it.
    CHECK_RC_BUT_CONTINUE( GpuTest::Cleanup());

    // Say something on error
    if( rc != OK) Printf(Tee::PriHigh,"RevisedSliApiTest::Cleanup: ERROR: %s \n", rc.Message());

    return rc;
}

//! \brief Free any resources and initialize structures.
//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails.
//! Data structures are initialized to constructed values.
//!
//! \pre m_hDisplayHandle must be zero unless it points to a handle that should be freed.
//----------------------------------------------------------------------------
void RevisedSliApiTest::Reset()
{
    LwRmPtr pLwRm;
    UINT32 i, j;

    // Free The Handle of Display Device only if it has been allocated
    if( m_hDisplayHandle) pLwRm->Free(m_hDisplayHandle);

    // Zeroes
    m_hClient        = 0;
    m_hDevice        = 0;
    m_hDisplayHandle = 0;
    m_MaxGpu         = 0;

    // Initialize the test data
    i= MAX_GPU_COUNT;
    while( i--)
    {
        m_Test[ i].m_Subdevice= NULL;               // NULL until we've determined a subdevice
        m_Test[ i].m_DisplayClass= 0;               // false until we're able to allocate a display object
        m_Test[ i].m_Tested= false;                 // false until we've run a test
        m_Test[ i].m_PinsetCount= UNINITIALIZED;    // UNINITIALIZED until we've gotten the pinset cound

        // Initialize the peer data
        j= MAX_PINSET_COUNT;
        while( j--)
        {
            m_Test[ i].m_Peer[ j].m_Gpu= UNINITIALIZED;
            m_Test[ i].m_Peer[ j].m_Pinset= UNINITIALIZED;
        }
    }
}

//! \brief Rebind a gpu device/subdevice to the test and allocate/reallocate a display object.
//!
//! \pre m_hDisplayHandle must be zero unless it points to a handle that should be freed.
//! \pre m_Test[ gpuId].m_Subdevice must point to the subdevice -- Not NULL
//! \post m_hClient is the client handle
//! \post m_hDevice is the device handle
//! \post m_hDisplayHandle is the newly allocated display handle -- Zero if not allocated
//! \post m_Test[ gpuId].m_DisplayClass is the display class -- Zero if there is no display
//!
//! \see Reset
//!
//! \return OK if all is okay
//!         Other values as defined by pLwRm->Alloc
//----------------------------------------------------------------------------
RC RevisedSliApiTest::RebindDeviceAndSubdevice( UINT32 gpuId)
{
    RC        rc;
    LwRmPtr   pLwRm;

    LwU32 classPreference[] = {GT214_DISPLAY, G94_DISPLAY, GT200_DISPLAY, G82_DISPLAY, LW50_DISPLAY};
    LwU32 classDesired = 0;

    MASSERT( gpuId< MAX_GPU_COUNT);
    GpuSubdevice *subdev = m_Test[ gpuId].m_Subdevice;
    MASSERT( subdev);
    GpuDevice *gpudev= subdev->GetParentDevice();
    MASSERT( gpudev);
    Display *display = gpudev->GetDisplay();
    MASSERT( display);

    TestDeviceMgr* pTestDeviceMgr = static_cast<TestDeviceMgr*>(DevMgrMgr::d_TestDeviceMgr);
    TestDevicePtr pTestDevice;
    CHECK_RC(pTestDeviceMgr->GetDevice(subdev->DevInst(), &pTestDevice));

    m_TestConfig.BindTestDevice(pTestDevice);
    m_Golden.BindTestDevice(pTestDevice);
    m_pTestDevice = pTestDevice;

    m_Golden.SetSubdeviceMask( 1<< subdev->GetSubdeviceInst());
    display->SetDisplaySubdevices( display->Selected(), 1<< subdev->GetSubdeviceInst());

    // Allocate client handle
    m_hClient= pLwRm->GetClientHandle();

    // Allocate Device handle
    m_hDevice= pLwRm->GetDeviceHandle(GetBoundGpuDevice());

    // Try each class until we get one that works for us.
    for( unsigned int i = 0; i < sizeof(classPreference)/sizeof(LwU32); i++)
    {
        if ( IsClassSupported(classPreference[i]) )
        {
            classDesired = classPreference[i];
            Printf(Tee::PriLow,"RevisedSliApiTest::RebindDeviceAndSubdevice: class selected: 0x%04x\n", (unsigned) classDesired);
            break;
        }
    }

    // Free The Handle of Display Device only if it has been allocated.
    if( m_hDisplayHandle)
    {
        pLwRm->Free( m_hDisplayHandle);
        m_hDisplayHandle = 0;
    }

    // Remember the display class -- Zero if not found
    m_Test[ gpuId].m_DisplayClass = classDesired;

    // Allocate the display object, the client is implicit
    if( classDesired)
    {
        rc= pLwRm->Alloc( m_hDevice, &m_hDisplayHandle, classDesired, NULL);  // Defined in sdk/lwpu/inc/class/cl5070.h
        if( rc!= OK) Printf( Tee::PriHigh, "RevisedSliApiTest::RebindDeviceAndSubdevice: ERROR: Can't allocate display object from the RM.\n");
    }

    return rc;
}

//! \brief Run the tests
//!
//! This function iterates through all devices/subdevices (i.e. GPUs).
//!
//! \pre m_Test[].m_Subdevice is NULL unless it points to a GPU that should
//! be tested.
//!
//! \post m_Test[].m_Subdevice is true for GPUs that have been tested.
//! \post m_Test[].... shows results for GPUs that have been tested.
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC RevisedSliApiTest::Run()
{   RC rc;

    Printf( Tee::PriLow, "RevisedSliApiTest::Run: start\n");

    for( UINT32 i= 0; i< m_MaxGpu; ++i)
    {
        if( m_Test[ i].m_Subdevice)                         // SetupTestMatrix left NULL any GPU not reported by MODS
        {
            // Allocate display on the tested device
            DisplayMgr::TestContext testCtx(
                    m_Test[i].m_Subdevice->GetParentDevice()->GetDisplayMgr());
            CHECK_RC(testCtx.AllocDisplay(DisplayMgr::RequireRealDisplay));

            CHECK_RC( RebindDeviceAndSubdevice( i));        // Rebind this test object to the GPU and create display object

            if( m_Test[ i].m_DisplayClass)                  // Test only if the display exists
            {
                CHECK_RC_BUT_CONTINUE( TestLw5070CtrlCmdGetPinsetCount( m_Test[ i]));

                CHECK_RC_BUT_CONTINUE( TestLw5070CtrlCmdGetPinsetPeer( m_Test[ i]));

                CHECK_RC_BUT_CONTINUE( TestLw5070CtrlCmdGetPinsetLockpins( m_Test[ i]));

                m_Test[ i].m_Tested= true;                  // Indicate testing complete on this GPU (subdevice)
            }
        }
    }

    CHECK_RC_BUT_CONTINUE( CrossReferenceLw5070CtrlCmdGetPinsetPeer()? RC::SOFTWARE_ERROR: OK);

    return rc;
}

//! \brief Test for command LW5070_CTRL_CMD_GET_PINSET_COUNT
//!
//! \post If the test passes, m_PinsetCount is set to the pin count.
//!
//! \see lw5070CtrlCmdGetPinsetCount
//! \see dispGetPinsetCount
//!
//! \return OK if the test passed
//!         SOFTWARE_ERROR if we didn't get the expected results
//!         Other values as defined by RmApiStatusToModsRC
//-----------------------------------------------------------------------------
RC RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetCount( s_Test &test)
{
    RC rc;
    LW5070_CTRL_GET_PINSET_COUNT_PARAMS param;

    MASSERT( test.m_Subdevice);
    Printf(Tee::PriLow, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetCount: gpu #%u: start\n",
        test.m_Subdevice->GetGpuInst());

    memset(&param, 0, sizeof (param));
    param.base.subdeviceIndex= test.m_Subdevice->GetSubdeviceInst();

    rc = RmApiStatusToModsRC( LwRmControl(
        m_hClient, m_hDisplayHandle,
        LW5070_CTRL_CMD_GET_PINSET_COUNT,
        &param, sizeof (param)));

    if (rc==OK)
    {
        Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetCount: Number of pinsets: %u\n", (unsigned) param.pinsetCount);

        if( param.pinsetCount <= MAX_PINSET_COUNT) test.m_PinsetCount = param.pinsetCount;   // Save off the pinset count
        else
        {                                                                       // Too many to believe
            Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetCount: ERROR: Number of pinsets exceeds %u\n", (unsigned) MAX_PINSET_COUNT);
            rc = RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

//! \brief Test for command LW5070_CTRL_CMD_GET_PINSET_PEER
//!
//! \pre m_PinsetCount must be set to the pin count.
//!
//! \see lw5070CtrlCmdGetPinsetPeer
//! \see dispGetPinsetPeer
//! \see CrossReferenceLw5070CtrlCmdGetPinsetPeer
//!
//! \return OK if the test passed
//!         SOFTWARE_ERROR if we didn't get the expected results
//!         Other values as defined by RmApiStatusToModsRC
//-----------------------------------------------------------------------------
RC RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetPeer( s_Test &test)
{
    UINT32 lwos_status = LW_OK;
    UINT32 pinset;
    LW5070_CTRL_GET_PINSET_PEER_PARAMS param;
    bool ilwalidValue= false;                      // Set to true if a value is not within the expected range

    MASSERT( test.m_Subdevice);
    Printf( Tee::PriLow, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetPeer: gpu #%u: start\n",
        test.m_Subdevice->GetGpuInst());

    // We loop until we get a not-okay result code.
    // The loop should end with LW_ERR_ILWALID_ARGUMENT when the pinset number is too large.
    for( pinset= 0; lwos_status== LW_OK && pinset<= MAX_PINSET_COUNT; ++pinset)
    {
        memset( &param, 0, sizeof( param));
        param.base.subdeviceIndex= test.m_Subdevice->GetSubdeviceInst();
        param.pinset= pinset;

        lwos_status= LwRmControl(
            m_hClient, m_hDisplayHandle,
            LW5070_CTRL_CMD_GET_PINSET_PEER,
            &param, sizeof (param));

        if (lwos_status!= LW_OK)                                  // It is not necessarily an error to get an error status code
        {                                                                       // We are testing to make sure that the command returns errors when approrpiate
            Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetPeer: pinset #%u: LWOS status code: 0x%04x\n",
                (unsigned) param.pinset, (unsigned) lwos_status);
        }

        else if( param.peerGpuInstance==  LW5070_CTRL_CMD_GET_PINSET_PEER_PEER_GPUINSTANCE_NONE
          &&     param.peerPinset== LW5070_CTRL_CMD_GET_PINSET_PEER_PEER_PINSET_NONE)
        {                                                                       // It's okay if both are _NONE
            Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetPeer: pinset #%u: not connected to peer\n",
                (unsigned) param.pinset);
        }

        else                                                                    // Display the results
        {
            Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetPeer: pinset #%u: connected to peer gpu #%s via peer pinset #%s\n",
                (unsigned) param.pinset,
                ValueUnlessNone( param.peerGpuInstance, LW5070_CTRL_CMD_GET_PINSET_PEER_PEER_GPUINSTANCE_NONE).c_str(),
                ValueUnlessNone( param.peerPinset, LW5070_CTRL_CMD_GET_PINSET_PEER_PEER_PINSET_NONE).c_str());

            if( param.peerGpuInstance==  LW5070_CTRL_CMD_GET_PINSET_PEER_PEER_GPUINSTANCE_NONE)
            {
                Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetPeer: pinset #%u: ERROR: Peer GPU ID (but not pinset) is _NONE\n",
                    (unsigned) param.pinset);                                      // If one is _NONE, then the other must be _NONE
                ilwalidValue= true;                                             // Flag the error but continue testung
            }

            else if( param.peerGpuInstance>= MAX_GPU_COUNT)
            {
                Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetPeer: pinset #%u: ERROR: Peer GPU ID exceeds or equals %u\n",
                    (unsigned) param.pinset, (unsigned) MAX_GPU_COUNT);         // Make sure the returned values are reasonable
                ilwalidValue= true;                                             // Flag the error but continue testung
            }

            if( param.peerPinset== LW5070_CTRL_CMD_GET_PINSET_PEER_PEER_PINSET_NONE)
            {
                Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetPeer: pinset #%u: ERROR: Peer pinset (but not GPU ID) is _NONE.\n",
                    (unsigned) param.pinset);                                   // If one is _NONE, then the other must be _NONE
                ilwalidValue= true;                                             // Flag the error but continue testung
            }

            else if( param.peerPinset>= MAX_PINSET_COUNT)
            {
                Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetPeer: pinset #%u: ERROR: Peer pinset number exceeds or equals %u.\n",
                    (unsigned) param.pinset, (unsigned) MAX_GPU_COUNT);         // Make sure the returned values are reasonable
                ilwalidValue= true;                                             // Flag the error but continue testung
            }
        }

        if (!ilwalidValue && param.pinset < MAX_PINSET_COUNT)                   // We have data that we can believe
        {
            test.m_Peer[ param.pinset].m_Gpu= param.peerGpuInstance;            // Remember the results so that we can
            test.m_Peer[ param.pinset].m_Pinset= param.peerPinset;              // later cross-reference them
        }
    }

    if( lwos_status== LW_OK)                                      // Loop should exit with an error
    {                                                                           // Too many to believe
        Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetPeer: ERROR: Number of pinsets on this GPU exceeds %u\n",
            (unsigned) MAX_PINSET_COUNT);
        return RC::SOFTWARE_ERROR;
    }

    if( ilwalidValue) return RC::SOFTWARE_ERROR;                                // Some previous value check failed

    if( lwos_status== LW_ERR_ILWALID_ARGUMENT && param.pinset == test.m_PinsetCount)  // Loop should terminate with this code
    {                                                                           // since it means that LW5070_CTRL_CMD_GET_PINSET_PEER
        return OK;                                                              // indicated an invalid pinset number correctly.
    }

    return RmApiStatusToModsRC( lwos_status);                                   // Translate to MODS error codes
}

//! \brief Cross-reference the results from multiple iterations of TestLw5070CtrlCmdGetPinsetPeer.
//!
//! \pre m_MaxGpu is one more then the largest ID of a GPU that was tested.
//! If the GPUs are numbered sequentially, then this should equal the number of GPUs tested.
//! It is an error if this value is zero.
//!
//! \pre m_Test contains the test results from TestLw5070CtrlCmdGetPinsetPeer.
//!
//! \see TestLw5070CtrlCmdGetPinsetPeer
//!
//! \return false if the test passed
//!         true if we didn't get the expected results
//-----------------------------------------------------------------------------

bool RevisedSliApiTest::CrossReferenceLw5070CtrlCmdGetPinsetPeer()
{
    bool bError= false;
    UINT32 peerGpu, peerPinset;

    for( UINT32 gpu= 0; gpu< MAX_GPU_COUNT; ++gpu)                             // Iterate through all known GPUs
    {

        if( !m_Test[ gpu].m_Tested)                                            // This GPU has not been tested
        {
            if( gpu< m_MaxGpu)                                                 // If the GPUs are numbered sequentially, it should have been tested
            {
                if( !m_Test[ gpu].m_DisplayClass)                              // No display exists for this gpu
                {                                                              // This is not an error since it's legal to have a GPU w/o a display
                    Printf( Tee::PriLow, "RevisedSliApiTest::CrossReferenceLw5070CtrlCmdGetPinsetPeer: gpu #%u: has no display\n",
                        (unsigned) gpu);
                }

                else
                {                                                              // This error may or may not refer to an error from TestLw5070CtrlCmdGetPinsetPeer
                    Printf( Tee::PriHigh, "RevisedSliApiTest::CrossReferenceLw5070CtrlCmdGetPinsetPeer: gpu #%u: ERROR: never tested because it wasn't attached to a device/subdevice or GPUs were not numbered sequentially.\n",
                        (unsigned) gpu);                                       // It may be that GPUs and/or pinsets are not numbered sequentially
                    bError= true;                                              // But it's most likely that an invalid value came back from the API
                }
            }
        }

        else                                                                    // This GPU was tested
        {
            MASSERT( gpu< m_MaxGpu);                                            // BindDevice logic fails if this assert hits

            if( !m_Test[ gpu].m_PinsetCount)                                    // No pinsets were reported for this gpu
            {
                Printf( Tee::PriLow, "RevisedSliApiTest::CrossReferenceLw5070CtrlCmdGetPinsetPeer: gpu #%u: has no pinsets\n",
                    (unsigned) gpu);
            }

            else for( UINT32 pinset= 0; pinset< m_Test[ gpu].m_PinsetCount; ++pinset)    // Iterate through all pinsets on the GPU
            {
                peerGpu=    m_Test[ gpu].m_Peer[ pinset].m_Gpu;                     // Get the test results
                peerPinset= m_Test[ gpu].m_Peer[ pinset].m_Pinset;

                if( peerGpu== UNINITIALIZED)                                        // TestLw5070CtrlCmdGetPinsetPeer never assigned a value
                {                                                                   // This error may or may not refer to an error from TestLw5070CtrlCmdGetPinsetPeer
                    Printf( Tee::PriHigh, "RevisedSliApiTest::CrossReferenceLw5070CtrlCmdGetPinsetPeer: gpu #%u: pinset #%u: ERROR: not successfully tested\n",
                        (unsigned) gpu, (unsigned) pinset);                         // It may be that GPUs and/or pinsets are not numbered sequentially
                    bError= true;                                                   // But it's most likely that an invalid value came back from the API
                }

                else if( peerGpu== LW5070_CTRL_CMD_GET_PINSET_PEER_PEER_GPUINSTANCE_NONE) // No connection here
                {                                                                   // TestLw5070CtrlCmdGetPinsetPeer has logic for this
                   MASSERT( peerPinset== LW5070_CTRL_CMD_GET_PINSET_PEER_PEER_PINSET_NONE);
                }

                else if( peerGpu== gpu)
                {
                    Printf( Tee::PriHigh, "RevisedSliApiTest::CrossReferenceLw5070CtrlCmdGetPinsetPeer: gpu #%u: pinset #%u: ERROR: thinks it is connected to itself via pinset #%u\n",
                        (unsigned) gpu, (unsigned) pinset, (unsigned) peerPinset);
                    bError= true;
                }

                else
                {
                    MASSERT( peerGpu<= MAX_GPU_COUNT);                              // TestLw5070CtrlCmdGetPinsetPeer has logic for these
                    MASSERT( peerPinset<= MAX_PINSET_COUNT);                        // and refuses to assign these invalid values

                    if( m_Test[ peerGpu].m_Peer[ peerPinset].m_Gpu!= gpu)           // I must be the peer of my peer
                    {                                                               // This test is the heart of the cross-reference validation
                        Printf( Tee::PriHigh, "RevisedSliApiTest::CrossReferenceLw5070CtrlCmdGetPinsetPeer: gpu #%u: pinset #%u: ERROR: thinks gpu #%u is connected to it, gpu #%u but does not agree\n",
                            (unsigned) gpu, (unsigned) pinset, (unsigned) peerGpu, (unsigned) peerGpu);
                        bError= true;                                               // Low-priority messsage in TestLw5070CtrlCmdGetPinsetPeer shows m_Test[ peerGpu].m_Gpu
                    }

                    if( m_Test[ peerGpu].m_Peer[ peerPinset].m_Pinset!= pinset)     // And I must agree on pinsets
                    {
                        Printf( Tee::PriHigh, "RevisedSliApiTest::CrossReferenceLw5070CtrlCmdGetPinsetPeer: gpu #%u: pinset #%u: ERROR: thinks gpu #%u uses its pinset #%u to connect to it, but gpu #%u does not agree\n",
                            (unsigned) gpu, (unsigned) pinset, (unsigned) peerGpu, (unsigned) peerPinset, peerGpu);
                        bError= true;                                               // This test is the lung of the cross-reference validation
                    }
                }
            }
        }
    }

    Printf( Tee::PriHigh, "RevisedSliApiTest::CrossReferenceLw5070CtrlCmdGetPinsetPeer: cross-reference %s\n",
        ( m_MaxGpu? ( bError? "failed": "okay"): "empty"));                     // "empty" means no GPUs were tested -- Did the javascript call BindDevice?
    return bError || !m_MaxGpu;                                                 // "failed" means at least one did not cross-reference
}

//! \brief Test for command LW5070_CTRL_CMD_GET_PINSET_LOCKPINS
//!
//! \pre m_PinsetCount must be set to the pin count.
//!
//! \note This test fails with ROMs that do not have GPIO tables,
//!       a limitation of MODS for LW50 and before.
//!
//! \see lw5070CtrlCmdGetPinsetLockpins
//!
//! \return OK if the test passed
//!         SOFTWARE_ERROR if we didn't get the expected results
//!         Other values as defined by RmApiStatusToModsRC
//-----------------------------------------------------------------------------

RC RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetLockpins( s_Test &test)
{
    UINT32 lwos_status = LW_OK;
    UINT32 pinset;
    LW5070_CTRL_GET_PINSET_LOCKPINS_PARAMS param;

    MASSERT( test.m_Subdevice);
    Printf(Tee::PriLow, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetLockpins: gpu #%u: start\n",
    test.m_Subdevice->GetGpuInst());

    // We loop until we get a not-okay result code.
    // The loop should end with LW_ERR_ILWALID_ARGUMENT when the pinset number is too large.
    for( pinset= 0; lwos_status== LW_OK && pinset<= MAX_PINSET_COUNT; ++pinset)
    {
        memset(&param, 0, sizeof (param));
        param.base.subdeviceIndex= test.m_Subdevice->GetSubdeviceInst();
        param.pinset= pinset;

        lwos_status= LwRmControl(
        m_hClient, m_hDisplayHandle,
        LW5070_CTRL_CMD_GET_PINSET_LOCKPINS,
        &param, sizeof (param));

        if (lwos_status!= LW_OK)                                  // It is not necessarily an error to get an error status code
        {                                                                       // We are testing to make sure that the command returns errors when approrpiate
            Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetLockpins: pinset #%u: LWOS status code: 0x%04x\n",
            (unsigned) param.pinset, (unsigned) lwos_status);
        }

        else                                                                    // Display the results
        {
            Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetLockpins: pinset #%u: scanlock=%s fliplock=%s\n",
                (unsigned) param.pinset,
                ValueUnlessNone( param.scanLockPin, LW5070_CTRL_GET_PINSET_LOCKPINS_SCAN_LOCK_PIN_NONE).c_str(),
                ValueUnlessNone( param.flipLockPin, LW5070_CTRL_GET_PINSET_LOCKPINS_FLIP_LOCK_PIN_NONE).c_str());
        }
    }

    if( lwos_status== LW_OK)                                      // Loop should exit with an error
    {                                                                           // Too many to believe
        Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetLockpins: ERROR: Number of pinsets exceeds %u\n",
        (unsigned) MAX_PINSET_COUNT);
        return RC::SOFTWARE_ERROR;
    }

    if( lwos_status== LW_ERR_ILWALID_ARGUMENT && param.pinset == test.m_PinsetCount)  // Loop should terminate with this code
    {                                                                           // since it means that LW5070_CTRL_CMD_GET_PINSET_PEER
        return OK;                                                              // indicated an invalid pinset number correctly.
    }

    if( lwos_status== LW_ERR_ILWALID_ARGUMENT)
    {
        Printf( Tee::PriHigh, "RevisedSliApiTest::TestLw5070CtrlCmdGetPinsetLockpins: ERROR: Number of pinsets (%u) does not match earlier test (%u).\n",
        (unsigned) param.pinset, test.m_PinsetCount);
    }

    return RmApiStatusToModsRC( lwos_status);                                   // Translate to MODS error codes
}

//! \brief Colwert number to string testing for magic "none" value.
//!
//! \note m_TextScratch is reused for every call, so this function is threadsafe
//!       only on the object level.
//!
//----------------------------------------------------------------------------

string RevisedSliApiTest::ValueUnlessNone( UINT32 n, UINT32 none)
{
    if( n== none) return "none";

    stringstream s;
    s<< n;
    return s.str();
}

//----------------------------------------------------------------------------
// JS Linkage
//----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ AllocObjectTest
//! object.
//!
//----------------------------------------------------------------------------
JS_CLASS_INHERIT(RevisedSliApiTest, RmTest, "Test for the revised SLI API for describing topology");

