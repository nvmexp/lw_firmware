/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2022 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// PEX ASPM test.  Validate that any PEX ASPM transitions that are capable (based
// on the GPU capability and the downstream port capability) do in fact occur
// when enabled.

#include "core/include/mgrmgr.h"
#include "core/include/pci.h"
#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "device/interface/pcie.h"

#include "gpu/include/dmawrap.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/gputest.h"
#include "gpu/utility/pcie/pexdev.h"

//------------------------------------------------------------------------------
class PexAspm : public GpuTest
{
public:
    PexAspm();
    virtual ~PexAspm() = default;

    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;

    SETGET_PROP(AspmEntryDelayMs,          UINT32);
    SETGET_PROP(SkipAspmStateMask,         UINT32);
    SETGET_PROP(SkipAspmL1SubstateMask,    UINT32);
    SETGET_PROP(SkipL1ClockPmSubstateMask, UINT32);
    SETGET_PROP(SkipDeepL1,                bool);
    SETGET_PROP(ShowTestCounts,            bool);

private:
    RC CheckAspmCounts
    (
        Pci::ASPMState aspmState,
        Pci::PmL1Substate aspmL1Substate,
        bool bEnableDeepL1,
        Pcie::L1ClockPmSubstates aspmL1ClkPmSubstate
    );
    RC DoSomeWork();
    RC InitAspmStatesToTest();
    RC InitAspmChipsetFilter();
    RC InitFromJs() override;
    RC RunAspmTest
    (
        Pci::ASPMState aspmState,
        Pci::PmL1Substate aspmL1Substate,
        bool bEnableDeepL1,
        Pcie::L1ClockPmSubstates aspmL1ClkPmSubstate
    );

    UINT32 m_AspmStatesToTest = 0;
    UINT32 m_AspmL1SSToTest   = 0;
    UINT32 m_L1ClkPmSSToTest  = 0;

    static constexpr UINT32 STATE_VALID_BIT = 0x80000000;
    UINT32 m_TestDevAspmState            = 0;
    UINT32 m_TestDevAspmCyaState         = 0;
    UINT32 m_TestDevAspmL1Substate       = 0;
    UINT32 m_TestDevAspmCyaL1Substate    = 0;
    UINT32 m_TestDevDeepL1               = 0;
    UINT32 m_TestDevClockPm              = 0;
    UINT32 m_TestDevL1ClockPmCyaSubstate = 0;
    UINT32 m_PexDevAspmState             = 0;
    UINT32 m_PexDevAspmL1Substate        = 0;
    bool   m_bTestDevHasDeepL1           = false;
    bool   m_bTestDevHasClockPm          = false;

    DmaWrapper m_DmaWrap;
    Surface2D  m_SrcSurf;
    Surface2D  m_DstSurf;

    struct ChipsetAspmFilter
    {
        UINT16 vendorId;
        vector<UINT16> l0sDeviceIdAspmChipsetFilter;
        vector<UINT16> l1DeviceIdAspmChipsetFilter;
    };
    vector<ChipsetAspmFilter> m_ChipsetAspmFilter;

    // JS properties
    UINT32 m_AspmEntryDelayMs           = 5;
    UINT32 m_SkipAspmStateMask          = 0;
    UINT32 m_SkipAspmL1SubstateMask     = 0;
    UINT32 m_SkipL1ClockPmSubstateMask  = 0;
    bool   m_SkipDeepL1                 = false;
    bool   m_ShowTestCounts             = false;
};

//------------------------------------------------------------------------------
PexAspm::PexAspm()
{
    SetName("PexAspm");
}

bool PexAspm::IsSupported()
{
    if (!GpuTest::IsSupported())
        return false;

    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    MASSERT(pPcie);

    PexDevice * pPexDev = nullptr;
    if ((pPcie->GetUpStreamInfo(&pPexDev) != RC::OK) || (pPexDev == nullptr))
    {
        Printf(Tee::PriLow,
               "%s skipped because PCI downstream port above the test device is not avaiable\n",\
               GetName().c_str());
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
RC PexAspm::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    PexDevice * pPexDev = nullptr;
    UINT32 port;
    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    MASSERT(pPcie);

    CHECK_RC(pPcie->GetUpStreamInfo(&pPexDev, &port));
    MASSERT(pPexDev);
    if (pPexDev == nullptr)
    {
        // If exelwtion reaches this spot then the test was forced on the command line
        // Explicitly fail if this is the case as this test requires access to the
        // downstream port above the test device
        Printf(Tee::PriError,
               "%s : Unable to run without access to the PCI downstream port above the test device\n",
               GetName().c_str());
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    CHECK_RC(InitAspmStatesToTest());

    // Save off the current ASPM states on the test device and downstream port above it
    m_TestDevAspmState    = pPcie->GetAspmState() | STATE_VALID_BIT;
    m_TestDevAspmCyaState = pPcie->GetAspmCyaState() | STATE_VALID_BIT;

    if (pPcie->HasAspmL1Substates())
    {
        CHECK_RC(pPcie->GetAspmL1ssEnabled(&m_TestDevAspmL1Substate));
        m_TestDevAspmL1Substate |= STATE_VALID_BIT;

        CHECK_RC(pPcie->GetAspmCyaL1SubState(&m_TestDevAspmCyaL1Substate));
        m_TestDevAspmCyaL1Substate |= STATE_VALID_BIT;
    }

    CHECK_RC(pPcie->HasAspmDeepL1(&m_bTestDevHasDeepL1));
    if (m_bTestDevHasDeepL1)
    {
        bool bDeepL1Enabled = false;
        CHECK_RC(pPcie->GetAspmDeepL1Enabled(&bDeepL1Enabled));
        m_TestDevDeepL1 = (bDeepL1Enabled ? 1 : 0) | STATE_VALID_BIT;
    }

    CHECK_RC(pPcie->HasL1ClockPm(&m_bTestDevHasClockPm));
    if (m_bTestDevHasClockPm)
    {
        bool bClockPmEnabled = false;
        CHECK_RC(pPcie->IsL1ClockPmEnabled(&bClockPmEnabled));
        m_TestDevClockPm = (bClockPmEnabled ? 1 : 0) | STATE_VALID_BIT;
    }

    if (pPcie->HasL1ClockPmSubstates())
    {
        CHECK_RC(pPcie->GetL1ClockPmCyaSubstate(&m_TestDevL1ClockPmCyaSubstate));
        m_TestDevL1ClockPmCyaSubstate |= STATE_VALID_BIT;
    }

    CHECK_RC(pPexDev->GetDownStreamAspmState(&m_PexDevAspmState, port));
    m_PexDevAspmState |= STATE_VALID_BIT;
    if (pPexDev->HasAspmL1Substates())
    {
        CHECK_RC(pPexDev->GetDownStreamAspmL1SubState(&m_PexDevAspmL1Substate,
                                                      port));
        m_PexDevAspmL1Substate |= STATE_VALID_BIT;
    }

    if (GetBoundTestDevice()->GetType() == Device::TYPE_LWIDIA_GPU)
    {
        // Create surface and DMA wrapper used to generate small amounts of PCI traffic
        // to induce ASPM entries
        GpuTestConfiguration * pTestConfig = GetTestConfiguration();
        CHECK_RC(m_DmaWrap.Initialize(pTestConfig,
                                      pTestConfig->NotifierLocation(),
                                      DmaWrapper::COPY_ENGINE));

        m_SrcSurf.SetWidth(pTestConfig->SurfaceWidth());
        m_SrcSurf.SetHeight(pTestConfig->SurfaceHeight());
        m_SrcSurf.SetColorFormat(ColorUtils::A8R8G8B8);
        m_SrcSurf.SetProtect(Memory::ReadWrite);
        m_SrcSurf.SetLayout(Surface2D::Pitch);
        m_SrcSurf.SetLocation(Memory::Fb);
        CHECK_RC(m_SrcSurf.Alloc(GetBoundGpuDevice()));

        m_DstSurf.SetWidth(pTestConfig->SurfaceWidth());
        m_DstSurf.SetHeight(pTestConfig->SurfaceHeight());
        m_DstSurf.SetColorFormat(ColorUtils::A8R8G8B8);
        m_DstSurf.SetProtect(Memory::ReadWrite);
        m_DstSurf.SetLayout(Surface2D::Pitch);
        m_DstSurf.SetLocation(Memory::Fb);
        CHECK_RC(m_DstSurf.Alloc(GetBoundGpuDevice()));

        m_DmaWrap.SetSurfaces(&m_SrcSurf, &m_DstSurf);
    }

    return rc;
}

//------------------------------------------------------------------------------
RC PexAspm::Run()
{
    bool bStopOnError = GetGoldelwalues()->GetStopOnError();
    StickyRC rc;

    for (UINT32 aspmState = Pci::ASPM_DISABLED; aspmState <= Pci::ASPM_L0S_L1; aspmState++)
    {
        if ((aspmState & m_AspmStatesToTest) != aspmState)
            continue;

        if (aspmState & Pci::ASPM_L1)
        {
            for (UINT32 aspmL1SS = Pci::PM_SUB_DISABLED;
                  aspmL1SS <= Pci::PM_SUB_L11_L12; aspmL1SS++)
            {
                if ((aspmL1SS & m_AspmL1SSToTest) != aspmL1SS)
                    continue;

                // Deep L1 disabled with any L1 substate enabled is not a valid test
                if (aspmL1SS == Pci::PM_SUB_DISABLED)
                {
                    rc = RunAspmTest(static_cast<Pci::ASPMState>(aspmState),
                                     static_cast<Pci::PmL1Substate>(aspmL1SS),
                                     false,
                                     Pcie::L1_CLK_PM_DISABLED);
                    if (bStopOnError)
                    {
                        CHECK_RC(rc);
                    }
                }

                // Deep L1 must be enabled by default at the current PState in order to
                // test it.  Deep L1 is required to be enabled to test any L1 substates
                //
                // Also, actual Deep L1 testing only oclwrs when substates are disabled
                // When substates are enabled, instead of entering Deep L1 the test device enters
                // the configured substates.  SkipDeepL1 should only apply to actual Deep L1
                // testing and not the L1 substates
                if ((m_TestDevDeepL1 & ~STATE_VALID_BIT) &&
                    (!m_SkipDeepL1 || (aspmL1SS != Pci::PM_SUB_DISABLED)))
                {
                    rc = RunAspmTest(static_cast<Pci::ASPMState>(aspmState),
                                     static_cast<Pci::PmL1Substate>(aspmL1SS),
                                     true,
                                     Pcie::L1_CLK_PM_DISABLED);
                    if (bStopOnError)
                    {
                        CHECK_RC(rc);
                    }
                }
            }

            // L1_CLK_PM_DISABLED will always be checked in the above loop, no need to recheck it
            for (UINT32 aspmL1ClkPmSS = Pcie::L1_CLK_PM_PLLPD;
                  aspmL1ClkPmSS <= Pcie::L1_CLK_PM_PLLPD_CPM; aspmL1ClkPmSS++)
            {
                if ((aspmL1ClkPmSS & m_L1ClkPmSSToTest) != aspmL1ClkPmSS)
                    continue;

                rc = RunAspmTest(static_cast<Pci::ASPMState>(aspmState),
                                 Pci::PM_SUB_DISABLED,
                                 false,
                                 static_cast<Pcie::L1ClockPmSubstates>(aspmL1ClkPmSS));
                if (bStopOnError)
                {
                    CHECK_RC(rc);
                }
            }
        }
        else
        {
            rc = RunAspmTest(static_cast<Pci::ASPMState>(aspmState),
                             Pci::PM_SUB_DISABLED,
                             false,
                             Pcie::L1_CLK_PM_DISABLED);
            if (bStopOnError)
            {
                CHECK_RC(rc);
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PexAspm::Cleanup()
{
    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    MASSERT(pPcie);

    StickyRC rc;

    // It is necessary to setup ASPM on the downstream port before the test device
    PexDevice * pPexDev = nullptr;
    UINT32 port;
    rc = pPcie->GetUpStreamInfo(&pPexDev, &port);
    if ((pPexDev != nullptr) && (m_PexDevAspmState & STATE_VALID_BIT))
    {
        rc = pPexDev->SetDownStreamAspmState(m_PexDevAspmState & ~STATE_VALID_BIT,
                                             port,
                                             false);
    }

    // Restore all ASPM states
    if (m_TestDevAspmState & STATE_VALID_BIT)
        rc = pPcie->SetAspmState(m_TestDevAspmState & ~STATE_VALID_BIT);
    if (m_TestDevAspmCyaState & STATE_VALID_BIT)
        rc = pPcie->SetAspmCyaState(m_TestDevAspmCyaState & ~STATE_VALID_BIT);
    if (m_TestDevAspmCyaL1Substate & STATE_VALID_BIT)
        rc = pPcie->SetAspmCyaL1SubState(m_TestDevAspmCyaL1Substate & ~STATE_VALID_BIT);
    if (m_TestDevDeepL1 & STATE_VALID_BIT)
        rc = pPcie->EnableAspmDeepL1((m_TestDevDeepL1 & 0x1) ? true : false);
    if (m_TestDevClockPm & STATE_VALID_BIT)
        rc = pPcie->EnableL1ClockPm((m_TestDevClockPm & 0x1) ? true : false);
    if (m_TestDevL1ClockPmCyaSubstate & STATE_VALID_BIT)
        rc = pPcie->SetL1ClockPmCyaSubstate(m_TestDevL1ClockPmCyaSubstate & ~STATE_VALID_BIT);

    if ((m_TestDevAspmL1Substate & STATE_VALID_BIT) &&
        (m_PexDevAspmL1Substate & STATE_VALID_BIT))
    {
        MASSERT(m_TestDevAspmL1Substate == m_PexDevAspmL1Substate);
        PexDevMgr* pPexDevMgr = (PexDevMgr*)DevMgrMgr::d_PexDevMgr;
        rc = pPexDevMgr->SetAspmL1SS(GetBoundTestDevice()->DevInst(),
                                     0,
                                     m_TestDevAspmL1Substate & ~STATE_VALID_BIT);
    }

    if (GetBoundTestDevice()->GetType() == Device::TYPE_LWIDIA_GPU)
    {
        rc = m_DmaWrap.Cleanup();
        m_DstSurf.Free();
        m_SrcSurf.Free();
    }
    rc = GpuTest::Cleanup();

    return rc;
}

//------------------------------------------------------------------------------
void PexAspm::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tAspmEntryDelayMs           : %u\n", m_AspmEntryDelayMs);
    Printf(pri, "\tSkipAspmStateMask          : 0x%x\n", m_SkipAspmStateMask);
    Printf(pri, "\tSkipAspmL1SubstateMask     : 0x%x\n", m_SkipAspmL1SubstateMask);
    Printf(pri, "\tSkipL1ClockPmSubstateMask  : 0x%x\n", m_SkipL1ClockPmSubstateMask);    
    Printf(pri, "\tSkipDeepL1                 : %s\n", m_SkipDeepL1 ? "true" : "false");
    Printf(pri, "\tShowTestCounts             : %s\n", m_ShowTestCounts ? "true" : "false");

    GpuTest::PrintJsProperties(pri);
}

//------------------------------------------------------------------------------
RC PexAspm::CheckAspmCounts
(
    Pci::ASPMState           aspmState,
    Pci::PmL1Substate        aspmL1Substate,
    bool                     bDeepL1Enable,
    Pcie::L1ClockPmSubstates aspmL1ClkPmSubstate
)
{
    StickyRC rc;
    Pcie* pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    MASSERT(pPcie);

    Pcie::AspmEntryCounters aspmCounts = { };
    CHECK_RC(pPcie->GetAspmEntryCounts(&aspmCounts));

    const string testModeStr =
        Utility::StrPrintf("%s : Testing ASPM %s, ASPM L1 Substate %s, Deep L1 %s, Clock PM %s",
                           GetName().c_str(),
                           Pci::AspmStateToString(aspmState),
                           Pci::AspmL1SubstateToString(aspmL1Substate),
                           bDeepL1Enable ? "Enabled" : "Disabled",
                           Pcie::L1ClockPMSubstateToString(aspmL1ClkPmSubstate));

    Printf(m_ShowTestCounts ? Tee::PriNormal : GetVerbosePrintPri(),
           "%s : %s, ASPM Counters:\n"
           "   L0S Entries                : %u\n"
           "   Upstream L0S Entries       : %u\n"
           "   L1 Entries                 : %u\n"
           "   L1P Entries                : %u\n"
           "   Deep L1 Entries            : %u\n"
           "   L1.1 Entries               : %u\n"
           "   L1.2 Entries               : %u\n"
           "   L1.2 Aborts                : %u\n"
           "   L1SS to Deep L1 Timeouts   : %u\n"
           "   L1 Short Durations         : %u\n"
           "   L1 Clock PM PLL PD Entries : %u\n"
           "   L1 Clock PM CPM Entries    : %u\n",
           GetName().c_str(),
           testModeStr.c_str(),
           aspmCounts.XmitL0SEntry,
           aspmCounts.UpstreamL0SEntry, aspmCounts.L1Entry,
           aspmCounts.L1PEntry, aspmCounts.DeepL1Entry,
           aspmCounts.L1_1_Entry, aspmCounts.L1_2_Entry,
           aspmCounts.L1_2_Abort, aspmCounts.L1SS_DeepL1Timouts,
           aspmCounts.L1_ShortDuration, aspmCounts.L1ClockPmPllPdEntry,
           aspmCounts.L1ClockPmCpmEntry);

    string failureStr = "";
    if (aspmState & Pci::ASPM_L0S)
    {
        if ((aspmCounts.XmitL0SEntry == 0) || (aspmCounts.UpstreamL0SEntry == 0))
        {
            failureStr +=
                Utility::StrPrintf("    Failed to enter L0S, Test Device Entries %u, "
                                   "Upstream Entries %u\n",
                                   aspmCounts.XmitL0SEntry, aspmCounts.UpstreamL0SEntry);
            rc = RC::HW_STATUS_ERROR;
        }
    }
    else
    {
        if ((aspmCounts.XmitL0SEntry != 0) || (aspmCounts.UpstreamL0SEntry != 0))
        {
            failureStr +=
                Utility::StrPrintf("    Unexpected L0S entries with L0S disabled, Test Device Entries %u,"
                                   " Upstream Entries %u\n",
                                   aspmCounts.XmitL0SEntry, aspmCounts.UpstreamL0SEntry);
            rc = RC::HW_STATUS_ERROR;
        }
    }

    if (aspmState & Pci::ASPM_L1)
    {
        if ((aspmCounts.L1Entry == 0) ||
            (GetBoundTestDevice()->HasFeature(Device::GPUSUB_SUPPORTS_ASPM_L1P) &&
             (aspmCounts.L1PEntry == 0)))
        {
            failureStr +=
                Utility::StrPrintf("    Failed to enter L1, L1 Entries %u, L1P Entries %u\n",
                                   aspmCounts.L1Entry, aspmCounts.L1PEntry);
            rc = RC::HW_STATUS_ERROR;
        }

        if (aspmL1Substate == Pci::PM_SUB_DISABLED)
        {
            if (bDeepL1Enable && (aspmCounts.DeepL1Entry == 0))
            {
                failureStr += Utility::StrPrintf("    Failed to enter Deep L1\n");
                rc = RC::HW_STATUS_ERROR;
            }
            else if (!bDeepL1Enable && (aspmCounts.DeepL1Entry != 0))
            {
                failureStr +=
                    Utility::StrPrintf("    Unexpected Deep L1 (%u) entries with Deep L1 "
                                       "disabled\n",
                                       aspmCounts.DeepL1Entry);
                rc = RC::HW_STATUS_ERROR;
            }
        }

        // Ignore any L1.1 counts when L1.2 is enabled
        if (!(aspmL1Substate & Pci::PM_SUB_L12))
        {
            if ((aspmL1Substate & Pci::PM_SUB_L11) && (aspmCounts.L1_1_Entry == 0))
            {
                    failureStr += Utility::StrPrintf("    Failed to enter L1.1 substate\n");
                    rc = RC::HW_STATUS_ERROR;
            }
            else if (!(aspmL1Substate & Pci::PM_SUB_L11) && (aspmCounts.L1_1_Entry != 0))
            {
                failureStr +=
                    Utility::StrPrintf("    Unexpected L1.1 entries (%u) with L1.1 disabled\n",
                                       aspmCounts.L1_1_Entry);
                rc = RC::HW_STATUS_ERROR;
            }
        }

        if ((aspmL1Substate & Pci::PM_SUB_L12) && (aspmCounts.L1_2_Entry == 0))
        {
                failureStr += Utility::StrPrintf("    Failed to enter L1.2 substate\n");
                rc = RC::HW_STATUS_ERROR;
        }
        else if (!(aspmL1Substate & Pci::PM_SUB_L12) && (aspmCounts.L1_2_Entry != 0))
        {
            failureStr +=
                Utility::StrPrintf("    Unexpected L1.2 entries (%u) with L1.2 disabled\n",
                                   aspmCounts.L1_2_Entry);
            rc = RC::HW_STATUS_ERROR;
        }

        if ((aspmL1ClkPmSubstate != Pcie::L1_CLK_PM_DISABLED) && (aspmCounts.L1ClockPmPllPdEntry == 0))
        {
                failureStr += Utility::StrPrintf("    Failed to enter L1 Clock PLL PD substate\n");
                rc = RC::HW_STATUS_ERROR;
        }
        else if ((aspmL1ClkPmSubstate == Pcie::L1_CLK_PM_DISABLED) && (aspmCounts.L1ClockPmPllPdEntry != 0))
        {
            failureStr +=
                Utility::StrPrintf("    Unexpected L1 Clock PLL PD entries (%u) with L1 Clock PLL PD and CPM disabled\n",
                                   aspmCounts.L1ClockPmPllPdEntry);
            rc = RC::HW_STATUS_ERROR;
        }
        
        if ((aspmL1ClkPmSubstate & Pcie::L1_CLK_PM_CPM) && (aspmCounts.L1ClockPmCpmEntry == 0))
        {
                failureStr += Utility::StrPrintf("    Failed to enter L1 Clock CPM substate\n");
                rc = RC::HW_STATUS_ERROR;
        }
        else if (!(aspmL1ClkPmSubstate & Pcie::L1_CLK_PM_CPM) && (aspmCounts.L1ClockPmCpmEntry != 0))
        {
            failureStr +=
                Utility::StrPrintf("    Unexpected L1 Clock CPM entries (%u) with L1 Clock CPM disabled\n",
                                   aspmCounts.L1ClockPmPllPdEntry);
            rc = RC::HW_STATUS_ERROR;
        }
    }
    else
    {
        if ((aspmCounts.L1Entry != 0)     || (aspmCounts.L1PEntry != 0)   ||
            (aspmCounts.DeepL1Entry != 0) || (aspmCounts.L1_1_Entry != 0) ||
            (aspmCounts.L1_2_Entry != 0)  || (aspmCounts.L1ClockPmPllPdEntry != 0) ||
            (aspmCounts.L1ClockPmCpmEntry != 0)) 
        {
            failureStr += Utility::StrPrintf("    Unexpected L1 entries during Disabled or L0S testing:\n"
                                             "        L1 Entries              : %u\n"
                                             "        L1P Entries             : %u\n"
                                             "        Deep L1 Entries         : %u\n"
                                             "        L1.1 Entries            : %u\n"
                                             "        L1.2 Entries            : %u\n"
                                             "        L1 Clock PLL PD Entries : %u\n"
                                             "        L1 Clock CPM Entries    : %u\n",
                                             aspmCounts.L1Entry, aspmCounts.L1PEntry,
                                             aspmCounts.DeepL1Entry, aspmCounts.L1_1_Entry,
                                             aspmCounts.L1_2_Entry, aspmCounts.L1ClockPmPllPdEntry,
                                             aspmCounts.L1ClockPmCpmEntry);
            rc = RC::HW_STATUS_ERROR;
        }
    }
    if (failureStr != "")
        Printf(Tee::PriError, "%s\n%s", testModeStr.c_str(), failureStr.c_str());

    return rc;
}

//------------------------------------------------------------------------------
RC PexAspm::DoSomeWork()
{
    RC rc;
    const UINT32 loops = GetTestConfiguration()->Loops();
    GpuTestConfiguration * pTestConfig = GetTestConfiguration();
    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();

    for (UINT32 lwrLoop = 0; lwrLoop < loops; ++lwrLoop)
    {
        if (GetBoundTestDevice()->GetType() == Device::TYPE_LWIDIA_GPU)
        {
            CHECK_RC(m_DmaWrap.Copy(0, 0, m_SrcSurf.GetPitch(), m_SrcSurf.GetHeight(), 0, 0,
                                    pTestConfig->TimeoutMs(),
                                    GetBoundGpuSubdevice()->GetSubdeviceInst(),
                                    true));
        }
        else
        {
            // Just read back the aspm state to generate some PCIE traffic
            (void)pPcie->GetAspmState();
        }
        Tasker::Sleep(m_AspmEntryDelayMs);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PexAspm::InitAspmStatesToTest()
{
    auto pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    MASSERT(pPcie);

    m_AspmStatesToTest = pPcie->AspmCapability();

    VerbosePrintf("%s : Test Device ASPM Capability %s\n",
                  GetName().c_str(),
                  Pci::AspmStateToString(static_cast<Pci::ASPMState>(m_AspmStatesToTest)));
    RC rc;
    PexDevice * pPexDev = nullptr;
    UINT32 port;
    CHECK_RC(pPcie->GetUpStreamInfo(&pPexDev, &port));
    MASSERT(pPexDev); // This is ensured in IsSupported/Setup

    VerbosePrintf("%s : Upstream device ASPM Capability %s\n",
                  GetName().c_str(),
                  Pci::AspmStateToString(static_cast<Pci::ASPMState>(pPexDev->GetAspmCap())));
    m_AspmStatesToTest &= pPexDev->GetAspmCap();

    // Some test devices have 2 CYAs for ASPM, a protected one and a non-protected one
    // Remove any states that are disabled in the protected CYA from the states to test
    UINT32 aspmProtCyaState = 0;
    CHECK_RC(pPcie->GetAspmCyaStateProtected(&aspmProtCyaState));
    if (aspmProtCyaState != Pci::ASPM_UNKNOWN_STATE)
    {
        VerbosePrintf("%s : Test Device protected ASPM CYA state %s\n",
                      GetName().c_str(),
                      Pci::AspmStateToString(static_cast<Pci::ASPMState>(aspmProtCyaState)));
        m_AspmStatesToTest &= aspmProtCyaState;
    }

    m_AspmStatesToTest &= ~m_SkipAspmStateMask;

    if (pPcie->HasAspmL1Substates())
    {
        CHECK_RC(pPcie->GetAspmL1SSCapability(&m_AspmL1SSToTest));
        VerbosePrintf("%s : Test Device ASPM L1 Substate Capability %s\n",
                     GetName().c_str(),
                     Pci::AspmL1SubstateToString(static_cast<Pci::PmL1Substate>(m_AspmL1SSToTest)));

        UINT32 aspmProtCyaL1Substate = 0;
        CHECK_RC(pPcie->GetAspmCyaL1SubStateProtected(&aspmProtCyaL1Substate));
        if (aspmProtCyaL1Substate != Pci::PM_UNKNOWN_SUB_STATE)
        {
            VerbosePrintf("%s : Test Device protected ASPM L1 Substate CYA state %s\n",
                GetName().c_str(),
                Pci::AspmL1SubstateToString(static_cast<Pci::PmL1Substate>(aspmProtCyaL1Substate)));
            m_AspmL1SSToTest &= aspmProtCyaL1Substate;
        }
        m_AspmL1SSToTest &= ~m_SkipAspmL1SubstateMask;
    }
    else
    {
        VerbosePrintf("%s : Test Device does not have ASPM L1 Substate Capability\n",
                      GetName().c_str());
        m_AspmL1SSToTest = 0;
    }

    if (pPcie->HasL1ClockPmSubstates())
    {
        m_L1ClkPmSSToTest = Pcie::L1_CLK_PM_PLLPD;

        bool bHasL1ClockPm = false;
        CHECK_RC(pPcie->HasL1ClockPm(&bHasL1ClockPm));
        VerbosePrintf("%s : Test Device is %scapable of Clock PM\n",
                      GetName().c_str(),
                      bHasL1ClockPm ? "" : "not ");

        if (bHasL1ClockPm)
            m_L1ClkPmSSToTest |= Pcie::L1_CLK_PM_CPM;

        UINT32 aspmProtCyaL1ClockPmSS = 0;
        CHECK_RC(pPcie->GetL1ClockPmCyaSubstateProtected(&aspmProtCyaL1ClockPmSS));
        VerbosePrintf("%s : Test Device protected ASPM L1 Clock PM Substate CYA state %s\n",
            GetName().c_str(),
            Pcie::L1ClockPMSubstateToString(static_cast<Pcie::L1ClockPmSubstates>(aspmProtCyaL1ClockPmSS)));

        m_L1ClkPmSSToTest &= aspmProtCyaL1ClockPmSS;
    }
    else
    {
        VerbosePrintf("%s : Test Device does not have ASPM L1 Clock PM substates\n",
                      GetName().c_str());
        m_L1ClkPmSSToTest = 0;
    }
    m_L1ClkPmSSToTest &= ~m_SkipL1ClockPmSubstateMask;

    CHECK_RC(InitAspmChipsetFilter());

    // The skip list for ASPM states is specified by the vendor and device ID of
    // the upstream chipset.  Apply any found skips to the ASPM states to test
    PexDevice::PciDevice* pPciDev = pPexDev->GetDownStreamPort(port);

    UINT16 vendorId = 0;
    CHECK_RC(Platform::PciRead16(pPciDev->Domain,
                                 pPciDev->Bus,
                                 pPciDev->Dev,
                                 pPciDev->Func,
                                 LW_PCI_VENDOR_ID,
                                 &vendorId));
    auto pVendorAspmChipsetFilter =
        find_if(m_ChipsetAspmFilter.begin(), m_ChipsetAspmFilter.end(),
                [&vendorId] (const ChipsetAspmFilter & chipsetBl) -> bool
                {
                    return chipsetBl.vendorId == vendorId;
                });
    if (pVendorAspmChipsetFilter != m_ChipsetAspmFilter.end())
    {
        UINT16 deviceId = 0;
        CHECK_RC(Platform::PciRead16(pPciDev->Domain,
                                     pPciDev->Bus,
                                     pPciDev->Dev,
                                     pPciDev->Func,
                                     LW_PCI_DEVICE_ID,
                                     &deviceId));
        auto pDeviceAspmChipsetFilter =
            find_if(pVendorAspmChipsetFilter->l0sDeviceIdAspmChipsetFilter.begin(),
                    pVendorAspmChipsetFilter->l0sDeviceIdAspmChipsetFilter.end(),
                    [&deviceId] (const UINT16 & blDeviceId) -> bool
                    {
                        return blDeviceId == deviceId;
                    });
        if (pDeviceAspmChipsetFilter != pVendorAspmChipsetFilter->l0sDeviceIdAspmChipsetFilter.end())
        {
            VerbosePrintf("%s : Skipping ASPM L0S testing due to chipset ASPM filter list, "
                          "Vendor 0x%04x, Device 0x%04x\n",
                          GetName().c_str(), vendorId, deviceId);
            m_AspmStatesToTest &= ~Pci::ASPM_L0S;
        }
        pDeviceAspmChipsetFilter =
            find_if(pVendorAspmChipsetFilter->l1DeviceIdAspmChipsetFilter.begin(),
                    pVendorAspmChipsetFilter->l1DeviceIdAspmChipsetFilter.end(),
                    [&deviceId] (const UINT16 & blDeviceId) -> bool
                    {
                        return blDeviceId == deviceId;
                    });
        if (pDeviceAspmChipsetFilter != pVendorAspmChipsetFilter->l1DeviceIdAspmChipsetFilter.end())
        {
            VerbosePrintf("%s : Skipping ASPM L1 testing due to chipset ASPM filter list, "
                          "Vendor 0x%04x, Device 0x%04x\n",
                          GetName().c_str(), vendorId, deviceId);
            m_AspmStatesToTest &= ~Pci::ASPM_L1;
        }
    }

    bool bHasDeepL1;
    bool bDeepL1Enabled;
    CHECK_RC(pPcie->HasAspmDeepL1(&bHasDeepL1));
    CHECK_RC(pPcie->GetAspmDeepL1Enabled(&bDeepL1Enabled));

    if (!bHasDeepL1 || !bDeepL1Enabled)
    {
        VerbosePrintf("%s : Skipping L1 substates because deep L1 is not enabled\n",
                      GetName().c_str());
        m_AspmL1SSToTest = 0;
    }

    string l1String;
    if (m_AspmStatesToTest & Pci::ASPM_L1)
    {
        l1String = Utility::StrPrintf(" (%sIncluding Deep L1), L1 Substates %s, L1 Clock PM Substates %s",
                    (bHasDeepL1 && bDeepL1Enabled) ? "" : "Not ",
                    Pci::AspmL1SubstateToString(static_cast<Pci::PmL1Substate>(m_AspmL1SSToTest)),
                    Pcie::L1ClockPMSubstateToString(static_cast<Pcie::L1ClockPmSubstates>(m_L1ClkPmSSToTest)));
    }
    VerbosePrintf("%s : Will test ASPM %s%s\n",
                  GetName().c_str(),
                  Pci::AspmStateToString(static_cast<Pci::ASPMState>(m_AspmStatesToTest)),
                  l1String.c_str());
    return rc;
}

//------------------------------------------------------------------------------
RC PexAspm::InitAspmChipsetFilter()
{
    if (!m_ChipsetAspmFilter.empty())
        return RC::OK;

    RC rc;
    JavaScriptPtr pJs;
    JsArray chipsetFilterArray;

    rc = pJs->GetProperty(GetJSObject(), "ChipsetFilter", &chipsetFilterArray);
    if (rc == RC::ILWALID_OBJECT_PROPERTY)
        return RC::OK;
    CHECK_RC(rc);

    auto InitDeviceIdList = [&pJs] (const JsArray & jsDevId, vector<UINT16> * pDevIdList) -> RC
    {
        RC rc;
        for (size_t jsDevIdIdx = 0; jsDevIdIdx < jsDevId.size(); jsDevIdIdx++)
        {
            UINT16 devId;
            CHECK_RC(pJs->FromJsval(jsDevId[jsDevIdIdx], &devId));
            pDevIdList->push_back(devId);
        }
        return rc;
    };

    m_ChipsetAspmFilter.resize(chipsetFilterArray.size());
    for (size_t filterIdx = 0; filterIdx < chipsetFilterArray.size(); filterIdx++)
    {
        JSObject * pAspmChipsetFilterObj;
        CHECK_RC(pJs->FromJsval(chipsetFilterArray[filterIdx], &pAspmChipsetFilterObj));
        CHECK_RC(pJs->GetProperty(pAspmChipsetFilterObj,
                                  "VendorId",
                                  &m_ChipsetAspmFilter[filterIdx].vendorId));
        JsArray tempArray;
        CHECK_RC(pJs->GetProperty(pAspmChipsetFilterObj, "L0SDeviceIds", &tempArray));
        CHECK_RC(InitDeviceIdList(tempArray,
                                  &m_ChipsetAspmFilter[filterIdx].l0sDeviceIdAspmChipsetFilter));
        tempArray.clear();
        CHECK_RC(pJs->GetProperty(pAspmChipsetFilterObj, "L1DeviceIds", &tempArray));
        CHECK_RC(InitDeviceIdList(tempArray,
                                  &m_ChipsetAspmFilter[filterIdx].l1DeviceIdAspmChipsetFilter));
    }
    return rc;
}

//------------------------------------------------------------------------------
RC PexAspm::InitFromJs()
{
    RC rc;
    CHECK_RC(GpuTest::InitFromJs());
    CHECK_RC(InitAspmChipsetFilter());
    return rc;
}

//------------------------------------------------------------------------------
RC PexAspm::RunAspmTest
(
    Pci::ASPMState           aspmState,
    Pci::PmL1Substate        aspmL1Substate,
    bool                     bDeepL1Enable,
    Pcie::L1ClockPmSubstates l1ClkPmSubstate
)
{
    RC rc;
    Pcie* pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    MASSERT(pPcie);

    PexDevice * pPexDev = nullptr;
    UINT32 port;
    CHECK_RC(pPcie->GetUpStreamInfo(&pPexDev, &port));

    // It is necessary to setup ASPM on the downstream port before the GPU
    CHECK_RC(pPexDev->SetDownStreamAspmState(aspmState, port, false));
    CHECK_RC(pPcie->SetAspmState(aspmState));
    CHECK_RC(pPcie->SetAspmCyaState(aspmState));

    if (m_bTestDevHasDeepL1)
        CHECK_RC(pPcie->EnableAspmDeepL1(bDeepL1Enable));

    if (pPcie->HasAspmL1Substates() && pPexDev->HasAspmL1Substates())
    {
        PexDevMgr *pPexDevMgr = (PexDevMgr *)DevMgrMgr::d_PexDevMgr;

        CHECK_RC(pPcie->SetAspmCyaL1SubState(aspmL1Substate));
        CHECK_RC(pPexDevMgr->SetAspmL1SS(GetBoundTestDevice()->DevInst(),
                                         0,
                                         aspmL1Substate));
    }

    // Clock PM substates are only controlled via a CYA
    if (pPcie->HasL1ClockPmSubstates())
    {
        CHECK_RC(pPcie->EnableL1ClockPm((l1ClkPmSubstate & Pcie::L1_CLK_PM_CPM) ? true : false));
        CHECK_RC(pPcie->SetL1ClockPmCyaSubstate(l1ClkPmSubstate));
    }

    CHECK_RC(pPcie->ResetAspmEntryCounters());
    CHECK_RC(DoSomeWork());
    CHECK_RC(CheckAspmCounts(aspmState, aspmL1Substate, bDeepL1Enable, l1ClkPmSubstate));

    return rc;
}

//------------------------------------------------------------------------------
JS_CLASS_INHERIT(PexAspm, GpuTest, "Pcie ASPM test");

CLASS_PROP_READWRITE(PexAspm, AspmEntryDelayMs, UINT32,
                     "Delay time in milliseconds to wait for ASPM entry");
CLASS_PROP_READWRITE(PexAspm, SkipAspmStateMask, UINT32,
                     "Mask of ASPM states to skip");
CLASS_PROP_READWRITE(PexAspm, SkipAspmL1SubstateMask, UINT32,
                     "Mask of ASPM L1 substates to skip");
CLASS_PROP_READWRITE(PexAspm, SkipL1ClockPmSubstateMask, UINT32,
                     "Mask of ASPM L1 clock PM substates to skip");
CLASS_PROP_READWRITE(PexAspm, SkipDeepL1, bool,
                     "Skip Deep L1 testing");
CLASS_PROP_READWRITE(PexAspm, ShowTestCounts, bool,
                     "Show ASPM tested states");
