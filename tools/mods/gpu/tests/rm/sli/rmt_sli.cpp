/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2008 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_sli.cpp
//! \brief RmTest that checks for Memory Leaks during forced SLI transition.
//!
//! This is the first phase of the Test where we check for valid SLI configlist and then make
//! forced SLI transition by Linking and Unlinking.
//! NOTE: This test yet to be tested on SLI setup because of bug 298006.

//! Using an unused error
#define UNUSED_ERROR_732 LWAPI_ERROR

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwRmApi.h"
#include "lwmisc.h"
#include "ctrl/ctrl0080.h"
#include "ctrl/ctrl0000.h"
#include "core/include/errormap.h"

//!-----Header for LWAPI interface
#include <lwapi.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "core/include/memcheck.h"

#define MIN_SUBDEVICES_FOR_SLI 2

#define PRINT_ERROR(LwApiStatus, function_string) \
{ \
     LwAPI_ShortString errorString; \
     LwAPI_GetErrorMessage(LwApiStatus, errorString); \
     Printf(Tee::PriHigh, "SLI Test : %s() failed: %d : %s\n", function_string, LwApiStatus, errorString); \
}

// Max registry key name length
#define MAX_KEY_LENGTH 1024

typedef struct
{
    UINT32 enable;
    bool disable;
    bool dynamic;
    bool topology;
    bool gpuindex;
    ULONG dindex;
    bool displaymask;
    ULONG dmask;
    bool mask;
} MBSET_STRUCT;

void SetKeys(const char *key_name, const UINT32 value);
void CleanKeys(const char *key_name);

class SliOperationsTest: public RmTest
{
public:
    SliOperationsTest();
    virtual ~SliOperationsTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(sli_enable, UINT32);     //!< Grab JS variable LoopVar

    RC EnableSli(MBSET_STRUCT *mbs);
    RC DisableSli(MBSET_STRUCT *mbs);
    RC FindGpuIndexInList(LwPhysicalGpuHandle hPhysicalGpu,
                        LwPhysicalGpuHandle *PhysicalGpuHandleArray,
                        UINT32  uPhysicalGpuCount, UINT32 *index);

private:

    LwPhysicalGpuHandle  m_PhysicalGpuHandleArray[LWAPI_MAX_PHYSICAL_GPUS];
    LW_GPU_VALID_GPU_TOPOLOGIES  m_oSliTopologies;
    UINT32  m_PhysicalGpuCount;
    UINT32 m_sli_enable;
    MBSET_STRUCT mbs;
};

//! \brief SliOperationsTest constructor
//!
//! Initializes variables to default values.
//!
//! \sa Setup()
//----------------------------------------------------------------------
SliOperationsTest::SliOperationsTest()
{
    SetName("SliOperationsTest");

    memset(&m_oSliTopologies, 0, sizeof(LW_GPU_VALID_GPU_TOPOLOGIES));

    memset(&mbs, 0, sizeof(MBSET_STRUCT));

    for (UINT32 i = 0; i < LWAPI_MAX_PHYSICAL_GPUS ; i++) {
       m_PhysicalGpuHandleArray[i] = 0;
    }
}

//! \brief SliOperationsTest destructor
//!
//! \sa Cleanup()
//---------------------------------------------------------------------
SliOperationsTest::~SliOperationsTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! Check for Hardware Platform. If not return False
//! Check for subdevices >= MIN_SUBDEVICES_FOR_SLI.
//! If less than that, return False
//!
//! \return True if the test can be run in the current environment,
//!         False otherwise
//----------------------------------------------------------------------
string SliOperationsTest::IsTestSupported()
{
    RC rc;
    UINT32  uStatus = 0;

    // Initialize LWAPI
    LwAPI_Status LwApiStatus = LwAPI_Initialize();
    if (LwApiStatus != LWAPI_OK)
    {
        PRINT_ERROR(LwApiStatus, "LwAPI_Initialize");
        rc = RC::LWAPI_ERROR;
    }

    // Get the valid topologies
    m_oSliTopologies.version = LW_GPU_VALID_GPU_TOPOLOGIES_VER;

    LwApiStatus = LwAPI_GetValidGpuTopologies( &m_oSliTopologies , &uStatus );
    if  ( LwApiStatus != LWAPI_OK )
    {
        PRINT_ERROR(LwApiStatus, "LwAPI_GetValidGpuTopologies");
        rc = RC::LWAPI_ERROR;
    }

    // Get the GPU count
    LwApiStatus = LwAPI_EnumPhysicalGPUs(m_PhysicalGpuHandleArray, &m_PhysicalGpuCount );
    if  ( LwApiStatus != LWAPI_OK )
    {
        PRINT_ERROR(LwApiStatus, "LwAPI_EnumPhysicalGPUs");
        rc = RC::LWAPI_ERROR;
    }

    if  ( m_PhysicalGpuCount < 2 )
    {
        Printf(Tee::PriHigh, "The gpu count is less than 2\n");
        rc = RC::SLI_ERROR;
    }

    if (rc == OK)
       return RUN_RMTEST_TRUE;
    else
       return rc.Message();
}

//! \brief Setup all necessary state before running the test
//!
//! Get valid SLI configs.
//! Allocate space for SLI config list.
//!
//! \sa SliOperationsTest()
//!
//! \return OK if the test passed, test-specific RC if it failed
//----------------------------------------------------------------------
RC SliOperationsTest::Setup()
{
    LwRmPtr pLwRm;
    RC rc;

    mbs.enable = m_sli_enable;

    return rc;
}

//! \brief Exelwtes SliOperationsTest
//!
//! Link and Unlink each of SLI config list.
//!
//! \return Returns OK if the test passed, otherwise test-specific RC
//------------------------------------------------------------------------
RC SliOperationsTest::Run()
{
    LwAPI_Status LwApiStatus;
    LwRmPtr pLwRm;
    RC rc;

    Printf(Tee::PriHigh, "SLI Test : %d \n", m_sli_enable);

    if (mbs.enable) {
           // Enable SLI operation
           SetKeys("MB_Enable", 1);
           rc = EnableSli(&mbs);
          if (rc == OK) {
             Printf(Tee::PriHigh, "SLI Test : Enable Success \n");
       } else {
             Printf(Tee::PriHigh, "SLI Test : Enable Failure \n");
       }
    } else {
           // Disable SLI operation
           CleanKeys("MB_Enable");
           rc = DisableSli(&mbs);
          if (rc == OK) {
             Printf(Tee::PriHigh, "SLI Test : Disable Success \n");
       } else {
             Printf(Tee::PriHigh, "SLI Test : Disable Failure \n");
       }
    }

    // Applies the link / Unlink operation configured Using Enable/Disable SLI.
    LwApiStatus = LwAPI_RestartDisplayDriver( );
    if (LwApiStatus != LWAPI_OK)
    {
        PRINT_ERROR(LwApiStatus, "LwAPI_RestartDisplayDriver");
        return RC::LWAPI_ERROR;
    }

    return rc;
}

//! \brief Cleans up any SliOperationsTest leftovers
//!
//! Free the allocated Memory
//!
//! \sa ~SliOperationsTest()
//-------------------------------------------------------------------------
RC SliOperationsTest::Cleanup()
{
    return OK;
}

//! Enable SLI using LWAPI
//! if topology is 0, find the 'best' sli config.
//! otherwise, select the topology from the index provided or the mask
RC SliOperationsTest::EnableSli(MBSET_STRUCT *mbs)
{
    LwAPI_Status LwApiStatus;
    UINT32 uSelectedTopologyIndex = -1;

    //! Differ set topology to display reconfigure stage
    UINT32 SetFlags = ( LW_SET_GPU_TOPOLOGY_DEFER_APPLY |
                     LW_SET_GPU_TOPOLOGY_DEFER_3D_APP_SHUTDOWN |
                     LW_SET_GPU_TOPOLOGY_DEFER_DISPLAY_RECONFIG );
    ULONG  mincount = 0;
    RC rc;

    // select the 1st topology with the max number of GPUs.
    for  (UINT32 ul = 0; ul < m_oSliTopologies.gpuTopoCount ; ul++ )
    {
       // select all gpus in SLI
        if  ( m_oSliTopologies.gpuTopo[ul].gpuCount > mincount )
        {
            uSelectedTopologyIndex = ul;
            mincount = m_oSliTopologies.gpuTopo[ul].gpuCount;
        }
    }

    //! If there is no valid topology, return error.
    if  ( uSelectedTopologyIndex == -1 )
    {
        Printf(Tee::PriHigh, "No matching topology found\n");
        return RC::SLI_ERROR;
    }

    if  ( m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].status == LW_GPU_TOPOLOGY_STATUS_OK )
    {
        //! Set the master gpu = display GPU
        LwPhysicalGpuHandle  hMasterGpu;

        //! consider specifed gpuindex/dindex, while setting as master gpu.
        if (mbs->gpuindex == 0)
        {
            hMasterGpu = m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].hPhysicalGpu[m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].displayGpuIndex];
        }
        else
        {
            if (mbs->dindex > m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].gpuCount)
            {
                Printf(Tee::PriHigh, "Display index too high\n");
                return RC::SLI_ERROR;
            }

            UINT32 pg;
            if (FindGpuIndexInList(m_PhysicalGpuHandleArray[mbs->dindex],
                                   m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].hPhysicalGpu,
                                   m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].gpuCount,
                                   &pg) != OK)
            {
                return RC::SLI_ERROR;
            }

            m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].displayGpuIndex = pg;
            hMasterGpu = m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].hPhysicalGpu[pg];
        }

        //! Consider specified gpuindex and display mask, Validate and set display mask
        UINT32  uOutputMask = 0;
        LwApiStatus = LwAPI_GPU_GetConnectedOutputs( hMasterGpu, &uOutputMask );

        if  ( LwApiStatus != LWAPI_OK )
        {
            PRINT_ERROR(LwApiStatus, "LwAPI_GPU_GetConnectedOutputs");
            return RC::LWAPI_ERROR;
        }

       if  ( uOutputMask == 0 )
        {
            if (mbs->gpuindex)
            {
                 Printf(Tee::PriHigh, "SLI Test: no active display on master GPU\n");
            }

            for ( UINT32 gpuIndex = 0 ; gpuIndex < m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].gpuCount ; gpuIndex++ )
            {
                LwApiStatus = LwAPI_GPU_GetConnectedOutputs( m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].hPhysicalGpu[gpuIndex], &uOutputMask );
                if  (uOutputMask)
                {
                    m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].displayGpuIndex = gpuIndex;
                    if (mbs->gpuindex)
                    {
                        Printf(Tee::PriHigh, "SLI Test: display GPU index changed to %d\n", gpuIndex);
                    }
                    break;
                }
            }
        }

        if ( uOutputMask == 0 )
        {
           Printf(Tee::PriHigh, "SLI Test: no connected display found\n");
        }

        if (mbs->displaymask)
        {
            if ((mbs->dmask & uOutputMask) == 0)
            {
                Printf(Tee::PriHigh, "SLI Test: no connected ouptput for display mask provided\n");
            }
            uOutputMask = mbs->dmask;
        }

        //! if more than one active/connected choose just one of them.
        UINT32  uTargetMask = 1;
        if (uOutputMask)
        {
            for ( UINT32 uBitNumber = 0 ; uBitNumber < sizeof(UINT32) * 8 ; uBitNumber++ )
            {
                if  ( uTargetMask & uOutputMask )
                {
                  //! Pick the first connection.
                    break;
                }

                uTargetMask <<= 1;
            }
        }

        m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].displayOutputTargetMask = uTargetMask;
        m_oSliTopologies.gpuTopo[uSelectedTopologyIndex].flags = 0;             //No special flags for now.

        //! Create a list of valid topologies with one topology, the one we selected.
        LW_GPU_VALID_GPU_TOPOLOGIES oSetSliTopologies;
        oSetSliTopologies.version = LW_GPU_VALID_GPU_TOPOLOGIES_VER;
        oSetSliTopologies.gpuTopoCount = 1;
        memcpy(oSetSliTopologies.gpuTopo,
               &m_oSliTopologies.gpuTopo[uSelectedTopologyIndex],
               sizeof(LW_GPU_TOPOLOGY));

       //! Sets up topology to enable linking and unlinking of GPU,
       //! while restarting the display
        LwApiStatus = LwAPI_SetGpuTopologies( &oSetSliTopologies, SetFlags );
        if (LwApiStatus != LWAPI_OK)
        {
            PRINT_ERROR(LwApiStatus, "LwAPI_SetGpuTopologies");
            return RC::LWAPI_ERROR;
        }
    }
    else
    {
        Printf(Tee::PriHigh, "Selected topology is not valid\n");
        return RC::SLI_ERROR;
    }

    return rc;
}

//! Disable SLI using LWAPI
//! Set the gpu count of all the topologies to one.
//! And apply the new topology
RC SliOperationsTest::DisableSli(MBSET_STRUCT *mbs)
{
    RC rc;
    LwAPI_Status  LwApiStatus;
    //! Differ set topology to display reconfigure stage
    UINT32 SetFlags = ( LW_SET_GPU_TOPOLOGY_DEFER_APPLY |
                     LW_SET_GPU_TOPOLOGY_DEFER_3D_APP_SHUTDOWN |
                     LW_SET_GPU_TOPOLOGY_DEFER_DISPLAY_RECONFIG );

    //! Create a topology list with one single GPU topology
    m_oSliTopologies.version = LW_GPU_VALID_GPU_TOPOLOGIES_VER;
    m_oSliTopologies.gpuTopoCount = m_PhysicalGpuCount;
    for (UINT32 nGpuIndex = 0; nGpuIndex < m_PhysicalGpuCount ; nGpuIndex++ )
    {
        m_oSliTopologies.gpuTopo[nGpuIndex].version = LW_GPU_VALID_GPU_TOPOLOGIES_VER;
        m_oSliTopologies.gpuTopo[nGpuIndex].hPhysicalGpu[0] = m_PhysicalGpuHandleArray[nGpuIndex];
        m_oSliTopologies.gpuTopo[nGpuIndex].displayOutputTargetMask = 0;
        m_oSliTopologies.gpuTopo[nGpuIndex].displayGpuIndex = 0;
        m_oSliTopologies.gpuTopo[nGpuIndex].flags = 0;
        m_oSliTopologies.gpuTopo[nGpuIndex].gpuCount = 1;
    }

    //! Sets the topology to cause unlinking of gpu's
    LwApiStatus = LwAPI_SetGpuTopologies(&m_oSliTopologies, SetFlags);

    if (LwApiStatus != LWAPI_OK)
    {
        PRINT_ERROR(LwApiStatus, "LwAPI_SetGpuTopologies");
        return RC::LWAPI_ERROR;
    }

    return rc;
}

//! Search for specified hPhysicalGpu and return its gpuindex
RC SliOperationsTest::FindGpuIndexInList(LwPhysicalGpuHandle hPhysicalGpu,
                        LwPhysicalGpuHandle *PhysicalGpuHandleArray,
                        UINT32  uPhysicalGpuCount, UINT32 *index)
{
    UINT32 pg;
    RC rc;

    for (pg=0; pg < uPhysicalGpuCount; pg++)
    {
        if (PhysicalGpuHandleArray[pg] == hPhysicalGpu)
        {
               break;
        }
    }

    if (pg == uPhysicalGpuCount)
    {
        Printf(Tee::PriHigh, "\nError: Handle '0x%x' in this topology not found\n", hPhysicalGpu);
        return RC::SLI_ERROR;
    }

    *index = pg;

    return rc;
}

// Remove MB_Enable keys
void CleanKeys(const char *key_name)
{
    CHAR     achKey[MAX_KEY_LENGTH];   // buffer for subkey name
    DWORD    cbName;                   // size of name string
    CHAR     achClass[MAX_PATH] = "";  // buffer for class name
    DWORD    cchClassName = MAX_PATH;  // size of class string
    DWORD    cSubKeys=0;               // number of subkeys
    DWORD    cbMaxSubKey;              // longest subkey size
    DWORD    cchMaxClass;              // longest class string
    DWORD    cValues;              // number of values for key
    DWORD    cchMaxValue;          // longest value name
    DWORD    cbMaxValueData;       // longest value data
    DWORD    cbSelwrityDescriptor; // size of security descriptor
    FILETIME ftLastWriteTime;      // last write time

    DWORD i, retCode;
    CHAR t1[4096];

    HKEY hKey;

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\LwrrentControlSet\\Control\\Video", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {

        // Get the class name and the value count.
        retCode = RegQueryInfoKey(
            hKey,                    // key handle
            achClass,                // buffer for class name
            &cchClassName,           // size of class string
            NULL,                    // reserved
            &cSubKeys,               // number of subkeys
            &cbMaxSubKey,            // longest subkey size
            &cchMaxClass,            // longest class string
            &cValues,                // number of values for this key
            &cchMaxValue,            // longest value name
            &cbMaxValueData,         // longest value data
            &cbSelwrityDescriptor,   // security descriptor
            &ftLastWriteTime);       // last write time

        // Enumerate the subkeys, until RegEnumKeyEx fails.

        if (cSubKeys)
        {
            for (i=0; i<cSubKeys; i++)
            {
                HKEY hNKey;

                cbName = MAX_KEY_LENGTH;
                retCode = RegEnumKeyEx(hKey, i,
                         achKey,
                         &cbName,
                         NULL,
                         NULL,
                         NULL,
                         &ftLastWriteTime);

                if (retCode == ERROR_SUCCESS)
                {
                    sprintf(t1, "SYSTEM\\LwrrentControlSet\\Control\\Video\\%s\\0000", achKey);

                    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, t1, 0, KEY_WRITE, &hNKey) == ERROR_SUCCESS)
                    {
                        retCode = RegDeleteValue(hNKey, key_name);
                        RegCloseKey(hNKey);
                    }

                    sprintf(t1, "SYSTEM\\LwrrentControlSet\\Control\\Video\\%s\\0001", achKey);

                    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, t1, 0, KEY_WRITE, &hNKey) == ERROR_SUCCESS)
                    {
                        retCode = RegDeleteValue(hNKey, key_name);
                        RegCloseKey(hNKey);
                    }
                }
            }
        }
        RegCloseKey(hKey);
    }
}

//! Set windows XP registry key to Value
void SetKeys(const char *key_name, const UINT32 value)
{
    HKEY hKeyDS;
    HKEY hKeyDT;
    DWORD dwType;
    char pVal[1000];
    char *p=pVal+57;
    DWORD dwRes;
    char t0[1000];
    char t1[1000];

    // Enable SLI by setting MB_Enable
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\VIDEO", 0, KEY_READ, &hKeyDS) == ERROR_SUCCESS)
    {
        for (int i=0; i<5; i++)
        {
            sprintf(t0, "\\Device\\Video%d", i);

            dwRes = 1000;

            if (RegQueryValueEx(hKeyDS, t0, 0, &dwType, (unsigned char *)pVal, &dwRes)==ERROR_SUCCESS)
            {
                sprintf(t1, "SYSTEM\\LwrrentControlSet\\Control\\Video\\%s",  p);

                if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, t1, 0, KEY_WRITE, &hKeyDT) == ERROR_SUCCESS)
                {
                    RegSetValueEx(hKeyDT, key_name, 0, REG_DWORD, (BYTE *)&value, sizeof(value));
                    RegCloseKey(hKeyDT);
                }
            }
        }
        RegCloseKey(hKeyDS);
    }

}

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ SliOperationsTest
//! object.
//-------------------------------------------------------------------------
JS_CLASS_INHERIT(SliOperationsTest, RmTest,
                 "SLI Memory Leak Test.");
CLASS_PROP_READWRITE(SliOperationsTest, sli_enable, UINT32,
    "SLI state change 1-Enable 0-Disable");
