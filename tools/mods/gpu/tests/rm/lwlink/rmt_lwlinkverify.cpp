/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \rmt_lwlinkverify.cpp
//! \Provides access to lwlink topology and counters to tests
//!

#include "class/cl90cc.h"

#include "gpu/tests/rmtest.h"

#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpumgr.h"
#include "core/include/platform.h"
#include "ctrl/ctrl90cc.h"
#include "ctrl/ctrl2080.h"
#include "lwRmApi.h"

#include "rmt_lwlinkverify.h"

#define DBDF_STRING_SIZE 14 // This is how many bytes long a dbdf string should be

//!
//! Default constructor. Do not explicitly use this.
//!-----------------------------------------------------------------------------
LwlinkVerify::LwlinkVerify()
{
    topology             = NULL;
    bCounterApiSupported = false;
    bCountersRecorded    = false;
    bCountersAllocated   = false;
    m_NumDevices         = 0;
    totalLinksFound      = 0;
    m_pGpuDevMgr         = NULL;
    subdevList           = NULL;
    handleList           = NULL;
}

//!
//! Setup function. Parameters are a GpuDeviceManager pointer to a
//! GpuDevMgr that will house GpuSubdevice information and a UINT32 that
//! specifies the number of GpuSubdevices in the topology.
//!-----------------------------------------------------------------------------
void LwlinkVerify::Setup
(
    GpuDevMgr *mgr,
    UINT32     numDevices
)
{
    unsigned int i;
    topology             = NULL;
    bCounterApiSupported = true;
    bCountersRecorded    = false;
    bCountersAllocated   = false;
    totalLinksFound      = 0;
    m_NumDevices         = numDevices;
    m_pGpuDevMgr         = mgr;

    // Allocate space for the subdevice list
    if (m_NumDevices != 0)
    {
        subdevList = (GpuSubdevice **)malloc(sizeof(GpuSubdevice *) * m_NumDevices);
        if (subdevList != NULL)
        {
            for (i = 0; i < m_NumDevices; i++)
            {
                subdevList[i] = NULL;
            }
        }
    }
    else
    {
        subdevList = NULL;
    }

    // Allocate space for the handle list
    if (subdevList != NULL)
    {
        handleList = (LwU32 *)malloc(sizeof(LwU32) * m_NumDevices);
        if (handleList != NULL)
        {
            memset(handleList, 0, sizeof(LwU32) * m_NumDevices);
        }
    }
    else
    {
        handleList = NULL;
    }

    setupLwlinkVerify();
    printTopologyInfo();
}

//!
//! Destructor
//!-----------------------------------------------------------------------------
LwlinkVerify::~LwlinkVerify()
{
    // Taken care of in Cleanup().
}

//!
//! Returns true if the parameter GpuSubdevice supports this class
//!-----------------------------------------------------------------------------
bool LwlinkVerify::IsSupported(GpuSubdevice *pSubdev)
{
    LwRmPtr   pLwRm;
    LW_STATUS status;
    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS capsParams;

    memset(&capsParams, 0, sizeof(capsParams));

    status = LwRmControl(pLwRm->GetClientHandle(),
                         pLwRm->GetSubdeviceHandle(pSubdev),
                         LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS,
                         &capsParams,
                         sizeof(capsParams));
    if (status == LW_OK)
    {
        return LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &capsParams.capsTbl),
                                           LW2080_CTRL_LWLINK_CAPS_SUPPORTED);
    }
    else
    {
        return false;
    }
}

//!
//! Allocates and populates the topology structure, allocates and readies
//! lwlink counters of GpuSubdevices in that structure, and populates
//! subdevList with the GpuSubdevices in that structure.Sigphi04
//!
//! Each struct LwlinkTopologyInfo corresponds to a unique ordered pair of
//! GpuSubdevices. To access information regarding one ilwolved
//! GpuSubdevice's view of a link, index into the topology array
//! with <localGpuNumber>*m_NumDevices + <remoteGpuNumber>. The local
//! and remote GpuNumber being the same will index into a struct
//! LwlinkTopologyInfo with information about lwlink loopback/loopout on
//! that particular GpuSubdevice.
//!
//! A function that simplifies indexing into this array is provided in this
//! class, getTopologyIndex().
//!-----------------------------------------------------------------------------
void LwlinkVerify::setupLwlinkVerify()
{
    if (m_NumDevices == 0)
    {
        bCounterApiSupported = false;
        return;
    }

    if ((subdevList == NULL) || (handleList == NULL))
    {
        bCounterApiSupported = false;
        Printf(Tee::PriHigh,
               "Failed to malloc necessary space for lwlinkVerify internal list,"
               "topology and counter functionality disabled for this run.\n");
        return;
    }

    if (topology != NULL)
    {
        // Don't allow over-writing a live topology pointer
        return;
    }

    GpuDevice    *pDev_i    = NULL;
    GpuSubdevice *pSubdev_i = NULL;
    GpuDevice    *pDev_j    = NULL;
    GpuSubdevice *pSubdev_j = NULL;

    unsigned int i, j;

    topology = (struct LwlinkTopologyInfo *)malloc(m_NumDevices * m_NumDevices *
                                                   sizeof(struct LwlinkTopologyInfo));
    if (topology == NULL)
    {
        bCounterApiSupported = false;
        Printf(Tee::PriHigh, "Failed to malloc necessary space for topology structure,"
                            " topology and counter functionality disabled for this run.\n");
        return;
    }
    memset(topology, 0, m_NumDevices * m_NumDevices * sizeof(struct LwlinkTopologyInfo));

    LwRmPtr pLwRm;

    for (i = 0; i < m_NumDevices; i++)
    {
        // Get the device
        pDev_i = ( i == 0 ? m_pGpuDevMgr->GetFirstGpuDevice() :
                            m_pGpuDevMgr->GetNextGpuDevice(pDev_i) );

        // Get the subdevice
        pSubdev_i = pDev_i->GetSubdevice(0);

        // Allocate a handle for the subdevice
        LwU32 subDevHandle = pLwRm->GetSubdeviceHandle(pSubdev_i);
        LwU32 objHandle    = 0;
        pLwRm->Alloc(subDevHandle, &objHandle, GF100_PROFILER, NULL);
        handleList[i]      = objHandle;

        if (objHandle == 0)
        {
            bCounterApiSupported = false;
        }

        if (bCounterApiSupported)
        {
            subdevList[i] = pSubdev_i;
        }

        setupLwlinkCounters(i);
    }

    /*
     * Get the topology information for all possible subdevice pairs  *
     * LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS triggers the topology *
     * discovery and returns all the connections associated with the  *
     * links for a given subdevice. The control call should be called *
     * on all the subdevice pairs                                     *
     */
    for (i = 0; i < m_NumDevices; i++)
    {
        // Get the first device
        pDev_i = ( i == 0 ? m_pGpuDevMgr->GetFirstGpuDevice() :
                            m_pGpuDevMgr->GetNextGpuDevice(pDev_i) );

        // Get the first subdevice
        pSubdev_i = pDev_i->GetSubdevice(0);

        for (j = i; j < m_NumDevices; j++)
        {
            // Get the second device
            pDev_j = ( j == i ? pDev_i :
                      m_pGpuDevMgr->GetNextGpuDevice(pDev_j) );

            // Get the second subdevice
            pSubdev_j = pDev_j->GetSubdevice(0);

            LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS linkParams_i;
            memset(&linkParams_i, 0, sizeof(linkParams_i));

            // Populate the links information for first subdevice
            LwRmControl(pLwRm->GetClientHandle(),
                        pLwRm->GetSubdeviceHandle(pSubdev_i),
                        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                        &linkParams_i,
                        sizeof(linkParams_i));

            LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS linkParams_j;
            memset(&linkParams_j, 0, sizeof(linkParams_j));

            // Populate the links information for second subdevice
            LwRmControl(pLwRm->GetClientHandle(),
                        pLwRm->GetSubdeviceHandle(pSubdev_j),
                        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                        &linkParams_j,
                        sizeof(linkParams_j));

            unsigned int pairLinksFound = 0;
            unsigned int linkNum;

            FOR_EACH_INDEX_IN_MASK(32, linkNum, linkParams_i.enabledLinkMask)
            {
                unsigned int remoteLinkNum = (unsigned int)linkParams_i.linkInfo[linkNum].remoteDeviceLinkNumber;

                if ( (remoteLinkNum < MAX_LWLINK_LINKS) &&

                    // Both the link and the remote link should be valid
                    (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &linkParams_i.linkInfo[linkNum].capsTbl),       LW2080_CTRL_LWLINK_CAPS_VALID)) &&
                    (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &linkParams_j.linkInfo[remoteLinkNum].capsTbl), LW2080_CTRL_LWLINK_CAPS_VALID)) &&

                    // DBDF values and link number should be consistent for the link's and remote link's topology structures
                    (linkParams_j.linkInfo[remoteLinkNum].remoteDeviceLinkNumber  == linkParams_i.linkInfo[linkNum].localDeviceLinkNumber)   &&
                    (linkParams_j.linkInfo[remoteLinkNum].remoteDeviceInfo.domain == linkParams_i.linkInfo[linkNum].localDeviceInfo.domain)  &&
                    (linkParams_j.linkInfo[remoteLinkNum].remoteDeviceInfo.bus    == linkParams_i.linkInfo[linkNum].localDeviceInfo.bus)     &&
                    (linkParams_j.linkInfo[remoteLinkNum].remoteDeviceInfo.device == linkParams_i.linkInfo[linkNum].localDeviceInfo.device)  &&
                    (linkParams_j.linkInfo[remoteLinkNum].localDeviceInfo.domain  == linkParams_i.linkInfo[linkNum].remoteDeviceInfo.domain) &&
                    (linkParams_j.linkInfo[remoteLinkNum].localDeviceInfo.bus     == linkParams_i.linkInfo[linkNum].remoteDeviceInfo.bus)    &&
                    (linkParams_j.linkInfo[remoteLinkNum].localDeviceInfo.device  == linkParams_i.linkInfo[linkNum].remoteDeviceInfo.device) &&

                    // The link between the subdevices should be marked connected
                    (linkParams_i.linkInfo[linkNum].connected == LW_TRUE) )
                {
                    // to find the link info on the 'i' side of the link, index with i first and j second
                    topology[getTopologyIndex(i, j)].linkMask |= (1 << linkNum);

                    // to find the link info on the 'j' side of the link, index with j first and i second
                    topology[getTopologyIndex(j, i)].linkMask |= (1 << remoteLinkNum);

                    // Cache the topology information for the local link
                    memcpy(&topology[getTopologyIndex(i, j)].linkStats[linkNum],
                           &linkParams_i.linkInfo[linkNum],
                           sizeof(LW2080_CTRL_LWLINK_LINK_STATUS_INFO));

                    // Cache the topology information for the remote link
                    memcpy(&topology[getTopologyIndex(j, i)].linkStats[remoteLinkNum],
                           &linkParams_j.linkInfo[remoteLinkNum],
                           sizeof(LW2080_CTRL_LWLINK_LINK_STATUS_INFO));

                    pairLinksFound++;
                    totalLinksFound++;
                }
                else if ((linkParams_i.linkInfo[linkNum].connected == LW_TRUE) &&
                        (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &linkParams_i.linkInfo[linkNum].capsTbl), LW2080_CTRL_LWLINK_CAPS_VALID))         &&
                        (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &linkParams_i.linkInfo[linkNum].capsTbl), LW2080_CTRL_LWLINK_CAPS_SYSMEM_ACCESS)) &&
                        (i == j))
                {
                    // Store SYSMEM links in loopback section of topology array
                    topology[getTopologyIndex(i, j)].linkMask |= (1 << linkNum);

                    // Cache the SYSMEM link topology information
                    memcpy(&topology[getTopologyIndex(i, j)].linkStats[linkNum],
                           &linkParams_i.linkInfo[linkNum],
                           sizeof(LW2080_CTRL_LWLINK_LINK_STATUS_INFO));

                    totalLinksFound++;
                    pairLinksFound++;
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;

            topology[getTopologyIndex(i, j)].numPairLinks = pairLinksFound;
            topology[getTopologyIndex(j, i)].numPairLinks = pairLinksFound;
        }
    }

    return;
}

//!
//! Prints out all information about the lwlink topology.
//!
//! If this is called after counter values have been recorded, it will also
//! print out the counters' results.
//!-----------------------------------------------------------------------------
void LwlinkVerify::printTopologyInfo()
{
    if (topology == NULL)
    {
        return;
    }

    unsigned int i, j;
    unsigned int linkNum, remoteLinkNum;

    char dbdf1[DBDF_STRING_SIZE];
    char dbdf2[DBDF_STRING_SIZE];

    if (totalLinksFound != 0)
    {
        Printf(Tee::PriLow, "LwLink topology information:\n");
    }
    else
    {
        return;
    }

    for (i = 0; i < m_NumDevices; i++)
    {
        for (j = i; j < m_NumDevices; j++)
        {
            FOR_EACH_INDEX_IN_MASK(32, linkNum, topology[getTopologyIndex(i, j)].linkMask)
            {
                remoteLinkNum = topology[getTopologyIndex(i, j)].linkStats[linkNum].remoteDeviceLinkNumber;

                setDBDFString(dbdf1,
                              topology[getTopologyIndex(i, j)].linkStats[linkNum].localDeviceInfo.domain,
                              topology[getTopologyIndex(i, j)].linkStats[linkNum].localDeviceInfo.bus,
                              topology[getTopologyIndex(i, j)].linkStats[linkNum].localDeviceInfo.device);

                setDBDFString(dbdf2,
                              topology[getTopologyIndex(i, j)].linkStats[linkNum].remoteDeviceInfo.domain,
                              topology[getTopologyIndex(i, j)].linkStats[linkNum].remoteDeviceInfo.bus,
                              topology[getTopologyIndex(i, j)].linkStats[linkNum].remoteDeviceInfo.device);

                if ((i == j) &&
                    (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &topology[getTopologyIndex(i, j)].linkStats[linkNum].capsTbl),
                                                LW2080_CTRL_LWLINK_CAPS_SYSMEM_ACCESS)))
                {
                    Printf(Tee::PriLow, "Lwlink SYSMEM link detected on %s (link %d) with lwlinkCapsTbl 0x%x\n",
                           dbdf1,
                           topology[getTopologyIndex(i, j)].linkStats[linkNum].localDeviceLinkNumber,
                           topology[getTopologyIndex(i, j)].linkStats[linkNum].capsTbl);

                    if (bCountersRecorded)
                    {
                        // Remember that this number is not entirely accurate, there may have been an overflow
                        Printf(Tee::PriLow, "Recorded traffic on (%s)'s link %d: %lld bytes sent and %lld bytes received\n",
                               dbdf1,
                               topology[getTopologyIndex(i, j)].linkStats[linkNum].localDeviceLinkNumber,
                               topology[getTopologyIndex(i, j)].bytesSent[linkNum],
                               topology[getTopologyIndex(i, j)].bytesReceived[linkNum]);
                    }
                }
                else
                {
                    Printf(Tee::PriLow, "LwLink link detected between %s (link %d) and %s (link %d):\n",
                           dbdf1,
                           topology[getTopologyIndex(i, j)].linkStats[linkNum].localDeviceLinkNumber,
                           dbdf2,
                           topology[getTopologyIndex(i, j)].linkStats[linkNum].remoteDeviceLinkNumber);

                    Printf(Tee::PriLow, "Link number %d of %s has lwlinkCapsTbl = 0x%x\n",
                           topology[getTopologyIndex(i, j)].linkStats[linkNum].localDeviceLinkNumber,
                           dbdf1,
                           topology[getTopologyIndex(i, j)].linkStats[linkNum].capsTbl);

                    if (bCountersRecorded)
                    {
                        // Remember that this number is not entirely accurate, there may have been an overflow
                        Printf(Tee::PriLow, "Recorded traffic on (%s)'s link %d: %lld bytes sent and %lld bytes received\n",
                               dbdf1,
                               topology[getTopologyIndex(i, j)].linkStats[linkNum].localDeviceLinkNumber,
                               topology[getTopologyIndex(i, j)].bytesSent[linkNum],
                               topology[getTopologyIndex(i, j)].bytesReceived[linkNum]);
                    }

                    if (remoteLinkNum < MAX_LWLINK_LINKS)
                    {
                        setDBDFString(dbdf2,
                                      topology[getTopologyIndex(i, j)].linkStats[linkNum].remoteDeviceInfo.domain,
                                      topology[getTopologyIndex(i, j)].linkStats[linkNum].remoteDeviceInfo.bus,
                                      topology[getTopologyIndex(i, j)].linkStats[linkNum].remoteDeviceInfo.device);

                        Printf(Tee::PriLow, "Link number %d of %s has lwlinkCapsTbl = 0x%x\n",
                               topology[getTopologyIndex(i, j)].linkStats[linkNum].remoteDeviceLinkNumber,
                               dbdf2,
                               topology[getTopologyIndex(j, i)].linkStats[remoteLinkNum].capsTbl);

                        if (bCountersRecorded)
                        {
                            // Remember that this number is not entirely accurate, there may have been an overflow
                            Printf(Tee::PriLow, "Recorded traffic on (%s)'s link %d: %lld bytes sent and %lld bytes received\n",
                                   dbdf2,
                                   topology[getTopologyIndex(i, j)].linkStats[linkNum].remoteDeviceLinkNumber,
                                   topology[getTopologyIndex(j, i)].bytesSent[remoteLinkNum],
                                   topology[getTopologyIndex(j, i)].bytesReceived[remoteLinkNum]);
                        }
                    }
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }
    }
}

//!
//! Returns an the total #lwlinks
//!-----------------------------------------------------------------------------
unsigned int LwlinkVerify::getTotalLinksFound()
{
    return totalLinksFound;
}

//!
//! Returns an index into the topology array that corresponds to the entry
//! in which the first parameter corresponds to the local end of the link
//! and the second parameter corresponds to the remote end of the link.
//!
//! Caution should be taken to pass parameters to this function that are
//! less than m_NumDevices because otherwise the topology array will likely
//! be accessed beyond its last element.
//!
//! The parameters are unsigned integers which are (or will be) the index in
//! this object's subdevList corresponding to the intended GpuSubdevice.
//!
//!-----------------------------------------------------------------------------
unsigned int LwlinkVerify::getTopologyIndex
(
    unsigned int localSubDev,
    unsigned int remoteSubDev
)
{
    return (localSubDev * m_NumDevices + remoteSubDev);
}

//!
//! Returns the integer index of a parameter GpuSubdevice if it is in
//! this object's list. Returns -1 otherwise.
//!-----------------------------------------------------------------------------
int LwlinkVerify::getIndexOfSubdev(GpuSubdevice *pSubdev)
{
    if (subdevList == NULL)
    {
        return -1;
    }

    unsigned int i;
    for (i = 0; i < m_NumDevices; i++)
    {
        if (subdevList[i] == pSubdev)
        {
            return (int)i;
        }
    }

    return -1;
}

//!
//! Returns true if all the links transition to the target low power state.
//!
//!-----------------------------------------------------------------------------
bool LwlinkVerify::verifyPowerStateTransition(unsigned int targetState)
{
    LwRmPtr       pLwRm;
    GpuDevice    *pDev_i    = NULL;
    GpuSubdevice *pSubdev_i = NULL;
    LW_STATUS     status    = LW_OK;

    LW2080_CTRL_LWLINK_SET_POWER_STATE_PARAMS setParams;
    LW2080_CTRL_LWLINK_GET_POWER_STATE_PARAMS getParams;
    unsigned int i, j, linkId, peerIdx;

    // Put the connections into the target power state
    for (i = 0; i < m_NumDevices; i++)
    {
        // Get the first device
        pDev_i = ( i == 0 ? m_pGpuDevMgr->GetFirstGpuDevice() :
                            m_pGpuDevMgr->GetNextGpuDevice(pDev_i) );

        // Get the first subdevice
        pSubdev_i = pDev_i->GetSubdevice(0);

        for (j = 0; j < m_NumDevices; j++)
        {
            peerIdx = getTopologyIndex(i, j);

            // If there are no connections between i and j, continue
            if (topology[peerIdx].linkMask == 0)
                continue;

            memset(&setParams, 0, sizeof(setParams));
            setParams.linkMask   = topology[peerIdx].linkMask;
            setParams.powerState = targetState;

            // Request target state on the connections
            status = LwRmControl(pLwRm->GetClientHandle(),
                                 pLwRm->GetSubdeviceHandle(pSubdev_i),
                                 LW2080_CTRL_CMD_LWLINK_SET_POWER_STATE,
                                 &setParams,
                                 sizeof(setParams));

            if (status == LW_WARN_MORE_PROCESSING_REQUIRED)
            {
                Printf(Tee::PriLow, "Links cannot transition to state 0x%x"
                                    " till both ends agree\n", targetState);
                continue;
            }

            if (status != LW_OK)
            {
                Printf(Tee::PriHigh, "Link state transition to state 0x%x failed."
                                     " Failing test\n", targetState);
                return false;
            }
        }
    }

    //
    // After target state is requested on all endpoints, all links should
    // transition. Get the power state of all the connections to confirm
    //
    for (i = 0; i < m_NumDevices; i++)
    {
        // Get the first device
        pDev_i = ( i == 0 ? m_pGpuDevMgr->GetFirstGpuDevice() :
                            m_pGpuDevMgr->GetNextGpuDevice(pDev_i) );

        // Get the first subdevice
        pSubdev_i = pDev_i->GetSubdevice(0);

        for (j = 0; j < m_NumDevices; j++)
        {
            peerIdx = getTopologyIndex(i, j);

            FOR_EACH_INDEX_IN_MASK(32, linkId, topology[peerIdx].linkMask)
            {
                memset(&getParams, 0, sizeof(getParams));
                getParams.linkId = linkId;

                // Get the power state of all the connections
                status = LwRmControl(pLwRm->GetClientHandle(),
                                     pLwRm->GetSubdeviceHandle(pSubdev_i),
                                     LW2080_CTRL_CMD_LWLINK_GET_POWER_STATE,
                                     &getParams,
                                     sizeof(getParams));

                // Fail the test if the connection is not in the desired state
                if ((status != LW_OK) || (getParams.powerState != targetState))
                {
                    Printf(Tee::PriHigh, "GPU%d: Link%d: did not transition to"
                                " target state 0x%x\n", i, linkId, targetState);

                    return false;
                }
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }
    }

    return true;
}

//!
//! Returns true if all sysmem links on the specified GpuSubdevice saw traffic.
//!
//!-----------------------------------------------------------------------------
bool LwlinkVerify::verifySysmemTraffic(GpuSubdevice *pSubdev)
{
    LwU32 desiredCaps = 0?LW2080_CTRL_LWLINK_CAPS_SYSMEM_ACCESS;
    desiredCaps = desiredCaps << (8 * (1?LW2080_CTRL_LWLINK_CAPS_SYSMEM_ACCESS));

    return verifyRelevantTraffic(pSubdev, pSubdev, desiredCaps, false);
}

//!
//! Returns true if all p2p links between the specified pair of GpuSubdevices
//! saw traffic.
//!
//!-----------------------------------------------------------------------------
bool LwlinkVerify::verifyP2PTraffic
(
    GpuSubdevice *pSubdev1,
    GpuSubdevice *pSubdev2
)
{
    LwU32 desiredCaps = 0?LW2080_CTRL_LWLINK_CAPS_P2P_SUPPORTED;
    desiredCaps = desiredCaps << ( 8 * (1?LW2080_CTRL_LWLINK_CAPS_P2P_SUPPORTED));

    //
    // Ideally the final parameter to the below call should be 'true', but it
    // has been making the tests fail because of mismatched counter values so far
    //

    return verifyRelevantTraffic(pSubdev1, pSubdev2, desiredCaps, true);
}

//!
//! Returns true if all expected traffic between the two specified GpuSubdevices
//! was seen, false otherwise. Also returns true if the counter API is
//! unsupported for the test.
//!
//! Traffic is expected on active links between the two GpuSubdevices that have
//! all of the specified Caps in desiredCaps.
//!
//! This function will call recordCounters() if bCountersRecorded is false.
//!
//!-----------------------------------------------------------------------------
bool LwlinkVerify::verifyRelevantTraffic
(
    GpuSubdevice *pSubdev1,
    GpuSubdevice *pSubdev2,
    LwU32         desiredCaps,
    bool          matchTraffic
)
{
    LwRmPtr  pLwRm;
    bool     ret    = true;
    bool     sysmem = false;
    int      index1 = getIndexOfSubdev(pSubdev1);
    int      index2 = getIndexOfSubdev(pSubdev2);
    int      linkNum;
    char     dbdf1[DBDF_STRING_SIZE];
    char     dbdf2[DBDF_STRING_SIZE];

    unsigned int linkId       = 0xFF;
    unsigned int remoteLinkId = 0;

    if( (!bCounterApiSupported) || (totalLinksFound == 0))
    {
        Printf(Tee::PriLow, "Lwlink TL throughput counters unsupported for this test,"
                            " so verifyRelevantTraffic passes by default.\n");
        return true;
    }

    // Record the counters associated with the links
    if (!bCountersRecorded)
    {
        recordCounters();
    }

    if ((index1 == -1) || (index2 == -1) || (topology == NULL))
    {
        Printf(Tee::PriHigh, "Invalid parameters to verifyRelevantTraffic, failing test\n");
        return false;
    }

    // Get the link statuses for links between subdev1 and subdev2
    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS linkParams_1;
    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS linkParams_2;

    memset(&linkParams_1, 0, sizeof(linkParams_1));
    memset(&linkParams_2, 0, sizeof(linkParams_2));

    // Populate the links information for first subdevice
    LwRmControl(pLwRm->GetClientHandle(),
                pLwRm->GetSubdeviceHandle(pSubdev1),
                LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                &linkParams_1,
                sizeof(linkParams_1));

    // Populate the links information for second subdevice
    LwRmControl(pLwRm->GetClientHandle(),
                pLwRm->GetSubdeviceHandle(pSubdev2),
                LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                &linkParams_2,
                sizeof(linkParams_2));

    // If the desired caps is for LWLink SYSMEM support and there are no SYSMEM links, return
    if (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&desiredCaps), LW2080_CTRL_LWLINK_CAPS_SYSMEM_ACCESS))
    {
        // For all the links of subdev1 that connect to subdev2 (subdev1 and subdev2 are same for sysmem)
        FOR_EACH_INDEX_IN_MASK(32, linkNum, topology[getTopologyIndex(index1, index2)].linkMask)
        {
            // If the link is not a SYSMEM link, then continue to the next link
            if (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &topology[getTopologyIndex(index1, index2)].linkStats[linkNum].capsTbl),
                                            LW2080_CTRL_LWLINK_CAPS_SYSMEM_ACCESS))
            {
                sysmem = true;
                break;
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;

        // If this is a non-IBM system which has no sysmem links, then return
        if (sysmem == false && !Platform::IsPPC())
        {
            Printf(Tee::PriHigh, "No sysmem lwlinks, traffic over pcie, passing test\n");
            return true;
        }
    }

    // For all the links of subdev1 that connect to subdev2
    FOR_EACH_INDEX_IN_MASK(32, linkNum, topology[getTopologyIndex(index1, index2)].linkMask)
    {
        // Link should be in ACTIVE state
        if ( !( (linkParams_1.linkInfo[linkNum].linkState == LW2080_CTRL_LWLINK_STATUS_LINK_STATE_ACTIVE)
                &&
                // Rx sublink should be in HS or Single-lane mode
                ((linkParams_1.linkInfo[linkNum].rxSublinkStatus == LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_HIGH_SPEED_1) ||
                 (linkParams_1.linkInfo[linkNum].rxSublinkStatus == LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_SINGLE_LANE))
                &&
                // Tx sublink should be in HS or Single-lane mode
                ((linkParams_1.linkInfo[linkNum].txSublinkStatus == LW2080_CTRL_LWLINK_STATUS_SUBLINK_TX_STATE_HIGH_SPEED_1) ||
                 (linkParams_1.linkInfo[linkNum].txSublinkStatus == LW2080_CTRL_LWLINK_STATUS_SUBLINK_TX_STATE_SINGLE_LANE))
           )  )
        {
            // Fail the test
            Printf(Tee::PriHigh, "GPU%d: Link%d: Link state not ACTIVE ! Failing test.\n",
                                  index1, linkNum);
            return false;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // For all the links of subdev2 that connect to subdev1
    FOR_EACH_INDEX_IN_MASK(32, linkNum, topology[getTopologyIndex(index2, index1)].linkMask)
    {
        // Link should be in ACTIVE state
        if ( !( (linkParams_2.linkInfo[linkNum].linkState == LW2080_CTRL_LWLINK_STATUS_LINK_STATE_ACTIVE)
                &&
                // Rx sublink should be in HS or Single-lane mode
                ((linkParams_2.linkInfo[linkNum].rxSublinkStatus == LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_HIGH_SPEED_1) ||
                 (linkParams_2.linkInfo[linkNum].rxSublinkStatus == LW2080_CTRL_LWLINK_STATUS_SUBLINK_RX_STATE_SINGLE_LANE))
                &&
                // Tx sublink should be in HS or Single-lane mode
                ((linkParams_2.linkInfo[linkNum].txSublinkStatus == LW2080_CTRL_LWLINK_STATUS_SUBLINK_TX_STATE_HIGH_SPEED_1) ||
                 (linkParams_2.linkInfo[linkNum].txSublinkStatus == LW2080_CTRL_LWLINK_STATUS_SUBLINK_TX_STATE_SINGLE_LANE))
           )  )
        {
            // Fail the test
            Printf(Tee::PriHigh, "GPU%d: Link%d: Link state not ACTIVE ! Failing test.\n",
                                  index2, linkNum);
            return false;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    /******** All links and sublinks are in ACTIVE and HS/SINGLE-LANE. Proceed to verify the counters **********/

    //
    // To get the right DBDF numbers for these GPUs, we need to actually find the indices
    // of a link they share, which we can do by checking for a high bit in the bitmask
    //
    FOR_EACH_INDEX_IN_MASK(32, linkNum, topology[getTopologyIndex(index1, index2)].linkMask)
    {
        if (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &topology[getTopologyIndex(index1, index2)].linkStats[linkNum].capsTbl),
                                        LW2080_CTRL_LWLINK_CAPS_P2P_SUPPORTED))
        {
            remoteLinkId = topology[getTopologyIndex(index1, index2)].linkStats[linkNum].remoteDeviceLinkNumber;
        }

        linkId = linkNum;
    }
    FOR_EACH_INDEX_IN_MASK_END;

    if (linkId == 0xFF)
    {
        Printf(Tee::PriLow, "No valid links to verify, failing test.\n");
        return false;
    }

    // Use the indices we found
    setDBDFString(dbdf1,
                  topology[getTopologyIndex(index1, index2)].linkStats[linkId].localDeviceInfo.domain,
                  topology[getTopologyIndex(index1, index2)].linkStats[linkId].localDeviceInfo.bus,
                  topology[getTopologyIndex(index1, index2)].linkStats[linkId].localDeviceInfo.device);

    setDBDFString(dbdf2,
                  topology[getTopologyIndex(index2, index1)].linkStats[remoteLinkId].localDeviceInfo.domain,
                  topology[getTopologyIndex(index2, index1)].linkStats[remoteLinkId].localDeviceInfo.bus,
                  topology[getTopologyIndex(index2, index1)].linkStats[remoteLinkId].localDeviceInfo.device);

    if (topology[getTopologyIndex(index1, index2)].linkMask != 0)
    {
        if (LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *)&desiredCaps), LW2080_CTRL_LWLINK_CAPS_SYSMEM_ACCESS))
        {
            Printf(Tee::PriLow, "Expecting traffic on the following SYSMEM links on %s:\n",
                   dbdf1);
        }
        else
        {
            Printf(Tee::PriLow, "Expecting traffic on the following links between %s and %s:\n",
                   dbdf1, dbdf2);
        }
    }

    if (!matchTraffic)
    {
        Printf(Tee::PriLow, "Assuming quantity of traffic on the remote end of these links matches"
               " that of the local end.\n");
    }

    FOR_EACH_INDEX_IN_MASK(32, linkNum, topology[getTopologyIndex(index1, index2)].linkMask)
    {
        // Check to see if this link has the specified capabilities
        if ((topology[getTopologyIndex(index1, index2)].linkStats[linkNum].capsTbl & desiredCaps)
            == desiredCaps)
        {
            bool passedHere;

            // Fail if we saw literally no traffic, but pass on send OR receive traffic
            passedHere = ((topology[getTopologyIndex(index1, index2)].bytesSent[linkNum]     != 0)    ||
                          (topology[getTopologyIndex(index1, index2)].bytesReceived[linkNum] != 0)    ||
                          (topology[getTopologyIndex(index1, index2)].bytesReceivedOverflow[linkNum]) ||
                          (topology[getTopologyIndex(index1, index2)].bytesSentOverflow[linkNum]));

            ret = ret && passedHere;

            Printf(Tee::PriLow, "%s sent %lld bytes on link %d\n",
                dbdf1, topology[getTopologyIndex(index1, index2)].bytesSent[linkNum], linkNum);

            Printf(Tee::PriLow, "%s received %lld bytes on link %d\n",
                dbdf1, topology[getTopologyIndex(index1, index2)].bytesReceived[linkNum], linkNum);

            if (!passedHere)
            {
                Printf(Tee::PriHigh, "FAILURE: Traffic expected on (%s)'s end of"
                       " link %d but no send or receive traffic observed.\n", dbdf1, linkNum);
            }
            else if (matchTraffic)
            {
                //
                // Set up overflow reporting strings; If and only if overflow differs,
                // report overflows in the failure printout
                //
                char *ovf  = " with overflow";
                char *novf = " with no overflow";

                unsigned int remoteLinkNum = topology[getTopologyIndex(index1, index2)].linkStats[linkNum].remoteDeviceLinkNumber;

                // Compare locally sent traffic and remotely received traffic
                if ((topology[getTopologyIndex(index1, index2)].bytesSent[linkNum] !=
                     topology[getTopologyIndex(index2, index1)].bytesReceived[remoteLinkNum]) ||
                    (topology[getTopologyIndex(index1, index2)].bytesSentOverflow[linkNum] !=
                     topology[getTopologyIndex(index2, index1)].bytesReceivedOverflow[remoteLinkNum]))
                {
                    bool overflowMismatch = (topology[getTopologyIndex(index1, index2)].bytesSentOverflow[linkNum] !=
                                             topology[getTopologyIndex(index2, index1)].bytesReceivedOverflow[remoteLinkNum]);

                    // The first GPU sent a different amount of traffic on this link than the second received, fail the test
                    Printf(Tee::PriHigh, "FAILURE: Traffic mismatch on (%s)'s link %d- %s sent %lld bytes of traffic %s"
                           " , but %s received %lld bytes%s!\n",
                           dbdf1,
                           linkNum,
                           dbdf1,
                           topology[getTopologyIndex(index1, index2)].bytesSent[linkNum],
                           overflowMismatch ? (topology[getTopologyIndex(index1, index2)].bytesSentOverflow[linkNum] ? (ovf) : (novf)) : (""),
                           dbdf2,
                           topology[getTopologyIndex(index2, index1)].bytesReceived[remoteLinkNum],
                           overflowMismatch ? (topology[getTopologyIndex(index2, index1)].bytesReceivedOverflow[remoteLinkNum] ? (ovf) : (novf)) : (""));

                    ret = false;
                }

                // Compare locally received traffic and remotely sent traffic
                if((topology[getTopologyIndex(index1, index2)].bytesReceived[linkNum] !=
                    topology[getTopologyIndex(index2, index1)].bytesSent[remoteLinkNum]) ||
                   (topology[getTopologyIndex(index1, index2)].bytesReceivedOverflow[linkNum] !=
                    topology[getTopologyIndex(index2, index1)].bytesSentOverflow[remoteLinkNum]))
                {
                    bool overflowMismatch = (topology[getTopologyIndex(index1, index2)].bytesReceivedOverflow[linkNum] !=
                                             topology[getTopologyIndex(index2, index1)].bytesSentOverflow[remoteLinkNum]);

                    // The first GPU received a different amount of traffic on this link than the second sent, fail the test
                    Printf(Tee::PriHigh, "FAILURE: Traffic mismatch on (%s)'s link %d - %s received %lld bytes of traffic%s"
                           " , but %s sent %lld bytes%s!\n",
                           dbdf1,
                           linkNum,
                           dbdf1,
                           topology[getTopologyIndex(index1, index2)].bytesReceived[linkNum],
                           overflowMismatch ? (topology[getTopologyIndex(index1, index2)].bytesReceivedOverflow[linkNum] ? (ovf) : (novf)) : (""),
                           dbdf2,
                           topology[getTopologyIndex(index2, index1)].bytesSent[remoteLinkNum],
                           overflowMismatch ? (topology[getTopologyIndex(index2, index1)].bytesSentOverflow[remoteLinkNum] ? (ovf) : (novf)) : (""));

                    ret = false;
                }
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    if (index1 != index2)
    {
        FOR_EACH_INDEX_IN_MASK(32, linkNum, topology[getTopologyIndex(index2, index1)].linkMask)
        {
            if ((topology[getTopologyIndex(index2, index1)].linkStats[linkNum].capsTbl & desiredCaps)
               == desiredCaps)
            {
                bool passedHere;
                passedHere = ((topology[getTopologyIndex(index2, index1)].bytesSent[linkNum] != 0) ||
                              (topology[getTopologyIndex(index2, index1)].bytesReceived[linkNum] != 0) ||
                              (topology[getTopologyIndex(index2, index1)].bytesReceivedOverflow[linkNum]) ||
                              (topology[getTopologyIndex(index2, index1)].bytesSentOverflow[linkNum]));

                ret = ret && passedHere;

                Printf(Tee::PriLow, "%s sent %lld bytes on link %d\n",
                       dbdf2,
                       topology[getTopologyIndex(index2, index1)].bytesSent[linkNum], linkNum);

                Printf(Tee::PriLow, "%s received %lld bytes on link %d\n",
                       dbdf2,
                       topology[getTopologyIndex(index2, index1)].bytesReceived[linkNum], linkNum);

                if (!passedHere)
                {
                    Printf(Tee::PriHigh, "FAILURE: Traffic expected on (%s)'s end of"
                           " link %d but no send or receive traffic observed.\n", dbdf2, linkNum);
                }
            }
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }

    return ret;
}

//!
//! Queries RM for the contents of the lwlink counters of all GpuSubdevices
//! in the topology, then logs them in the topology structure.
//!
//! This function assumes that bCountersRecorded is false when it is called.
//!
//! Counter data is stored into an unsigned long long.
//! If there is any overflow in either of the counters in the pair, an overflow
//! bool in the topology structure corresponding to the link is set to true.
//!
//!-----------------------------------------------------------------------------
void LwlinkVerify::recordCounters()
{
    LwRmPtr pLwRm;

    if ((topology == NULL) || (subdevList == NULL) ||
        (!bCountersAllocated) || (!bCounterApiSupported))
    {
        bCountersRecorded = true;
        return;
    }

    unsigned int i, j;
    unsigned int linkId;
    unsigned int counterType;

    // Construct counter type mask
    LwU32 counterTypeMask = 0;
    LwU32 transmitMask    = 0;
    LwU32 receiveMask     = 0;

    transmitMask |= DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_TX0);
    receiveMask  |= DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_RX0);

    counterTypeMask = transmitMask | receiveMask;

    for (i = 0; i < m_NumDevices; i++)
    {
        for (j = 0; j < m_NumDevices; j++)
        {
            // Iterate over each ordered pair of subdevices
            LW90CC_CTRL_LWLINK_GET_COUNTERS_PARAMS counterVals;
            memset(&counterVals, 0, sizeof(counterVals));

            counterVals.linkMask = topology[getTopologyIndex(i, j)].linkMask;
            FOR_EACH_INDEX_IN_MASK(32, linkId, counterVals.linkMask)
            {
                counterVals.counterData[linkId].counterMask = counterTypeMask;
            }
            FOR_EACH_INDEX_IN_MASK_END;

            LW_STATUS status = LwRmControl(pLwRm->GetClientHandle(),
                                           handleList[i],
                                           LW90CC_CTRL_CMD_LWLINK_GET_COUNTERS,
                                           &counterVals,
                                           sizeof(counterVals));
            if (status != LW_OK)
            {
                Printf(Tee::PriHigh, "FAILURE: Couldn't get counter information!\n");
                MASSERT(true);
            }

            FOR_EACH_INDEX_IN_MASK(32, linkId, topology[getTopologyIndex(i, j)].linkMask)
            {
                unsigned long long txBytes = 0;
                unsigned long long rxBytes = 0;
                bool txOverflow = false;
                bool rxOverflow = false;

                FOR_EACH_INDEX_IN_MASK(32, counterType, counterTypeMask)
                {
                    if (((counterVals.counterData[linkId].overflowMask & BIT(counterType)) != 0) ||
                        (counterVals.counterData[linkId].counters[counterType] != 0))
                    {
                        //
                        // Because of the counter overflow possibility, the reporting is not guaranteed to deliver exact
                        // number of bytes ilwolved, but it is guaranteed to distinguish between traffic and no traffic.
                        //
                        if ((1 << counterType) & transmitMask)
                        {
                            txBytes += counterVals.counterData[linkId].counters[counterType];
                            if (counterVals.counterData[linkId].overflowMask & BIT(counterType))
                            {
                                txOverflow = true;
                            }
                        }

                        if ((1 << counterType) & receiveMask)
                        {
                            rxBytes += counterVals.counterData[linkId].counters[counterType];
                            if (counterVals.counterData[linkId].overflowMask & BIT(counterType))
                            {
                                rxOverflow = true;
                            }
                        }
                    }
                }
                FOR_EACH_INDEX_IN_MASK_END;

                // Check throughput, log counter information in topology struct
                topology[getTopologyIndex(i, j)].bytesSent[linkId] = txBytes;
                topology[getTopologyIndex(i, j)].bytesReceived[linkId] = rxBytes;
                topology[getTopologyIndex(i, j)].bytesSentOverflow[linkId] = txOverflow;
                topology[getTopologyIndex(i, j)].bytesReceivedOverflow[linkId] = rxOverflow;
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }
    }

    bCountersRecorded = true;
    return;
}

//!
//! Acquire and ready the lwlink counters of the parameter GpuSubdevice.
//!
//! If there have been any counter API failures in this test so far,
//! this function will not attempt to allocate counters.
//!-----------------------------------------------------------------------------
void LwlinkVerify::setupLwlinkCounters(LwU32 index)
{
    LwRmPtr   pLwRm;
    LW_STATUS status = LW_OK;

    if ((m_NumDevices == 0) || (!bCounterApiSupported))
    {
        return;
    }

    // Acquire ownership of the first GPU's counters
    status = LwRmControl(pLwRm->GetClientHandle(),
                         handleList[index],
                         LW90CC_CTRL_CMD_LWLINK_RESERVE_COUNTERS,
                         NULL,
                         0);
    if (status != LW_OK)
    {
        Printf(Tee::PriLow, "Warning: Throughput counters not supported for"
                            " this run; cannot verify LWLink traffic.\n");
        bCounterApiSupported = false;
        subdevList[index]    = NULL; // we don't want to release this subdevice's counters
                                     // because we couldn't successfully allocate them
        releaseLwlinkCounters();
    }
    else
    {
        bCountersAllocated = true;
        readyLwlinkCounters(index);
    }

    return;
}

//!
//! Release lwlink counters that this object has reserved.
//!-----------------------------------------------------------------------------
void LwlinkVerify::releaseLwlinkCounters()
{
    LwRmPtr pLwRm;
    unsigned int i;

    if ((!bCountersAllocated) || (handleList == NULL) || (subdevList == NULL))
    {
        return;
    }

    for (i = 0; i < m_NumDevices; i++)
    {
        if ((subdevList[i] != NULL) && (handleList[i] != 0))
        {
            LW_STATUS status = LwRmControl(pLwRm->GetClientHandle(),
                                           handleList[i],
                                           LW90CC_CTRL_CMD_LWLINK_RELEASE_COUNTERS,
                                           NULL,
                                           0);
            if (status != LW_OK)
            {
                Printf(Tee::PriLow, "FAILURE: Unable to release lwlink counters!\n");
                MASSERT(true);
            }
        }
    }

    bCountersAllocated = false;

    return;
}

//!
//! Clear and reconfigure lwlink counters of the parameter GpuSubdevice.
//!-----------------------------------------------------------------------------
void LwlinkVerify::readyLwlinkCounters(LwU32 index)
{
    unsigned int i;
    LW_STATUS status = LW_OK;
    LwRmPtr   pLwRm;

    if (bCounterApiSupported)
    {
        LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS linkParams;
        memset(&linkParams, 0, sizeof(linkParams));

        LwRmControl(pLwRm->GetClientHandle(),
                    pLwRm->GetSubdeviceHandle(subdevList[index]),
                    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                    &linkParams,
                    sizeof(linkParams));

        LwU32 linkmask = linkParams.enabledLinkMask;

        // clear the counters
        LwU32 countermask = 0;
        countermask = (DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_TX0)  |
                       DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_TX1)) |
                      (DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_RX0)  |
                       DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_RX1));

        LW90CC_CTRL_LWLINK_CLEAR_COUNTERS_PARAMS clearParams;
        memset(&clearParams, 0, sizeof(clearParams));

        clearParams.linkMask = linkmask;

        FOR_EACH_INDEX_IN_MASK(32, i, linkmask)
        {
            clearParams.counterMask[i] = countermask;
        }
        FOR_EACH_INDEX_IN_MASK_END;

        status = LwRmControl(pLwRm->GetClientHandle(),
                             handleList[index],
                             LW90CC_CTRL_CMD_LWLINK_CLEAR_COUNTERS,
                             &clearParams,
                             sizeof(clearParams));

        if (status != LW_OK)
        {
            bCounterApiSupported = false;
        }

        //
        // Set up counterCfgMask for the SET_TL_COUNTER_CFG call coming up so we get
        // the desired counter behavior
        //
        LW90CC_CTRL_LWLINK_SET_TL_COUNTER_CFG_PARAMS counterConfigs;
        memset(&counterConfigs, 0, sizeof(counterConfigs));

        LwU32 counterCfgMask = 0;
        counterCfgMask = FLD_SET_DRF(90CC, _CTRL_LWLINK_COUNTER_TL_CFG,
                                     _UNIT, _BYTES, counterCfgMask); //count bytes

        // The following fields are outdated for volta but needed for pascal functionality
        counterCfgMask |= DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_CFG_PKTFILTER);
        counterCfgMask |= DRF_SHIFTMASK(LW90CC_CTRL_LWLINK_COUNTER_TL_CFG_ATTRFILTER);

        counterCfgMask = FLD_SET_DRF(90CC, _CTRL_LWLINK_COUNTER_TL_CFG,
                                     _PMSIZE, _1, counterCfgMask);

        // Apply the counterCfgMask to relevant links in the parameter struct
        counterConfigs.linkMask = linkmask;
        FOR_EACH_INDEX_IN_MASK(32, i, linkmask)
        {
            counterConfigs.linkCfg[i].tx0Cfg = counterCfgMask;
            counterConfigs.linkCfg[i].rx0Cfg = counterCfgMask;
            counterConfigs.linkCfg[i].tx1Cfg = counterCfgMask;
            counterConfigs.linkCfg[i].rx1Cfg = counterCfgMask;
        }
        FOR_EACH_INDEX_IN_MASK_END;

        // RM call to apply our specified counter configuration to the first GPU
        status = LwRmControl(pLwRm->GetClientHandle(),
                             handleList[index],
                             LW90CC_CTRL_CMD_LWLINK_SET_TL_COUNTER_CFG,
                             &counterConfigs,
                             sizeof(counterConfigs));
        if (status != LW_OK)
        {
            bCounterApiSupported = false;
        }

        // Unfreeze counters
        LW90CC_CTRL_LWLINK_SET_COUNTERS_FROZEN_PARAMS freezeParams;
        memset(&freezeParams, 0, sizeof(freezeParams));

        freezeParams.linkMask = linkmask;

        FOR_EACH_INDEX_IN_MASK(32, i, linkmask)
        {
            freezeParams.counterMask[i] = 0xFFFFFFFF;
        }
        FOR_EACH_INDEX_IN_MASK_END;

        freezeParams.bFrozen = LW_FALSE;
        status = LwRmControl(pLwRm->GetClientHandle(),
                             handleList[index],
                             LW90CC_CTRL_CMD_LWLINK_SET_COUNTERS_FROZEN,
                             &freezeParams,
                             sizeof(freezeParams));
        if (status != LW_OK)
        {
            bCounterApiSupported = false;
            Printf(Tee::PriLow, "Failed to unfreeze counters!\n");
        }

        // Did anything fail?
        if (!bCounterApiSupported)
        {
            //print a warning, release the counter(s)
            Printf(Tee::PriLow, "Warning: Throughput counters not supported for this"
                                " run; cannot verify LWLink traffic.\n");

            releaseLwlinkCounters();
        }
    }

    return;
}

//!
//! Clear all lwlink counters so that they can measure a new transaction
//! Returns true if the counter API is supported by the current test and
//! there are already counters allocated, false if not.
//!-----------------------------------------------------------------------------
bool LwlinkVerify::resetLwlinkCounters()
{
    unsigned int i;

    if( (subdevList == NULL)    || (handleList == NULL) ||
        (!bCounterApiSupported) || (!bCountersAllocated))
    {
        return false;
    }

    bCountersRecorded = false;
    for (i = 0; i < m_NumDevices; i++)
    {
        if (subdevList[i] != NULL)
        {
            readyLwlinkCounters(i);
        }
    }

    return bCounterApiSupported;
}

//!
//! Injects an lwlink error onto the specified GpuSubdevice. Whether the error
//! is fatal is determined by the parameter 'bFatal'.
//! Injects errors on all links in the parameter linkMask.
//!-----------------------------------------------------------------------------
void LwlinkVerify::injectLwlinkErrorOnLinks
(
    GpuSubdevice *pSubdev,
    LwBool        bFatal,
    LwU32         linkMask
)
{
    LwRmPtr pLwRm;
    LW2080_CTRL_LWLINK_INJECT_ERROR_PARAMS eParams;

    memset(&eParams, 0, sizeof(eParams));

    eParams.linkMask    = linkMask;
    eParams.bFatalError = bFatal;

    LwRmControl(pLwRm->GetClientHandle(),
                pLwRm->GetSubdeviceHandle(pSubdev),
                LW2080_CTRL_CMD_LWLINK_INJECT_ERROR,
                &eParams,
                sizeof(eParams));

    Tasker::Sleep(2000); //Allow some time for the effects of the error injection to manifest
}

//!
//! Injects an lwlink error onto the specified GpuSubdevice. Whether the error
//! is fatal is determined by the parameter 'bFatal'. Injects an error on all links.
//!-----------------------------------------------------------------------------
void LwlinkVerify::injectLwlinkError
(
    GpuSubdevice *pSubdev,
    LwBool        bFatal
)
{
    LwRmPtr pLwRm;
    LW2080_CTRL_LWLINK_INJECT_ERROR_PARAMS eParams;

    memset(&eParams, 0, sizeof(eParams));

    eParams.linkMask    = 0xFFFFFFFF;
    eParams.bFatalError = bFatal;

    LwRmControl(pLwRm->GetClientHandle(),
                pLwRm->GetSubdeviceHandle(pSubdev),
                LW2080_CTRL_CMD_LWLINK_INJECT_ERROR,
                &eParams,
                sizeof(eParams));

    Tasker::Sleep(2000); //Allow some time for the effects of the error injection to manifest
}

//!
//! Sets the dbdf string to match the parameters. Function is always 0.
//!-----------------------------------------------------------------------------
void LwlinkVerify::setDBDFString
(
    char         *dbdf,
    unsigned int  domain,
    unsigned int  bus,
    unsigned int  device
)
{
    memset(dbdf, 0, DBDF_STRING_SIZE);
    sprintf(dbdf, "%04x:%02x:%02x.%02x", domain, bus, device, 0);
}

//!
//! Cleans up allocated resources.
//!-----------------------------------------------------------------------------
void LwlinkVerify::Cleanup()
{
    LwU32 i;
    LwRmPtr pLwRm;

    releaseLwlinkCounters();
    if (topology != NULL)
    {
        free(topology);
        topology = NULL;
    }

    if (handleList != NULL)
    {
        for (i = 0; (i < m_NumDevices); i++)
        {
            if (handleList[i] != 0)
            {
                pLwRm->Free(handleList[i]);
                handleList[i] = 0;
            }
        }
        free(handleList);
        handleList = NULL;
    }

    if (subdevList != NULL)
    {
        free(subdevList);
        subdevList = NULL;
    }

    return;
}

//!
//! Returns true if the device supports the given LWLink power state.
//!
//!-----------------------------------------------------------------------------
bool LwlinkVerify::isPowerStateSupported(GpuSubdevice *pSubdev, unsigned int powerState)
{
    LwRmPtr   pLwRm;
    LW_STATUS status;
    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS_PARAMS capsParams;

    memset(&capsParams, 0, sizeof(capsParams));

    status = LwRmControl(pLwRm->GetClientHandle(),
                         pLwRm->GetSubdeviceHandle(pSubdev),
                         LW2080_CTRL_CMD_LWLINK_GET_LWLINK_CAPS,
                         &capsParams,
                         sizeof(capsParams));

    // If LWLink is not supported, return false
    if ((status != LW_OK) ||
         !LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &capsParams.capsTbl),
                                    LW2080_CTRL_LWLINK_CAPS_SUPPORTED))
    {
        return false;
    }

    // Check if the given power state is supported
    switch (powerState)
    {
        case LW2080_CTRL_LWLINK_POWER_STATE_L0:
        {
            if (!LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &capsParams.capsTbl),
                    LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L0))
                return false;
            break;
        }
        case LW2080_CTRL_LWLINK_POWER_STATE_L1:
        {
            if (!LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &capsParams.capsTbl),
                    LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L1))
                return false;
            break;
        }
        case LW2080_CTRL_LWLINK_POWER_STATE_L2:
        {
            if (!LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &capsParams.capsTbl),
                    LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L2))
                return false;
            break;
        }
        case LW2080_CTRL_LWLINK_POWER_STATE_L3:
        {
            if (!LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &capsParams.capsTbl),
                    LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L3))
                return false;
            break;
        }
        default:
        {
            Printf(Tee::PriHigh, "Unsupported power state!\n");
            return false;
        }
    }

    return true;
}

//!
//! Returns true if the links on the device have the power state enabled.
//!
//!-----------------------------------------------------------------------------
bool LwlinkVerify::isPowerStateEnabled(GpuSubdevice *pSubdev, unsigned int powerState)
{
    LwRmPtr   pLwRm;
    LW_STATUS status;
    LwU32     i;
    LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS_PARAMS params;

    memset(&params, 0, sizeof(params));

    status = LwRmControl(pLwRm->GetClientHandle(),
                         pLwRm->GetSubdeviceHandle(pSubdev),
                         LW2080_CTRL_CMD_LWLINK_GET_LWLINK_STATUS,
                         &params,
                         sizeof(params));

    if (status != LW_OK)
    {
        return false;
    }

    // All the links of the subdevice should have the link state enabled
    FOR_EACH_INDEX_IN_MASK(32, i, params.enabledLinkMask)
    {
        // Check if the given power state is supported
        switch (powerState)
        {
            case LW2080_CTRL_LWLINK_POWER_STATE_L0:
            {
                if (!LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &params.linkInfo[i].capsTbl),
                        LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L0))
                    return false;
                break;
            }
            case LW2080_CTRL_LWLINK_POWER_STATE_L1:
            {
                if (!LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &params.linkInfo[i].capsTbl),
                        LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L1))
                    return false;
                break;
            }
            case LW2080_CTRL_LWLINK_POWER_STATE_L2:
            {
                if (!LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &params.linkInfo[i].capsTbl),
                        LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L2))
                    return false;
                break;
            }
            case LW2080_CTRL_LWLINK_POWER_STATE_L3:
            {
                if (!LW2080_CTRL_LWLINK_GET_CAP(((LwU8 *) &params.linkInfo[i].capsTbl),
                        LW2080_CTRL_LWLINK_CAPS_POWER_STATE_L3))
                    return false;
                break;
            }
            default:
            {
                Printf(Tee::PriHigh, "Unsupported power state!\n");
                return false;
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    return true;
}
