/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file hdcputils.h
//! \brief hdcp api utils
//!

#ifndef __HDCPUTILS_H__
#define __HDCPUTILS_H__

#ifndef INCLUDED_DISPLAY_H
#include "core/include/display.h"
#endif

class HDCPUtils
{
public :
    RC Init(Display *pDisplay, DisplayID hdcpDisplay);

    RC Init(Display *pDisplay, DisplayID hdcpDisplay, LwU64 *cKeys, LwU64 cKeyChecksum,  LwU64 cKsv, LwU32 sleepInMills);

    enum HDCPJOB
    {
        // DisplayInfo of the current display.
        DISPLAY_INFO = 0,
        QUERY_HEAD_CONFIG,
        RENEGOTIATE_LINK,
        READ_LINK_STATUS,
    };

    RC PerformJob(HDCPJOB hdcpJob);

    RC PrintHDCPInfo(DisplayID hdcpDisplay,
                     void *hdcpCtrlParams,
                     bool hdcpStatusCached);

    RC GetHDCPLinkParams
    (
       const DisplayID  Display,
       const UINT64     Cn,
       const UINT64     CKsv,
       UINT32           *LinkCount,
       UINT32           *NumAps,
       UINT64           *Cs,
       vector <UINT64>  *Status,
       vector <UINT64>  *An,
       vector <UINT64>  *AKsv,
       vector <UINT64>  *BKsv,
       vector <UINT64>  *DKsv,
       vector <UINT64>  *BKsvList,
       vector <UINT64>  *Kp,
       void *pHdcpCtrlParams,
       LwU32 hdcpCommand
     );

    RC GetHDCPInfo
    (
        DisplayID   hdcpDisplay,
        bool        hdcpStatusCached,
        bool&       isHdcp1xEnabled,
        bool&       isHdcp1xCapable,
        bool&       isGpuCapable,
        bool&       isHdcp22Capable,
        bool&       isHdcp22Enabled,
        bool&       isRepeater
    );

    void Reset();

    LwU32 GetSleepAfterRenegotiateTime();

    ~HDCPUtils();

private:

    LwU64        m_lwrrentCksv;
    LwU64        m_lwrrentCkeyChecksum;
    LwU64       *m_pLwrrentCkeys;
    Display     *m_pDisplay;
    DisplayID    m_hdcpDisplay;
    LwU32        m_sleepInMills;
};

#endif // __HDCPUTILS_H__
