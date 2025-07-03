/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_emp.cpp
//! \brief Empmain incarnation based on RM Test Framework
//!
//! Out of maintenance cost reason, instead of keeping its relation loosely coupled to RM
//! and risking broken compatibility with interface/structure changes from within RM,
//! EMpmain is rewritten using RM Test Framework.

#include <cassert>

#include "gpu/tests/rmtest.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include <string> // Only use <> for built-in C++ stuff
#include "core/include/memcheck.h"

#include "core/include/memtypes.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "core/include/platform.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/color.h"
#include "gpu/utility/surf2d.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/dmawrap.h"
#include "gpu/tests/rm/clk/uct/uctpstate.h"
#include "gpu/tests/rm/utility/clk/clkbase.h"

#include "gpu/utility/semawrap.h"
#include "core/include/utility.h"
#include "gpu/utility/surfrdwr.h"

#include "core/include/xp.h"
#include "core/include/cmdline.h"

#include "class/cla0b5.h"   // KEPLER_DMA_COPY_A
#include "class/clb0b5.h"   // MAXWELL_DMA_COPY_A
#include "class/clc0b5.h"   // PASCAL_DMA_COPY_A
#include "ctrl/ctrl0000.h"  // LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS

//#include "maxwell/gm108/dev_mmu.h"
#include "turing/tu102/dev_mmu.h"

#ifdef LW_WINDOWS
#include <math.h>
#include <windows.h>
#else
#include <cmath>
#endif
#include <sys/timeb.h>

#define MAX_NUM_DEVICES             4
#define GP10X_P2P_COPY_THRESHOLD    44

// to map a surface as cpu visible, it has to be fitted into BAR1 space. Mapping it piece by piece to save BAR1 space.
#define MAP_REGION_SIZE             (2*1024*1024)

typedef enum
{
    ST_None = 0,
    ST_Compressed = 1,
    ST_BlockLinear = 2,
    // handy combinations:
    ST_CompressedBlockLinear = ST_Compressed | ST_BlockLinear,
    ST_ZBuffer = 4,
    ST_RT = 8,
}SurfaceType;

typedef enum
{
    SemError = 0,
    SemBegin = 1,
    SemEnd = 2,
    SemNotify = 3,
    SemMax = 16,
} SemaphoreType;

typedef struct _stressAttrib
{
    SurfaceType surfType;
    const char* pteKind;
    UINT32 aaSamples;
    ColorUtils::Format format;
    const char* name;
} StressAttrib;

// About PTE kinds,please refer to
// C:\p4\sw\dev\gpu_drv\bugfix_main\drivers\common\inc\hwref\*\*\dev_mmu.h
// and \\hw\kepler1_gk104\bin\gen_kind_macros.pl for a little more info.
// It is important to have matched PTE kind with the surface type
// during P2P copy using 2D engine.
// Without giving a right PTE kind, there would be graphics exceptions
// complaining about PTE kind mismatch with the layout specified in
// the texture header.
StressAttrib stressAttribs[] =
{
    //  Surface type                PTE kind                    AA samples
    //      Format                          Name
    { ST_None,						"PITCH",                        1,
    ColorUtils::R5G6B5,             "pitch-2BPP" },
    { ST_None,						"PITCH",                        1,
    ColorUtils::A8R8G8B8,           "pitch-4BPP" },
    { ST_None,					    "PITCH",                        1,
    ColorUtils::R16_G16_B16_A16,    "pitch-8BPP" },
    { ST_RT,                        "GENERIC_16BX2",                1,
    ColorUtils::R5G6B5,             "RT-2BPP" }, 
    { ST_RT,                        "GENERIC_16BX2",                1,
    ColorUtils::A8R8G8B8,           "RT-4BPP" },
    { ST_RT,                        "GENERIC_16BX2",                1,
    ColorUtils::R16_G16_B16_A16,    "RT-8BPP" },
    { ST_BlockLinear,               "GENERIC_16BX2",                1,
    ColorUtils::A8R8G8B8,           "BL-4BPP" },
    { ST_CompressedBlockLinear,     "GENERIC_16BX2",                1,
    ColorUtils::A8R8G8B8,           "BL-comp-4BPP" },
    { ST_ZBuffer,                   "Z16",                          1,
    ColorUtils::Z16,                "Z16" },
    { ST_ZBuffer,                   "Z24S8",                        1,
    ColorUtils::Z24S8,              "Z24S8" },
    { ST_ZBuffer,                   "ZF32",                         1,
    ColorUtils::ZF32,               "ZF32" },
    { ST_ZBuffer,                   "Z16",                          8,
    ColorUtils::Z16,                "Z16-8xAA" },
    { ST_RT,                        "GENERIC_16BX2",                8,
    ColorUtils::A8R8G8B8,           "RT-4BPP-8xAA" },
    { ST_ZBuffer,                   "Z24S8",                        8,
    ColorUtils::X8Z24,              "Z24X8-8xAA" }
};

class EmpTest : public RmTest
{
public:
    EmpTest();
    virtual ~EmpTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(TestNum, UINT32);
    void    EnableEmpMainStyleArgParsing(bool bEnable);

protected:
    virtual RC SetupInternal();
    virtual RC CleanupInternal();

private:
    GpuDevMgr       *m_pGpuDevMgr;

    GpuDevice       *m_pGpuDevices[MAX_NUM_DEVICES];
    DmaWrapper      m_DmaWrappers[MAX_NUM_DEVICES];
    Channel         *m_pChannels[MAX_NUM_DEVICES];
    LwRm::Handle    m_hChannels[MAX_NUM_DEVICES];

    Surface2D       m_Semaphores[MAX_NUM_DEVICES];
    UINT32          m_FreeHeapSize;

    bool    m_bCompareResults;

    bool    m_bP2P;
    bool    m_bP2PReadsSupported;
    bool    m_bP2PWritesSupported;
    bool    m_bP2PROPSupported;
    bool    m_bP2PLwlinkSupported;
    UINT32  m_p2pThreshold;
    UINT32  m_NumDevices;

    DmaWrapper::PREF_TRANS_METH m_TransferMethod;

    bool    m_bAllowEmpMainStyleArgs; // whether to enable EmpMain style command line parsing
    UINT32  m_TestNum;
    UINT32  m_Dev;            // the argument specifying which device to test
    UINT32  m_VerboseLevel;   // the level of verbose messages
    UINT32  m_Buffers;        // the number of buffers
    UINT32  m_BufferSize;     // the size of buffers
    UINT32  m_Pitch;
    UINT32  m_Width;
    UINT32  m_Height;
    ColorUtils::Format  m_Format;
    UINT32  m_AaSamples;
    UINT32  m_ZsPacking;
    bool    m_bPreserveClk;
    bool    m_bShowClk;
    bool    m_bShowPState;
    UINT32  m_bP2PRead;
    UINT32  m_bStress;
    UINT32  m_bRendertarget;
    UINT32  m_bBlockLinear;
    UINT32  m_bZBuffer;
    UINT32  m_bCompressed;

    void    ParseArgs();

    UINT32  GetNumDevices();
    bool    IsP2pEnabled(GpuSubdevice *pSubdev1, GpuSubdevice *pSubdev2);
    bool    IsDmaWrapperSupported(GpuDevice *pDev);
    void    UpdateFreeHeapSize();

    void    BoostPState();
    void    ClearForcedPState();
    void    PrintLwrrentPState();
    void    PrintIndividualClocks();

    RC      WriteNotification(Channel *pSemWrap, UINT32 DeviceIndex, SemaphoreType semaphore, UINT32 payload);
    RC      GetNsTime(UINT64 *pTime, UINT32 *pReadback, UINT32 DeviceIndex);
    RC      GetTime(double *pTime, UINT32 *pReadback, UINT32 DeviceIndex);

    RC      AllocSemaphores(GpuDevice *pGpuDevice, Surface2D *pSemaphores);
    RC      AllocDMABuffer(UINT32 Device, Surface2D *pSurf, Memory::Location location, UINT32 dwBlockSize, Memory::Protect protection, StressAttrib *pAttrs, UINT32 value, bool bIgnorePteKind = true);

    RC      DMABandWidthOnAllGpus(Memory::Location srcLocation, Memory::Location dstLocation, UINT32 dwTimes, UINT32 dwBlockSize, StressAttrib *pAttrs, const char* pExtraInfo = "");
    RC      DMABandWidth(UINT32 Device, Memory::Location srcLocation, Memory::Location dstLocation, UINT32 dwTimes, UINT32 dwBlockSize, StressAttrib *pAttrs, const char* pExtraInfo = "");

    UINT64  MTCPUFill(UINT32 nThreads, UINT32 dwAffinityMask, UINT32 dwTimes, UINT32 dwSize, UINT32 *p, const char *pCaption);
    RC      CpuFill(GpuDevice *pGpuDevice, UINT32 dwThreads, UINT32 dwAffinity, UINT32 dwBlockSize, UINT32 dwTimes, StressAttrib *pAttrs, const char* pExtraInfo);

    RC      P2PSpeed(UINT32 dwBlockSize, UINT32 dwTimes, StressAttrib *pAttrs, const char* pExtraInfo = "");              // test 20
    RC      CrossP2PSpeed(UINT32 dwBlockSize, UINT32 dwTimes, StressAttrib *pAttrs, const char* pExtraInfo = "");         // test 201
    RC      TestP2PCorrectness(UINT32 dwBlockSize, UINT32 dwTimes, StressAttrib *pAttrs, const char* pExtraInfo = "");    // test 203
};

//! Compare memory of some range
static bool memdiff(UINT32 *pFirst, UINT32 *pSecond, UINT32 size)
{
    UINT32 i = 0, *p1, *p2;

    for (p1 = pFirst, p2 = pSecond; i < size; p1++, p2++)
    {
        UINT32 v1 = MEM_RD32(p1);
        UINT32 v2 = MEM_RD32(p2);

        if (v1 != v2)
        {
            Printf(Tee::PriHigh, "Surface2D mapping mismatched at 0x%0x%0x !\n",
                (UINT32)((UINT64)p1 >> 32), (UINT32)((UINT64)p1 & 0xffffffff));
            return true;
        }
        i += 4;
    }
    return false;
}

//! minimum value function
UINT64 min(UINT64 a, UINT64 b)
{
    return (a > b) ? b : a;
}

//! \brief Constructor of EmpTest
//------------------------------------------------------------------------------
EmpTest::EmpTest() :
    m_pGpuDevMgr(nullptr),
    m_FreeHeapSize(0),
    m_bCompareResults(false),
    m_bP2P(false),
    m_bP2PReadsSupported(false),
    m_bP2PWritesSupported(false),
    m_p2pThreshold(0),
    m_TransferMethod(DmaWrapper::COPY_ENGINE),
    m_bAllowEmpMainStyleArgs(false),
    m_TestNum(0),
    m_Dev(0xffffffff),
    m_VerboseLevel(0),
    m_Buffers(0),
    m_BufferSize(0),
    m_Pitch(0),
    m_Width(0),
    m_Height(0),
    m_Format(ColorUtils::LWFMT_NONE),
    m_AaSamples(0),
    m_ZsPacking(0),
    m_bPreserveClk(false),
    m_bShowClk(false),
    m_bShowPState(false),
    m_bP2PRead(false),
    m_bStress(false),
    m_bRendertarget(false),
    m_bBlockLinear(false),
    m_bZBuffer(false),
    m_bCompressed(false)
{
    memset(&m_pGpuDevices[0], 0, sizeof(m_pGpuDevices));

    SetName("EmpTest");
}

//! \brief Destructor of EmpTest
//------------------------------------------------------------------------------
EmpTest::~EmpTest()
{
}

//! Enable/disable Empmain style command line parsing
void EmpTest::EnableEmpMainStyleArgParsing(bool bEnable)
{
    m_bAllowEmpMainStyleArgs = bEnable;
}

//! Parse the command line in empmain fashion, to guarantee the command line compatibility
//!------------------------------------------------------------------------------
void EmpTest::ParseArgs()
{
    if (!m_bAllowEmpMainStyleArgs)
    {
        // So EmpTest is running from rmtest.js.
        // In such case, we will perform default tests in stress mode.
        // By saying default, it means test case 2, 3, 4 & 5 for every gpu,
        // and 20, 201 & 203 for multi-adapter platforms.
        m_bStress = true;
        return;
    }

    char ArgBuff[1024] = { 0 };
    char *Args[16] = { 0 };
    m_VerboseLevel = m_TestConfig.Verbose() ? 1 : 0;

    // Retrieve the command line arguments
    CommandLine::ArgDb()->ListAllArgs(&ArgBuff[0], sizeof(ArgBuff));

    // Parse the arguments
    char *pArg = &ArgBuff[0];
    UINT32 iArg = 0;

    while (*pArg && iArg<(sizeof(Args) / sizeof(Args[0])))
    {
        Args[iArg++] = pArg;
        pArg = strstr(pArg, " ");

        if (!pArg)
        {
            break;
        }

        *pArg = '\0';
        pArg++;
    }

    if (iArg == 0)
    {
        return;
    }

    // Process the arguments
    UINT32 iArgCount = iArg;
    iArg = 0;

    m_TestNum = atoi(Args[0]);
    if (m_TestNum)
    {
        iArg++;
    }

    while (iArg < iArgCount)
    {
        if (!stricmp("-h", Args[iArg]))
        {
            iArg++;
            if (iArg < iArgCount)
            {
                m_Dev = atoi(Args[iArg]);
            }
        }
        else
        if (!stricmp("-vv", Args[iArg]))
        {
            m_VerboseLevel = 2;
        }
        else
        if (!stricmp("-v", Args[iArg]))
        {
            m_VerboseLevel = 1;
        }
        else
        if (!stricmp("-count", Args[iArg]) || !stricmp("-buffers", Args[iArg]))
        {
            iArg++;
            if (iArg < iArgCount)
            {
                m_Buffers = strtoul(Args[iArg], nullptr, 0);
            }
        }
        else
        if (!stricmp("-pitch", Args[iArg]))
        {
            iArg++;
            if (iArg < iArgCount)
            {
                m_Pitch = strtoul(Args[iArg], nullptr, 0);
            }
        }
        else
        if (!stricmp("-width", Args[iArg]))
        {
            iArg++;
            if (iArg < iArgCount)
            {
                m_Width = strtoul(Args[iArg], nullptr, 0);
            }
        }
        else
        if (!stricmp("-height", Args[iArg]))
        {
            iArg++;
            if (iArg < iArgCount)
            {
                m_Height = strtoul(Args[iArg], nullptr, 0);
            }
        }
        else
        if (!stricmp("-format", Args[iArg]))
        {
            iArg++;
            if (iArg < iArgCount)
            {
                m_Format = ColorUtils::StringToFormat(Args[iArg]);
            }
        }
        else
        if (!stricmp("-comp", Args[iArg]))
        {
            m_bCompressed = true;
        }
        else
        if (!stricmp("-zb", Args[iArg]))
        {
            m_bZBuffer = true;
        }
        else
        if (!stricmp("-rt", Args[iArg]))
        {
            m_bRendertarget = true;
        }
        else
        if (!stricmp("-bl", Args[iArg]))
        {
            m_bBlockLinear = true;
        }
        else
        if (!stricmp("-aaSamples", Args[iArg]))
        {
            iArg++;
            if (iArg < iArgCount)
            {
                m_AaSamples = atoi(Args[iArg]);
            }
        }
        else
        if (!stricmp("-zsPacking", Args[iArg]))
        {
            iArg++;
            if (iArg < iArgCount)
            {
                m_ZsPacking = atoi(Args[iArg]);
            }
        }
        else
        if (!stricmp("-useasync", Args[iArg]))
        {
            m_TransferMethod = DmaWrapper::COPY_ENGINE;
        }
        else
        if (!stricmp("-usetwod", Args[iArg]))
        {
            m_TransferMethod = DmaWrapper::TWOD;
        }
        else
        if (!stricmp("-buffersize", Args[iArg]))
        {
            iArg++;
            if (iArg < iArgCount)
            {
                m_BufferSize = strtoul(Args[iArg], nullptr, 0);
            }
        }
        else
        if (!stricmp("-preserveclk", Args[iArg]))
        {
            m_bPreserveClk = true;
        }
        else
        if (!stricmp("-showclk", Args[iArg]))
        {
            m_bShowClk = true;
        }
        else
        if (!stricmp("-showpstate", Args[iArg]))
        {
            m_bShowPState = true;
        }
        else
        if (!stricmp("-stress", Args[iArg]))
        {
            m_bStress = true;
        }
        else
        if (!stricmp("-p2pread", Args[iArg]))
        {
            m_bP2PRead = true;
        }
        else
        if (!stricmp("-p2pthreshold", Args[iArg]))
        {
            m_p2pThreshold = strtoul(Args[iArg], nullptr, 0);
        }
        else
        if (!stricmp("-testnum", Args[iArg]))
        {
            iArg++;
            if (iArg < iArgCount)
            {
                m_TestNum = atoi(Args[iArg]);
            }
        }

        iArg++;
    }
}

//! \brief GetFBInfo Function
//!
//! Generic FB info retrieve function
//!
//! \return any error returned by LW2080_CTRL_CMD_FB_GET_INFO rm control call
//------------------------------------------------------------------------------
RC GetFBInfo(GpuSubdevice *pSubdev, UINT32 index, UINT32 *pResult)
{
    RC rc = OK;

    // Obtain GPU's available BAR1 size
    LwRm *pLwRm0 = LwRmPtr(0).Get();

    LW2080_CTRL_FB_INFO fbInfo = { 0 };
    fbInfo.index = index;
    LW2080_CTRL_FB_GET_INFO_PARAMS params = { 1, LW_PTR_TO_LwP64(&fbInfo) };
    rc = pLwRm0->ControlBySubdevice(
        pSubdev,
        LW2080_CTRL_CMD_FB_GET_INFO,
        &params, sizeof(params));

    if (OK == rc)
    {
        *pResult = fbInfo.data;
    }
    else
    {
        *pResult = 0;
    }

    return rc;
}

//! \brief GpufreeHeapSize Function
//!
//! Callwlate free heap size right now
//!
//! \return any error returned by LW2080_CTRL_CMD_FB_GET_INFO rm control call
//------------------------------------------------------------------------------
RC GetFreeHeapSize(GpuSubdevice *pSubdev, UINT32 *pResult)
{
    // Obtain GPU's free heap size
    return GetFBInfo(pSubdev, LW2080_CTRL_FB_INFO_INDEX_HEAP_FREE, pResult);
}

//! \brief GetMaxContiguousAvailBar1Size Function
//!
//! Callwlate max contiguous available BAR1 size right now
//!
//! \return any error returned by LW2080_CTRL_CMD_FB_GET_INFO rm control call
//------------------------------------------------------------------------------
RC GetMaxContiguousAvailBar1Size(GpuSubdevice *pSubdev, UINT32 *pResult)
{
    // Obtain GPU's available BAR1 size
    return GetFBInfo(pSubdev, LW2080_CTRL_FB_INFO_INDEX_BAR1_MAX_CONTIGUOUS_AVAIL_SIZE, pResult);
}

//! \brief IsTestSupported Function
//!
//! Check whether this test is supported
//!
//!     1. DmaWrapper has to be supportd
//!     2. P2P has to be enabled
//!
//! \return RUN_RMTEST_TRUE if the test can be run in the current environment,
//!         skip reason otherwise
//------------------------------------------------------------------------------
string EmpTest::IsTestSupported()
{
    m_pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;

    m_NumDevices = 0;

    GpuSubdevice *pSubdev = nullptr;
    GpuDevice *pDev = m_pGpuDevMgr->GetFirstGpuDevice();
    while (pDev)
    {
        pSubdev = pDev->GetSubdevice(0);
        if (!IsDmaWrapperSupported(pDev) || !IsP2pEnabled(pSubdev, pSubdev))
        {
            return "Either DMAWrapper not supported, or P2P is not enabled";
        }

        m_NumDevices++;
        pDev = m_pGpuDevMgr->GetNextGpuDevice(pDev);
    }

    UpdateFreeHeapSize();

    return RUN_RMTEST_TRUE;
}

//! \brief UpdateFreeHeapSize Function
//!
//! Update m_FreeHeapSize by querying RM for free heap size
//!
//------------------------------------------------------------------------------
void EmpTest::UpdateFreeHeapSize()
{
    GpuSubdevice *pSubdev = nullptr;
    GpuDevice *pDev = m_pGpuDevMgr->GetFirstGpuDevice();
    while (pDev)
    {
        pSubdev = pDev->GetSubdevice(0);
        UINT32 freeHeapSize = 0;
        GetFreeHeapSize(pSubdev, &freeHeapSize);
        if (m_FreeHeapSize == 0 || m_FreeHeapSize > freeHeapSize)
        {
            m_FreeHeapSize = freeHeapSize;
        }

        for (UINT32 iSubDevice = 1;
            iSubDevice < pDev->GetNumSubdevices();
            iSubDevice++)
        {
            pSubdev = pDev->GetSubdevice(iSubDevice);
            GetFreeHeapSize(pSubdev, &freeHeapSize);

            if (m_FreeHeapSize == 0 || m_FreeHeapSize > freeHeapSize)
            {
                m_FreeHeapSize = freeHeapSize;
            }
        }
        pDev = m_pGpuDevMgr->GetNextGpuDevice(pDev);
    }

    return;
}

//! \brief Setup Function
//------------------------------------------------------------------------------
RC EmpTest::Setup()
{
    RC rc = OK;

    // Ask our base class(es) to setup internal state based on JS settings
    CHECK_RC(InitFromJs());

    ParseArgs();

    CHECK_RC(SetupInternal());

    return rc;
}

//! \brief SetupInternal Function
//------------------------------------------------------------------------------
RC EmpTest::SetupInternal()
{
    RC rc = OK;

    m_TestConfig.SetAllowMultipleChannels(true);

    UINT32 iDevice = 0;
    GpuDevice *pDevice = m_pGpuDevMgr->GetFirstGpuDevice();
    while (pDevice && iDevice<m_NumDevices)
    {
        m_TestConfig.BindGpuDevice(pDevice);
        CHECK_RC(m_TestConfig.AllocateChannel(&m_pChannels[iDevice], &m_hChannels[iDevice]));
        CHECK_RC(m_DmaWrappers[iDevice].Initialize(&m_TestConfig, m_pChannels[iDevice], Memory::Optimal, m_TransferMethod, DmaWrapper::DEFAULT_ENGINE_INSTANCE));

        CHECK_RC(AllocSemaphores(pDevice, &m_Semaphores[iDevice]));

        m_pGpuDevices[iDevice++] = pDevice;
        pDevice = m_pGpuDevMgr->GetNextGpuDevice(pDevice);
    }

    m_TestConfig.BindGpuDevice(m_pGpuDevMgr->GetFirstGpuDevice());

    if (!m_bPreserveClk)
    {
        BoostPState();
    }

    // GP10x has a problem with CE to handle a lot of P2P copies of blocklinear surfaces without flushing.
    // Setup a threshold, so that CE will block/flush for every m_p2pThreshold P2P copies.
    if (IsGP10X(m_pGpuDevices[0]->GetSubdevice(0)->DeviceId()) && m_p2pThreshold == 0)
    {
        m_p2pThreshold = GP10X_P2P_COPY_THRESHOLD;
    }

    return rc;
}

//! \brief Cleanup Function
//------------------------------------------------------------------------------
RC EmpTest::Cleanup()
{
    RC rc = OK;

    if (m_bAllowEmpMainStyleArgs)
    {
        CommandLine::ArgDb()->ClearArgs();
    }

    CHECK_RC(CleanupInternal());

    return rc;
}

//! \brief CleanupInternal Function
//------------------------------------------------------------------------------
RC EmpTest::CleanupInternal()
{
    if (!m_bPreserveClk)
    {
        ClearForcedPState();
    }

    for (UINT32 i = 0; i<MAX_NUM_DEVICES; i++)
    {
        if (m_Semaphores[i].IsAllocated())
        {
            m_Semaphores[i].Unmap();
            m_Semaphores[i].Free();
        }

        m_TestConfig.FreeChannel(m_pChannels[i]);
        m_DmaWrappers[i].Cleanup();
    }

    memset(&m_pGpuDevices[0], 0, sizeof(m_pGpuDevices));
    return OK;
}

//! \brief Run Function
//! \return OK if the tests passed, specific RC if failed
//------------------------------------------------------------------------------
RC EmpTest::Run()
{
    char *TransferMethod[] = { "3D Pipe M2M", "Async Copy Engine", "3D Pipe TWOD" };
    char *MappedTestName[] = { "Test 2", "Test 3", "Test 4", "Test 5" };
    Memory::Location SrcLocation[] =
    { Memory::Fb, Memory::Coherent, Memory::Coherent, Memory::Fb };
    Memory::Location DstLocation[] =
    { Memory::Coherent, Memory::Fb, Memory::Coherent, Memory::Fb };

    if (m_bShowPState)
    {
        PrintLwrrentPState();
    }

    if (m_bShowClk)
    {
        PrintIndividualClocks();
    }

    UpdateFreeHeapSize();

    // Exclude resources that will be created later from the free heap space
    const UINT32 dwReservedForEachDMABuffer = m_NumDevices * 0x360 * 1024;
    UINT32 dwHeapLimit = m_FreeHeapSize * 1024;
    UINT32 dwSingleBlockLimit = ((dwHeapLimit - dwReservedForEachDMABuffer) & 0x7ffc0000);
    UINT32 dwBlockSize = (m_BufferSize > 0) ? m_BufferSize : 8 * 1024 * 1024;
    UINT32 dwBlockSizeX2 = (m_BufferSize > 0) ? m_BufferSize : 8 * 1024 * 1024;
    UINT32 dwBlockSizeP2P = (m_BufferSize > 0) ? m_BufferSize : 8 * 1024 * 1024;
    UINT32 dwBlockSize203 = (m_BufferSize > 0) ? m_BufferSize : 8 * 1024 * 1024;

    // Do not allow a default block size larger than free heap
    if (dwBlockSize > dwSingleBlockLimit)
    {
        dwBlockSize = dwSingleBlockLimit;
    }

    // Do not allow a default block size larger than 1/2 of free heap
    if (dwBlockSizeX2 > ((dwSingleBlockLimit / 2) & 0x7ffc0000))
    {
        dwBlockSizeX2 = (dwSingleBlockLimit / 2) & 0x7ffc0000;
    }

    // Do not allow a default block size for test 20 and 201
    // larger than free heap, or they will fail
    if (dwBlockSizeP2P > ((dwSingleBlockLimit * 3 / 4) & 0x7ffc0000))
    {
        dwBlockSizeP2P = ((dwSingleBlockLimit * 3 / 4) & 0x7ffc0000);
    }

    // Do not allow a default block size for test 203
    // larger than 1/2 of free heap, or it will fail
    if (dwBlockSize203 > ((dwSingleBlockLimit / 4) & 0x7ffc0000))
    {
        dwBlockSize203 = ((dwSingleBlockLimit / 4) & 0x7ffc0000);
    }

    if (m_BufferSize > 0)
    {
        if ((m_TestNum == 0 ||
            (m_TestNum >= 2 && m_TestNum <= 4) ||
            (m_TestNum >= 600 && m_TestNum <= 603) ||
            (m_TestNum >= 610 && m_TestNum <= 613)) &&
            (m_BufferSize > dwBlockSize))
        {
            Printf(Tee::PriNormal,
                "Allocation size is capped at 0x%x KB for test 2, 3 and CPU fill tests\n",
                dwBlockSize / 1024);
        }

        if ((m_TestNum == 0 ||
            (m_TestNum >= 4 && m_TestNum <= 5)) &&
            (m_BufferSize > dwBlockSizeX2))
        {
            Printf(Tee::PriNormal,
                "Allocation size is capped at 0x%x KB for test 4 and 5\n",
                dwBlockSizeX2 / 1024);
        }

        // Do not allow a default block size for test 20 and 201
        // larger than 1/2 of free heap, or we may risk fail them
        if ((m_TestNum == 0 || m_TestNum == 20 || m_TestNum == 201) &&
            (m_BufferSize > dwBlockSizeP2P))
        {
            Printf(Tee::PriNormal,
                "Allocation size is capped at 0x%x KB for P2P speed tests\n",
                dwBlockSizeP2P / 1024);
        }

        // Do not allow a default block size for test 203
        // larger than 1/4 of free heap, or we may risk fail it
        if ((m_TestNum == 0 || m_TestNum == 203) &&
            (m_BufferSize > dwBlockSize203))
        {
            Printf(Tee::PriNormal,
                "Allocation size is capped at of 0x%x KB for test 203\n",
                dwBlockSize203 / 1024);
        }
    }

    RC rc = OK;
    UINT32 dwTimes = m_Buffers ? m_Buffers : 1000;

    StressAttrib attrs;
    StressAttrib *pAttrs = &attrs;

    memset(pAttrs, 0, sizeof(StressAttrib));
    attrs.format = (m_Format != ColorUtils::LWFMT_NONE) ? m_Format : ColorUtils::A8R8G8B8;
    attrs.aaSamples = m_AaSamples ? m_AaSamples : 1;
    attrs.surfType = SurfaceType(attrs.surfType | (m_bCompressed ? ST_Compressed : ST_None));
    attrs.surfType = SurfaceType(attrs.surfType | (m_bZBuffer ? ST_ZBuffer : ST_None));
    attrs.surfType = SurfaceType(attrs.surfType | (m_bRendertarget ? ST_RT : ST_None));
    attrs.surfType = SurfaceType(attrs.surfType | (m_bBlockLinear ? ST_BlockLinear : ST_None));
    attrs.pteKind = (attrs.surfType == ST_None) ? "PITCH" : "GENERIC_16BX2";

    attrs.name = "";

    if (m_TestNum == 0 || (m_TestNum >= 2 && m_TestNum <= 5))
    {
        for (UINT32 iTest = 0; iTest < sizeof(SrcLocation) / sizeof(SrcLocation[0]); iTest++)
        {
            if (m_TestNum != 0 && m_TestNum != (iTest + 2))
            {
                continue;
            }

            Printf(Tee::PriNormal, "Free heap size is 0x%x KB, "\
                "going to test allocation size of 0x%x KB\n",
                m_FreeHeapSize,
                ((SrcLocation[iTest] != DstLocation[iTest]) ?
                    dwBlockSize : dwBlockSizeX2) / 1024);

            Printf(Tee::PriNormal, "%s, %s\n", MappedTestName[iTest], TransferMethod[m_TransferMethod]);
            for (UINT32 iLoop = 0; iLoop < sizeof(stressAttribs) / sizeof(stressAttribs[0]); iLoop++)
            {
                if (!m_bStress && iLoop != 1)
                {
                    continue;
                }

                if (m_bStress)
                {
                    pAttrs = &stressAttribs[iLoop];
                }

                CHECK_RC(DMABandWidthOnAllGpus(SrcLocation[iTest],
                    DstLocation[iTest],
                    dwTimes,
                    (SrcLocation[iTest] != DstLocation[iTest]) ?
                    dwBlockSize : dwBlockSizeX2,
                    pAttrs,
                    pAttrs->name));
            }
        }
    }

    if (m_TestNum == 0)
    {
        CleanupInternal();
        SetupInternal();
    }

    if (m_TestNum == 6 || (m_TestNum >= 600 && m_TestNum <= 603) || (m_TestNum >= 610 && m_TestNum <= 613))
    {
        Printf(Tee::PriNormal, "Free heap size is 0x%x KB, "\
            "going to test allocation size of 0x%x KB\n",
            m_FreeHeapSize, dwBlockSize / 1024);

        UINT32 dwThreads = 1;
        UINT32 dwAffinity = 0;
        switch (m_TestNum)
        {
        case 600:
        case 601:
        case 602:
        case 603:
            dwAffinity = 1 << (m_TestNum - 600);
            break;
        case 610:
        case 611:
        case 612:
        case 613:
            dwThreads = (m_TestNum - 609) * 2;
            break;
        }

        UINT32 contBar1AvailSize = 0;
        GetMaxContiguousAvailBar1Size(m_pGpuDevices[0]->GetSubdevice(0), &contBar1AvailSize);

        UINT32 dwCpuBlockSize = dwBlockSize;
        if (contBar1AvailSize * 1024 < dwCpuBlockSize)
        {
            dwCpuBlockSize = contBar1AvailSize * 1024;

            Printf(Tee::PriNormal, "Max contiguous BAR1 available space is 0x%x KB,"\
                "going to test allocation size of 0x%x KB\n",
                contBar1AvailSize,
                dwCpuBlockSize / 1024);

        }

        Printf(Tee::PriNormal, "Test %d, CPU\n", m_TestNum);
        for (UINT32 iLoop = 0; iLoop < sizeof(stressAttribs) / sizeof(stressAttribs[0]); iLoop++)
        {
            if (!m_bStress && iLoop != 1)
            {
                continue;
            }

            if (m_bStress)
            {
                pAttrs = &stressAttribs[iLoop];
            }

            // CPU fill is slow, so looping only one time by default, if not specified otherwise
            CHECK_RC(CpuFill(m_pGpuDevices[0], dwThreads, dwAffinity, dwBlockSize, m_Buffers ? m_Buffers : 1, pAttrs, pAttrs->name));
        }
    }

    if (m_NumDevices > 1)
    {
        if (m_TestNum == 0 || m_TestNum == 20)
        {
            Printf(Tee::PriNormal, "Test 20, %s\n", TransferMethod[m_TransferMethod]);
            Printf(Tee::PriNormal, "Free heap size is 0x%x KB, "\
                "going to test allocation size of 0x%x KB\n",
                m_FreeHeapSize, dwBlockSizeP2P / 1024);
            for (UINT32 iLoop = 0; iLoop < sizeof(stressAttribs) / sizeof(stressAttribs[0]); iLoop++)
            {
                if (!m_bStress && iLoop != 1)
                {
                    continue;
                }

                if (m_bStress)
                {
                    pAttrs = &stressAttribs[iLoop];
                }

                // TWOD can't do P2P transfers of BL, RT, compressed or z buffers without setting pte kind correctly,
                // but LWOS32_FUNCTION_ALLOC_* used by Surface2D does not override pte kind when LW_VERIF_FEATURES
                // is not defined. Without properly setting pte kind, P2P copy with 2D engine ends up in
                // graphics exceptions of mismatched texture layout and pte kind like the below one
                if (m_TransferMethod == DmaWrapper::TWOD && ((pAttrs->surfType & (ST_BlockLinear | ST_Compressed | ST_ZBuffer | ST_RT))))
                {
                    // Pascal or later does not support P2P blocklinear copy with 2D engine at all.
                    if (IsPASCALorBetter(m_pGpuDevices[0]->GetSubdevice(0)->DeviceId()))
                    {
                        continue;
                    }

                    Surface2D test;

                    // AllocDMABuffer reports LWRM_NOT_SUPPORTED when PTE Kind mismatch is detected
                    if (RC::LWRM_NOT_SUPPORTED == AllocDMABuffer(0, &test, Memory::Fb, dwBlockSizeP2P, Memory::ReadWrite, pAttrs, 0x55555555, false))
                    {
                        continue;
                    }

                    test.Free();
                }

                CHECK_RC(P2PSpeed(dwBlockSizeP2P, dwTimes, pAttrs, pAttrs->name)); // test 20
            }
        }

        if (m_TestNum == 0)
        {
            CleanupInternal();
            SetupInternal();
        }

        if (m_TestNum == 0 || m_TestNum == 201)
        {
            Printf(Tee::PriNormal, "Test 201, %s\n", TransferMethod[m_TransferMethod]);
            Printf(Tee::PriNormal, "Free heap size is 0x%x KB, "\
                "going to test allocation size of 0x%x KB\n",
                m_FreeHeapSize, dwBlockSizeP2P / 1024);
            for (UINT32 iLoop = 0; iLoop < sizeof(stressAttribs) / sizeof(stressAttribs[0]); iLoop++)
            {
                if (!m_bStress && iLoop != 1)
                {
                    continue;
                }

                if (m_bStress)
                {
                    pAttrs = &stressAttribs[iLoop];
                }

                // TWOD can't do P2P transfers of BL, RT, compressed or z buffers without setting pte kind correctly,
                // but LWOS32_FUNCTION_ALLOC_* used by Surface2D does not override pte kind when LW_VERIF_FEATURES
                // is not defined. Without properly setting pte kind, P2P copy with 2D engine ends up in
                // graphics exceptions of mismatched texture layout and pte kind.
                if (m_TransferMethod == DmaWrapper::TWOD && ((pAttrs->surfType & (ST_BlockLinear | ST_Compressed | ST_ZBuffer | ST_RT))))
                {
                    // Pascal or later does not support P2P blocklinear copy with 2D engine at all.
                    if (IsPASCALorBetter(m_pGpuDevices[0]->GetSubdevice(0)->DeviceId()))
                    {
                        continue;
                    }

                    Surface2D test;

                    // AllocDMABuffer reports LWRM_NOT_SUPPORTED when PTE Kind mismatch is detected
                    if (RC::LWRM_NOT_SUPPORTED == AllocDMABuffer(0, &test, Memory::Fb, dwBlockSizeP2P, Memory::ReadWrite, pAttrs, 0x55555555, false))
                    {
                        continue;
                    }

                    test.Free();
                }

                CHECK_RC(CrossP2PSpeed(dwBlockSizeP2P, dwTimes, pAttrs, pAttrs->name));               // test 201
            }
        }

        if (m_TestNum == 0)
        {
            CleanupInternal();
            SetupInternal();
        }

        if (m_TestNum == 0 || m_TestNum == 203)
        {
            Printf(Tee::PriNormal, "Test 203, %s\n", TransferMethod[m_TransferMethod]);
            Printf(Tee::PriNormal, "Free heap size is 0x%x KB, "\
                "going to test allocation size of 0x%x KB\n",
                m_FreeHeapSize, dwBlockSize203 / 1024);
            for (UINT32 iLoop = 0; iLoop < sizeof(stressAttribs) / sizeof(stressAttribs[0]); iLoop++)
            {
                if (!m_bStress && iLoop != 1)
                {
                    continue;
                }

                if (m_bStress)
                {
                    pAttrs = &stressAttribs[iLoop];
                }

                // TWOD can't do P2P transfers of BL, RT, compressed or z buffers without setting pte kind correctly,
                // but LWOS32_FUNCTION_ALLOC_* used by Surface2D does not override pte kind when LW_VERIF_FEATURES
                // is not defined. Without properly setting pte kind, P2P copy with 2D engine ends up in
                // graphics exceptions of mismatched texture layout and pte kind.
                if (m_TransferMethod == DmaWrapper::TWOD && ((pAttrs->surfType & (ST_BlockLinear | ST_Compressed | ST_ZBuffer | ST_RT))))
                {
                    // Pascal or later does not support P2P blocklinear copy with 2D engine at all.
                    if (IsPASCALorBetter(m_pGpuDevices[0]->GetSubdevice(0)->DeviceId()))
                    {
                        continue;
                    }

                    Surface2D test;

                    // AllocDMABuffer reports LWRM_NOT_SUPPORTED when PTE Kind mismatch is detected
                    if (RC::LWRM_NOT_SUPPORTED == AllocDMABuffer(0, &test, Memory::Fb, dwBlockSize203, Memory::ReadWrite, pAttrs, 0x55555555, false))
                    {
                        continue;
                    }

                    test.Free();
                }

                CHECK_RC(TestP2PCorrectness(dwBlockSize203, dwTimes, pAttrs, pAttrs->name));          // test 203
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
// Private Member Functions
//------------------------------------------------------------------------------

//!
//! Allocate semaphores
//!-----------------------------------------------------------------------------
RC EmpTest::AllocSemaphores(GpuDevice *pGpuDevice, Surface2D *pSemaphores)
{
    RC rc = OK;
    LwRm *pLwRm = m_TestConfig.GetRmClient();

    // Allocate the buffer that holds the back-end semaphores
    pSemaphores->SetAddressModel(Memory::Paging);
    pSemaphores->SetLayout(Surface2D::Pitch);
    pSemaphores->SetWidth(SemMax * 16);
    pSemaphores->SetPitch(SemMax * 16);
    pSemaphores->SetHeight(m_NumDevices);
    pSemaphores->SetDepth(1);
    pSemaphores->SetColorFormat(ColorUtils::Y8);
    pSemaphores->SetLocation(Memory::Coherent);
    CHECK_RC(pSemaphores->Alloc(pGpuDevice, pLwRm));

    pSemaphores->Fill(0, 0);

    MASSERT(pSemaphores->GetSize() >= SemMax * 16);
    CHECK_RC(pSemaphores->Map(pGpuDevice->GetSubdevice(0)->GetSubdeviceInst()));

    return rc;
}

//!
//! Call DMABandWidth for every GPU device
//!-----------------------------------------------------------------------------
RC EmpTest::DMABandWidthOnAllGpus(Memory::Location srcLocation, Memory::Location dstLocation, UINT32 dwTimes, UINT32 dwBlockSize, StressAttrib *pAttrs, const char* pExtraInfo)
{
    RC rc = OK;
    for (UINT32 iDev = 0; iDev < m_NumDevices; iDev++)
    {
        // Continue if not the specified device
        if (m_Dev != 0xffffffff && iDev != m_Dev)
        {
            continue;
        }

        rc = DMABandWidth(iDev, srcLocation, dstLocation, dwTimes, dwBlockSize, pAttrs, pExtraInfo);

        if (rc != OK)
        {
            Printf(Tee::PriHigh, "DMABandWidth fails with error %s (0x%x)\n", rc.Message(), rc.Get());
            break;
        }
    }
    return rc;
}

//!
//! Get the number of devices in the machine.
//!-----------------------------------------------------------------------------
UINT32 EmpTest::GetNumDevices()
{
    LwRmPtr pLwRm;
    UINT32 NumDevices = 0;

    LW0000_CTRL_GPU_GET_ATTACHED_IDS_PARAMS gpuAttachedIdsParams = { { 0 } };
    pLwRm->Control(pLwRm->GetClientHandle(),
        LW0000_CTRL_CMD_GPU_GET_ATTACHED_IDS,
        &gpuAttachedIdsParams,
        sizeof(gpuAttachedIdsParams));

    // Count num devices and subdevices.
    for (UINT32 i = 0; i < LW0000_CTRL_GPU_MAX_ATTACHED_GPUS; i++)
    {
        if (gpuAttachedIdsParams.gpuIds[i] == GpuDevice::ILWALID_DEVICE)
            continue;
        NumDevices++;
    }

    return NumDevices;
}

//!
//! Checks for P2P capability in a Gpu
//!------------------------------------------------------------------------------
bool EmpTest::IsP2pEnabled(GpuSubdevice *pSubdev1, GpuSubdevice *pSubdev2)
{
    LwRmPtr pLwRm;
    LW0000_CTRL_SYSTEM_GET_P2P_CAPS_PARAMS  p2pCapsParams;
    memset(&p2pCapsParams, 0, sizeof(p2pCapsParams));

    m_bP2P = false;
    p2pCapsParams.gpuIds[0] = pSubdev1->GetGpuId();
    p2pCapsParams.gpuIds[1] = pSubdev2->GetGpuId();
    p2pCapsParams.gpuCount = 2;

    RC rc = pLwRm->Control(pLwRm->GetClientHandle(),
        LW0000_CTRL_CMD_SYSTEM_GET_P2P_CAPS,
        &p2pCapsParams, sizeof(p2pCapsParams));

    if (OK != rc)
    {
        return false;
    }

    m_bP2PReadsSupported = FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _READS_SUPPORTED, _TRUE,
        p2pCapsParams.p2pCaps);
    m_bP2PWritesSupported = FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _WRITES_SUPPORTED, _TRUE,
        p2pCapsParams.p2pCaps);
    m_bP2PROPSupported = FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _PROP_SUPPORTED, _TRUE,
        p2pCapsParams.p2pCaps);
    m_bP2PLwlinkSupported = FLD_TEST_DRF(0000_CTRL_SYSTEM, _GET_P2P_CAPS, _LWLINK_SUPPORTED, _TRUE,
        p2pCapsParams.p2pCaps);

    m_bP2P = (m_bP2PReadsSupported || m_bP2PWritesSupported);

    if (!m_bP2P)
    {
        if (pSubdev1 == pSubdev2)
        {
            Printf(Tee::PriLow, "P2P not supported. Error code=0x%x (%s)\n", rc.Get(), rc.Message());
        }
        else
        {
            Printf(Tee::PriLow, "P2P not supported between GPU ID 0x%x and 0x%x. Error code=0x%x (%s)\n", pSubdev1->GetGpuId(), pSubdev2->GetGpuId(), rc.Get(), rc.Message());
        }
    }

    if (m_VerboseLevel > 0)
    {
        Printf(Tee::PriLow,
            "P2P status between GPU 0x%x and 0x%x (read, write, lwlink, atomics, prop, loopback) = (%d, %d, %d, %d, %d, %d)\n",
            pSubdev1->GetGpuId(),
            pSubdev2->GetGpuId(),
            p2pCapsParams.p2pCapsStatus[LW0000_CTRL_P2P_CAPS_INDEX_READ],
            p2pCapsParams.p2pCapsStatus[LW0000_CTRL_P2P_CAPS_INDEX_WRITE],
            p2pCapsParams.p2pCapsStatus[LW0000_CTRL_P2P_CAPS_INDEX_LWLINK],
            p2pCapsParams.p2pCapsStatus[LW0000_CTRL_P2P_CAPS_INDEX_ATOMICS],
            p2pCapsParams.p2pCapsStatus[LW0000_CTRL_P2P_CAPS_INDEX_PROP],
            p2pCapsParams.p2pCapsStatus[LW0000_CTRL_P2P_CAPS_INDEX_LOOPBACK]);
    }

    return true;
}

//!
//! Reports the If DmaWrapper class is supported
//!----------------------------------------------------------------------------
bool EmpTest::IsDmaWrapperSupported(GpuDevice *pDev)
{
    // Unless CE engine will be removed in some future chip,
    // otherwise every chip in Kepler or later family should be supposed
    if (pDev->GetFamily() >= GpuDevice::Kepler)
        return true;

    return false;
}

//!
//! Push commands to write the notifier
//!------------------------------------------------------------------------------
RC EmpTest::WriteNotification(Channel *pChannel, UINT32 DeviceIndex, SemaphoreType semaphore, UINT32 payload)
{
    RC rc = OK;

    if (!m_Semaphores[DeviceIndex].IsAllocated())
    {
        return RC::LWRM_ILWALID_OBJECT_ERROR;
    }

    char *p = (char *)m_Semaphores[DeviceIndex].GetAddress();
    MEM_WR64(p + semaphore * 16, 0);
    MEM_WR64(p + semaphore * 16 + 8, 0xffffffffffffffffULL);

    m_Semaphores[DeviceIndex].BindGpuChannel(pChannel->GetHandle());
    UINT64 SemOffset = (m_Semaphores[DeviceIndex].GetCtxDmaOffsetGpu() + semaphore * 16);

    CHECK_RC(pChannel->SetContextDmaSemaphore(m_Semaphores[DeviceIndex].GetCtxDmaHandleGpu()));
    CHECK_RC(pChannel->SetSemaphoreOffset(SemOffset));
    CHECK_RC(pChannel->SemaphoreRelease(payload));

    return rc;
}

//!
//! Get the timestamp of a semaphore in integer ns unit
//!------------------------------------------------------------------------------
RC EmpTest::GetNsTime(UINT64 *pTime, UINT32 *pReadback, UINT32 DeviceIndex)
{
    if (!m_Semaphores[DeviceIndex].IsAllocated())
    {
        return RC::LWRM_ILWALID_OBJECT_ERROR;
    }

    char *p = (char *)m_Semaphores[DeviceIndex].GetAddress();
    UINT64 t1 = (UINT64)MEM_RD64(p + SemBegin * 16 + 8);
    UINT64 t2 = (UINT64)MEM_RD64(p + SemEnd * 16 + 8);
    // Payload might be written before timestamp is set
    if (t1 != 0xffffffffffffffffULL && t2 != 0xffffffffffffffffULL)
    {
        *pReadback = MEM_RD32(p + SemEnd * 16);
        *pTime = t2 - t1;
    }
    else
    {
        // indicate the end semaphore not released yet
        *pReadback = 0xffffffff;
        *pTime = 0xffffffffffffffffULL;
    }
    return OK;
}

//!
//! Get the timestamp of a semaphore in float-pointed second unit
//!------------------------------------------------------------------------------
RC EmpTest::GetTime(double *pTime, UINT32 *pReadback, UINT32 DeviceIndex)
{
    RC rc = OK;
    UINT64 time = 0;

    CHECK_RC(GetNsTime(&time, pReadback, DeviceIndex));

    if (rc == OK && pTime)
    {
        *pTime = (double)time / 1000000000.0;
    }
    return rc;
}

//!
//! Boost P-state to P0
//!------------------------------------------------------------------------------
void EmpTest::BoostPState()
{
    LwRmPtr pLwRm;
    uct::VbiosPState::Index nPState = 0; // LW2080_CTRL_PERF_PSTATES_P0;

    for (UINT32 iDevice = 0; iDevice < m_NumDevices; iDevice++)
    {
        GpuDevice *pDevice = m_pGpuDevices[iDevice];

        LW2080_CTRL_PERF_SET_FORCE_PSTATE_PARAMS setForcePStateParams = { 0 };

        Printf(Tee::PriHigh, "\n== Switching PState to P%u (%s)\n", (unsigned)nPState,
            rmt::String::function(__PRETTY_FUNCTION__).c_str());

        // Set the structure parameters for the RMAPI call
        setForcePStateParams.forcePstate = BIT(nPState);
        setForcePStateParams.fallback = LW2080_CTRL_PERF_PSTATE_FALLBACK_RETURN_ERROR;

        // call API
        RC rc = pLwRm->ControlBySubdevice(pDevice->GetSubdevice(0), LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE,
            &setForcePStateParams, sizeof(setForcePStateParams));
        if (OK != rc)
        {
            Printf(Tee::PriHigh, "Failed to force perflevel P0 for GPU device %d, error %s (0x%x)\n", iDevice, rc.Message(), rc.Get());
        }

        Perf* pPerf = pDevice->GetSubdevice(0)->GetPerf();
        if (pPerf->HasPStates())
        {
            Perf::PerfPoint perfPt(0, Perf::GpcPerf_MAX);
            pPerf->SetPerfPoint(perfPt);
            if (OK != pPerf->GetLwrrentPerfPoint(&perfPt))
            {
                Printf(Tee::PriLow,
                    "%s : Getting current perf point failed, assuming sufficient clock speeds\n",
                    GetName().c_str());
            }

            if ((perfPt.PStateNum != 0) || (perfPt.SpecialPt != Perf::GpcPerf_MAX))
            {
                string warnStr =
                    Utility::StrPrintf("%s with bandwidth checking is only supported in 0.max\n"
                        "    Skipping bandwidth check!\n",
                        GetName().c_str());
                Printf(Tee::PriWarn, "%s\n", warnStr.c_str());
            }
        }
    }
}

//!
//! Clear forced P-state
//!------------------------------------------------------------------------------
void EmpTest::ClearForcedPState()
{
    LwRmPtr pLwRm;

    for (UINT32 iDevice = 0; iDevice < m_NumDevices; iDevice++)
    {
        GpuDevice *pDevice = m_pGpuDevices[iDevice];
        if (pDevice)
        {
            GpuSubdevice *pSubdevice = pDevice->GetSubdevice(0);
            if (pSubdevice)
            {
                Perf *pPerf = pSubdevice->GetPerf();
                pPerf->ClearForcedPState();
            }
        }
    }
}

//!
//! read and report the current P-state
//!------------------------------------------------------------------------------
void EmpTest::PrintLwrrentPState()
{
    LwRmPtr pLwRm;
    for (UINT32 iDevice = 0; iDevice < m_NumDevices; iDevice++)
    {
        GpuDevice *pDevice = m_pGpuDevices[iDevice];
        LW2080_CTRL_PERF_GET_LWRRENT_PSTATE_PARAMS stPState = { 0 };
        if (pLwRm->ControlBySubdevice(pDevice->GetSubdevice(0), LW2080_CTRL_CMD_PERF_GET_LWRRENT_PSTATE,
            &stPState, sizeof(LW2080_CTRL_PERF_GET_LWRRENT_PSTATE_PARAMS)) != LWOS_STATUS_SUCCESS)
        {
            Printf(Tee::PriHigh, "ERROR: Failed to query p-state for GPU%d.\n", iDevice);
        }

        LOWESTBITIDX_32(stPState.lwrrPstate);
        Printf(Tee::PriNormal, "GPU%d P-State: %d\n", iDevice, stPState.lwrrPstate);
    }
}

//!
//! read and report the current clocks
//!------------------------------------------------------------------------------
void EmpTest::PrintIndividualClocks()
{
    UINT32 i;

    Printf(Tee::PriNormal, "GPU clocks (in MHz)\nGPU ");

    // Clocks we'll query and print if they are available
    UINT32 interestingClocks[] =
    {
        LW2080_CTRL_CLK_DOMAIN_MCLK,            // Fermi
        LW2080_CTRL_CLK_DOMAIN_HOSTCLK,         // Fermi
        LW2080_CTRL_CLK_DOMAIN_DISPCLK,         // Fermi
        LW2080_CTRL_CLK_DOMAIN_GPC2CLK,         // Fermi (full speed: shaders, halved: texture)
        LW2080_CTRL_CLK_DOMAIN_LTC2CLK,         // Fermi (includes ROP)
        LW2080_CTRL_CLK_DOMAIN_SYS2CLK,         // Fermi (includes FE, host, PD)
    };
    const UINT32 MAX_CLOCKS = sizeof(interestingClocks) / sizeof(LwU32);

    char *interestingClockNames[MAX_CLOCKS] =
    {
        "M",
        "HOST",
        "DISP",
        "GPC2",
        "LTC2",
        "SYS2",
    };

    for (UINT32 subDev = 0; subDev < m_NumDevices; subDev++)
    {
        LW2080_CTRL_CLK_GET_DOMAINS_PARAMS stDomains = { 0 };
        LW2080_CTRL_CLK_GET_INFO_PARAMS stClkParams = { 0 };

        GpuDevice *pGpuDevice = m_pGpuDevices[subDev];
        GpuSubdevice *pSubDevice = pGpuDevice->GetSubdevice(0);

        LwRmPtr pLwRm;

        // Query the RM to fill in the clock domain info struct.
        if (OK != pLwRm->Control(pLwRm->GetSubdeviceHandle(pSubDevice), LW2080_CTRL_CMD_CLK_GET_DOMAINS,
            &stDomains, sizeof(LW2080_CTRL_CLK_GET_DOMAINS_PARAMS)))
        {
            Printf(Tee::PriNormal, "\nERROR: Failed to query clock domain for GPU%d.\n", subDev);
            continue;
        }

        // Build a list of interesting domains that are available
        LW2080_CTRL_CLK_INFO stClkInfo[MAX_CLOCKS];
        memset(&stClkInfo[0], 0, sizeof(stClkInfo));
        UINT32 nClocks = 0;
        for (i = 0; i < MAX_CLOCKS; ++i)
        {
            if (stDomains.clkDomains & interestingClocks[i])
            {
                if (!subDev)
                    Printf(Tee::PriNormal, " \t%s", interestingClockNames[i]);
                stClkInfo[nClocks].clkSource = LW2080_CTRL_CLK_SOURCE_DEFAULT;
                stClkInfo[nClocks].clkDomain = interestingClocks[i];

                ++nClocks;
            }
        }
        if (!subDev)
            Printf(Tee::PriNormal, "\n");
        stClkParams.clkInfoListSize = nClocks;
        stClkParams.clkInfoList = LW_PTR_TO_LwP64(&stClkInfo[0]);

        // Query RM for the clks we want
        if (OK != pLwRm->Control(pLwRm->GetSubdeviceHandle(pSubDevice), LW2080_CTRL_CMD_CLK_GET_INFO,
            &stClkParams, sizeof(LW2080_CTRL_CLK_GET_INFO_PARAMS)))
        {
            Printf(Tee::PriNormal, "\nERROR: Failed to get clocks for GPU%d.\n", subDev);
            continue;
        }

        // print the clocks in MHz (RM returns them in KHz)
        Printf(Tee::PriNormal, "%d", subDev);
        for (i = 0; i<stClkParams.clkInfoListSize; ++i)
            Printf(Tee::PriNormal, " \t%d", (stClkInfo[i].actualFreq + 500) / 1000);
        Printf(Tee::PriNormal, "\n");
    }
}

//!
//! Allocate DMA buffer according to info given by the specified Surface2D object,
//! also map it to a specified GPU device and fill it with a specific value
//!------------------------------------------------------------------------------
RC EmpTest::AllocDMABuffer(UINT32 Device, Surface2D *pSurf, Memory::Location location, UINT32 dwBlockSize, Memory::Protect protection, StressAttrib *pAttrs, UINT32 value, bool bIgnorePteKind)
{
    RC rc = OK;
    GpuDevice *pGpuDevice = m_pGpuDevices[Device];

    UINT32 dwSx = m_Width ? m_Width : 1024;
    UINT32 dwPitch = m_Pitch ? m_Pitch : dwSx*ColorUtils::PixelBytes(pAttrs->format);
    UINT32 dwSy = m_Height ? m_Height : (dwBlockSize + dwPitch - 1) / dwPitch;
	UINT32 dwPteKind = LW_MMU_PTE_KIND_ILWALID;
    const char *szPteKind = pAttrs->pteKind;

    if (pGpuDevice->GetFamily() >= GpuDevice::Turing)
    {
        // See also bug 1890761 and bug 1948733
        if (!strcmp("PITCH", szPteKind) || !strcmp("GENERIC_16BX2", szPteKind) || !strcmp("ZF32", szPteKind))
        {
            if (pAttrs->surfType&(ST_Compressed) && location == Memory::Fb)
            {
                szPteKind = "GENERIC_MEMORY_COMPRESSIBLE";
                pSurf->SetCompressed(true);
            }
            else
            {
                szPteKind = "GENERIC_MEMORY";
            }
        }
    }

	if (!pGpuDevice->GetSubdevice(0)->GetPteKindFromName(szPteKind, &dwPteKind))
	{
        Printf(Tee::PriNormal, "PTE kind %s is not supported on this GPU. PTE kind setting is ignored.\n", pAttrs->pteKind);
		dwPteKind = LW_MMU_PTE_KIND_ILWALID;
	}

    pSurf->SetWidth(dwSx);
    pSurf->SetHeight(dwSy);
    pSurf->SetBytesPerPixel(ColorUtils::PixelBytes(pAttrs->format));
    pSurf->SetColorFormat(pAttrs->format);
    pSurf->SetLocation(location);
    pSurf->SetProtect(protection);
    pSurf->SetLayout((pAttrs->surfType&(ST_BlockLinear | ST_ZBuffer | ST_RT)) ? Surface2D::BlockLinear : Surface2D::Pitch);
    pSurf->SetMemoryMappingMode(Surface2D::MapDirect); // Bug 1482818: Deprecate reflected mappings in production code

	if (dwPteKind != LW_MMU_PTE_KIND_ILWALID)
	{
	    pSurf->SetPteKind(dwPteKind);
	}

    pSurf->SetAddressModel(Memory::Paging);
    pSurf->SetPhysContig(true);

    if (pAttrs->surfType&(ST_ZBuffer | ST_RT))
    {
        pSurf->SetAASamples(pAttrs->aaSamples);
    }

    if (Surface2D::BlockLinear == pSurf->GetLayout())
    {
        pSurf->SetLogBlockWidth(0);
        pSurf->SetLogBlockHeight(0);
        pSurf->SetLogBlockDepth(0);
    }
    else
    {
        pSurf->SetPitch(dwPitch);
    }

    if (m_TransferMethod != DmaWrapper::TWOD)
    {
        if (pAttrs->surfType&ST_ZBuffer)
        {
            pSurf->SetType(LWOS32_TYPE_DEPTH);
        }
        else
            if (pAttrs->surfType&(ST_Compressed | ST_RT))
            {
                pSurf->SetType(LWOS32_TYPE_IMAGE);
            }
            else
            {
                pSurf->SetType(LWOS32_TYPE_TEXTURE);
            }
    }
    else
    {
        pSurf->SetType(LWOS32_TYPE_IMAGE);
    }

    pSurf->SetForceSizeAlloc(true);
    pSurf->SetVirtAlignment(4096);
    //pSurf->SetCompressed((location == Memory::Fb) && (surfType & ST_Compressed) ? true : false);

    if (m_VerboseLevel > 1)
    {
        UINT32 freeHeapSize = 0;
        GetFreeHeapSize(pGpuDevice->GetSubdevice(0), &freeHeapSize);
        Printf(Tee::PriNormal, "RM free heap size = 0x%x before creating a new allocation\n", freeHeapSize);
    }

    rc = pSurf->Alloc(pGpuDevice);

    if (m_VerboseLevel > 1)
    {
        UINT32 freeHeapSize = 0;
        GetFreeHeapSize(pGpuDevice->GetSubdevice(0), &freeHeapSize);
        Printf(Tee::PriNormal, "RM free heap size = 0x%x after created a new allocation\n", freeHeapSize);
    }

    if (OK == rc && !bIgnorePteKind && ((UINT32)pSurf->GetPteKind()) != dwPteKind && dwPteKind != LW_MMU_PTE_KIND_ILWALID)
    {
        // Try to set the PTE kind again via an alternative way
        if (pSurf->ChangePteKind(dwPteKind) != OK)
        {
            if (m_VerboseLevel > 0)
            {
                Printf(Tee::PriNormal, "Expected PTE kind 0x%x. Allocated PTE kind 0x%x.\n", dwPteKind, pSurf->GetPteKind());
            }

            pSurf->Free();
            rc = RC::LWRM_NOT_SUPPORTED;
        }
    }

    CHECK_RC(rc);

    // It is not a good idea to map the whole surface at once, since it can be larger than the max contiguous BAR1 available space.
    UINT32 dwStart = 0;
    UINT32 dwFillSize = dwBlockSize;
    while (dwFillSize > 0)
    {
        UINT32 dwSize = (dwFillSize < MAP_REGION_SIZE) ? dwFillSize : MAP_REGION_SIZE;
        CHECK_RC(pSurf->MapRegion(dwStart, dwSize));
        memset(pSurf->GetAddress(), 0, dwSize);
        pSurf->Unmap();
        dwFillSize -= dwSize;
        dwStart += dwSize;
    }

    return rc;
}

//!
//! Dma band width measurement function, shared by test case 2 to 5
//!------------------------------------------------------------------------------
RC EmpTest::DMABandWidth(UINT32 Device, Memory::Location srcLocation, Memory::Location dstLocation, UINT32 dwTimes, UINT32 dwBlockSize, StressAttrib *pAttrs, const char* pExtraInfo)
{
    RC rc = OK;
    RC errorRc = OK;
    DmaWrapper *pDmaWrapper = nullptr;
    Channel *pChannel = nullptr;
    UINT32 payload = 0x4e574441;

    LwU64 dwBW = 0;
    UINT64 d1;
    UINT64 d2;
    double f = -1;

    GpuDevice *pGpuDevice = m_pGpuDevices[Device];

    m_TestConfig.BindGpuDevice(pGpuDevice);

    Surface2D Source;
    Surface2D Dest;

    CHECK_RC_MSG(AllocDMABuffer(Device, &Dest, dstLocation, dwBlockSize, Memory::Writeable, pAttrs, 0x55555555, false), "Unable to allocate the destination surface");
    CHECK_RC_MSG(AllocDMABuffer(Device, &Source, srcLocation, dwBlockSize, Memory::Readable, pAttrs, 0xaaaaaaaa, false), "Unable to allocate the source surface");

    pDmaWrapper = &m_DmaWrappers[Device];
    pDmaWrapper->SetSurfaces(&Source, &Dest);

    pChannel = m_pChannels[Device];

    // Reset the channel
    pChannel->SetCachedPut(0);
    pChannel->SetPut(0);
    pChannel->Flush();

    WriteNotification(pChannel, Device, SemBegin, payload++);

    for (UINT32 i = 0; i < dwTimes; i++)
    {
        CHECK_RC(pDmaWrapper->Copy(0, 0, Dest.GetWidth()*Dest.GetBytesPerPixel(), Dest.GetHeight(), 0, 0, m_TestConfig.TimeoutMs() * 10, 0,
            false /* not blocked */,
            false /* no flush */,
            true /* write to the channel semaphore */));
    }

    WriteNotification(pChannel, Device, SemEnd, payload);

    d1 = Xp::GetWallTimeUS();

    pChannel->Flush();

    UINT32 readback;
    do
    {
        readback = 0;
        CHECK_RC(pDmaWrapper->Wait(m_TestConfig.TimeoutMs()));
        CHECK_RC(GetTime(&f, &readback, Device));
        if (readback != payload && m_VerboseLevel > 1)
        {
            Printf(Tee::PriNormal, "end notifier payload readback=0x%x\n", readback);
        }
    } while (readback != payload);
    errorRc = pChannel->WaitIdle(2000);
    if (errorRc != OK)
    {
        Printf(Tee::PriNormal, "waitIdle error %s (0x%x)\n", errorRc.Message(), errorRc.Get());
        rc = errorRc;
    }
    CHECK_RC(errorRc);

    d2 = Xp::GetWallTimeUS();

    dwBW = (UINT64)floor((double)(dwTimes*Source.GetSize()) / (f * 1024 * 1024));

    Printf(Tee::PriNormal, "GPU%d BW %lld MB/s %s\n", Device, dwBW, pExtraInfo);
    if (m_VerboseLevel > 1)
    {
        Printf(Tee::PriNormal, "%lld MB copied %d times, %f GPU seconds, %f CPU seconds\n", Source.GetSize() / (1024 * 1024), dwTimes, f, ((double)(d2 - d1)) / 1000000.0);
    }

    if (m_bCompareResults)
    {
        // It is not a good idea to map the whole surface at once, since it can be larger than the max contiguous BAR1 available space.
        UINT32 bytesToCompare = (UINT32)Dest.GetSize();
        UINT64 start = 0;

        while (bytesToCompare > 0)
        {
            UINT32 mapRegionSize = (MAP_REGION_SIZE < bytesToCompare) ? MAP_REGION_SIZE : bytesToCompare;

            Source.MapRegion(start, mapRegionSize, pGpuDevice->GetSubdevice(0)->GetSubdeviceInst());
            Dest.MapRegion(start, mapRegionSize, pGpuDevice->GetSubdevice(0)->GetSubdeviceInst());

            if (memdiff((UINT32*)Source.GetAddress(), (UINT32*)Dest.GetAddress(), mapRegionSize))
            {
                if (m_VerboseLevel > 0)
                {
                    Printf(Tee::PriNormal, "Comparing: src and dst are different\n");
                }
                break;
            }

            bytesToCompare -= mapRegionSize;
            start += mapRegionSize;
        }
    }

    return rc;
}

typedef struct
{
    Tasker::ThreadID    hThread;
    UINT32              dwThreadId;
    UINT32              dwTimes;
    UINT32              dwAffinityMask;
    UINT32              dwSize;
    UINT32              *p;
    const char          *pCaption;
    UINT64              dwRes;
    UINT32              dwVerboseLevel;
}ThreadData;

//!
//! Thread fill function for CPU Fill tests 6, 600 to 603, 610 to 613
//!------------------------------------------------------------------------------
void threadfill(void * data)
{
    UINT64 d1 = 0, d2 = 0;
    UINT32 k;
    ThreadData * pThreadData = (ThreadData *)data;

#ifdef LW_WINDOWS
    DWORD_PTR dwProcessAffinityMask, dwSystemAffinityMask;

    GetProcessAffinityMask(GetLwrrentProcess(), &dwProcessAffinityMask, &dwSystemAffinityMask);

    if (pThreadData->dwAffinityMask && !(pThreadData->dwAffinityMask & dwSystemAffinityMask))
    {
        Printf(Tee::PriNormal, "the specified CPU affinity is wrong, reverting to 0xF\n");
    }

    if (pThreadData->dwAffinityMask && (pThreadData->dwAffinityMask & dwSystemAffinityMask) && (dwProcessAffinityMask != 1 || dwSystemAffinityMask != 1))
    {
        Printf(Tee::PriNormal, "Multicore/HT CPU, affinity supported= 0x%x/0x%x\n", dwProcessAffinityMask, dwSystemAffinityMask);
        Printf(Tee::PriNormal, "setting affinity to 0x%x\n", pThreadData->dwAffinityMask);
        SetThreadAffinityMask(GetLwrrentThread(), pThreadData->dwAffinityMask);
    }
#else
    // Sorry that, anybody knows how to specify CPU affinity in a portable way?
    vector<UINT32> cpuMasks;
    cpuMasks.clear();
    Tasker::SetCpuAffinity(pThreadData->hThread, cpuMasks);
#endif

    d1 = Xp::GetWallTimeUS();;

#ifdef LW_WINDOWS
#define ftime  _ftime
#define timeb  _timeb
#endif
    struct timeb start;
    struct timeb end;

    ftime(&start);

    UINT64 liPC;
    UINT32 * p = (UINT32 *)&liPC;

    liPC = Xp::QueryPerformanceCounter();

    if (pThreadData->dwVerboseLevel > 1)
    {
        Printf(Tee::PriNormal, "thread %d: start sys time=0x%08x.0x%08x, QPC = 0x%08x/0x%08x\n", pThreadData->dwThreadId, (unsigned int)start.time, start.millitm, p[1], p[0]);
    }

    UINT32 dwValue;

    for (k = 0; k< pThreadData->dwTimes; k++)
    {
        dwValue = k * 10 * (pThreadData->dwThreadId + 1);

        assert(pThreadData->p);
        memset(pThreadData->p, dwValue, pThreadData->dwSize);
    }

    d2 = Xp::GetWallTimeUS();;

    ftime(&end);

    liPC = Xp::QueryPerformanceCounter();

    if (pThreadData->dwVerboseLevel > 1)
    {
        Printf(Tee::PriNormal, "thread %d: end   sys time=0x%08x.0x%08x, QPC = 0x%08x/0x%08x\n", pThreadData->dwThreadId, (unsigned int)end.time, end.millitm, p[1], p[0]);
    }

    UINT64 dwBW = ((UINT64)pThreadData->dwSize)*pThreadData->dwTimes * 1000000 / (1024 * 1024 * (d2 - d1));

    if (pThreadData->dwVerboseLevel > 1)
    {
        Printf(Tee::PriNormal, "thread %d, started at %f, finished at %f\n", pThreadData->dwThreadId, d1 / 1000.0, d2 / 1000.0);

        Printf(Tee::PriNormal, "thread %d, spent %f seconds, write to FB %s: %lld MB/s\n", pThreadData->dwThreadId, ((double)(d2 - d1)) / 1000000.0, pThreadData->pCaption, dwBW);
    }

    pThreadData->dwRes = dwBW;
}

//!
//! Thread launcher for CPU Fill tests 6, 600 to 603, 610 to 613
//!------------------------------------------------------------------------------
UINT64 EmpTest::MTCPUFill(UINT32 dwThreads, UINT32 dwAffinityMask, UINT32 dwTimes, UINT32 dwSize, UINT32 *p, const char *pCaption)
{
    UINT64 dwBW = 0;

    Tasker::ThreadID hThreads[10];
    ThreadData aThreadData[10];

    UINT32 i;

    for (i = 0; i<dwThreads; i++)
    {
        //launch threads
        aThreadData[i].dwThreadId = i;
        aThreadData[i].dwTimes = dwTimes;
        aThreadData[i].dwSize = dwSize;
        aThreadData[i].p = p;
        aThreadData[i].dwAffinityMask = dwAffinityMask;
        aThreadData[i].pCaption = pCaption;
        aThreadData[i].dwVerboseLevel = m_VerboseLevel;

        hThreads[i] = Tasker::CreateThread(threadfill, &aThreadData[i], Tasker::MIN_STACK_SIZE, "threadfill");
    }

    for (i = 0; i<dwThreads; i++)
    {
        Tasker::Join(hThreads[i]);
        dwBW += aThreadData[i].dwRes;
    }

    if (m_VerboseLevel > 1)
    {
        Printf(Tee::PriNormal, "Combined BW from all threads: %lld MB/s\n", dwBW);
    }
    return dwBW;
}

//!
//! CPU Fill tests 6, 600 to 603, 610 to 613
//!------------------------------------------------------------------------------
RC EmpTest::CpuFill(GpuDevice *pGpuDevice, UINT32 dwThreads, UINT32 dwAffinity, UINT32 dwBlockSize, UINT32 dwTimes, StressAttrib *pAttrs, const char* pExtraInfo)
{
    RC rc = OK;

    UINT32 c = 0;
    while (c < m_NumDevices && m_pGpuDevices[c] != pGpuDevice) c++;

    if (c >= m_NumDevices)
    {
        return RC::LWRM_ILWALID_DEVICE;
    }

    m_TestConfig.BindGpuDevice(pGpuDevice);

    UINT64 dwTotalBroadcastBW = 0;
    Surface2D Buffer;

    CHECK_RC(AllocDMABuffer(c, &Buffer, Memory::Fb, dwBlockSize, Memory::Writeable, pAttrs, 0x55555555));
    CHECK_RC(Buffer.Map(pGpuDevice->GetSubdevice(0)->GetSubdeviceInst()));

    for (UINT32 i = 0; i < dwTimes; i++)
    {
        const UINT64 d1 = Xp::GetWallTimeUS();

        dwTotalBroadcastBW += MTCPUFill(dwThreads, dwAffinity, 1000, dwBlockSize, (UINT32 *)Buffer.GetAddress(), (m_NumDevices<2 ? "SINGLE" : "BROADCAST SLI"));

        const UINT64 d2 = Xp::GetWallTimeUS();

        if (m_VerboseLevel > 1)
        {
            Printf(Tee::PriNormal, "Overall time: %f seconds\n", ((double)(d2 - d1)) / 1000000.0);
        }
    }

    if (m_NumDevices<2)
    {
        Printf(Tee::PriNormal, "SingleGpu BW %lld MB/s %s\n", dwTotalBroadcastBW / dwTimes, pExtraInfo);
    }
    else
    {
        Printf(Tee::PriNormal, "Master GPU BW %lld MB/s, %lld MB\n", dwTotalBroadcastBW / dwTimes, dwTotalBroadcastBW);
    }

    return rc;
}

//!
//! Test 20, P2P Speed
//!------------------------------------------------------------------------------
RC EmpTest::P2PSpeed(UINT32 dwBlockSize, UINT32 dwTimes, StressAttrib *pAttrs, const char* pExtraInfo)
{
    if (m_NumDevices<2)
    {
        Printf(Tee::PriHigh, "SLI ONLY!!!\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (!m_bP2P)
    {
        Printf(Tee::PriHigh, "SLI device does not support P2P!!!\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if ((!m_bP2PReadsSupported) && (!m_bP2PWritesSupported))
    {
        Printf(Tee::PriHigh, "Neither P2P Read/Write is Enabled. Quit.\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    RC rc = OK;
    UINT32 i;

    Surface2D Surface[2];
    LwRm::Handle peerCtxDmaHandle[2] = { 0 };
    UINT64 peerCtxDmaOffset[2] = { 0 };

    DmaWrapper *pDmaWrapper = nullptr;
    Channel *pChannel = nullptr;

    UINT32 loops = 2;

    UINT32 payload = 0x4e574441;

    for (UINT32 w = 0; w<loops; w++)
    {
        if (w == 0)
        {
            Printf(Tee::PriHigh, "P2P FB writes\n");
        }
        else
        {
            Printf(Tee::PriHigh, "P2P FB reads\n");
        }

        for (UINT32 b = 0; b<m_NumDevices; b++)
        {
            for (UINT32 c = 0; c<m_NumDevices; c++)
            {
                if (c == b)
                {
                    continue;
                }

                // Refresh P2P support flags for this GPU combination
                if (!IsP2pEnabled(m_pGpuDevices[b]->GetSubdevice(0), m_pGpuDevices[c]->GetSubdevice(0)))
                {
                    continue;
                }

                // TWOD does not support P2P write on LWLINK
                if ((w == 0) && (!m_bP2PWritesSupported || (m_TransferMethod == DmaWrapper::TWOD && m_bP2PLwlinkSupported)))
                {
                    Printf(Tee::PriHigh, "P2PWrite not allowed between GPU %d and GPU %d\n", b, c);
                    continue;
                }
                else if ((w == 1) && !m_bP2PReadsSupported)
                {
                    Printf(Tee::PriHigh, "P2PRead not allowed between GPU %d and GPU %d\n", b, c);
                    continue;
                }

                UINT32 localMapping[2] = { 0 };
                UINT32 peerMapping[2] = { 0 };

                localMapping[w] = b;
                localMapping[1 - w] = c;
                peerMapping[w] = c;
                peerMapping[1 - w] = b;

                m_TestConfig.BindGpuDevice(m_pGpuDevices[b]);
                pDmaWrapper = &m_DmaWrappers[b];
                pChannel = m_pChannels[b];
                pChannel->Flush();
                pChannel->WaitIdle(1000);
                pChannel->ClearPushbuffer();

                m_TestConfig.BindGpuDevice(m_pGpuDevices[b]);

                // Make sure we have a larger contiguous space for new allocations later
                for (i = 0; i < 2; i++)
                {
                    Surface[i].Free();
                }

                for (i = 0; i < 2; i++)
                {
                    CHECK_RC(AllocDMABuffer(localMapping[i], &Surface[i], Memory::Fb, dwBlockSize, Memory::ReadWrite, pAttrs, 0x55555555 * (i + 1), false));
                    CHECK_RC(Surface[i].MapPeer(m_pGpuDevices[peerMapping[i]]));
                    CHECK_RC(Surface[i].BindGpuChannel(pChannel->GetHandle()));

                    peerCtxDmaHandle[i] = Surface[i].GetCtxDmaHandleGpuPeer(m_pGpuDevices[peerMapping[i]]);
                    peerCtxDmaOffset[i] = Surface[i].GetCtxDmaOffsetGpuPeer(m_pGpuDevices[localMapping[i]]->GetSubdevice(0)->GetSubdeviceInst(), m_pGpuDevices[peerMapping[i]], m_pGpuDevices[peerMapping[i]]->GetSubdevice(0)->GetSubdeviceInst());
                }

                // force usage of remote ctxdma's as default.
                Surface[1 - w].SetCtxDmaHandleGpu(peerCtxDmaHandle[1 - w]);
                Surface[1 - w].SetCtxDmaOffsetGpu(peerCtxDmaOffset[1 - w]);
                pDmaWrapper->SetSurfaces(&Surface[0], &Surface[1]);

                WriteNotification(pChannel, b, SemBegin, payload++);
                for (i = 0; i< dwTimes; i++)
                {
                    bool bNeedToBlockAndFlush = false;
                    if (m_p2pThreshold > 0 &&
                        m_TransferMethod != DmaWrapper::TWOD &&
                        Surface2D::BlockLinear == Surface[0].GetLayout() &&
                        ((i%m_p2pThreshold) == (m_p2pThreshold-1)))
                    {
                        bNeedToBlockAndFlush = true;
                    }

                    CHECK_RC(pDmaWrapper->Copy(0, 0, Surface[0].GetWidth()*Surface[0].GetBytesPerPixel(), Surface[0].GetHeight(), 0, 0, m_TestConfig.TimeoutMs() * 100, 0,
                        bNeedToBlockAndFlush /* blocked if bNeedToBlockAndFlush */,
                        bNeedToBlockAndFlush /* flush if bNeedToBlockAndFlush */,
                        false /* write to the channel semaphore */));

                    if (m_VerboseLevel > 1)
                        Printf(Tee::PriNormal, "passed %d iterations (%d total), (0x%X bytes gpu%d->gpu%d)\n", i + 1, dwTimes, dwBlockSize, b, c);
                }
                WriteNotification(pChannel, b, SemEnd, payload);

                const UINT64 d1 = Xp::GetWallTimeUS();

                pChannel->Flush();

                double f = -1;
                UINT32 readback;
                do
                {
                    readback = 0;
                    // We really need longer wait time here for GP10x
                    CHECK_RC(pDmaWrapper->Wait(m_TestConfig.TimeoutMs()*100));
                    CHECK_RC(GetTime(&f, &readback, b));
                } while (readback != payload);
                payload++;
                RC errorRc = pChannel->WaitIdle(10000);
                if (errorRc != OK)
                {
                    Printf(Tee::PriNormal, "waitIdle error %s (0x%x)\n", errorRc.Message(), errorRc.Get());
                    rc = errorRc;
                }
                CHECK_RC(errorRc);

                const UINT64 d2 = Xp::GetWallTimeUS();

                // Invalid value. There must be something wrong.
                if (f <= 0)
                {
                    return RC::TIMEOUT_ERROR;
                }

                UINT64 dwBW = (UINT64)floor((double)(dwTimes*Surface[0].GetSize()) / (f * 1024 * 1024));

                if (m_VerboseLevel > 1)
                {
                    Printf(Tee::PriNormal, "Looped %d times: %f GPU seconds, %f CPU second/s\n", dwTimes, f, ((double)(d2 - d1)) / 1000000.0);
                }

                Printf(Tee::PriNormal, "GPU%d->GPU%d BW %lld MB/s %s\n", b, c, dwBW, pExtraInfo);
            }
        }
    }

    return OK;
}

//!
//! Test 201, Cross P2P Speed
//!------------------------------------------------------------------------------
RC EmpTest::CrossP2PSpeed(
    UINT32 dwBlockSize,
    UINT32 dwTimes,
    StressAttrib *pAttrs,
    const char* pExtraInfo)
{
    if (m_NumDevices<2)
    {
        Printf(Tee::PriHigh, "SLI ONLY!!!\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (!m_bP2P)
    {
        Printf(Tee::PriHigh, "SLI device does not support P2P!!!\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if ((!m_bP2PReadsSupported) && (!m_bP2PWritesSupported))
    {
        Printf(Tee::PriHigh, "Neither P2P Read/Write is Enabled. Quit.\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    RC rc = OK;
    UINT32 i;

    Surface2D Surface[2];
    LwRm::Handle saveCtxDmaHandle[2] = { 0 };
    UINT64 saveCtxDmaOffset[2] = { 0 };
    LwRm::Handle peerCtxDmaHandle[2] = { 0 };
    UINT64 peerCtxDmaOffset[2] = { 0 };

    DmaWrapper *pDmaWrapper[2] = { 0 };
    Channel *pChannel[2] = { 0 };

    UINT32 payload = 0x4e574441;

    UINT32 gA, gB;

    bool bReverse = false;

    // TWOD does not support P2P write on LWLINK
    if ((bReverse && !m_bP2PReadsSupported) ||
        (!bReverse && (!m_bP2PWritesSupported || (m_TransferMethod == DmaWrapper::TWOD && m_bP2PLwlinkSupported))))
    {
        Printf(Tee::PriNormal,
            "Specified Operation %s not allowed. Switch to %s.\n",
            bReverse ? "Read" : "Write", !bReverse ? "Read" : "Write");
        bReverse = !bReverse;
    }

    for (UINT32 src = 0; src<m_NumDevices; src++)
        for (UINT32 dst = 0; dst<m_NumDevices; dst++)
        {
            gA = src;
            gB = dst;

            if (gA == gB)
                continue;

            // Refresh P2P support flags for this GPU combination
            if (!IsP2pEnabled(m_pGpuDevices[gA]->GetSubdevice(0), m_pGpuDevices[gB]->GetSubdevice(0)))
            {
                continue;
            }

            // TWOD does not support P2P write on LWLINK
            if (!bReverse && (!m_bP2PWritesSupported || (m_TransferMethod == DmaWrapper::TWOD && m_bP2PLwlinkSupported)))
            {
                Printf(Tee::PriHigh, "P2PWrite not allowed between GPU %d and GPU %d\n", gA, gB);
                continue;
            }
            else if (bReverse && !m_bP2PReadsSupported)
            {
                Printf(Tee::PriHigh, "P2PRead not allowed between GPU %d and GPU %d\n", gA, gB);
                continue;
            }

            UINT32 localMapping[2] = { gA, gB };
            UINT32 peerMapping[2] = { gB, gA };

            if (m_VerboseLevel > 1)
            {
                Printf(Tee::PriNormal,
                    "Cross P2P %s VID->VID \n", bReverse ? "reads" : "writes");
            }

            // Make sure we have a larger contiguous space for new allocations later
            for (i = 0; i < 2; i++)
            {
                m_TestConfig.BindGpuDevice(m_pGpuDevices[localMapping[i]]);
                pDmaWrapper[i] = &m_DmaWrappers[localMapping[i]];
                pChannel[i] = m_pChannels[localMapping[i]];
                pChannel[i]->Flush();
                pChannel[i]->WaitIdle(1000);
                pChannel[i]->ClearPushbuffer();
                Surface[i].Free();
            }

            for (i = 0; i < 2; i++)
            {
                CHECK_RC(AllocDMABuffer(localMapping[i],
                    &Surface[i],
                    Memory::Fb,
                    dwBlockSize,
                    Memory::ReadWrite,
                    pAttrs,
                    0x55555555 * (i + 1),
                    false));
                CHECK_RC(Surface[i].MapPeer(m_pGpuDevices[peerMapping[i]]));
            }

            WriteNotification(pChannel[0], gA, SemBegin, payload++);
            WriteNotification(pChannel[1], gB, SemBegin, payload++);

            for (i = 0; i< dwTimes; i++)
            {
                bool bNeedToBlockAndFlush = false;
                if (m_p2pThreshold > 0 &&
                    m_TransferMethod != DmaWrapper::TWOD &&
                    Surface2D::BlockLinear == Surface[0].GetLayout() &&
                    ((i%m_p2pThreshold) == (m_p2pThreshold-1)))
                {
                    bNeedToBlockAndFlush = true;
                }

                m_TestConfig.BindGpuDevice(m_pGpuDevices[localMapping[bReverse ? 1 : 0]]);

                for (UINT32 j = 0; j < 2; j++)
                {
                    Surface[j].BindGpuChannel(pChannel[bReverse ? 1 : 0]->GetHandle());
                    saveCtxDmaHandle[j] = Surface[j].GetCtxDmaHandleGpu();
                    saveCtxDmaOffset[j] = Surface[j].GetCtxDmaOffsetGpu();
                    peerCtxDmaHandle[j] = Surface[j].GetCtxDmaHandleGpuPeer(
                        m_pGpuDevices[peerMapping[j]]);
                    peerCtxDmaOffset[j] =
                        Surface[j].GetCtxDmaOffsetGpuPeer(
                            m_pGpuDevices[localMapping[j]]->GetSubdevice(0)->GetSubdeviceInst(),
                            m_pGpuDevices[peerMapping[j]],
                            m_pGpuDevices[peerMapping[j]]->GetSubdevice(0)->GetSubdeviceInst());
                }

                // force usage of remote ctxdma's as default.
                Surface[bReverse ? 0 : 1].SetCtxDmaHandleGpu(peerCtxDmaHandle[bReverse ? 0 : 1]);
                Surface[bReverse ? 0 : 1].SetCtxDmaOffsetGpu(peerCtxDmaOffset[bReverse ? 0 : 1]);

                pDmaWrapper[bReverse ? 1 : 0]->SetSurfaces(&Surface[0], &Surface[1]);

                CHECK_RC(pDmaWrapper[bReverse ? 1 : 0]->Copy(0, 0, Surface[0].GetWidth()*Surface[0].GetBytesPerPixel(), Surface[0].GetHeight(), 0, 0, m_TestConfig.TimeoutMs() * 100, 0,
                    bNeedToBlockAndFlush, /* block if bNeedToBlockAndFlush */
                    bNeedToBlockAndFlush, /* flush if bNeedToBlockAndFlush */
                    false /* no write to the channel semaphore */));

                // restore ctxdma.
                Surface[bReverse ? 0 : 1].SetCtxDmaHandleGpu(saveCtxDmaHandle[bReverse ? 0 : 1]);
                Surface[bReverse ? 0 : 1].SetCtxDmaOffsetGpu(saveCtxDmaOffset[bReverse ? 0 : 1]);

                m_TestConfig.BindGpuDevice(m_pGpuDevices[localMapping[bReverse ? 0 : 1]]);
                for (UINT32 j = 0; j < 2; j++)
                {
                    Surface[j].BindGpuChannel(pChannel[bReverse ? 0 : 1]->GetHandle());
                    saveCtxDmaHandle[j] = Surface[j].GetCtxDmaHandleGpu();
                    saveCtxDmaOffset[j] = Surface[j].GetCtxDmaOffsetGpu();
                    peerCtxDmaHandle[j] = Surface[j].GetCtxDmaHandleGpuPeer(m_pGpuDevices[peerMapping[j]]);
                    peerCtxDmaOffset[j] = Surface[j].GetCtxDmaOffsetGpuPeer(m_pGpuDevices[localMapping[j]]->GetSubdevice(0)->GetSubdeviceInst(), m_pGpuDevices[peerMapping[j]], m_pGpuDevices[peerMapping[j]]->GetSubdevice(0)->GetSubdeviceInst());
                }

                // force usage of remote ctxdma's as default.
                Surface[bReverse ? 1 : 0].SetCtxDmaHandleGpu(peerCtxDmaHandle[bReverse ? 1 : 0]);
                Surface[bReverse ? 1 : 0].SetCtxDmaOffsetGpu(peerCtxDmaOffset[bReverse ? 1 : 0]);

                pDmaWrapper[bReverse ? 0 : 1]->SetSurfaces(&Surface[1], &Surface[0]);

                CHECK_RC(pDmaWrapper[bReverse ? 0 : 1]->Copy(0, 0, Surface[0].GetWidth()*Surface[0].GetBytesPerPixel(), Surface[0].GetHeight(), 0, 0, m_TestConfig.TimeoutMs() * 100, 0,
                    bNeedToBlockAndFlush, /* block if bNeedToBlockAndFlush */
                    bNeedToBlockAndFlush, /* flush if bNeedToBlockAndFlush */
                    false /* no write to the channel semaphore */));

                // restore ctxdma.
                Surface[bReverse ? 1 : 0].SetCtxDmaHandleGpu(saveCtxDmaHandle[bReverse ? 1 : 0]);
                Surface[bReverse ? 1 : 0].SetCtxDmaOffsetGpu(saveCtxDmaOffset[bReverse ? 1 : 0]);

                if (m_VerboseLevel > 1)
                    Printf(Tee::PriNormal, "passed %d iterations (%d total), 0x%X bytes 0->1 and 0x%X bytes 1->0\n", i + 1, dwTimes, dwBlockSize, dwBlockSize);
            }

            m_TestConfig.BindGpuDevice(m_pGpuDevices[localMapping[bReverse ? 1 : 0]]);
            WriteNotification(pChannel[0], gA, SemEnd, payload);
            m_TestConfig.BindGpuDevice(m_pGpuDevices[localMapping[bReverse ? 0 : 1]]);
            WriteNotification(pChannel[1], gB, SemEnd, payload);

            for (i = 0; i < 2; i++)
            {
                pChannel[i]->Flush();
            }

            double f = -1;
            double f2 = -1;
            UINT32 readback;
            do
            {
                readback = 0;
                // We really need longer wait time here for GP10x
                CHECK_RC(pDmaWrapper[0]->Wait(m_TestConfig.TimeoutMs() * 100));
                CHECK_RC(GetTime(&f, &readback, gA));
            } while (readback != payload);
            do
            {
                readback = 0;
                // We really need longer wait time here for GP10x
                CHECK_RC(pDmaWrapper[1]->Wait(m_TestConfig.TimeoutMs() * 100));
                CHECK_RC(GetTime(&f2, &readback, gB));
            } while (readback != payload);
            payload++;

            RC errorRc = OK;

            for (i = 0; i < 2; i++)
            {
                errorRc = pChannel[i]->WaitIdle(10000);
                if (errorRc != OK)
                {
                    Printf(Tee::PriNormal, "waitIdle error %s (0x%x) on GPU%d\n", errorRc.Message(), errorRc.Get(), localMapping[i]);
                }
                CHECK_RC(errorRc);
            }

            UINT64 dwBW = (UINT64)floor((double)(dwTimes*Surface[0].GetSize()) / (f * 1024 * 1024));

            if (m_VerboseLevel > 1)
            {
                Printf(Tee::PriNormal, "Looped %d times: %f GPU seconds\n", dwTimes, f);
            }

            Printf(Tee::PriNormal, "GPU%d<->GPU%d BW %lld MB/s %s\n", gA, gB, dwBW, pExtraInfo);
        }

    return OK;
}

//!
//! Test 203, P2P Correctness
//!------------------------------------------------------------------------------
RC EmpTest::TestP2PCorrectness(UINT32 dwBlockSize, UINT32 dwTimes, StressAttrib *pAttrs, const char* pExtraInfo)
{
    if (m_NumDevices<2)
    {
        Printf(Tee::PriNormal, "SLI ONLY!!!\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (m_NumDevices>4)
    {
        Printf(Tee::PriNormal, "Support up to 4-way SLI only, when there are %d GPUs installed!!!\n", m_NumDevices);
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if (!m_bP2P)
    {
        Printf(Tee::PriNormal, "SLI device does not support P2P!!!\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    if ((!m_bP2PReadsSupported) && (!m_bP2PWritesSupported))
    {
        Printf(Tee::PriNormal, "Neither P2P Read/Write is Enabled. Quit.\n");
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    RC rc = OK;
    UINT32 i, j;

    Surface2D Surface[4];
    LwRm::Handle peerCtxDmaHandle[4] = { 0 };
    UINT64 peerCtxDmaOffset[4] = { 0 };

    DmaWrapper *pDmaWrapper = nullptr;
    Channel *pChannel = nullptr;

    bool bReverse = false;

    // TWOD does not support P2P write on LWLINK
    if ((bReverse && !m_bP2PReadsSupported) ||
        (!bReverse && (!m_bP2PWritesSupported || (m_TransferMethod == DmaWrapper::TWOD && m_bP2PLwlinkSupported))))
    {
        Printf(Tee::PriNormal,
            "Specified Operation %s not allowed. Switch to %s.\n",
            bReverse ? "Read" : "Write", !bReverse ? "Read" : "Write");
        bReverse = !bReverse;
    }

    UINT32 dwClearColors[4] = { 0x11223344, 0x55667788, 0x99aa00bb, 0xccddeeff };

    UINT64 dwReadbackVals[4];

    for (UINT32 gA = 0; gA<m_NumDevices; gA++) for (UINT32 gB = 0; gB<m_NumDevices; gB++)
    {
        if (gA == gB)
            continue;

        // Refresh P2P support flags for this GPU combination
        if (!IsP2pEnabled(m_pGpuDevices[gA]->GetSubdevice(0), m_pGpuDevices[gB]->GetSubdevice(0)))
        {
            continue;
        }

        // TWOD does not support P2P write on LWLINK
        if (!bReverse && (!m_bP2PWritesSupported || (m_TransferMethod == DmaWrapper::TWOD && m_bP2PLwlinkSupported)))
        {
            Printf(Tee::PriHigh, "P2PWrite not allowed between GPU %d and GPU %d\n", gA, gB);
            continue;
        }
        else if (bReverse && !m_bP2PReadsSupported)
        {
            Printf(Tee::PriHigh, "P2PRead not allowed between GPU %d and GPU %d\n", gA, gB);
            continue;
        }

        UINT32 devMask = (BIT(m_NumDevices) - 1) & ~(BIT(gA) | BIT(gB));
        UINT32 gC = 0, gD = 0;

        i = 0;
        while (i < 4)
        {
            if (devMask & BIT(i))
            {
                gC = i++;
                break;
            }
            i++;
        }

        while (i < 4)
        {
            if (devMask & BIT(i))
            {
                gD = i;
                break;
            }
            i++;
        }

        UINT32 localMapping[4] = { gA, gB, gC, gD };
        UINT32 peerMapping[4] = { gB, gA, gD, gC };
        UINT32 DevMapIndex = bReverse ? 1 : 0;

        m_TestConfig.BindGpuDevice(m_pGpuDevices[localMapping[DevMapIndex]]);
        pDmaWrapper = &m_DmaWrappers[localMapping[DevMapIndex]];
        pChannel = m_pChannels[localMapping[DevMapIndex]];
        LwRm::Handle hChannel = pChannel->GetHandle();

        // Make sure we have a larger contiguous space for new allocations later
        for (i = 0; i < 4; i++)
        {
            Surface[i].Free();
        }

        for (i = 0; i < 4; i++)
        {
            CHECK_RC_MSG(AllocDMABuffer(localMapping[i], &Surface[i], Memory::Fb, dwBlockSize, Memory::ReadWrite, pAttrs, dwClearColors[localMapping[i] & 3], false), "Unable to allocate surface");

            // Only Surface[0] and Surface[1] are used for P2P copy. The rest surfaces are only for CPU access check against unexpected change
            if (i<2)
            {
                CHECK_RC_MSG(Surface[i].MapPeer(m_pGpuDevices[peerMapping[i]]), "Unable to map peer");

                CHECK_RC_MSG(Surface[i].BindGpuChannel(hChannel), "Unable to bind channel");
                peerCtxDmaHandle[i] = Surface[i].GetCtxDmaHandleGpuPeer(m_pGpuDevices[peerMapping[i]]);
                peerCtxDmaOffset[i] = Surface[i].GetCtxDmaOffsetGpuPeer(m_pGpuDevices[localMapping[i]]->GetSubdevice(0)->GetSubdeviceInst(), m_pGpuDevices[peerMapping[i]], m_pGpuDevices[peerMapping[i]]->GetSubdevice(0)->GetSubdeviceInst());
            }
        }

        // force usage of remote ctxdma's as default.
        Surface[1 - DevMapIndex].SetCtxDmaHandleGpu(peerCtxDmaHandle[1 - DevMapIndex]);
        Surface[1 - DevMapIndex].SetCtxDmaOffsetGpu(peerCtxDmaOffset[1 - DevMapIndex]);

        pDmaWrapper->SetSurfaces(&Surface[0], &Surface[1]);

        bool success = true;
        Printf(Tee::PriNormal, "GPU%d->GPU%d(%s)\n", gA, gB, pExtraInfo);

        // Determine what was written to the buffer by the clear
        UINT64 mapRegionSize = (MAP_REGION_SIZE < Surface[0].GetSize()) ? MAP_REGION_SIZE : Surface[0].GetSize();
        for (i = 0; i < 4; i++)
        {
            CHECK_RC_MSG(Surface[i].MapRegion(0, mapRegionSize), "Unable to map region");
            char *p = (char *)Surface[i].GetAddress();
            dwReadbackVals[i] = (UINT64)MEM_RD64(p);
            Surface[i].Unmap();
        }

        CHECK_RC_MSG(pDmaWrapper->Copy(0, 0, Surface[0].GetPitch(), Surface[0].GetHeight(), 0, 0, m_TestConfig.TimeoutMs() * 10, 0,
            true /* blocked */,
            true /* flush */,
            true /* no write to the channel semaphore */),
            "Unable to copy");

        UINT64 start = 0;
        mapRegionSize = (MAP_REGION_SIZE < Surface[0].GetSize()) ? MAP_REGION_SIZE : Surface[0].GetSize();
        for (i = 0; i < 2; i++)
        {
            Surface[i].MapRegion(start, mapRegionSize, m_pGpuDevices[localMapping[i]]->GetSubdevice(0)->GetSubdeviceInst());
        }

        // Compare results
        UINT64* pA = (UINT64*)Surface[0].GetAddress();
        UINT64* pB = (UINT64*)Surface[1].GetAddress();
        while (start < Surface[0].GetSize()) {
            LwU64 A = *pA++, B = *pB++;
            if (A != dwReadbackVals[0] || B != A)
            {
                Printf(Tee::PriNormal, "mismatch at offset 0x%llX: src=0x%llX, dst=0x%llX, clearColor=0x%x%x,\nwidth=0x%x, pitch=0x%x, alloc_width=0x%x\n",
                    (Surface[0].GetSize() - start) / sizeof(UINT64), A, B, dwClearColors[localMapping[i] & 3], dwClearColors[localMapping[i] & 3],
                    Surface[0].GetWidth(), Surface[0].GetPitch(), Surface[0].GetAllocWidth());
                success = false;
                break;
            }

            mapRegionSize-=sizeof(UINT64);
            start+=sizeof(UINT64);
            if (mapRegionSize == 0)
            {
                mapRegionSize = min(MAP_REGION_SIZE, Surface[0].GetSize() - start);
                if (mapRegionSize > 0)
                {
                    for (i = 0; i < 2; i++)
                    {
                        Surface[i].Unmap();
                        Surface[i].MapRegion(start, mapRegionSize, m_pGpuDevices[localMapping[i]]->GetSubdevice(0)->GetSubdeviceInst());
                    }
                    pA = (UINT64*)Surface[0].GetAddress();
                    pB = (UINT64*)Surface[1].GetAddress();
                }
            }
        }
        // check other buffers if there is any unexpected change
        for (j = 2; j<m_NumDevices; j++)
        {
            if (j == gA || j == gB)
                continue;

            UINT64 testSize = Surface[0].GetSize() / sizeof(LwU64);
            start = 0;
            mapRegionSize = min(MAP_REGION_SIZE, Surface[j].GetSize());
            Surface[j].MapRegion(0, mapRegionSize, m_pGpuDevices[localMapping[j]]->GetSubdevice(0)->GetSubdeviceInst());

            UINT64* p = (UINT64*)Surface[j].GetAddress();

            while (start < Surface[j].GetSize()) {
                LwU64 A = *p++;
                if (A != dwReadbackVals[j])
                {
                    Printf(Tee::PriNormal, "corruption on gpu%d: pixel 0x%llX has 0x%llX, clearColor=0x%x%x\n",
                        localMapping[j], (Surface[j].GetSize() - start) / sizeof(UINT64), dwReadbackVals[j],
                        dwClearColors[localMapping[j] & 3], dwClearColors[localMapping[j] & 3]);
                    success = false;
                    break;
                }
                testSize--;

                mapRegionSize-=sizeof(UINT64);
                start+=sizeof(UINT64);
                if (mapRegionSize == 0)
                {
                    Surface[j].Unmap();
                    mapRegionSize = min(MAP_REGION_SIZE, Surface[j].GetSize() - start);
                    if (mapRegionSize > 0)
                    {
                        Surface[j].MapRegion(start, mapRegionSize, m_pGpuDevices[localMapping[j]]->GetSubdevice(0)->GetSubdeviceInst());
                        p = (UINT64*)Surface[j].GetAddress();
                    }
                }
            }
        }

        Printf(Tee::PriNormal, "%s\n", success ? "PASS" : "  FAIL!!");
        if (!success)
        {
            return RC::FAILED_TO_COPY_MEMORY;
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

// Set up a JavaScript class that creates and owns a C++ EmpTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(EmpTest, RmTest,
    "EMP test");

JS_STEST_LWSTOM(EmpTest,
    EnableEmpMainStyleArgParsing,
    1,
    "Enable EmpTest to parse arguments like EmpMain.exe does.")
{
    JavaScriptPtr pJs;

    // Check the arguments.
    UINT32 Enable = 0;
    if (1 != NumArguments
        || (OK != pJs->FromJsval(pArguments[0], &Enable))
        )
    {
        JS_ReportError(pContext,
            "Usage: EmpTest.EnableEmpMainStyleArgParsing(1) to enable\n"
            "       EmpTest.EnableEmpMainStyleArgParsing(0) to disable");
        return JS_FALSE;
    }

    EmpTest *pTest;
    if ((pTest = JS_GET_PRIVATE(EmpTest, pContext, pObject, nullptr)) != 0)
    {
        pTest->EnableEmpMainStyleArgParsing(Enable ? 1 : 0);
    }
    RETURN_RC(OK);
}
