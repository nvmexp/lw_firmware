/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_TEGRA_DISP_TEST_COMN_H
#define INCLUDED_TEGRA_DISP_TEST_COMN_H

#include "gputest.h"
#include "gputestc.h"
#include "gpu/display/tegra_disp.h"
#ifdef TEGRA_MODS
#   include "gpu/display/tegra_disp_lwdc.h"
#endif
#include "cheetah/include/tegrasocdevice.h"

extern SObject TegraDispTestComn_Object;

//! Base class for a cheetah display test
//!
//! Implements common funtionality used in cheetah display tests
class TegraDispTestComn : public GpuTest
{
    public:
        TegraDispTestComn();
        virtual ~TegraDispTestComn() { }
        struct ModeDesc
        {
            UINT32  width  = 0;
            UINT32  height = 0;
            UINT32  depth  = 0;
            UINT32  refreshRate = 0;
            bool    bInterlaced = false;

            // For DSI and DP
            CheetAh::SocDevice::DsiInfo dsiInfo;
            CheetAh::SocDevice::DpInfo  dpInfo;
            vector<UINT08> edid;

            Display::OpeFmt    opeFormat = Display::OpeFmt::RGB;
            Display::OpeYuvBpp opeYuvBpp = Display::OpeYuvBpp::BPP_NONE;
        };
        struct TestMode
        {
            TegraDisplay::ConnectionType connection = TegraDisplay::CT_NUM_TYPES;
            ModeDesc mode;
            TestMode()
            {
                mode.dsiInfo.infoBlock.clear();
                mode.edid.clear();
            }
        };

        void PrintJsProperties(Tee::Priority pri);
        RC   InitFromJs();
        void AddMode
        (
            const TestMode testMode
        );

        SETGET_PROP(UseRealDisplays, bool);
    protected:

        enum
        {
            NON_OR_DISPLAY       = 0x1,  //!< Display ID for testing without an
                                         //!< output resource
        };

        const vector<TestMode> &GetTestModes() { return m_TestModes; }
        class OverrideOrInfo
        {
            public:
                OverrideOrInfo();
                ~OverrideOrInfo();
                RC SetOverride(const CheetAh::SocDevice::DsiInfo& dsiInfo);
                RC SetOverride(const CheetAh::SocDevice::DpInfo& dpInfo);
            private:
                OverrideOrInfo(const OverrideOrInfo&);
                OverrideOrInfo& operator=(const OverrideOrInfo&);

                enum OverrideState
                {
                    OVR_STATE_NONE
                   ,OVR_STATE_DSI
                   ,OVR_STATE_DP
                };
                OverrideState             m_OvrState;
                CheetAh::SocDevice::DsiInfo m_DsiInfo;
                CheetAh::SocDevice::DpInfo  m_DpInfo;
        };

        class OverrideEdid
        {
            public:
                OverrideEdid();
                ~OverrideEdid();
                RC SetOverride
                (
                    TegraDisplay*              pDisplay,
                    UINT32                     display,
                    const vector<UINT08>&      edid
                );
            private:
                OverrideEdid(const OverrideEdid&);
                OverrideEdid& operator=(const OverrideEdid&);

                bool m_bSaved;
                Display* m_pDisplay;
                UINT32 m_Display;
                vector<UINT08> m_Edid;
        };

        class ManualDisplayUpdateMode
        {
            public:
                ManualDisplayUpdateMode(Display *pDisplay);
                ~ManualDisplayUpdateMode();
            private:
                ManualDisplayUpdateMode(const ManualDisplayUpdateMode&);
                ManualDisplayUpdateMode& operator=(const ManualDisplayUpdateMode&);

                Display *m_pDisplay;
                Display::UpdateMode m_SavedUpdateMode;
        };
        RC SetMode
        (
            TegraDisplay *pDisplay,
            UINT32 head,
            UINT32 display,
            const TestMode &testMode,
            OverrideOrInfo *pOverrideOrInfo,
            OverrideEdid * pOverrideEdid,
            bool *pModeSkipped
        );
        RC CheckUnderflows
        (
            TegraDisplay *pDisplay,
            UINT32 head,
            const TegraDisplay::UnderflowCounts& origCounts
        );
        RC GetConnectionType
        (
            TegraDisplay                 *pDisplay,
            UINT32                        display,
            TegraDisplay::ConnectionType *pConType,
            TegraDisplay::DsiId          *pDsiId,
            bool                         *pbSkipped
        );
        RC SetupDisplay();
        RC CleanupDisplay();
        static string StringifyOPE(Display::OpeFmt opeFmt, Display::OpeYuvBpp opeYuvBpp);

    private:
        UINT32                       m_CachedDisplay;
        TegraDisplay::ConnectionType m_CachedConType;
        TegraDisplay::DsiId          m_CachedDsiId;
        bool                         m_CachedSkippedConn;
        bool                         m_UseRealDisplays;

        //! Modes
        vector<TestMode> m_TestModes;
        set<string> m_SkippedConnectionTypes;

        UINT32 m_OrigDisplay;
        bool   m_RealDisplayDisabled;
        UINT32 m_DispWidth;
        UINT32 m_DispHeight;
        UINT32 m_DispDepth;
        UINT32 m_DispRefresh;
};

#endif // INCLUDED_TEGRA_DISP_TEST_COMN_H
