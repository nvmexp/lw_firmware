/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_vprhwscrubberbasictest.cpp
//! \brief rmtest for testing basic functionality of HW scrubber unit
//!

#include "gpu/tests/rmtest.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "core/include/tee.h"
#include "core/include/rc.h"
#include "rmpsdl.h"
#include "core/include/platform.h"
#include "gpu/utility/surf2d.h"
#include "core/include/memcheck.h"
#include "mods_reg_hal.h"
#include "mmu/mmucmn.h"

#define DEFAULT_SCRUB_SIZE          0x10000
#define DEFAULT_SCRUB_PATTERN       0xAA55AA55

class VprHwScrubberBasicTest : public RmTest
{
public:
    VprHwScrubberBasicTest();
    virtual ~VprHwScrubberBasicTest();
    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();
    enum scrubType {Forward =1, Reverse=2, ThroughSec=3};
    SETGET_PROP(scrubSize, UINT32);
    SETGET_PROP(scrubPattern, UINT32);
    SETGET_PROP(scrubStartAddr, UINT64);
    SETGET_PROP(bLwstomStartAddr, bool);
    SETGET_PROP(bSkipClearBeforeStartingScrubber, bool);
private:
    Surface2D       m_vidmemSurface;
    GpuSubdevice   *m_pGpuSubdevice;
    UINT32          m_scrubSize;
    UINT32          m_scrubSizeMax;
    UINT32          m_scrubPattern;
    UINT64          m_scrubStartAddr;
    UINT64          m_scrubEndAddr;
    bool            m_bLwstomStartAddr;
    bool            m_bPrivSecEnabled;
    bool            m_bCpuCanWriteScrubberRegisters;
    bool            m_bSkipClearBeforeStartingScrubber;

    RC VerifyPattern(UINT32 pattern);
    RC TriggerHwScrubberManuallyForward(UINT32 surfStartAddrPhysical, UINT32 endAddrFWScrubbing, UINT32 scrubPattern);
    RC TriggerHwScrubberManuallyReverse(UINT32 startAddrBWScrubbing, UINT32 surfEndAddrPhysical, UINT32 scrubPattern);
    RC TriggerHwScrubberThroughSec(UINT32 surfStartAddrPhysical, UINT32 surfEndAddrPhysical, UINT32 scrubPattern);
    RC WaitForScrubbing(UINT64 startTimeNs, UINT64 surfStartAddrPhysical, UINT64 surfEndAddrPhysical, scrubType st);
    RC CheckStartAndEndAddr(UINT64 surfStartAddrPhysical, UINT64 surfEndAddrPhysical, UINT64 startTimeNs,
                                                                                             scrubType st);
};

//! \brief VprHwScrubberBasicTest constructor
//!
//! Doesn't do much except SetName.
//!
//! \sa Setup
//------------------------------------------------------------------------------
VprHwScrubberBasicTest::VprHwScrubberBasicTest()
{
    SetName("VprHwScrubberBasicTest");
    m_pGpuSubdevice    = nullptr;
    m_scrubSize        = DEFAULT_SCRUB_SIZE;
    m_scrubSizeMax     = DEFAULT_SCRUB_SIZE;
    m_scrubPattern     = DEFAULT_SCRUB_PATTERN;
    m_scrubStartAddr   = 0;
    m_scrubEndAddr     = m_scrubStartAddr + m_scrubSizeMax - 1;
    m_bLwstomStartAddr = true;
    m_bPrivSecEnabled  = false;
    m_bSkipClearBeforeStartingScrubber = false;
    m_bCpuCanWriteScrubberRegisters = false;
}

//! \brief VprHwScrubberBasicTest destructor
//!
//! Doesn't do much.
//!
//! \sa Cleanup
//------------------------------------------------------------------------------
VprHwScrubberBasicTest::~VprHwScrubberBasicTest()
{
}

//! \brief IsTestSupported
//!
//! Checks if the test can be run in the current environment.
//!
//! \return True if the test can be run in the current environment,
//!         false otherwise
//------------------------------------------------------------------------------
string VprHwScrubberBasicTest::IsTestSupported()
{
    m_pGpuSubdevice = GetBoundGpuSubdevice();
    if (!(m_pGpuSubdevice->HasFeature(Device::GPUSUB_HAS_SELWRE_FB_SCRUBBER)))
    {
        return "Test only supported on GP100 or later chips";
    }
    return RUN_RMTEST_TRUE;
}

//! \brief Do Init from Js
//!
//! \return OK if no error happens, otherwise corresponding rc
//------------------------------------------------------------------------------
RC VprHwScrubberBasicTest::Setup()
{
    RC rc;
    RegHal &regs = GetBoundGpuSubdevice()->Regs();
    // Alloc surface and map to cpu virtual memory.
    m_vidmemSurface.SetForceSizeAlloc(true);
    m_vidmemSurface.SetLayout(Surface2D::Pitch);
    m_vidmemSurface.SetColorFormat(ColorUtils::X8R8G8B8); // each pixel is 32bits
    m_vidmemSurface.SetLocation(Memory::Fb);
    m_vidmemSurface.SetPhysContig(true);
    m_vidmemSurface.SetArrayPitchAlignment(256);
    m_vidmemSurface.SetArrayPitch(1);
    m_vidmemSurface.SetArraySize(m_scrubSize);
    m_vidmemSurface.SetAddressModel(Memory::Paging);
    m_vidmemSurface.SetAlignment(1<<12);
    m_scrubStartAddr   = ((LwU64)regs.Read32(MODS_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_MAX_RANGE_START_ADDR_MB_ALIGNED)) << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;
    m_scrubSizeMax     = ((LwU64)regs.Read32(MODS_PGC6_BSI_VPR_SELWRE_SCRATCH_13_MAX_VPR_SIZE_MB))  << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;

    if (m_bLwstomStartAddr)
    {
        m_vidmemSurface.SetGpuPhysAddrHint(m_scrubStartAddr);
        m_vidmemSurface.SetGpuPhysAddrHintMax(m_scrubEndAddr);
    }

    Printf(Tee::PriHigh, "%s: Allocating a surface in vidmem of size 0x%x\n", __FUNCTION__, m_scrubSize);

    CHECK_RC(m_vidmemSurface.Alloc(GetBoundGpuDevice()));
    CHECK_RC(m_vidmemSurface.Map());

    if (!m_bSkipClearBeforeStartingScrubber)
    {
        UINT32 clearPattern = 0x12345678;
        if (clearPattern == m_scrubPattern)
        {
            //
            // In case the user supplied scrub pattern matches with our clear pattern, let's
            // just making sure that our clearPattern is different than the scrub pattern
            //
            clearPattern = ~clearPattern;
        }

        Printf(Tee::PriHigh, "%s: Let's first fill the surface with pattern 0x%x by writing it from CPU side\n",
                __FUNCTION__, clearPattern);

        CHECK_RC(m_vidmemSurface.Fill(clearPattern));
        CHECK_RC(Platform::FlushCpuWriteCombineBuffer());
        CHECK_RC(VerifyPattern(clearPattern));
    }
    if (DEFAULT_SCRUB_SIZE != m_scrubSize)
    {
        Printf(Tee::PriHigh, "%s: Using custom scrub size           : 0x%x\n", __FUNCTION__, m_scrubSize);
    }
    if (DEFAULT_SCRUB_PATTERN != m_scrubPattern)
    {
        Printf(Tee::PriHigh, "%s: Using custom scrub pattern        : 0x%x\n", __FUNCTION__, m_scrubPattern);
    }
    if (m_bLwstomStartAddr)
    {
        Printf(Tee::PriHigh, "%s: Using custom scrub start address  : 0x%llx\n", __FUNCTION__, m_scrubStartAddr);
    }

    if (regs.TestField(regs.Read32(MODS_FUSE_OPT_PRIV_SEC_EN_DATA), MODS_FUSE_OPT_PRIV_SEC_EN_DATA_YES))
    {
        m_bPrivSecEnabled = true;
    }
    else
    {
        m_bPrivSecEnabled = false;
    }
    if (regs.TestField(regs.Read32(MODS_PFB_NISO_SCRUB_PRIV_LEVEL_MASK),
                                   MODS_PFB_NISO_SCRUB_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_ENABLE))
    {
        m_bCpuCanWriteScrubberRegisters = true;
    }

    return rc;
}

//! \Wait for scrubbing to take place
//!
//!
//------------------------------------------------------------------------------
RC VprHwScrubberBasicTest::WaitForScrubbing(UINT64 startTimeNs, UINT64 surfStartAddrPhysical,
                                            UINT64 surfEndAddrPhysical, scrubType st)
{

    UINT64 endTimeNs, totalTimeConsumedNs;
    RC rc;
    RegHal   &regs = GetBoundGpuSubdevice()->Regs();
    UINT32 count = 0;
    while (regs.TestField(regs.Read32(MODS_PFB_NISO_SCRUB_STATUS_FLAG),
                          MODS_PFB_NISO_SCRUB_STATUS_FLAG_PENDING))
    {
        if (!(++count % 100000))
        {
            Printf(Tee::PriLow, "%s: Waiting for scrub to finish\n", __FUNCTION__);
        }

        //
        // During forward scrubbing, start address is constant but end address is changing as scrubbing progresses.
        // We check this behaviour by range check for varying address and stability check for constant address.
        //
        if ( st == Forward && ((regs.Read32(MODS_PFB_NISO_SCRUB_LAST_ADDR_STATUS) < surfStartAddrPhysical)   ||
                               (regs.Read32(MODS_PFB_NISO_SCRUB_LAST_ADDR_STATUS) > surfEndAddrPhysical)))
        {
             Printf(Tee::PriLow, "%s: Forward Scrubbing : Value of Last Addr Status is not in range 0x%llx and 0x%llx\n",__FUNCTION__,
                                  surfStartAddrPhysical,
                                  surfEndAddrPhysical);
        }


        if ( st == Forward && (regs.Read32(MODS_PFB_NISO_SCRUB_START_ADDR_STATUS) != surfStartAddrPhysical))
        {
             Printf(Tee::PriLow, "%s: Forward Scrubbing : Value of Start Addr Status is not fixed with value 0x%llx\n",__FUNCTION__,
                              surfStartAddrPhysical);
        }

        //
        // During reverse scrubbing, end address is constant but start address is changing as scrubbing progresses.
        // We check this behaviour by range check for varying address and stability check for constant address.
        //
        if ( st == Reverse && ((regs.Read32(MODS_PFB_NISO_SCRUB_START_ADDR_STATUS) < surfStartAddrPhysical) ||
                               (regs.Read32(MODS_PFB_NISO_SCRUB_START_ADDR_STATUS) > surfEndAddrPhysical)))
        {
            Printf(Tee::PriLow, "%s: Reverse Scrubbing : Value of Start Addr Status is not in range 0x%llx and 0x%llx\n",__FUNCTION__,
                                  surfStartAddrPhysical,
                                  surfEndAddrPhysical);
        }

        if ( st == Reverse && (regs.Read32(MODS_PFB_NISO_SCRUB_LAST_ADDR_STATUS) != surfEndAddrPhysical))
        {
            Printf(Tee::PriLow, "%s: Reverse Scrubbing : Value of Last Addr Status is not fixed with value 0x%llx\n",__FUNCTION__,
                                   surfEndAddrPhysical);
        }

    }
    endTimeNs = Platform::GetTimeNS();
    totalTimeConsumedNs = endTimeNs - startTimeNs;

    Printf(Tee::PriHigh, "%s: HW scrubber done with scrubbing in %llu nanoseconds, pattern was 0x%x\n", __FUNCTION__, totalTimeConsumedNs, m_scrubPattern);

    return rc;
}

RC VprHwScrubberBasicTest::CheckStartAndEndAddr(UINT64 surfStartAddrPhysical,
                                                UINT64 surfEndAddrPhysical,
                                                UINT64 startTimeNs,
                                                scrubType st
                                               )
{
    UINT64 temp;
    RegHal   &regs = GetBoundGpuSubdevice()->Regs();
    RC rc;

    // Let us wait for the scrubbing to finish
    CHECK_RC(WaitForScrubbing(startTimeNs, surfStartAddrPhysical, surfEndAddrPhysical, st));

    //
    // Let's read the 'read-only' registers in which scrubber reports the start and end addresses that were
    // actually scrubbed.
    //
    if (( temp = (UINT64)(regs.Read32(MODS_PFB_NISO_SCRUB_START_ADDR_STATUS))) != surfStartAddrPhysical)
    {
        Printf(Tee::PriHigh, "%s: Start addr mismatch, we programmed 0x%llx as start addr, scrubber reports it scrubbed from 0x%llx as start addr\n", __FUNCTION__, surfStartAddrPhysical, temp);
        rc = RC::ILWALID_ADDRESS;
    }

    if (( temp = (UINT64)(regs.Read32(MODS_PFB_NISO_SCRUB_LAST_ADDR_STATUS))) != surfEndAddrPhysical)
    {
        Printf(Tee::PriHigh, "%s: End addr mismatch, we programmed 0x%llx as end addr, scrubber reports it scrubbed until 0x%llx as end addr\n", __FUNCTION__, surfEndAddrPhysical, temp);
        rc = RC::ILWALID_ADDRESS;
    }

    return rc;
}

//! \brief Run the test
//!
//! \return OK all tests pass, else corresponding rc
//------------------------------------------------------------------------------
RC VprHwScrubberBasicTest::Run()
{
    PHYSADDR physAddr;
    UINT32   surfSize = UNSIGNED_CAST(UINT32, m_vidmemSurface.GetSize());
    UINT32   chipId;
    UINT64   surfStartAddrPhysical, surfEndAddrPhysical, frwdScrubLastAddr;
    UINT64   startTimeNs;
    RC       rc;
    CHECK_RC(m_vidmemSurface.GetDevicePhysAddress(0, Surface2D::GPUVASpace, &physAddr));

    surfStartAddrPhysical = (UINT64)physAddr;
    surfEndAddrPhysical   = surfStartAddrPhysical + surfSize - 1;

    Printf(Tee::PriHigh, "%s: Surface start addr (physical) = 0x%llx\n", __FUNCTION__, surfStartAddrPhysical);
    Printf(Tee::PriHigh, "%s: Surface end   addr (physical) = 0x%llx\n", __FUNCTION__, surfEndAddrPhysical);

    surfStartAddrPhysical >>= 12;
    surfEndAddrPhysical >>= 12;

    Printf(Tee::PriHigh, "%s: Surface start addr (physical, 4K aligned) = 0x%llx\n", __FUNCTION__, surfStartAddrPhysical);
    Printf(Tee::PriHigh, "%s: Surface end   addr (physical, 4K aligned) = 0x%llx\n", __FUNCTION__, surfEndAddrPhysical);

        // Note start time for Forward scrubbing / scrubbing via sec
    startTimeNs = Platform::GetTimeNS();

    if (!m_bPrivSecEnabled || m_bCpuCanWriteScrubberRegisters)
    {
        // Trigger scrubber directly via register writes
        Printf(Tee::PriHigh, "%s: Priv security is disabled or we can write scrubber registers directly, we will trigger HW scrubber manually\n", __FUNCTION__);

        chipId = GetBoundGpuSubdevice()->DeviceId();
        if (chipId >= Gpu::TU102)
        {
          // Callwlating mid address which will be mid point for two way scrubbing
          frwdScrubLastAddr = (surfStartAddrPhysical + surfEndAddrPhysical)/2;
        }
        else
        {
          frwdScrubLastAddr = surfEndAddrPhysical;
        }

        // Trigger HW Scrubber in Forward direction
        CHECK_RC(TriggerHwScrubberManuallyForward((UINT32)surfStartAddrPhysical, (UINT32)frwdScrubLastAddr,
                                                                                             m_scrubPattern));
        CHECK_RC(CheckStartAndEndAddr(surfStartAddrPhysical, frwdScrubLastAddr, startTimeNs, Forward));

        // Reverse scrubbing has been introduced Turing onwards and thus below will be
        // skipped for pre Turing chips
        if (chipId >= Gpu::TU102)
        {
            // Note start time for reverse scrubbing
            startTimeNs = Platform::GetTimeNS();

            // Trigger HW Scrubber in Reverse direction
            CHECK_RC(TriggerHwScrubberManuallyReverse((UINT32)(frwdScrubLastAddr + 1), (UINT32)surfEndAddrPhysical,
                                                                                             m_scrubPattern));
            CHECK_RC(CheckStartAndEndAddr(frwdScrubLastAddr+1, surfEndAddrPhysical, startTimeNs, Reverse));
        }
   }
    else
    {
        // Trigger scrubber via SEC
        Printf(Tee::PriHigh, "%s: Priv security is enabled, we will trigger HW scrubber through SEC engine\n", __FUNCTION__);

        CHECK_RC(TriggerHwScrubberThroughSec((UINT32)surfStartAddrPhysical, (UINT32)surfEndAddrPhysical, m_scrubPattern));
        CHECK_RC(CheckStartAndEndAddr(surfStartAddrPhysical, surfEndAddrPhysical, startTimeNs, ThroughSec));

    }

    // Now lets read the FB and verify that the pattern we wrote via scrubber is actually present in the FB
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());

    // Unmap the current CPU virtual address mapping.
    m_vidmemSurface.Unmap();

    // Remap the FB again.
    CHECK_RC(m_vidmemSurface.Map());
    CHECK_RC(Platform::FlushCpuWriteCombineBuffer());
    CHECK_RC(VerifyPattern(m_scrubPattern));

    Printf(Tee::PriHigh, "%s: Scrubber test passed\n", __FUNCTION__);

    return rc;
}

RC VprHwScrubberBasicTest::VerifyPattern(UINT32 pattern)
{
    UINT32   surfSize = UNSIGNED_CAST(UINT32, m_vidmemSurface.GetSize());
    RC       rc       = OK;

    UINT32   *surfAddr = (UINT32*)m_vidmemSurface.GetAddress();

    Printf(Tee::PriHigh, "%s: Verifying that the surface is filled with pattern 0x%x\n", __FUNCTION__, pattern);

    for (UINT32 i = 0; i < surfSize/sizeof(UINT32); i++)
    {
        if (MEM_RD32(surfAddr + i) != pattern)
        {
            Printf(Tee::PriHigh, "%s: Verification failed for pattern 0x%x, at offset 0x%x, pattern read from memory = 0x%x, expected = 0x%x\n", __FUNCTION__, pattern, i*4, MEM_RD32(surfAddr +i), pattern);
            rc = RC::BAD_MEMORY;
        }
    }
    if (rc == OK)
    {
        Printf(Tee::PriHigh, "%s: Verification passed for pattern 0x%x, surface is correctly filled with this pattern\n", __FUNCTION__, pattern);
    }

    return rc;
}


//! \brief Run the HW Scrubber in Forward direction
//! \params
//!    surfStartAddrPhysical - Start Addr for scrubbing , START_ADDR_STATUS is latched to this
//!    endAddrFWScrubbing    - Last Addr to scrub, LAST_ADDR_STATUS inc from Start Addr to this end Addr
//!    scrubPattern          - Pattern used by scrubber to scrub
//! \return OK, else corresponding rc
//------------------------------------------------------------------------------
RC
VprHwScrubberBasicTest::TriggerHwScrubberManuallyForward
(
    UINT32  surfStartAddrPhysical,
    UINT32  endAddrFWScrubbing,
    UINT32  scrubPattern
)
{
    RC    rc;
    LwU32 cmd;
    RegHal &regs = GetBoundGpuSubdevice()->Regs();
    UINT32 chipId = GetBoundGpuSubdevice()->DeviceId();

    // If any previous scrub is pending, wait for it to complete
    UINT32 count = 0;

    while (regs.TestField(regs.Read32(MODS_PFB_NISO_SCRUB_STATUS_FLAG), MODS_PFB_NISO_SCRUB_STATUS_FLAG_PENDING))
    {
        if (!(++count % 100000))
        {
            Printf(Tee::PriLow, "%s: Scrubber was already engaged, waiting for the previous scrub to finish\n",
                                                                                                  __FUNCTION__);
        }
    }

    // Program the start addr, end addr and the pattern into the scrubber
    regs.Write32(MODS_PFB_NISO_SCRUB_START_ADDR, surfStartAddrPhysical);
    regs.Write32(MODS_PFB_NISO_SCRUB_END_ADDR, endAddrFWScrubbing);
    regs.Write32(MODS_PFB_NISO_SCRUB_PATTERN, scrubPattern);

    Printf(Tee::PriHigh, "%s: Triggering HW scrubber in forward direction now with pattern = 0x%x\n",
                                                                       __FUNCTION__, m_scrubPattern);

    // Trigger the scrub in forward direction
    cmd = regs.Read32(MODS_PFB_NISO_SCRUB_TRIGGER);
    regs.SetField((LwU32 *)&cmd, MODS_PFB_NISO_SCRUB_TRIGGER_FLAG_ENABLED);
    if (chipId >= Gpu::TU102)
    {
        regs.SetField((LwU32 *)&cmd, MODS_PFB_NISO_SCRUB_TRIGGER_DIRECTION_LOW_TO_HIGH);
    }
    regs.Write32(MODS_PFB_NISO_SCRUB_TRIGGER, (LwU32) cmd);

    return OK;
}

//! \brief Run the HW Scrubber in Reverse direction
//! \params
//!    surfStartAddrPhysical - Last Addr for scrubbing , LAST_ADDR_STATUS is latched to this
//!    endAddrFWScrubbing    - First Addr to scrub, START_ADDR_STATUS dec from this end Addr to Start Addr
//!    scrubPattern          - Pattern used by scrubber to scrub
//! \return OK, else corresponding rc
//------------------------------------------------------------------------------
RC
VprHwScrubberBasicTest::TriggerHwScrubberManuallyReverse
(
    UINT32  startAddrBWScrubbing,
    UINT32  surfEndAddrPhysical,
    UINT32  scrubPattern
)
{
    RC    rc;
    RegHal &regs = GetBoundGpuSubdevice()->Regs();
    LwU32  cmd;

    // If any previous scrub is pending, wait for it to complete
    UINT32 count = 0;
    while (regs.TestField(regs.Read32(MODS_PFB_NISO_SCRUB_STATUS_FLAG), MODS_PFB_NISO_SCRUB_STATUS_FLAG_PENDING))
    {
        if (!(++count % 100000))
        {
            Printf(Tee::PriLow, "%s: Scrubber was already engaged, waiting for the previous scrub to finish\n",
                                                                                                  __FUNCTION__);
        }
    }

    // Program the start addr, end addr and the pattern into the scrubber
    regs.Write32(MODS_PFB_NISO_SCRUB_START_ADDR, startAddrBWScrubbing);
    regs.Write32(MODS_PFB_NISO_SCRUB_END_ADDR, surfEndAddrPhysical);
    regs.Write32(MODS_PFB_NISO_SCRUB_PATTERN, scrubPattern);

    Printf(Tee::PriHigh, "%s: Triggering HW scrubber in reverse direction now with pattern = 0x%x\n",
                                                                       __FUNCTION__, m_scrubPattern);

    cmd = regs.Read32(MODS_PFB_NISO_SCRUB_TRIGGER);
    // Trigger the scrub in reverse direction
    regs.SetField((LwU32 *)&cmd, MODS_PFB_NISO_SCRUB_TRIGGER_FLAG_ENABLED);
    regs.SetField((LwU32 *)&cmd, MODS_PFB_NISO_SCRUB_TRIGGER_DIRECTION_HIGH_TO_LOW);

    regs.Write32(MODS_PFB_NISO_SCRUB_TRIGGER, (LwU32) cmd);

    return OK;
}

RC
VprHwScrubberBasicTest::TriggerHwScrubberThroughSec
(
    UINT32  surfStartAddrPhysical,
    UINT32  surfEndAddrPhysical,
    UINT32  scrubPattern
)
{
    // TODO needs to be implemented once SEC code is in place.
    MASSERT(!"%s: This functionality isn't implemented yet");
    return RC::UNSUPPORTED_FUNCTION;
}

//! \brief Cleanup, does nothing for this test
//------------------------------------------------------------------------------
RC VprHwScrubberBasicTest::Cleanup()
{
    m_vidmemSurface.Free();
    return OK;
}

//------------------------------------------------------------------------------
// JS Linkage
//------------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Setup the JS hierarchy to match the C++ one.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(VprHwScrubberBasicTest, RmTest, "VPR HW scrubber test\n");

CLASS_PROP_READWRITE(VprHwScrubberBasicTest, scrubSize, UINT32, "Scrub size in bytes, default = 0x10000 (64KB)");
CLASS_PROP_READWRITE(VprHwScrubberBasicTest, scrubPattern, UINT32, "Scrub pattern, default = 0xAA55AA55");
CLASS_PROP_READWRITE(VprHwScrubberBasicTest, scrubStartAddr, UINT64, "Scrub start address to be used as hint, default is whatever is allocated by RM");
CLASS_PROP_READWRITE(VprHwScrubberBasicTest, bLwstomStartAddr, bool, "True if scrub start address passed from command line");
CLASS_PROP_READWRITE(VprHwScrubberBasicTest, bSkipClearBeforeStartingScrubber, bool, "True if we should skip the clearing of the range before triggering the scrubber");
