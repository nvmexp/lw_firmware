/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009,2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "fermi_host_bar1_perf.h"

//! Constructor
//!
//! \param lwgpu Pointer to an LWGpuResource object to use for memory/register access. This pointer will not be freed by HostBar1PerfTest
FermiHostBar1PerfTest::FermiHostBar1PerfTest(LWGpuResource * lwgpu) : HostBar1PerfTest(lwgpu),
    pRegRemap1(NULL), pRegRemap2(NULL), pRegRemap4(NULL), pRegRemap6(NULL), pRegRemap7(NULL),
    pmRegOffset(0), pmRegStartVal(0), pmRegStopVal(0)
{
    GOB_BYTES_WIDTH = lwgpu->GetGpuDevice()->GobWidth();
    GOB_HEIGHT = lwgpu->GetGpuDevice()->GobHeight();
    GOB_BYTES = lwgpu->GetGpuDevice()->GobSize();
    MASSERT(GOB_BYTES_WIDTH * GOB_HEIGHT == GOB_BYTES);

    // Get some default register values based on Current GPU
    GpuSubdevice * pGpuSubdev = lwgpu->GetGpuSubdevice();
    MASSERT(pGpuSubdev != NULL);
    // Get pointers to all the bl_remapper registers.
    RefManual *pRefMan = pGpuSubdev->GetRefManual();
    MASSERT(pRefMan != NULL);
    pRegRemap1 = pRefMan->FindRegister("LW_PBUS_BL_REMAP_1");
    MASSERT(pRegRemap1 != NULL);
    pRegRemap2 = pRefMan->FindRegister("LW_PBUS_BL_REMAP_2");
    MASSERT(pRegRemap2 != NULL);
    pRegRemap4 = pRefMan->FindRegister("LW_PBUS_BL_REMAP_4");
    MASSERT(pRegRemap4 != NULL);
    pRegRemap6 = pRefMan->FindRegister("LW_PBUS_BL_REMAP_6");
    MASSERT(pRegRemap6 != NULL);
    pRegRemap7 = pRefMan->FindRegister("LW_PBUS_BL_REMAP_7");
    MASSERT(pRegRemap7 != NULL);

    // Get the offset of the LW_PERF_PMASYS_TRIGGER_GLOBAL register
    const RefManualRegister * pRegPmasys = pRefMan->FindRegister("LW_PERF_PMASYS_TRIGGER_GLOBAL");
    MASSERT(pRegPmasys != NULL);
    const RefManualRegisterField * pFieldPmasysTrigger = pRegPmasys->FindField("LW_PERF_PMASYS_TRIGGER_GLOBAL_MANUAL_START");
    MASSERT(pFieldPmasysTrigger != NULL);
    pmRegOffset = pRegPmasys->GetOffset();
    pmRegStartVal = pFieldPmasysTrigger->FindValue("LW_PERF_PMASYS_TRIGGER_GLOBAL_MANUAL_START_PULSE")->Value
                    << (pFieldPmasysTrigger->GetLowBit());
    // Start and stop both use the same pulse
    pmRegStopVal = pmRegStartVal;

}

//! \brief Set's up host's BL Remapper; param remapper is one of the eight remapper's index
void FermiHostBar1PerfTest::SetupHostBlRemapper(const UINT32 remapper,
    const MdiagSurf &surface, const MdiagSurf &blSurface, const UINT32 srcSize) const
{

    UINT32 blockHeightGobs = 1;
    UINT32 blockDepthGobs = DEPTH;
    UINT32 imgGobHeight = blImgHeightInGobs(srcSize);
    UINT32 imgGobPitch = IMG_PITCH/(GOB_BYTES_WIDTH);
    UINT32 totalSurfaceGobs = imgGobPitch * imgGobHeight;
    UINT32 imgBpp = 4;

    DebugPrintf("surf.GetAddress()           = 0x%x\n",surface.GetAddress());
    DebugPrintf("surf.GetPhysAddress()       = 0x%x\n",surface.GetPhysAddress());
    DebugPrintf("surf.GetCtxDmaOffsetGpu()   = 0x%x\n",surface.GetCtxDmaOffsetGpu());
    DebugPrintf("surf.GetVidMemOffset()      = 0x%x\n",surface.GetVidMemOffset());

    DebugPrintf("blSurf.GetAddress()         = 0x%x\n",blSurface.GetAddress());
    DebugPrintf("blSurf.GetPhysAddress()     = 0x%x\n",blSurface.GetPhysAddress());
    DebugPrintf("blSurf.GetCtxDmaOffsetGpu() = 0x%x\n",blSurface.GetCtxDmaOffsetGpu());
    DebugPrintf("blSurf.GetVidMemOffset()    = 0x%x\n",blSurface.GetVidMemOffset());

    DebugPrintf("GpuPtr()->PhysFbBase()      = 0x%x\n", lwgpu->PhysFbBase());

    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - srcSize:%d IMG_PITCH:%d GOB_HEIGHT:%d GOB_BYTES_WIDTH:%d\n",
                srcSize, IMG_PITCH, GOB_HEIGHT, GOB_BYTES_WIDTH);

    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - numGobs:%d img_height_in_gobs:%d img_width_in_gobs:%d BLOCK_HEIGHT:%d BLOCK_DEPTH:%d\n",
                totalSurfaceGobs, imgGobHeight, imgGobPitch, blockHeightGobs, blockDepthGobs );

    UINT32 writeVal = 0;
    writeVal |= totalSurfaceGobs << (pRegRemap2->FindField("LW_PBUS_BL_REMAP_2_SIZE")->GetLowBit());
    writeVal |= blockHeightGobs  << (pRegRemap2->FindField("LW_PBUS_BL_REMAP_2_BLOCK_HEIGHT")->GetLowBit());
    writeVal |= blockDepthGobs   << (pRegRemap2->FindField("LW_PBUS_BL_REMAP_2_BLOCK_DEPTH")->GetLowBit());
    // Setup the surface size
    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - writting register remap_2. value:: 0x%08x\n", writeVal);
    lwgpu->RegWr32(pRegRemap2->GetOffset(remapper), writeVal);

    UINT32 blockBase4 = ((blSurface.GetPhysAddress() - lwgpu->PhysFbBase()) >> 9);
    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - BL_REMAP_4_BLOCK_BASE: 0x%08x\n", blockBase4);

    writeVal = blockBase4 << (pRegRemap4->FindField("LW_PBUS_BL_REMAP_4_BLOCK_BASE")->GetLowBit());
    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - writting register remap_4. value:: 0x%08x\n", writeVal);
    lwgpu->RegWr32(pRegRemap4->GetOffset(remapper), writeVal);

    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - BL_REMAP_6_IMAGE_HEIGHT: %d\n", imgGobHeight);

    writeVal = imgGobHeight << (pRegRemap6->FindField("LW_PBUS_BL_REMAP_6_IMAGE_HEIGHT")->GetLowBit());
    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - writting register remap_6. value:: 0x%08x\n", writeVal);
    lwgpu->RegWr32(pRegRemap6->GetOffset(remapper), writeVal);

    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - BL_REMAP_7_PITCH: %d, BL_REMAP_7_NUM_BYTES_PIXEL: %d\n",
                imgGobPitch, imgBpp);

    writeVal  = imgGobPitch << (pRegRemap7->FindField("LW_PBUS_BL_REMAP_7_PITCH")->GetLowBit());
    writeVal |= imgBpp      << (pRegRemap7->FindField("LW_PBUS_BL_REMAP_7_NUM_BYTES_PIXEL")->GetLowBit());
    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - writting register remap_7. value:: 0x%08x\n", writeVal);
    lwgpu->RegWr32(pRegRemap7->GetOffset(remapper), writeVal);

    // Enable the Remapper, and set the base address
    UINT32 remap_base1 = ((surface.GetPhysAddress() - lwgpu->PhysFbBase()) >> 9);
    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - BL_REMAP_1_BASE: 0x%08x\n", remap_base1);
    writeVal  = remap_base1 << (pRegRemap1->FindField("LW_PBUS_BL_REMAP_1_BASE")->GetLowBit());
    UINT32 valEnable = pRegRemap1->FindField("LW_PBUS_BL_REMAP_1_BL_ENABLE")->FindValue("LW_PBUS_BL_REMAP_1_BL_ENABLE_ON")->Value;
    writeVal |= valEnable << (pRegRemap1->FindField("LW_PBUS_BL_REMAP_1_BL_ENABLE")->GetLowBit());
    DebugPrintf("host_bar1_perfTest::SetupHostBLRemapper() - writting register remap_1. value:: 0x%08x\n", writeVal);
    lwgpu->RegWr32(pRegRemap1->GetOffset(remapper), writeVal);

}

//! Backs up the current settings of the passed in remapper.
void FermiHostBar1PerfTest::BackupBlRemapperRegisters(const UINT32 remapper)
{
    backupBlRegister.remap_1 = lwgpu->RegRd32(pRegRemap1->GetOffset(remapper));
    backupBlRegister.remap_2 = lwgpu->RegRd32(pRegRemap2->GetOffset(remapper));
    backupBlRegister.remap_4 = lwgpu->RegRd32(pRegRemap4->GetOffset(remapper));
    backupBlRegister.remap_6 = lwgpu->RegRd32(pRegRemap6->GetOffset(remapper));
    backupBlRegister.remap_7 = lwgpu->RegRd32(pRegRemap7->GetOffset(remapper));
}

//! Restores the previous settings of the passed in remapper.
void FermiHostBar1PerfTest::RestoreBlRemapperRegisters(const UINT32 remapper) const
{
    lwgpu->RegWr32(pRegRemap1->GetOffset(remapper), backupBlRegister.remap_1);
    lwgpu->RegWr32(pRegRemap2->GetOffset(remapper), backupBlRegister.remap_2);
    lwgpu->RegWr32(pRegRemap4->GetOffset(remapper), backupBlRegister.remap_4);
    lwgpu->RegWr32(pRegRemap6->GetOffset(remapper), backupBlRegister.remap_6);
    lwgpu->RegWr32(pRegRemap7->GetOffset(remapper), backupBlRegister.remap_7);
}
//! Uses register writes to start the PmCounters
void FermiHostBar1PerfTest::StartPmTriggers(void) const
{
    UINT32 pmRegOrigVal = lwgpu->RegRd32(pmRegOffset);
    lwgpu->RegWr32( pmRegOffset, pmRegOrigVal | pmRegStartVal );
}
//! Uses register writes to stop the PmCounters
void FermiHostBar1PerfTest::StopPmTriggers(void) const
{
    UINT32 pmRegOrigVal = lwgpu->RegRd32(pmRegOffset);
    lwgpu->RegWr32( pmRegOffset, pmRegOrigVal | pmRegStopVal );
    GpuDevice* const dev = lwgpu->GetGpuDevice();
    for (UINT32 subdev = 0; subdev < dev->GetNumSubdevices(); ++subdev) {
        dev->GetSubdevice(subdev)->StopVpmCapture();
    }
}
