/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2011,2013-2014,2016-2021 by LWPU Corporation.  All rights
 * reserved.  All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   edid.h
 *
 * @brief  Abstraction for the EDID
 *
 *
 *
 */

#pragma once

#include "lwtypes.h"
#include "display.h"  // For EDID_SIZE_1 and EDID_SIZE_MAX
#include "rc.h"
#include <set>
#include <vector>

#include "modeext.h"

#include "lwtiming.h"

class Edid
{
public:
    // Public so others can use the set version of this
    struct Resolution
    {
        LwU32 width;
        LwU32 height;
        LwU32 refresh;

        Resolution(LwU32 w, LwU32 h, LwU32 r) : width(w), height(h), refresh(r)
            {};

        void operator= (Resolution const &i)
        {
            width   = i.width;
            height  = i.height;
            refresh = i.refresh;
        };

        bool operator== (Resolution const &i) const
        {
            return((width   == i.width    &&
                    height  == i.height   &&
                    refresh == i.refresh )
                   ? true : false);
        };

        friend bool operator< (Resolution const &i, Resolution const &j)
        {
            if (i.width   < j.width)   return true;
            if (i.width   > j.width)   return false;
            if (i.height  < j.height)  return true;
            if (i.height  > j.height)  return false;
            if (i.refresh < j.refresh) return true;
            return false;
        };
    };

    typedef set<Resolution> ListedRes;
    typedef set<Resolution>::iterator ListedResIterator;

    Edid(Display *pDisplay);
    virtual ~Edid();

    void Ilwalidate(UINT32 display);

    RC GetDdNativeRes(
                     UINT32 display,
                     UINT32 *pWidth,
                     UINT32 *pHeight,
                     UINT32 *pRefresh
                    );

    RC GetMonitorMaxRes(
                        UINT32 display,
                        UINT32 *pWidth,
                        UINT32 *pHeight,
                        UINT32 *pRefresh
                       );

    // Get the "RAW" edid shoved straight into memory
    RC GetRawEdid(
                  UINT32 display,
                  UINT08 * edid,
                  UINT32 * pEdidSize
             );

    // Get the Edid for this display, pre-parsed so that modeset.nxt is happy
    RC GetParsedEdid(
                   UINT32 display,
                   PARSED_EDID *pParsedEdid
                   );
    RC GetEdidMisc(
                   UINT32 display,
                   EDID_MISC *pEdidMisc
                   );
    RC GetCea861Info(
                   UINT32 display,
                   LWT_EDID_CEA861_INFO *pCea861Info
                   );

    bool IsListedResolution(UINT32 display, UINT32 width,
                            UINT32 height,  UINT32 refresh);

    RC GetNumListedResolutions(
                   UINT32 display,
                   UINT32 *pNumListed
                   );

    //! You must pass arrays of the correct length to this function
    RC GetListedResolutions(
                              UINT32   display,
                              UINT32 * widthsArr,
                              UINT32 * heightsArr,
                              UINT32 * refreshArr
                             );

    RC GetListedResolutions(
                              UINT32   display,
                              UINT32 * widthsArr,
                              UINT32 * heightsArr,
                              UINT32 * refreshArr,
                              UINT32 * bpp
                             );

    RC GetProductName(
                         UINT32 display,
                         UINT32 nameLen,
                         UINT08 *pName
                     );

    const Edid::ListedRes * GetListedResolutionsList(UINT32 display);

    RC SupportsResolution(
                          UINT32 display,
                          UINT32 width,
                          UINT32 height,
                          UINT32 refresh,
                          bool *pNativeAllowed
                         );

    RC SupportsHDMI(UINT32 display, bool *pSupportsHDMI);
    RC SupportsHDMIFrl(UINT32 display, bool *pSupportsHdmiFrl);

    RC PrintEdid(UINT32 display, UINT32 edidNum = 1);
    RC PrintRawEdid(UINT32 display, UINT32 edidNum = 1);
    RC PrintListedResolutions(UINT32 display);

    enum TimingType
    {
        TT_DETAILED = LWT_TYPE_EDID_DTD,
        TT_STANDARD = LWT_TYPE_EDID_STD,
        TT_ESTABLISHED = LWT_TYPE_EDID_EST,
        TT_CVT = LWT_TYPE_EDID_CVT,
        TT_861ST = LWT_TYPE_EDID_861ST,
        TT_DETAILED_EXT = LWT_TYPE_EDID_EXT_DTD,
        TT_HDMI_EXT = LWT_TYPE_HDMI_EXT,

        // MODS Timing shortlwts starting at a high number to differentiate
        // them from the lwTiming enums
        TT_MODS_START = 0x10000,
        TT_861ALL = TT_MODS_START,
        TT_ALL
    };

    UINT32 GetEdidTimingCount() const;
    RC GetEdidTiming(UINT32 Index, LWT_TIMING *pTiming) const;
    RC GetEdidTimings(TimingType Type, vector<LWT_TIMING> *pTimings) const;
    static LWT_TIMING_TYPE GetEdidTimingType(LWT_TIMING *pTiming);
    const Edid::ListedRes * GetEdidListedResolutionsList(UINT32 display);
    RC GetLwtEdidInfo(UINT32 display, LWT_EDID_INFO *pLwtEdidInfo);
    RC GetLwtEdidInfo(UINT32 display, bool skipDetected, LWT_EDID_INFO *pLwtEdidInfo);

    static const UINT08 *GetDefaultLVDSEdid(UINT32 *Size);
    static const UINT08 *GetDefaultEdid(UINT32 *Size);

    static const UINT32 DEFAULT_NATIVE_WIDTH   = 1280;
    static const UINT32 DEFAULT_NATIVE_HEIGHT  = 1024;
    static const UINT32 DEFAULT_NATIVE_REFRESH = 85;

    static const UINT32 DEFAULT_MAX_WIDTH   = 2560;
    static const UINT32 DEFAULT_MAX_HEIGHT  = 1600;
    static const UINT32 DEFAULT_MAX_REFRESH = 60;

private:
    RC ReadEdid(UINT32 display);
    RC ReadEdid(UINT32 display, bool bSkipDetected);
    void SetDisplay(UINT32 display);
    RC PrintEdid1(UINT32 priority, UINT32 display);
    RC PrintRawEdidBytes(UINT32 priority, UINT32 display);
    RC PrintListedResolutions(UINT32 priority, UINT32 display);
    RC PrintEdid1(Tee::PriDebugStub, UINT32 display)
    {
        return PrintEdid1(Tee::PriSecret, display);
    }
    RC PrintListedResolutions(Tee::PriDebugStub, UINT32 display)
    {
        return PrintListedResolutions(Tee::PriSecret, display);
    }
    static void ResInsertHelper(set<Resolution> *pResSet,
                          UINT32 w, UINT32 h, UINT32 r);
    RC CreateEdidTimingDB();

    RC SelectSupportedMaxAndNative();
    vector<UINT08> m_RawEdid;
    LwU32 m_LwrDisplay;
    MODE_INFO m_MaxMode; // The maximum mode _LISTED_ in the EDID
    MODE_INFO m_NativeMode; // The "optimal" mode for a digital display
    set<Resolution> m_ListedRes;
    PARSED_EDID m_ParsedEdid;
    EDID_MISC m_EdidMisc;
    Display *m_pDisplay;
    LwU8 m_Bpc;

    LWT_EDID_INFO *m_pEdidInfo;
    set<Resolution> m_EdidListedRes;
};
