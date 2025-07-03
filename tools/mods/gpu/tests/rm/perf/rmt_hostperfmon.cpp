/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_hostperfmon.cpp
//! \brief RM Test to test PCIE utilization sampling result
//!
//! We are adding PCIE utilization to perfmon control in Maxwell
//! What this test does is send traffic burst on PCIe bus
//! and then monitor the utilization sampling result changes to
//! make sure it's in desired range.

#include "core/include/gpu.h"
#include "core/include/jscript.h"
#include "core/include/memcheck.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/xp.h"
#include "class/clb097.h"  // MAXWELL_A
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "device/interface/pcie.h"
#include "gpu/include/dmawrap.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "gpu/utility/gpuutils.h"
#include "gpu/utility/pcie/pexdev.h"
#include "gpu/utility/surf2d.h"
#include "maxwell/gm107/dev_lw_xve.h"
#include "sim/IChip.h"

#include <limits.h>
#include <string> // Only use <> for built-in C++ stuff

// Number of loops to test PEX utilization percentage
#define PCIE_UTIL_TEST_LOOPS 10
#define INTERVAL_IDLE_TIME   20
#define TIMEOUT_MS           5000
#define SURFACE_HEIGHT       64

class HostPerfMonTest : public RmTest
{
public:
    HostPerfMonTest();
    virtual ~HostPerfMonTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(tolerance, UINT32);

private:
    //! \brief Initializes the source buffer.
    RC InitSrcBuffer();

    //! \brief Set's up  surface buffers
    RC SetupSurfaceBuffer();

    //! \brief Enables the backdoor access
    void EnableBackdoor() const;
    //! \brief Disable the backdoor access
    void DisableBackdoor() const;

    RC TestAtPexSpeed(Pci::PcieLinkSpeed linkSpeed);
    RC VerifyPexUtilRateAtSpeed(Pci::PcieLinkSpeed linkSpeed, bool downstream, bool upstream);
    //! \brief Generate Traffic on PCIe bus
    RC GenTrafficOnPCIEBus(bool downstream, bool upstream, UINT32 &timerUs);
    //! \brief Get PEX utilizatoin sampling result
    RC GetLwrrentPCIEUtilPerc(UINT32 &perc);

    LwRm::Handle   m_hBasicSubDev;
    GpuSubdevice  *m_pSubdevice;
    Perf          *m_pPerf;

    enum surfIds{
        surfSys,
        surfFb,
        surfNUM
    };

    Surface2D  m_surfBuf[surfNUM]; // Data to use in the src surface.
    UINT32     m_srcSize;  // Source buffer size from user input.
    DmaWrapper m_DmaWrap;
    DmaWrapper m_DmaWrap2;
    UINT32     m_DmaMethod;
    // Size of bursts to pass to MODS, the test will do srcSize/burstSize reads/writes
    UINT32 m_burstNumber;
    // Time to yield between bursts, 0 disables yields. Default is for yield to be off.
    UINT32 m_burstYieldTime;
    // Utilization percentage tolerance.
    UINT32 m_tolerance;
    // Stored mods perf setting
    Perf::PerfPoint m_perfPoint;
};

//! \brief Initialize subdevice pointers for conmicate with RM
//!
//! \sa Setup
//------------------------------------------------------------------------------
HostPerfMonTest::HostPerfMonTest() :
    m_srcSize(50 * 1000 * 1024),
    m_DmaMethod(DmaWrapper::COPY_ENGINE),
    m_burstNumber(50),
    m_burstYieldTime(0),
    m_tolerance(10)
{
    SetName("HostPerfMonTest");
    LwRmPtr pLwRm;

    m_pSubdevice   = GetBoundGpuSubdevice();
    m_pPerf        = m_pSubdevice->GetPerf();
    m_hBasicSubDev = pLwRm->GetSubdeviceHandle(m_pSubdevice);
}

//! \brief Clean up allocated resources for this test.
//! \sa Cleanup
//------------------------------------------------------------------------------
HostPerfMonTest::~HostPerfMonTest()
{
    Cleanup();
}

//! \brief Check whether we can run this test in current elwiroment.
//!        This test is added for new PEX Utilization registers in Maxwell.
//!        We need to have a vbios has at least two pstates to support this
//!        test.
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         skip reason otherwise
string HostPerfMonTest::IsTestSupported()
{
    LwU32 regVal;
    Gpu::LwDeviceId chipName = GetBoundGpuSubdevice()->DeviceId();
    if (!m_pSubdevice)
        m_pSubdevice = GetBoundGpuSubdevice();
    if (!m_pPerf)
        m_pPerf = m_pSubdevice->GetPerf();

    if (Platform::GetSimulationMode() == Platform::Fmodel ||
       Platform::GetSimulationMode() == Platform::RTL)
    {
        return "Host PerfMon Test does not support Fmodel and RTL";
    }

    if (m_pSubdevice->IsEmulation())
    {
        return "Host PerfMon Test does not support Emulaion.";
    }

    if (!IsMAXWELLorBetter(chipName))
        return "HostPerfMon Test is only supported for Maxwell+";

    // Check whether PEX utilization counters are enabled or not.
    regVal = m_pSubdevice->RegRd32( DEVICE_BASE(LW_PCFG)| LW_XVE_PCIE_UTIL_CTRL);
    if (FLD_TEST_DRF_NUM(_XVE, _PCIE_UTIL_CTRL, _ENABLE, 0, regVal))
        return "PEX utiliztion counter is not enabled";

    return RUN_RMTEST_TRUE;
}

//! \brief Setup Source buffer and target buffer
//!
//!         Setup will allocation some memory in sysmem as source buffer. The
//!         Size of buffer is input by user using srcSize parameter. Source
//!         buffere will be allocated using random numbers. Then it will
//!         allocate target buffer in FB.
//!
//! \return OK if the tests passed, specific RC if failed
RC HostPerfMonTest::Setup()
{
    RC rc;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    Printf(Tee::PriHigh, "HostPerfMonTest: Setup() starts\n");

    // Set up the surface buffers.
    CHECK_RC(SetupSurfaceBuffer());
    CHECK_RC(InitSrcBuffer());

    // Turn off backdoor accesses
    DisableBackdoor();

    Printf(Tee::PriHigh, "HostPerfMonTest: Setup() done succesfully\n");

    return rc;
}

//! \brief Run Function
//!        1. Generate traffic on PCIe bus from both direction.
//!        2. Read and average the PMU perfmon sampling result.
//!        3. Check whether this result is in the target range given by user.
//!        4. Repeat above steps for PCIE_UTIL_TEST_LOOPS times.
//!
//! \return OK if the tests passed, specific RC if failed
RC HostPerfMonTest::Run()
{
    RC rc;
    UINT32 lwrrPState;
    UINT32 maxSpeed = GetBoundGpuSubdevice()->GetInterface<Pcie>()->LinkSpeedCapability();
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();

    Printf(Tee::PriHigh, "HostPerfMonTest: Run() starts\n");
    if (OK == pPerf->GetLwrrentPState(&lwrrPState))
    {
        UINT32 lwrMaxSpeed = 0;
        CHECK_RC(pPerf->GetPexSpeedAtPState(lwrrPState, &lwrMaxSpeed));
        maxSpeed = min(maxSpeed, lwrMaxSpeed);
    }

    CHECK_RC(TestAtPexSpeed(Pci::Speed2500MBPS));

    if (maxSpeed >= Pci::Speed5000MBPS)
    {
        CHECK_RC(TestAtPexSpeed(Pci::Speed5000MBPS));
    }

    if (maxSpeed >= Pci::Speed8000MBPS)
    {
        CHECK_RC(TestAtPexSpeed(Pci::Speed8000MBPS));
    }
    Printf(Tee::PriHigh, "HostPerfMonTest: Run() ends\n");
    return rc;
}

RC HostPerfMonTest::TestAtPexSpeed(Pci::PcieLinkSpeed linkSpeed)
{
    RC rc;
    string linkSpeedStr = "UNKNOWN";
    switch (linkSpeed)
    {
        case Pci::Speed2500MBPS:
            linkSpeedStr = "Gen 1";
            break;
        case Pci::Speed5000MBPS:
            linkSpeedStr = "Gen 2";
            break;
        case Pci::Speed8000MBPS:
            linkSpeedStr = "Gen 3";
            break;
        default:
            MASSERT(!"Unknown PEX Speed\n");
            break;
    }
    Printf(Tee::PriHigh, "HostPerfMonTest: Testing at Link speed %s\n", linkSpeedStr.c_str());
    CHECK_RC(GetBoundGpuSubdevice()->GetInterface<Pcie>()->SetLinkSpeed(linkSpeed));
    for (UINT32 index = 0; index < PCIE_UTIL_TEST_LOOPS; index++ )
    {
        m_burstYieldTime += 10;
        Printf(Tee::PriHigh, "HostPerfMonTest: Set: %d\n", index);
        Printf(Tee::PriHigh, "---------------------------------------------------\n");
        // Test upstream.
        Printf(Tee::PriHigh, "HostPerfMonTest: Upstream Only\n");
        CHECK_RC(VerifyPexUtilRateAtSpeed(linkSpeed, false, true));
        // Test downstream.
        Printf(Tee::PriHigh, "HostPerfMonTest: Downstream Only\n");
        CHECK_RC(VerifyPexUtilRateAtSpeed(linkSpeed, true, false));
        // Test both direction.
        //Printf(Tee::PriHigh, "HostPerfMonTest: Both Direction\n");
        //CHECK_RC(VerifyPexUtilRateAtSpeed(linkSpeed, true, true));
    }
    m_burstYieldTime = 0;
    return rc;
}

RC HostPerfMonTest::VerifyPexUtilRateAtSpeed(Pci::PcieLinkSpeed linkSpeed, bool downstream, bool upstream)
{
    RC rc;
    const FLOAT64 bandWidthKBPerUs = (FLOAT64)linkSpeed *
        GetBoundGpuSubdevice()->GetInterface<Pcie>()->GetLinkWidth() / (8.0 * 1000.0);

    UINT64  bytesTransfered =
        static_cast<UINT64>(m_surfBuf[surfSys].GetPitch()) *
        m_surfBuf[surfSys].GetWidth() * m_burstNumber;
    FLOAT64 utilRate = 0;
    UINT32  timeUs = 0;
    UINT32  samplingResult = 0;

    if (linkSpeed <= Pci::Speed5000MBPS)
    {
        bytesTransfered = bytesTransfered * 10 / 8; // encoding rate 8b/10b.
    }
    else
    {
        bytesTransfered = bytesTransfered * 130 / 128; // encoding rate 128b/130b.
    }

    bytesTransfered += bytesTransfered / 20; // 5% overhead.

    GenTrafficOnPCIEBus(downstream, upstream, timeUs);
    GetLwrrentPCIEUtilPerc(samplingResult);

    utilRate = (bytesTransfered / (10 * timeUs)) / bandWidthKBPerUs;
    Printf(Tee::PriHigh, " %llu bytes transfered over %u (us)\n",
           bytesTransfered, timeUs);
    Printf(Tee::PriHigh, " Bandwidth: %.2f GB/s, Utilization Rate: %%%.2f, Sampling Result: %%%u\n",
           bandWidthKBPerUs, utilRate, samplingResult);
    if ((samplingResult < (utilRate > m_tolerance ? utilRate - m_tolerance : 0)) ||
         (samplingResult > utilRate + m_tolerance))
    {
        Printf(Tee::PriHigh, "HostPerfMonTest: PEX utilization sampling result is out of range\n");
        return RC::SOFTWARE_ERROR;
    }
    return rc;
}

//! \brief Generate traffic on PCIe bus
//!
//! \param downstream: system to FB surface
//! \param upstrean: FB surface to FB
//!
//! \return OK, specific RC code otherwise.
RC HostPerfMonTest::GenTrafficOnPCIEBus(bool downstream, bool upstream, UINT32 &timerUs)
{
    RC     rc;
    const UINT32 SubdevInst = GetBoundGpuSubdevice()->GetSubdeviceInst();

    timerUs = static_cast<UINT32>(Xp::GetWallTimeUS());
    for (UINT32 i = 0 ; i < m_burstNumber ; ++i)
    {
        Tasker::Sleep(m_burstYieldTime);
        if (downstream)
        {
            m_DmaWrap.SetSurfaces(&m_surfBuf[surfSys], &m_surfBuf[surfFb]);
            CHECK_RC(m_DmaWrap.Copy(
                         0,                        // Starting src X, in bytes not pixels.
                         0,                        // Starting src Y, in lines.
                         m_surfBuf[surfSys].GetPitch(), // Width of copied rect, in bytes.
                         m_surfBuf[surfSys].GetHeight(),// Height of copied rect, in bytes
                         0,                        // Starting dst X, in bytes not pixels.
                         0,                        // Starting dst Y, in lines.
                         TIMEOUT_MS,
                         SubdevInst,
                         false));
        }
        if (upstream)
        {
            m_DmaWrap2.SetSurfaces(&m_surfBuf[surfFb], &m_surfBuf[surfSys]);
            CHECK_RC(m_DmaWrap2.Copy(
                         0,                        // Starting src X, in bytes not pixels.
                         0,                        // Starting src Y, in lines.
                         m_surfBuf[surfFb].GetPitch(),  // Width of copied rect, in bytes.
                         m_surfBuf[surfFb].GetHeight(),// Height of copied rect, in bytes
                         0,                        // Starting dst X, in bytes not pixels.
                         0,                        // Starting dst Y, in lines.
                         TIMEOUT_MS,
                         SubdevInst,
                         false));
        }
        CHECK_RC(m_DmaWrap.Wait(TIMEOUT_MS));
        CHECK_RC(m_DmaWrap2.Wait(TIMEOUT_MS));
    }
    timerUs = static_cast<UINT32>(Xp::GetWallTimeUS() - timerUs);
    return rc;
}

//! \brief Get current last sampling reasult of PEX utilization
//!
//! \param [out] percentage : last sampling reasult of PEX utilization
//! \return OK, specific RC code otherwise
RC HostPerfMonTest::GetLwrrentPCIEUtilPerc(UINT32 &percentage)
{
    RC rc;
    LwRmPtr pLwRm;
    LW2080_CTRL_PERF_PERFMON_SAMPLE utilSample = {0};
    LW2080_CTRL_PERF_GET_PERFMON_SAMPLE_PARAMS getSamplePara = {0};

    utilSample.clkDomain = LW2080_CTRL_CLK_DOMAIN_HOSTCLK;
    getSamplePara.clkSamplesListSize = 1;
    getSamplePara.clkSamplesList = (LwP64)&utilSample;

    // Reset all counters.
    CHECK_RC(pLwRm->Control(m_hBasicSubDev,
                           LW2080_CTRL_CMD_PERF_GET_PERFMON_SAMPLE,
                           &getSamplePara,
                           sizeof(getSamplePara)));

    percentage = utilSample.clkPercentBusy;
    return rc;
}

//! \brief Clean up allocate buffer and restore perfmon threshold setting.
//!
//! \return OK if the tests passed, specific RC if failed
RC HostPerfMonTest::Cleanup()
{
    RC rc;
    Printf(Tee::PriHigh, "HostPerfMonTest: Cleanup() starts\n");

    // Free the surface and objects we used for drawing
    m_DmaWrap.Wait(TIMEOUT_MS);
    m_DmaWrap.Cleanup();
    m_DmaWrap2.Wait(TIMEOUT_MS);
    m_DmaWrap2.Cleanup();
    m_surfBuf[surfFb].Free();
    m_surfBuf[surfSys].Free();

    // Turn back on the backdoor post read/write
    EnableBackdoor();

    Printf(Tee::PriHigh, "HostPerfMonTest: Cleanup() done.\n");

    return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------
//! \brief Set's up the Surface Buffers
RC HostPerfMonTest::SetupSurfaceBuffer()
{
    RC rc;

    Printf(Tee::PriHigh, "HostPerfMonTest: Setting up surface buffers of size %d KB.\n", m_srcSize/1024);

    for (int i=0; i<surfNUM; i++)
    {
        switch (i){
            case surfSys: m_surfBuf[i].SetLocation(Memory::Coherent); break;
            case surfFb : m_surfBuf[i].SetLocation(Memory::Fb); break;
        }
        m_surfBuf[i].SetWidth(m_srcSize/(SURFACE_HEIGHT * 4));
        m_surfBuf[i].SetHeight(SURFACE_HEIGHT);
        m_surfBuf[i].SetColorFormat(ColorUtils::A8R8G8B8);
        m_surfBuf[i].SetProtect(Memory::ReadWrite);
        m_surfBuf[i].SetLayout(Surface::Pitch);
        CHECK_RC(m_surfBuf[i].Alloc(GetBoundGpuDevice()));
        CHECK_RC(m_surfBuf[i].Map(GetBoundGpuSubdevice()->GetSubdeviceInst()));
    }
    Printf(Tee::PriHigh, "HostPerfMonTest: Sys Surface Pitch: %u, Height: %u.\n",
           m_surfBuf[surfSys].GetPitch(), m_surfBuf[surfSys].GetHeight());
    Printf(Tee::PriHigh, "HostPerfMonTest: Fb Surface Pitch: %u, Height: %u.\n",
           m_surfBuf[surfFb].GetPitch(), m_surfBuf[surfFb].GetHeight());

    // Initialize DMA copy wrapper.
    DmaCopyAlloc alloc;
    if ((DmaWrapper::COPY_ENGINE == m_DmaMethod) &&
       !(alloc.IsSupported(GetBoundGpuDevice())))
    {
        Printf(Tee::PriLow, "Copy Engine not supported. Using TwoD.\n");
        m_DmaMethod = DmaWrapper::TWOD;
    }

    CHECK_RC(m_DmaWrap.Initialize(GetTestConfiguration(),
                                  m_TestConfig.NotifierLocation(),
                                  (DmaWrapper::PREF_TRANS_METH)m_DmaMethod));

    CHECK_RC(m_DmaWrap2.Initialize(GetTestConfiguration(),
                                  m_TestConfig.NotifierLocation(),
                                  (DmaWrapper::PREF_TRANS_METH)m_DmaMethod));

    return OK;
}

// Initialize the src sufrace
RC HostPerfMonTest::InitSrcBuffer()
{
    RC rc;
    UINT32 seed;

    seed = GpuUtility::GetLwTimerMs(m_pSubdevice);
    CHECK_RC(Memory::FillRandom(m_surfBuf[surfSys].GetAddress(),
                                 seed,
                                 INT_MIN,
                                 INT_MAX,
                                 m_surfBuf[surfSys].GetWidth(),
                                 m_surfBuf[surfSys].GetHeight(),
                                 m_surfBuf[surfSys].GetBitsPerPixel(),
                                 m_surfBuf[surfSys].GetPitch()));

    return rc;
}

//! \brief Enables the backdoor access
void HostPerfMonTest::EnableBackdoor() const
{

    // Enable backdoor access.
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 1);
    }
}

//! \brief Disable the backdoor access
void HostPerfMonTest::DisableBackdoor() const
{
    //
    // Disable simulation backdoors by writing 0 to the 32-bit fields (4 bytes).
    // Note that if simulation backdoors are enabled, access to FB bypass L2C.
    // This cirlwmvents FB ECC since FB ECC logic is in the interface between FB
    // and L2C.
    //
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
    }
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
JS_CLASS_INHERIT(HostPerfMonTest, RmTest,
    "RM Test to test PCIE utilization sampling result");

CLASS_PROP_READWRITE(HostPerfMonTest, tolerance, UINT32, "Utilization percentage tolerance");
