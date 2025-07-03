/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/tests/lwdastst.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/surf2d.h"
#include "gpu/utility/surfrdwr.h"
#include "core/include/platform.h"

//! LWCA-based test for verifying MMIO operations, Yield and Nanosleep instructions
//-----------------------------------------------------------------------------
class LwdaMmioYield : public LwdaStreamTest
{
public:
    LwdaMmioYield();
    virtual ~LwdaMmioYield() { }
    virtual bool IsSupported() override;
    virtual RC Setup() override;
    virtual RC RunMmio();
    virtual RC RunYield(UINT32 testmode);
    virtual RC Run() override;
    virtual RC Cleanup() override;
    bool IsSupportedVf() override { return true; }

    SETGET_PROP(InitVal, UINT32);
    SETGET_PROP(TestMode, UINT32);
    SETGET_PROP(Count, UINT32);
    SETGET_PROP(EnableGpuCache, bool);

    enum TestMode
    {
        TESTMODE_YIELD = 1 << 0,
        TESTMODE_SLEEP = 1 << 1,
        TESTMODE_MMIO  = 1 << 2,
        TESTMODE_ALL   = (TESTMODE_MMIO << 1) - 1
    };

private:
    UINT32 m_GridWidth = 1;
    UINT32 m_BlockWidth = 32;

    // This parameter sets the loop count that checks for yield using volatile
    // ld/st and nanosleep instruction
    UINT32 m_InitVal = 0xFF;
    UINT32 m_TestMode = TESTMODE_ALL;
    UINT32 m_Count = 1;
    bool   m_EnableGpuCache = true;
    Lwca::DeviceMemory m_InputBuffer;
    Lwca::DeviceMemory m_OutputBuffer;
    UINT32 m_BufferSize = 0;

    Lwca::Module m_Module;
    Lwca::Function m_Yield;
    Lwca::Function m_Nanosleep;
    Lwca::Function m_Mmio;

    // For MMIO
    Surface2D       m_InputSurf;
    UINT32          m_InputSurfSize = 0;
    Lwca::ClientMemory m_InputMem;
    Surface2D       m_OutputSurf;
    UINT32          m_OutputSurfSize = 0;
    Lwca::ClientMemory m_OutputMem;
    Surface2D       m_FlagSurf;
    UINT32          m_FlagSurfSize = 0;
    Lwca::ClientMemory m_FlagMem;

    RC SetupMmioResources();

};

//-----------------------------------------------------------------------------
LwdaMmioYield::LwdaMmioYield()
{
    SetName("LwdaMmioYield");
}

//-----------------------------------------------------------------------------
bool LwdaMmioYield::IsSupported()
{
    // Check if compute is supported at all
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();

    // Not supported on CheetAh
    if (GetBoundGpuSubdevice()->IsSOC())
        return false;

    return cap >= Lwca::Capability::SM_70; // Volta+ only
}

//-----------------------------------------------------------------------------
RC LwdaMmioYield::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    // InitVal:
    // Two MSB bits in InitVal is set in the kernel:
    // MSB bit: as a fail condition if yield/nanosleep did not occur
    // MSB-1 bit: as a pass condition that yield/nanosleep oclwrred
    // The other bits store a counter which varies from [InitVal -> 2 * InitVal]
    // The counter is just for debugging, since we don't want the final value of
    // counter to modify the MSB bits, limit its value
    if (m_InitVal > 0xFFFFFF)
    {
        Printf(Tee::PriError, "InitVal is too high, select a lower number.\n");
        CHECK_RC(RC::BAD_PARAMETER);
    }

    // Set Launch params
    m_GridWidth = GetBoundLwdaDevice().GetShaderCount();
    m_BlockWidth = GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_WARP_SIZE);

    // The yield kernel is hardcoded to use 32 unsigned ints as shared mem
    MASSERT(m_BlockWidth == 32);

    // Load correct module.
    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule("yield", &m_Module));

    if (m_TestMode & TESTMODE_YIELD)
    {
        m_Yield = m_Module.GetFunction("Yield", m_GridWidth, m_BlockWidth);
        CHECK_RC(m_Yield.InitCheck());
    }

    if (m_TestMode & TESTMODE_SLEEP)
    {
        if (GetBoundGpuSubdevice()->HasBug(2212722))
        {
            Printf(Tee::PriError, "Cannot test Nanosleep due to bug: http://lwbugs/2212722\n");
            CHECK_RC(RC::BAD_PARAMETER);
        }
        m_Nanosleep = m_Module.GetFunction("Nanosleep", m_GridWidth, m_BlockWidth);
        CHECK_RC(m_Nanosleep.InitCheck());
    }
    m_BufferSize = m_GridWidth * m_BlockWidth;

    if (m_TestMode & TESTMODE_MMIO)
    {    // A single thread is sufficient to verify MMIO
        m_Mmio = m_Module.GetFunction("Mmio", 1, 1);
        CHECK_RC(m_Mmio.InitCheck());
        CHECK_RC(SetupMmioResources());
    }

    // allocate buffers
    if ((m_TestMode & TESTMODE_YIELD) || (m_TestMode & TESTMODE_SLEEP))
    {
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_BufferSize*sizeof(UINT32), &m_InputBuffer));
        CHECK_RC(GetLwdaInstance().AllocDeviceMem(m_BufferSize*sizeof(UINT32), &m_OutputBuffer));
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC LwdaMmioYield::Cleanup()
{
    m_Module.Unload();
    if ((m_TestMode & TESTMODE_YIELD) || (m_TestMode & TESTMODE_SLEEP))
    {
        m_InputBuffer.Free();
        m_OutputBuffer.Free();
    }

    m_InputMem.Free();
    m_InputSurf.Free();
    m_OutputMem.Free();
    m_OutputSurf.Free();
    m_FlagMem.Free();
    m_FlagSurf.Free();
    return LwdaStreamTest::Cleanup();
}

//-----------------------------------------------------------------------------
RC LwdaMmioYield::RunYield(UINT32 testMode)
{
    RC rc;
    // Set data in inputBuffer
    vector<UINT32> initData(m_BufferSize, m_InitVal);
    vector<UINT32, Lwca::HostMemoryAllocator<UINT32> >
        hostData(m_BufferSize * sizeof(UINT32), UINT32(),
                Lwca::HostMemoryAllocator<UINT32>(GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    std::copy(initData.begin(), initData.end(), hostData.begin());

    CHECK_RC(m_InputBuffer.Set(&hostData[0], m_BufferSize * sizeof(UINT32)));

    if (testMode == TESTMODE_SLEEP)
    {
        CHECK_RC(m_Nanosleep.Launch(m_InputBuffer.GetDevicePtr(),
                 m_OutputBuffer.GetDevicePtr()));
    }
    if (testMode == TESTMODE_YIELD)
    {
        CHECK_RC(m_Yield.Launch(m_InputBuffer.GetDevicePtr(),
                 m_OutputBuffer.GetDevicePtr()));
    }

    // Copy results back to sysmem for verifying
    vector<UINT32, Lwca::HostMemoryAllocator<UINT32> > outputBuf(m_BufferSize * sizeof(UINT32), UINT32(),
            Lwca::HostMemoryAllocator<UINT32>(GetLwdaInstancePtr(), GetBoundLwdaDevice()));
    CHECK_RC(m_OutputBuffer.Get(&outputBuf[0], outputBuf.size()));
    CHECK_RC(GetLwdaInstance().Synchronize());

    bool fail = false;
    for (UINT32 i = 0; i < m_BufferSize; i++)
    {
        if (i % m_BlockWidth == 0)
            Printf(Tee::PriDebug, "\n");
        Printf(Tee::PriDebug, " 0x%x ", outputBuf[i]);
        if ((i % m_BlockWidth) >= (m_BlockWidth / 2))
        {
            if((outputBuf[i] & 0x80000000) ||  // kernel sets MSB if yield /nanosleep doesn't occur
               (!(outputBuf[i] & 0x40000000))) // MSB-1th bit must be set if it did
            {
                fail = true;
            }
        }
    }
    Printf(Tee::PriDebug, "\n");
    if (fail)
        CHECK_RC(RC::HW_ERROR);
    return rc;
}
//-------------------------------------------------------------------------------------------------
// Allocate and map the 3 surfaces for MMIO test.
//
RC LwdaMmioYield::SetupMmioResources()
{
    RC rc;
    GpuDevice *pGpuDevice = GetBoundGpuDevice();
    GpuSubdevice *  m_pSubdev = GetBoundGpuSubdevice();
    UINT32 m_SubdevInst = m_pSubdev->GetSubdeviceInst();

    //set the GPU's input surface
    const UINT32 inputbufSize = 64; // Input buffer needs to fill atleast a cache line (128 bytes)
    m_InputSurfSize = inputbufSize * sizeof(UINT32);
    m_InputSurf.SetWidth(m_InputSurfSize);
    m_InputSurf.SetHeight(1);
    m_InputSurf.SetColorFormat(ColorUtils::VOID32);
    m_InputSurf.SetLocation(Memory::Coherent);
    m_InputSurf.SetMemoryMappingMode(Surface2D::MapDirect);
    m_InputSurf.SetGpuCacheMode(m_EnableGpuCache ? Surface2D::GpuCacheOn : Surface2D::GpuCacheOff);
    CHECK_RC(m_InputSurf.Alloc(pGpuDevice));

    // Map through Surface2D so we can get a virtual address.
    m_InputSurf.Map(m_SubdevInst);

    // map through LWCA to get a device pointer
    m_InputMem = GetLwdaInstance().MapClientMem(m_InputSurf.GetMemHandle(), 0,
        m_InputSurfSize, pGpuDevice,
        m_InputSurf.GetLocation());
    CHECK_RC(m_InputMem.InitCheck());

    // set the GPU's output surface
    const UINT32 outputBufferSize = 4;
    m_OutputSurfSize = outputBufferSize * sizeof(UINT32);
    m_OutputSurf.SetWidth(m_OutputSurfSize);
    m_OutputSurf.SetHeight(1);
    m_OutputSurf.SetColorFormat(ColorUtils::VOID32);
    m_OutputSurf.SetLocation(Memory::NonCoherent);
    m_OutputSurf.SetMemoryMappingMode(Surface2D::MapDirect);
    CHECK_RC(m_OutputSurf.Alloc(pGpuDevice));
    // Map through Surface2D so we can get a virtual address.
    m_OutputSurf.Map(m_SubdevInst);

    //Map through LWCA so we can get a device pointer
    m_OutputMem = GetLwdaInstance().MapClientMem(m_OutputSurf.GetMemHandle(), 0,
        m_OutputSurfSize, pGpuDevice,
        m_OutputSurf.GetLocation());
    CHECK_RC(m_OutputMem.InitCheck());

    //Set command flag memory used by both CPU & GPU to synchronize data access.
    m_FlagSurfSize = sizeof(UINT32);
    m_FlagSurf.SetWidth(m_FlagSurfSize);
    m_FlagSurf.SetHeight(1);
    m_FlagSurf.SetColorFormat(ColorUtils::VOID32);
    m_FlagSurf.SetLocation(Memory::NonCoherent);
    m_FlagSurf.SetMemoryMappingMode(Surface2D::MapDirect);
    CHECK_RC(m_FlagSurf.Alloc(pGpuDevice));
    m_FlagMem = GetLwdaInstance().MapClientMem(m_FlagSurf.GetMemHandle(), 0,
        m_FlagSurfSize, pGpuDevice, m_FlagSurf.GetLocation());
    CHECK_RC(m_FlagMem.InitCheck());
    m_FlagSurf.Map(m_SubdevInst);

    return rc;
}

//------------------------------------------------------------------------------------------------
// This routine will test the GPU's abitilty to perform MMIO accesses to system memory.
// When a MMIO access is performed the L1 & L2 caches are bypassed and the sysmem location is
// accessed directly.
// Note: For this test to run properly the GPU must have a direct mapping (no BAR1) of the sysmem.
// Note: In release builds of LWCA this algorithm will not work because the low level asm
// instruction that we are using will not be exposed. See bug
RC LwdaMmioYield::RunMmio()
{
    enum Flag
    {
        FLAG_CPU_WRITE1 = 0x100,
        FLAG_GPU_WRITE  = 0x101,
        FLAG_CPU_WRITE2 = 0x102
    };

    const UINT32 pattern1 = 0x12345678; // Random number
    const UINT32 pattern2 = 0x22223333; // Random number
    volatile UINT32 *pInputSurf  = (volatile UINT32*)m_InputSurf.GetAddress();
    volatile UINT32 *pFlagSurf   = (volatile UINT32*)m_FlagSurf.GetAddress();
    volatile UINT32 *pOutputSurf = (volatile UINT32*)m_OutputSurf.GetAddress();

    StickyRC rc;
    for (UINT32 loopCount = 0; loopCount < m_Count; loopCount++)
    {
        *pFlagSurf = FLAG_CPU_WRITE1;
        *pInputSurf = pattern1;

        //run kernel
        CHECK_RC(m_Mmio.Launch(m_FlagMem.GetDevicePtr(),
                               m_InputMem.GetDevicePtr(),
                               m_OutputMem.GetDevicePtr()));

        //wait for write from kernel to respond with 0x101
        while (*pFlagSurf != FLAG_GPU_WRITE);

        *pInputSurf = pattern2;
        if (m_InputSurf.GetLocation() != Memory::Coherent)
        {
            Platform::FlushCpuWriteCombineBuffer();
        }

        *pFlagSurf = FLAG_CPU_WRITE2;
        CHECK_RC(GetLwdaInstance().Synchronize());

        VerbosePrintf(
               "vals: mmio(ilwalidate cache):0x%x (volatile rd):0x%x (ld_cg):0x%x (mmio):0x%x \n",
               pOutputSurf[0], pOutputSurf[1], pOutputSurf[2], pOutputSurf[3]);

        if (m_InputSurf.GetGpuCacheMode() == Surface2D::GpuCacheOff)
        {
            if (pOutputSurf[2] != pattern2) // should not be cached value
            {
                Printf(Tee::PriError, "Incorrect cached value of:0x%x s/b 0x%x\n",
                       pOutputSurf[2], pattern2);
                rc = RC::HW_ERROR;
            }
        }
        else
        {
            if (pOutputSurf[2] != pattern1) // should be cached value
            {
                Printf(Tee::PriError, "Incorrect cached value of:0x%x s/b 0x%x\n",
                       pOutputSurf[2], pattern1);
                rc = RC::HW_ERROR;
            }
        }

        // MMIO reads are at index 3 in kernel
        if (pOutputSurf[3] != (pattern2)) // should always be uncached value
        {
            Printf(Tee::PriError, "Incorrect uncached value of:0x%x s/b 0x%x\n",
                   pOutputSurf[3], pattern2);
            rc = RC::HW_ERROR;
        }
    }
    return rc;
}

//-----------------------------------------------------------------------------
RC LwdaMmioYield::Run()
{
    StickyRC stickyRc;
    if (m_TestMode & TESTMODE_MMIO)
    {
        RC rc = RunMmio();
        stickyRc = rc;
        if (rc != OK)
            Printf(Tee::PriError, "Fail: Mmio.\n");
    }
    if (m_TestMode & TESTMODE_YIELD)
    {
        RC rc = RunYield(TESTMODE_YIELD);
        stickyRc = rc;
        if (rc != OK)
            Printf(Tee::PriError, "Fail: Yield.\n");
    }
    if (m_TestMode & TESTMODE_SLEEP)
    {
        RC rc = RunYield(TESTMODE_SLEEP);
        stickyRc = rc;
        if (rc != OK)
            Printf(Tee::PriError, "Fail: Nanosleep.\n");
    }
    return stickyRc;
}

//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(LwdaMmioYield, LwdaStreamTest, "LWCA test for MMIO/Yield/nanosleep");
CLASS_PROP_READWRITE(LwdaMmioYield, InitVal, UINT32,  "Initial value for the input buffer");
CLASS_PROP_READWRITE(LwdaMmioYield, TestMode, UINT32, "Set Test mode: 1 = yield, 2 = nanosleep, 4 = mmio, 7 = all)"); //$
CLASS_PROP_READWRITE(LwdaMmioYield, Count, UINT32, "Number of times to run:");
CLASS_PROP_READWRITE(LwdaMmioYield, EnableGpuCache, bool, "Enable/Disable GpuCache:1=enable 0=disable"); //$

PROP_CONST(LwdaMmioYield, TESTMODE_YIELD,  LwdaMmioYield::TESTMODE_YIELD);
PROP_CONST(LwdaMmioYield, TESTMODE_SLEEP,  LwdaMmioYield::TESTMODE_SLEEP);
PROP_CONST(LwdaMmioYield, TESTMODE_MMIO,   LwdaMmioYield::TESTMODE_MMIO);
PROP_CONST(LwdaMmioYield, TESTMODE_ALL,    LwdaMmioYield::TESTMODE_ALL);
