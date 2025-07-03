/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef _LWLINK_VERIFY_H_
#define _LWLINK_VERIFY_H_

// This is the max number of bits in a link mask
#define MAX_LWLINK_LINKS 32

struct LwlinkTopologyInfo
{
    // #links and pairs
    unsigned int linkMask;
    unsigned int numPairLinks;

    // Per link status information
    LW2080_CTRL_LWLINK_LINK_STATUS_INFO linkStats[MAX_LWLINK_LINKS];

    // Traffic information (#bytes sent/received)
    unsigned long long bytesSent[MAX_LWLINK_LINKS];
    unsigned long long bytesReceived[MAX_LWLINK_LINKS];
    bool               bytesSentOverflow[MAX_LWLINK_LINKS];
    bool               bytesReceivedOverflow[MAX_LWLINK_LINKS];
};

class LwlinkVerify
{

public:

    // Constructors/destructors
    LwlinkVerify();
    ~LwlinkVerify();
    void Cleanup();

    // Test Setup and support
    bool IsSupported(GpuSubdevice *pSubdev);
    void Setup(GpuDevMgr *mgr, UINT32 numDevices);

    // Functions to verify all types of LWLink traffic
    bool verifySysmemTraffic(GpuSubdevice *pSubdev);
    bool verifyP2PTraffic(GpuSubdevice *pSubdev1, GpuSubdevice *pSubdev2);
    bool verifyRelevantTraffic(GpuSubdevice *pSubdev1, GpuSubdevice *pSubdev2,
                               LwU32 desiredCaps, bool matchTraffic);

    // LWLink Topology functions
    void printTopologyInfo();
    unsigned int getTotalLinksFound();

    // LWLink Power Feature Functions
    bool isPowerStateSupported(GpuSubdevice *pSubdev, unsigned int powerState);
    bool isPowerStateEnabled(GpuSubdevice *pSubdev, unsigned int powerState);
    bool verifyPowerStateTransition(unsigned int targetState);

    // Counter controls
    bool resetLwlinkCounters();

    // Error controls
    void injectLwlinkError(GpuSubdevice *pSubdev, LwBool bFatal);
    void injectLwlinkErrorOnLinks(GpuSubdevice *pSubdev, LwBool bFatal, LwU32 linkMask);

private:

    //
    // Device and sub device information
    // Position of a subdevice in subdevList will index into the topology array
    // 0th handle corresponds to the 0th subdevice, and so on
    //
    GpuDevMgr     *m_pGpuDevMgr;
    GpuSubdevice **subdevList;
    LwU32         *handleList;
    UINT32         m_NumDevices;

    // Topology information variables
    struct LwlinkTopologyInfo *topology;
    unsigned int totalLinksFound;

    // Counter control variables
    bool bCounterApiSupported;
    bool bCountersAllocated;
    bool bCountersRecorded;

    // Test Setup and support functions
    void setupLwlinkVerify();

    // Counter control functions
    void setupLwlinkCounters(LwU32 index);
    void readyLwlinkCounters(LwU32 index);
    void releaseLwlinkCounters();
    void recordCounters();

    // Utility functions
    int          getIndexOfSubdev(GpuSubdevice *pSubdev);
    unsigned int getTopologyIndex(unsigned int localSubDev,
                                  unsigned int remoteSubDev);
    void         setDBDFString(char *dbdf, unsigned int domain,
                               unsigned int bus, unsigned int device);
};

#endif
