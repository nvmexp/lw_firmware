/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file PcieSpeedChange.cpp
 * @brief The file used to test PEX speed change on the GPU and its upstream
 * devices
 */

#include "core/include/jscript.h"
#include "core/include/jsonlog.h"
#include "core/include/mle_protobuf.h"
#include "core/include/platform.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "ctrl/ctrl2080.h"
#include "device/interface/pcie.h"
#include "device/interface/pcie/pcieuphy.h"
#include "gpu/js/fpk_comm.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#include "gpu/uphy/uphyreglogger.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/pcie/pexdev.h"
#include "gpu/utility/surfrdwr.h"
#include "pextest.h"

/**
 *  @class PcieSpeedChange
 *  @brief
 *
 */
class PcieSpeedChange: public PexTest
{
public:
    // Test interfaces.
    RC Setup();
    virtual RC RunTest();
    RC Cleanup();

    // constructor
    PcieSpeedChange();
    ~PcieSpeedChange();
    bool IsSupported() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC SetDefaultsForPicker(UINT32 Idx) override;
    SETGET_PROP(LinesPerCopy, UINT32);
    SETGET_PROP(SkipUpStreamCheck, bool);
    SETGET_PROP(SpeedsToSkipMask, UINT32);
    SETGET_PROP(SpeedChangeDelayMs, UINT32);
    SETGET_PROP(AllowLinkTrainUphyLogging, bool);
    SETGET_PROP(SkipDmaTest, bool);

    enum
    {
         GEN1_BIT = 0x1
        ,GEN2_BIT = 0x2
        ,GEN3_BIT = 0x4
        ,GEN4_BIT = 0x8
        ,GEN5_BIT = 0x10
        ,ALL_GEN_BITS = 0x1F
    };

private:

    // Initialize the test.
    RC InitProperties();
    RC SetLinkSpeed(Pci::PcieLinkSpeed NewSpeed);
    RC ChangeLinkSpeed(Pci::PcieLinkSpeed NewSpeed);
    RC ClearPexErrorsFromSpeedChange();

    enum SubTest
    {
        BUS_SPEED_READBACK,
        DMA_TRANSFER_TEST
    };
    RC SubtestWrapper(SubTest Test);
    void SubtestErrorMsg(SubTest Test);
    RC CheckUpStreamSpeed(Pci::PcieLinkSpeed Speed);
    RC BusSpeedReadBack(UINT32 *pLastLoop);
    RC DmaTransferTest(UINT32 *pLastLoop);
    RC DmaTestSetupPattern();
    RC DmaTestCheckBuffer();
    void PrintPcieErrorStatus();
    void PrintPcieLinkSpeedMismatch
    (
        PexDevice* pPexDev,
        Pci::PcieLinkSpeed actualSpeed,
        Pci::PcieLinkSpeed expectedSpeed
    );
    void PrintPcieLinkSpeedMismatch
    (
        Pcie *pPcie,
        Pci::PcieLinkSpeed actualSpeed,
        Pci::PcieLinkSpeed expectedSpeed
    );
    void PrintPcieLinkWidthMismatch(Pcie *pPcie, UINT32 actualWidth, UINT32 expectedWidth);

    FLOAT64            m_TimeoutMs;
    Surface2D         *m_pFbMem;    // display surface
    Surface2D          m_SysMem;    // sysmem allocated for this test
    Surface2D          m_TempSurf;  // used for blanking the Fb/Sys mem surfaces
    Surface2D          m_GoldenSurf;
    DmaWrapper        *m_Dma;
    FancyPicker::FpContext  *m_pFpCtx;
    FancyPickerArray        *m_pPickers;

    UINT32             m_SpeedsToSkipMask;
    UINT32             m_OrgLinkWidth;
    Pci::PcieLinkSpeed m_OrgLinkSpeed;
    UINT32             m_LinesPerCopy;
    UINT64             m_PerfFrequency;
    bool               m_SkipUpStreamCheck;
    UINT32             m_SpeedChangeDelayMs;
    bool               m_AllowLinkTrainUphyLogging;
    bool               m_SkipDmaTest;

    struct
    {
        UINT32 Width;
        UINT32 Height;
        UINT32 Depth;
        UINT32 RefreshRate;
    } m_Mode;

    vector<Pci::PcieLinkSpeed> m_TargetSpeeds;
    UINT32                     m_GpuSpeedCap;
    UINT32                     m_SavedUphyLogPointMask;
};

//-------------------------------------------------------
// PcieSpeedChange properties
//-------------------------------------------------------
JS_CLASS_INHERIT(PcieSpeedChange, PexTest, "PcieSpeedChange Test");

PcieSpeedChange::PcieSpeedChange()
 :  m_TimeoutMs(1000),
    m_pFbMem(nullptr),
    m_Dma(nullptr),
    m_SpeedsToSkipMask(0),
    m_OrgLinkWidth(0),
    m_OrgLinkSpeed(Pci::SpeedUnknown),
    m_LinesPerCopy(48),
    m_PerfFrequency(0),
    m_SkipUpStreamCheck(false),
    m_SpeedChangeDelayMs(0),
    m_AllowLinkTrainUphyLogging(false),
    m_SkipDmaTest(false),
    m_Mode({}),
    m_GpuSpeedCap(Pci::Speed5000MBPS)
{
    SetName("PcieSpeedChange");
    m_pFpCtx = GetFpContext();
    CreateFancyPickerArray(FPK_PEXSPEEDTST_NUM_PICKERS);
    m_pPickers = GetFancyPickerArray();
}

PcieSpeedChange::~PcieSpeedChange()
{
}

CLASS_PROP_READWRITE(PcieSpeedChange, LinesPerCopy, UINT32,
                     "UINT32: number lines per copy for Mem2Mem link test");

CLASS_PROP_READWRITE(PcieSpeedChange, SkipUpStreamCheck, bool,
                     "bool: enable only the device link check - skip all upstream ports");

CLASS_PROP_READWRITE(PcieSpeedChange, SpeedsToSkipMask, UINT32,
                     "UINT32: Mask of speeds to skip (bit 0 = gen1, bit 1 = gen2, etc.)");

CLASS_PROP_READWRITE(PcieSpeedChange, SpeedChangeDelayMs, UINT32,
                     "UINT32: Delay in ms after setting speed (default = 0)");

CLASS_PROP_READWRITE(PcieSpeedChange, AllowLinkTrainUphyLogging, bool,
                     "bool: Allow uphy logging during link training (default = false)");

CLASS_PROP_READWRITE(PcieSpeedChange, SkipDmaTest, bool,
                     "bool: Skip the DMA portion of the test (default = false)");

PROP_CONST(PcieSpeedChange, Gen1Bit, PcieSpeedChange::GEN1_BIT);
PROP_CONST(PcieSpeedChange, Gen2Bit, PcieSpeedChange::GEN2_BIT);
PROP_CONST(PcieSpeedChange, Gen3Bit, PcieSpeedChange::GEN3_BIT);
PROP_CONST(PcieSpeedChange, Gen4Bit, PcieSpeedChange::GEN4_BIT);
PROP_CONST(PcieSpeedChange, Gen5Bit, PcieSpeedChange::GEN5_BIT);

/**
 *  @brief Check to make sure that test is supported
 * @return bool - True if supported, false otherwise.
 */
bool PcieSpeedChange::IsSupported()
{
    if (!PexTest::IsSupported())
    {
        return false;
    }

    UINT32 NumPStates = 0;
    UINT32 MaxPStatePexSpeed = Pci::Speed5000MBPS;
    Pcie* pPcie = GetBoundTestDevice()->GetInterface<Pcie>();
    if (GetBoundGpuSubdevice())
    {
        Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
        pPerf->GetNumPStates(&NumPStates);
        if (NumPStates > 0)
        {
            UINT32 LwrrPState = Perf::ILWALID_PSTATE;
            // just in case user uses -pstate_disable
            if (OK == pPerf->GetLwrrentPState(&LwrrPState))
            {
                pPerf->GetPexSpeedAtPState(LwrrPState, &MaxPStatePexSpeed);
            }
        }
        else
        {
            MaxPStatePexSpeed = pPcie->LinkSpeedCapability();
        }
    }
    else
    {
        MaxPStatePexSpeed = pPcie->LinkSpeedCapability();
    }

    if ((pPcie->LinkSpeedCapability() < Pci::Speed5000MBPS) ||
        (MaxPStatePexSpeed < Pci::Speed5000MBPS))
    {
        Printf(Tee::PriHigh,
               "PcieSpeedChange : The testdevice must support at least Gen2 link speed "
               "at the current pstate (Gen1 speed detected)\n");
        return false;
    }

    if (m_SpeedsToSkipMask && GetAllowCoverageHole())
    {
        // If we are here then at least Gen1 and Gen2 are available.  Disable
        // Gen3 and Gen4 if not supported
        UINT32 speedsToTestMask = ALL_GEN_BITS;
        if (MaxPStatePexSpeed < Pci::Speed8000MBPS)
            speedsToTestMask &= ~GEN3_BIT;
        if (MaxPStatePexSpeed < Pci::Speed16000MBPS)
            speedsToTestMask &= ~GEN4_BIT;
        if (MaxPStatePexSpeed < Pci::Speed32000MBPS)
            speedsToTestMask &= ~GEN5_BIT;

        speedsToTestMask &= ~m_SpeedsToSkipMask;
        if (!speedsToTestMask || !(speedsToTestMask & (speedsToTestMask - 1)))
        {
            Printf(Tee::PriHigh,
                   "PcieSpeedChange : Not enough link speeds to test after "
                   "applying skip mask\n");
            return false;
        }
    }

    return true;
}

void PcieSpeedChange::PrintJsProperties(Tee::Priority pri)
{
    PexTest::PrintJsProperties(pri);

    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tLinesPerCopy:               %u\n", m_LinesPerCopy);
    Printf(pri, "\tSkipUpStreamCheck:          %s\n", m_SkipUpStreamCheck ? "true" : "false");
    Printf(pri, "\tSpeedsToSkipMask:           0x%x\n",   m_SpeedsToSkipMask);
    Printf(pri, "\tSpeedChangeDelayMs:         %u\n",   m_SpeedChangeDelayMs);
    Printf(pri, "\tAllowLinkTrainUphyLogging:  %s\n", m_AllowLinkTrainUphyLogging ? "true" : "false");
    Printf(pri, "\tSkipDmaTest:                %s\n", m_SkipDmaTest ? "true" : "false");
}

RC PcieSpeedChange::SetDefaultsForPicker(UINT32 Idx)
{
    FancyPickerArray * pPickers = GetFancyPickerArray();
    FancyPicker &Fp = (*pPickers)[Idx];
    switch (Idx)
    {
        case FPK_PEXSPEEDTST_LINKSPEED:
            Fp.ConfigRandom();
            // by default, run just Gen1/Gen2. We will add Gen3 and/or Gen4 if we
            // detect that the GPU supports them. SetDefaultsForPicker
            // is called in the constructor
            Fp.AddRandItem(1, FPK_PEXSPEEDTST_LINKSPEED_gen1);
            Fp.AddRandItem(1, FPK_PEXSPEEDTST_LINKSPEED_gen2);
            Fp.CompileRandom();
            break;
        default:
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

/**
 * @brief Declare and setup ds to run its test(s).
 * @return RC - Indicates whether all setup succeeded.
 */
RC PcieSpeedChange::Setup()
{
    RC rc;
    CHECK_RC(PexTest::Setup());
    CHECK_RC(InitProperties());

    if (GetBoundGpuDevice())
    {
        CHECK_RC(GpuTest::AllocDisplayAndSurface(false/*useBlockLinear*/));

        UINT32 SubdevInst = GetBoundGpuSubdevice()->GetSubdeviceInst();
        // allocate surfaces
        m_pFbMem = GetDisplayMgrTestContext()->GetSurface2D();
        CHECK_RC(m_pFbMem->Map(SubdevInst));

        m_SysMem.SetWidth(m_Mode.Width);
        // We are writing a pattern from Golden surface to Sysmem and then start
        // constructing a surface based on the first strip that's copied -
        // copy one strip from sysmem to fb, then fb to sysmem, etc. So at the
        // last loop, there would be one extra strip to copy into sysmem
        m_SysMem.SetHeight(m_Mode.Height + m_LinesPerCopy);
        m_SysMem.SetLayout(Surface2D::Pitch);
        m_SysMem.SetColorFormat(m_pFbMem->GetColorFormat());
        m_SysMem.SetLocation(Memory::NonCoherent);
        m_SysMem.SetProtect(Memory::ReadWrite);
        CHECK_RC(m_SysMem.Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_SysMem.Map(SubdevInst));

        m_TempSurf.SetWidth(m_Mode.Width);
        m_TempSurf.SetHeight(m_Mode.Height + m_LinesPerCopy);
        m_TempSurf.SetLayout(Surface2D::Pitch);
        m_TempSurf.SetColorFormat(m_pFbMem->GetColorFormat());
        m_TempSurf.SetLocation(Memory::NonCoherent);
        m_TempSurf.SetProtect(Memory::ReadWrite);
        CHECK_RC(m_TempSurf.Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_TempSurf.Map(SubdevInst));
        CHECK_RC(m_TempSurf.Fill(0, SubdevInst));

        m_GoldenSurf.SetWidth(m_Mode.Width);
        m_GoldenSurf.SetHeight(m_LinesPerCopy);
        m_GoldenSurf.SetLayout(Surface2D::Pitch);
        m_GoldenSurf.SetColorFormat(m_pFbMem->GetColorFormat());
        m_GoldenSurf.SetLocation(Memory::NonCoherent);
        m_GoldenSurf.SetProtect(Memory::ReadWrite);
        CHECK_RC(m_GoldenSurf.Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_GoldenSurf.Map(SubdevInst));
        CHECK_RC(Memory::FillAddress(m_GoldenSurf.GetAddress(),
                                     m_Mode.Width,
                                     m_LinesPerCopy,
                                     m_Mode.Depth,
                                     m_GoldenSurf.GetPitch()));

        m_Dma = new DmaWrapper(&m_TestConfig);
    }

    m_PerfFrequency = Xp::QueryPerformanceFrequency();

    m_SavedUphyLogPointMask = UphyRegLogger::GetLogPointMask();
    if (UphyRegLogger::GetLoggingEnabled() &&
        (m_SavedUphyLogPointMask & UphyRegLogger::LogPoint::PostLinkTraining) &&
        !m_AllowLinkTrainUphyLogging)
    {
        const UINT32 newLogPointMask = m_SavedUphyLogPointMask & ~UphyRegLogger::LogPoint::PostLinkTraining;
        CHECK_RC(UphyRegLogger::SetLogPointMask(newLogPointMask));
    }

    return rc;
}

/**
 * @brief Actually starts and runs the test(s).
 * Test 1: upgrade/downgrade speed and check if link status
 * reads the correct speed and link width
 * @return RC - Indicates whether the test(s) passed or failed.
 */
RC PcieSpeedChange::RunTest()
{
    RC rc;
    m_OrgLinkWidth = GetPcie()->GetLinkWidth();
    m_OrgLinkSpeed = GetPcie()->GetLinkSpeed(Pci::SpeedUnknown);
    if(m_OrgLinkWidth == 0 || m_OrgLinkSpeed == 0)
    {
        return RC::PCIE_BUS_ERROR;
    }

    if (0 == m_TargetSpeeds.size())
    {
        Printf(Tee::PriNormal, "no test speed to test. Config failure\n");
        return RC::SOFTWARE_ERROR;
    }

    Printf(Tee::PriLow, "original link width = %d\n", m_OrgLinkWidth);
    CHECK_RC(SubtestWrapper(BUS_SPEED_READBACK));

    if (!m_SkipDmaTest)
    {
        CHECK_RC(SubtestWrapper(DMA_TRANSFER_TEST));
    }
    return rc;
}

RC PcieSpeedChange::ChangeLinkSpeed(Pci::PcieLinkSpeed NewSpeed)
{
    RC rc;
    Pcie *pPcie = GetPcie();
    CHECK_RC(pPcie->SetLinkSpeed(NewSpeed));
    if (m_SpeedChangeDelayMs)
        Tasker::Sleep(m_SpeedChangeDelayMs);

    // set the speed of ALL the bridges above
    PexDevice* pPexDev;
    CHECK_RC(pPcie->GetUpStreamInfo(&pPexDev));
    // no PexDev above or user asks to skip upstream bridges
    if ((pPexDev == NULL) || m_SkipUpStreamCheck)
    {
        return OK;
    }

    UINT32 UpStreamDevicesChecked = 0;
    while ((UpStreamDevicesChecked < GetUpStreamDepth()) && (pPexDev != NULL))
    {
        if(OK != pPexDev->ChangeUpStreamSpeed(NewSpeed))
        {
            PrintPcieLinkSpeedMismatch(pPexDev, pPexDev->GetUpStreamSpeed(), NewSpeed);

            Printf(Tee::PriNormal, "Cannot train bus %d to %dMB/s\n",
                   pPexDev->GetUpStreamPort()->Bus, NewSpeed);
            return RC::PCIE_BUS_ERROR;
        }
        pPexDev = pPexDev->GetUpStreamDev();
        UpStreamDevicesChecked++;
    }
    return OK;
}

// Bug 361633
// Correctable errors are expected to occur sometimes during a speed change.
// Our hardware can potentially propagate this error up the stream of bridge
// devices. RM takes care of only clearing the correctable error at the GPU
// side we need to clear the CEs on the chain of BR04/chipset ourselves.
//
// - By 'Reading' PEX errors, we clear the PEX status errors.
// - we still want to fail the test when a fatal error is detected along the
// chain
RC PcieSpeedChange::ClearPexErrorsFromSpeedChange()
{
    RC rc;
    PexDevice* pPexDev;
    UINT32 Port;
    CHECK_RC(GetGpuPcie()->GetUpStreamInfo(&pPexDev, &Port));
    // no Device above - this is a weird scenario (can happen in Apple because
    // Pci Config Read/writes are not working)
    // Even if there is no 'PexDevice' there should at least be a chipset
    if (pPexDev == NULL)
    {
        return OK;
    }

    Pci::PcieErrorsMask HostErrors = Pci::NO_ERROR;
    Pci::PcieErrorsMask LocErrors = Pci::NO_ERROR;
    // this is for the link between GPU and its direct Host
    pPexDev->GetDownStreamErrors(&HostErrors, Port);
    if (HostErrors & Pci::FATAL_ERROR)
    {
        Printf(Tee::PriNormal,
                "Fatal error detected on PCI device at bus %d\n",
               pPexDev->GetUpStreamPort()->Bus);
        Printf(Tee::PriNormal, "UpStream Error Mask = 0x%x\n", HostErrors);
        return RC::PCIE_BUS_ERROR;
    }

    // move upwards until root
    while (pPexDev && !pPexDev->IsRoot())
    {
        LocErrors = Pci::NO_ERROR;
        HostErrors = Pci::NO_ERROR;

        CHECK_RC(pPexDev->GetUpStreamErrors(&LocErrors, &HostErrors));
        if ((LocErrors & Pci::FATAL_ERROR)||
            (HostErrors & Pci::FATAL_ERROR))
        {
            Printf(Tee::PriNormal,
                    "Fatal error detected on PCI device at bus %d\n",
                   pPexDev->GetUpStreamPort()->Bus);
            Printf(Tee::PriNormal, "Downstream Error Mask = 0x%x\n", LocErrors);
            Printf(Tee::PriNormal, "UpStream Error Mask = 0x%x\n", HostErrors);
            return RC::PCIE_BUS_ERROR;
        }
        pPexDev = pPexDev->GetUpStreamDev();
    }
    return rc;
}

/**
 * @brief: This will attempt to set the speed of the path from the current
 *         Gpu under test to the chipset.
 */
RC PcieSpeedChange::SetLinkSpeed(Pci::PcieLinkSpeed NewSpeed)
{
    RC rc;
    CHECK_RC(ChangeLinkSpeed(NewSpeed));
    if (GetBoundTestDevice()->HasBug(361633))
    {
        CHECK_RC(ClearPexErrorsFromSpeedChange());
    }

    return rc;
}

/**
 * @brief: Check Upstream Speed. See if the speed of all devices above the
 *         current GPU is as expected
 */
RC PcieSpeedChange::CheckUpStreamSpeed(Pci::PcieLinkSpeed Speed)
{
    // TODO HEWU: In ::Setup() or in PexDev's Setup, find out the PEX speed
    // capability of the device. Compare this with the entry in boards.js
    // The UpStreamDev entry in boards.js should specify the depth and
    // expected capability of the device
    // Waiting on integration of BoardDef and RM's query on PEX speed capaibility
    RC rc;

    Pcie *pPcie = GetPcie();
    Pci::PcieLinkSpeed GpuLinkSpeed = pPcie->GetLinkSpeed(Speed);
    if (Speed != GpuLinkSpeed)
    {
        PrintPcieLinkSpeedMismatch(pPcie, GpuLinkSpeed, Speed);

        Printf(Tee::PriNormal,
               "Device link speed incorrect. should be %d, but got %d\n",
               Speed, GpuLinkSpeed);
        return RC::PCIE_BUS_ERROR;
    }

    // continue to check the speed of ALL the PexDevs above
    PexDevice* pPexDev;
    CHECK_RC(pPcie->GetUpStreamInfo(&pPexDev));
    // no PexDev above or user says don't check upstream bridges
    if ((pPexDev == NULL) || m_SkipUpStreamCheck)
    {
        return rc;
    }

    UINT32 UpStreamDevicesChecked = 0;
    while ((UpStreamDevicesChecked < GetUpStreamDepth()) && (pPexDev != NULL))
    {
        Pci::PcieLinkSpeed UpStreamSpeed = pPexDev->GetUpStreamSpeed();
        if (Speed != UpStreamSpeed)
        {
            PrintPcieLinkSpeedMismatch(pPexDev, UpStreamSpeed, Speed);

            Printf(Tee::PriNormal,
                   "PexDev 0x%x Speed check failed. Should be %d, got %d\n",
                   pPexDev->GetUpStreamPort()->Bus, Speed, UpStreamSpeed);
            return RC::PCIE_BUS_ERROR;
        }
        pPexDev = pPexDev->GetUpStreamDev();
        UpStreamDevicesChecked++;
    }

    return rc;
}

void PcieSpeedChange::SubtestErrorMsg(SubTest Test)
{
    switch (Test)
    {
        case BUS_SPEED_READBACK:
            Printf(Tee::PriNormal,
                   "Bus speed readback test failed\n"
                   "Warning : This test must be run on a motherboard that "
                   "supports Gen%d PCIE\n",
                   (m_GpuSpeedCap == Pci::Speed32000MBPS) ? 5 :
                       (m_GpuSpeedCap == Pci::Speed16000MBPS) ? 4 :
                           (m_GpuSpeedCap == Pci::Speed8000MBPS) ? 3 : 2);
            break;
        case DMA_TRANSFER_TEST:
            Printf(Tee::PriNormal, "PEX speed switch + DMA test failed\n");
            break;
        default:
            MASSERT(!"unknown subtest");
            break;
    }
}

RC PcieSpeedChange::SubtestWrapper(SubTest Test)
{
    StickyRC rc;
    UINT32 LastExelwtedLoop = 0;
    switch (Test)
    {
        case BUS_SPEED_READBACK:
            rc = BusSpeedReadBack(&LastExelwtedLoop);
            break;
        case DMA_TRANSFER_TEST:
            rc = DmaTransferTest(&LastExelwtedLoop);
            break;
        default:
            rc = RC::SOFTWARE_ERROR;
    }
    rc = SetLinkSpeed(m_OrgLinkSpeed);

    if (rc != OK)
    {
        GetJsonExit()->AddFailLoop(LastExelwtedLoop);
        SubtestErrorMsg(Test);
        PrintPcieErrorStatus();
    }
    return rc;
}

/**
 * @brief This mini test changes the link speed and reads back
 *        the link speed to see if the change oclwrs. Also check
 *        the link width to see if any lanes failed.
 */
RC PcieSpeedChange::BusSpeedReadBack(UINT32 *pLastLoop)
{
    MASSERT(pLastLoop);
    RC rc;
    const UINT32 Loops = static_cast<UINT32>(m_TargetSpeeds.size());
    Pcie *pPcie = GetPcie();

    for (UINT32 Loop = 0; Loop< Loops ; Loop++)
    {
        *pLastLoop = Loop;
        Printf(GetVerbosePrintPri(), "BusSpeedReadBack: loop =%u, speed =%u Mbit/s\n",
                Loop, static_cast<unsigned>(m_TargetSpeeds[Loop]));
        CHECK_RC(SetLinkSpeed(m_TargetSpeeds[Loop]));
        CHECK_RC(CheckUpStreamSpeed(m_TargetSpeeds[Loop]));
        UINT32 LinkWidth = pPcie->GetLinkWidth();
        if (LinkWidth != m_OrgLinkWidth)
        {

            PrintPcieLinkWidthMismatch(pPcie, LinkWidth, m_OrgLinkWidth);

            Printf(Tee::PriHigh, "Incorrect Link Width (%d) on loop %d\n"
                   "Expecting Gen%d speed with width = %d\n",
                   LinkWidth, Loop,
                   (m_GpuSpeedCap == Pci::Speed32000MBPS) ? 5 :
                       (m_GpuSpeedCap == Pci::Speed16000MBPS) ? 4 :
                           (m_GpuSpeedCap == Pci::Speed8000MBPS) ? 3 : 2,
                   m_OrgLinkWidth);
            CHECK_RC(RC::PCIE_BUS_ERROR);
        }
    }
    return rc;
}

/**
 * @brief This test will perform memory to memory copy at
 *        changing speeds - switching between 2500 and 5000 MB/s
 *        The result will be checked at the end to see if the
 *        expected pattern is constructed on the system memory
 */
RC PcieSpeedChange::DmaTransferTest(UINT32 *pLastLoop)
{
    MASSERT(pLastLoop);
    RC rc;
    GpuSubdevice *pSubdevice = GetBoundGpuSubdevice();
    Pcie *pPcie = GetPcie();
    UINT32 IndexInTargetSpeed = 0;
    UINT32 loop;
    for (loop=0; loop< m_TargetSpeeds.size(); loop++)
    {
        *pLastLoop = loop;
        Printf(GetVerbosePrintPri(), "DmaTransferTest: loop = %d\n", loop);

        if (pSubdevice)
        {
            CHECK_RC(DmaTestSetupPattern());
            Printf(GetVerbosePrintPri(), "do mem copy. Height=%d, lines/copy=%d\n",
                   m_Mode.Height, m_LinesPerCopy);
        }

        for (UINT32 lwrY = 0; lwrY <= m_Mode.Height-m_LinesPerCopy;
               lwrY += m_LinesPerCopy)
        {
            Pci::PcieLinkSpeed speed = m_TargetSpeeds[IndexInTargetSpeed];
            CHECK_RC(SetLinkSpeed(speed));

            if (pSubdevice)
            {
                Printf(Tee::PriDebug, "lwrY=%d\n", lwrY);
                UINT32 Pitch = m_pFbMem->GetPitch();
                // copy one line from sys mem to fb, and then take the copied lines
                // and paste it back to sysmem
                m_Dma->SetSrcDstSurf(&m_SysMem, m_pFbMem);
                CHECK_RC(m_Dma->Copy(0,lwrY, Pitch, m_LinesPerCopy, 0,
                                     lwrY, m_TimeoutMs,
                                     pSubdevice->GetSubdeviceInst()));
                m_Dma->SetSrcDstSurf(m_pFbMem, &m_SysMem);
                CHECK_RC(m_Dma->Copy(0,lwrY, Pitch, m_LinesPerCopy, 0,
                                     lwrY+m_LinesPerCopy, m_TimeoutMs,
                                     pSubdevice->GetSubdeviceInst()));
            }
            else // LwSwtich
            {
                // Just read the link speed several times to create light PEX traffic
                for (UINT32 i = 0; i < m_LinesPerCopy; i++)
                {
                    Pci::PcieLinkSpeed lwrSpeed = pPcie->GetLinkSpeed(speed);
                    if (speed != lwrSpeed)
                    {
                        PrintPcieLinkSpeedMismatch(pPcie, lwrSpeed, speed);

                        Printf(Tee::PriNormal,
                               "Device link speed incorrect. should be %d, but got %d\n",
                               speed, lwrSpeed);
                        return RC::PCIE_BUS_ERROR;
                    }
                }
            }

            CHECK_RC(GetPcieErrors(GetBoundTestDevice()));

            UINT32 LinkWidth = pPcie->GetLinkWidth();
            if (LinkWidth != m_OrgLinkWidth)
            {
                PrintPcieLinkWidthMismatch(pPcie, LinkWidth, m_OrgLinkWidth);

                Printf(Tee::PriHigh, "Incorrect Link Width.\n current link width "
                       "=%d, expecting = %d\n", LinkWidth, m_OrgLinkWidth);
                return RC::BAD_LINK_WIDTH;
            }
            IndexInTargetSpeed = (IndexInTargetSpeed + 1)%m_TargetSpeeds.size();
        }

        if (pSubdevice)
        {
            // check for errors
            CHECK_RC(DmaTestCheckBuffer());
        }
    }
    CHECK_RC(CheckPcieErrors(GetBoundTestDevice()));
    return rc;
}

/**
 * @brief For DmaTransferTest. This will fill one line of the
 *        system memory surface with addresses.
 */
RC PcieSpeedChange::DmaTestSetupPattern()
{
    RC rc;
    // fill sys mem and fb with 0
    m_Dma->SetSrcDstSurf(&m_TempSurf, &m_SysMem);
    CHECK_RC(m_Dma->Copy(0,0,
                         m_TempSurf.GetPitch(),
                         m_TempSurf.GetHeight(),
                         0,0, m_TimeoutMs,
                         GetBoundGpuSubdevice()->GetSubdeviceInst()));
    m_Dma->SetSrcDstSurf(&m_TempSurf, m_pFbMem);
    CHECK_RC(m_Dma->Copy(0,0,
                         m_TempSurf.GetPitch(),
                         m_TempSurf.GetHeight() - m_LinesPerCopy,
                         0,0, m_TimeoutMs,
                         GetBoundGpuSubdevice()->GetSubdeviceInst()));

    // copy the address on data pattern on the first rectangle on sysmeme surface
    m_Dma->SetSrcDstSurf(&m_GoldenSurf, &m_SysMem);
    CHECK_RC(m_Dma->Copy(0,0,
                         m_GoldenSurf.GetPitch(),
                         m_LinesPerCopy,
                         0,0, m_TimeoutMs,
                         GetBoundGpuSubdevice()->GetSubdeviceInst()));
    return rc;
}

/**
 * @brief Use the golden line array to check whether the read
 *        and write backs were successful
 *        ## Regarding why lwrY = m_Mode.Height - m_LinesPerCopy: ##
 *        In the test, the surfaces are divided into rectangular girds. The data
 *        is copied from one AddresOnData Box pattern from  SytemMemory to
 *        FB -> and the FB to SysMem sequence moving down the Surface. Since the
 *        copy are the same, we can approximate that the copying is likely correct if
 *        the data in the last bottom most SysMem box is the same as the
 *        GoldenData. There can be the exception where we have a double flip during
 *        the data transfer, but the trade off for test time saving is much better.
 */
RC PcieSpeedChange::DmaTestCheckBuffer()
{
    StickyRC rc;
    size_t CopyCount = m_GoldenSurf.GetPitch() * m_LinesPerCopy / sizeof(UINT32);
    vector<UINT32> SysMemPix(CopyCount);
    vector<UINT32> GoldPix(CopyCount);
    UINT32 SubdevInst = GetBoundGpuSubdevice()->GetSubdeviceInst();
    CHECK_RC(SurfaceUtils::ReadSurface(m_GoldenSurf,
                                       0,
                                       &GoldPix[0],
                                       CopyCount*sizeof(UINT32),
                                       SubdevInst));
    UINT32 lwrY = m_Mode.Height - m_LinesPerCopy;
    CHECK_RC(SurfaceUtils::ReadSurface(m_SysMem,
                                       m_SysMem.GetPixelOffset(0,lwrY),
                                       &SysMemPix[0],
                                       CopyCount*sizeof(UINT32),
                                       SubdevInst));

    for (size_t i = 0; i < CopyCount; i++)
    {
        if (SysMemPix[i] != GoldPix[i])
        {
            Printf(Tee::PriHigh,
                   "at lwrX=%d, lwrY=%d: got 0x%x but expecting 0x%x\n",
                   (UINT32)i%m_Mode.Width,
                   (UINT32)i/m_Mode.Width,
                   SysMemPix[i],
                   GoldPix[i]);
            rc = RC::VECTORDATA_VALUE_MISMATCH;
        }
    }
    return rc;
}

/**
 * @brief Cleans up any allocated memory and resets objects as
 *        needed.
 */
RC PcieSpeedChange::Cleanup()
{
    RC rc, firstRc;

    Printf(Tee::PriLow, "PcieSpeedChange::Cleanup\n");
    m_TargetSpeeds.clear();

    if (m_Dma != NULL)
    {
        delete m_Dma;
        m_Dma = NULL;
    }

    m_SysMem.Free();
    m_TempSurf.Free();
    m_GoldenSurf.Free();

    FIRST_RC(PexTest::Cleanup());
    CHECK_RC(UphyRegLogger::SetLogPointMask(m_SavedUphyLogPointMask));
    return rc;
}

/**
 * @brief Initializes any properties required by this test.
 *
 * @return RC - Indicates whether everything was initialized
 *         successfully or not.
 */
RC PcieSpeedChange::InitProperties()
{
    RC rc;
    CHECK_RC(m_TestConfig.InitFromJs(GetJSObject()));

    m_TimeoutMs        = m_TestConfig.TimeoutMs();
    m_Mode.Width       = m_TestConfig.SurfaceWidth();
    m_Mode.Height      = m_TestConfig.SurfaceHeight();
    if (m_LinesPerCopy < 1)
    {
        return RC::BAD_PARAMETER;
    }
    if ((m_Mode.Height % (2*m_LinesPerCopy) != 0) &&
        (m_Mode.Height >= 2*m_LinesPerCopy))
    {
        Printf(Tee::PriHigh, "Height must be greater than LinesPerCopy "
                             "and divisible by LinesPerCopy\n");
        return RC::BAD_PARAMETER;
    }
    m_Mode.Depth       = m_TestConfig.DisplayDepth();
    m_Mode.RefreshRate = m_TestConfig.RefreshRate();

    m_GpuSpeedCap = GetPcie()->LinkSpeedCapability();

    if (GetBoundGpuSubdevice())
    {
        // don't test more than what the current PState allows
        Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
        UINT32 LwrrPState;
        if (OK == pPerf->GetLwrrentPState(&LwrrPState))
        {
            if (LwrrPState != Perf::ILWALID_PSTATE)
            {
                UINT32 LwrMaxSpeed = 0;
                CHECK_RC(pPerf->GetPexSpeedAtPState(LwrrPState, &LwrMaxSpeed));
                if (m_GpuSpeedCap > LwrMaxSpeed)
                {
                    m_GpuSpeedCap = LwrMaxSpeed;
                }

                m_GpuSpeedCap = AllowCoverageHole(m_GpuSpeedCap);
            }
        }
    }

    FancyPickerArray *pPickers = GetFancyPickerArray();
    FancyPicker      &Fp       = (*pPickers)[FPK_PEXSPEEDTST_LINKSPEED];
    // default is just Gen1/Gen2. If the GPU supports Gen3, Gen4, or if user specifically
    // want to skip a PEX speed, we have to recompile the picker.
    // Also, m_SpeedToSkip doesn't make sense unless we don't have at least Gen3 as well (ie. can't skip
    // any if the Gpu only supports Gen1/Gen2.. test becomes pointless)
    if ((m_GpuSpeedCap == Pci::Speed8000MBPS  ||
         m_GpuSpeedCap == Pci::Speed16000MBPS ||
         m_GpuSpeedCap == Pci::Speed32000MBPS) &&
        !(*pPickers)[FPK_PEXSPEEDTST_LINKSPEED].WasSetFromJs())
    {
        Fp.ConfigRandom();
        string skippedSpeeds = "";
        if ((m_SpeedsToSkipMask & GEN1_BIT) && GetAllowCoverageHole())
            skippedSpeeds += "2500 ";
        else
            Fp.AddRandItem(1, FPK_PEXSPEEDTST_LINKSPEED_gen1);
        if ((m_SpeedsToSkipMask & GEN2_BIT) && GetAllowCoverageHole())
            skippedSpeeds += "5000 ";
        else
            Fp.AddRandItem(1, FPK_PEXSPEEDTST_LINKSPEED_gen2);
        if ((m_SpeedsToSkipMask & GEN3_BIT) && GetAllowCoverageHole())
            skippedSpeeds += "8000 ";
        else
            Fp.AddRandItem(1, FPK_PEXSPEEDTST_LINKSPEED_gen3);

        if (m_GpuSpeedCap >= Pci::Speed16000MBPS)
        {
            if ((m_SpeedsToSkipMask & GEN4_BIT) && GetAllowCoverageHole())
                skippedSpeeds += "16000 ";
            else
                Fp.AddRandItem(1, FPK_PEXSPEEDTST_LINKSPEED_gen4);
        }

        if (m_GpuSpeedCap >= Pci::Speed32000MBPS)
        {
            if ((m_SpeedsToSkipMask & GEN5_BIT) && GetAllowCoverageHole())
                skippedSpeeds += "32000 ";
            else
                Fp.AddRandItem(1, FPK_PEXSPEEDTST_LINKSPEED_gen5);
        }

        if (skippedSpeeds != "")
        {
            Printf(Tee::PriNormal,
                    "Warning: Skipping the following speeds: %s\n",
                    skippedSpeeds.c_str());
        }
        Fp.CompileRandom();
    }

    for (m_pFpCtx->LoopNum = m_TestConfig.StartLoop();
         m_pFpCtx->LoopNum < (m_TestConfig.StartLoop() + m_TestConfig.Loops());
           ++m_pFpCtx->LoopNum)
    {
         if ((m_pFpCtx->LoopNum == m_TestConfig.StartLoop()) ||
             (m_pFpCtx->LoopNum % m_TestConfig.RestartSkipCount() == 0))
         {
             m_pFpCtx->Rand.SeedRandom(m_TestConfig.Seed() + m_pFpCtx->LoopNum);
             m_pFpCtx->RestartNum = m_pFpCtx->LoopNum/m_TestConfig.RestartSkipCount();
             m_pPickers->Restart();
         }

         UINT32 Index = (Pci::PcieLinkSpeed)(*m_pPickers)[FPK_PEXSPEEDTST_LINKSPEED].Pick();
         Pci::PcieLinkSpeed Speed = Pci::SpeedUnknown;
         switch (Index)
         {
             case FPK_PEXSPEEDTST_LINKSPEED_gen1:
                 Speed = Pci::Speed2500MBPS;
                 break;
             case FPK_PEXSPEEDTST_LINKSPEED_gen2:
                 Speed = Pci::Speed5000MBPS;
                 break;
             case FPK_PEXSPEEDTST_LINKSPEED_gen3:
                 Speed = Pci::Speed8000MBPS;
                 break;
             case FPK_PEXSPEEDTST_LINKSPEED_gen4:
                 Speed = Pci::Speed16000MBPS;
                 break;
             case FPK_PEXSPEEDTST_LINKSPEED_gen5:
                 Speed = Pci::Speed32000MBPS;
                 break;
             default:
                 return RC::SOFTWARE_ERROR;
         }
         m_TargetSpeeds.push_back(Speed);
    }
    return OK;
}

void PcieSpeedChange::PrintPcieErrorStatus()
{
    Pci::PcieErrorsMask devMask = Pci::NO_ERROR;
    Pci::PcieErrorsMask hostMask = Pci::NO_ERROR;

    if (RC::OK != GetPcie()->GetPolledErrors(&devMask, &hostMask))
    {
        Printf(Tee::PriError, "Failed to get PCIE Errors!\n");
        return;
    }

    const vector<pair<Pci::PcieErrorsMask, string>> masks =
    {
        { devMask, GetBoundTestDevice()->GetName() },
        { hostMask, Utility::StrPrintf("upstream device from %s", GetBoundTestDevice()->GetName().c_str()) }
    };

    for (auto maskPair : masks)
    {
        const Pci::PcieErrorsMask& mask = maskPair.first;
        const string& maskName = maskPair.second;

        if (mask & Pci::CORRECTABLE_ERROR)
        {
            Printf(Tee::PriError, "Detected correctable error on %s\n",
                   maskName.c_str());
        }
        if (mask & Pci::NON_FATAL_ERROR)
        {
            Printf(Tee::PriError, "Detected non fatal error on %s\n",
                    maskName.c_str());
        }
        if (mask & Pci::FATAL_ERROR)
        {
            Printf(Tee::PriError, "Detected fatal error on %s\n",
                    maskName.c_str());
        }
        if (mask & Pci::UNSUPPORTED_REQ)
        {
            Printf(Tee::PriError, "Detected unsupported speed change request on %s\n",
                   maskName.c_str());
        }
    }
}


void PcieSpeedChange::PrintPcieLinkSpeedMismatch
(
    PexDevice* pPexDev,
    Pci::PcieLinkSpeed actualSpeed,
    Pci::PcieLinkSpeed expectedSpeed
)
{
    MASSERT(pPexDev);

    ByteStream bs;
    auto entry = Mle::Entry::pcie_link_speed_mismatch(&bs);

    PexDevice* pPexDevUpstream = pPexDev->GetUpStreamDev();
    if (pPexDevUpstream != NULL)
    {
        UINT32 port = pPexDev->GetParentDPortIdx();
        entry.link()
            .host()
                .domain(pPexDevUpstream->GetDownStreamPort(port)->Domain)
                .bus(pPexDevUpstream->GetDownStreamPort(port)->Bus)
                .device(pPexDevUpstream->GetDownStreamPort(port)->Dev)
                .function(pPexDevUpstream->GetDownStreamPort(port)->Func);
    }
    entry.link()
        .local()
            .domain(pPexDev->GetUpStreamPort()->Domain)
            .bus(pPexDev->GetUpStreamPort()->Bus)
            .device(pPexDev->GetUpStreamPort()->Dev)
            .function(pPexDev->GetUpStreamPort()->Func);
    entry.actual_link_speed_mbps(actualSpeed)
        .expected_min_link_speed_mbps(expectedSpeed)
        .expected_max_link_speed_mbps(expectedSpeed);
    entry.Finish();
    Tee::PrintMle(&bs);
}

void PcieSpeedChange::PrintPcieLinkSpeedMismatch
(
    Pcie *pPcie,
    Pci::PcieLinkSpeed actualSpeed,
    Pci::PcieLinkSpeed expectedSpeed
)
{
    MASSERT(pPcie);

    ByteStream bs;
    auto entry = Mle::Entry::pcie_link_speed_mismatch(&bs);

    PexDevice* pPexDevUpstream;
    UINT32 port = 0;
    if ((RC::OK == pPcie->GetUpStreamInfo(&pPexDevUpstream, &port)) && pPexDevUpstream != NULL)
    {
        entry.link()
            .host()
                .domain(pPexDevUpstream->GetDownStreamPort(port)->Domain)
                .bus(pPexDevUpstream->GetDownStreamPort(port)->Bus)
                .device(pPexDevUpstream->GetDownStreamPort(port)->Dev)
                .function(pPexDevUpstream->GetDownStreamPort(port)->Func);
    }
    entry.link()
        .local()
            .domain(pPcie->DomainNumber())
            .bus(pPcie->BusNumber())
            .device(pPcie->DeviceNumber())
            .function(pPcie->FunctionNumber());
    entry.actual_link_speed_mbps(actualSpeed)
        .expected_min_link_speed_mbps(expectedSpeed)
        .expected_max_link_speed_mbps(expectedSpeed);
    entry.Finish();
    Tee::PrintMle(&bs);
}

void PcieSpeedChange::PrintPcieLinkWidthMismatch
(
    Pcie *pPcie,
    UINT32 actualWidth,
    UINT32 expectedWidth
)
{
    MASSERT(pPcie);

    ByteStream bs;
    auto entry = Mle::Entry::pcie_link_width_mismatch(&bs);

    PexDevice* pPexDevUpstream;
    UINT32 port = 0;
    if ((RC::OK == pPcie->GetUpStreamInfo(&pPexDevUpstream, &port)) && pPexDevUpstream != NULL)
    {
        entry.link()
            .host()
                .domain(pPexDevUpstream->GetDownStreamPort(port)->Domain)
                .bus(pPexDevUpstream->GetDownStreamPort(port)->Bus)
                .device(pPexDevUpstream->GetDownStreamPort(port)->Dev)
                .function(pPexDevUpstream->GetDownStreamPort(port)->Func);
    }
    entry.link()
        .local()
            .domain(pPcie->DomainNumber())
            .bus(pPcie->BusNumber())
            .device(pPcie->DeviceNumber())
            .function(pPcie->FunctionNumber());
    entry.is_host(false)
        .actual_link_width(actualWidth)
        .expected_min_link_width(expectedWidth)
        .expected_max_link_width(expectedWidth);
    entry.Finish();
    Tee::PrintMle(&bs);
}
