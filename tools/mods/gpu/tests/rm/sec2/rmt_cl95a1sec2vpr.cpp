/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// Class95a1Sec2Vpr test.
//

#include "rmt_cl95a1sec2.h"
#include "core/include/gpu.h"
#include "gpu/include/gpusbdev.h"
#include "maxwell/gm107/dev_fuse.h"
#include "random.h"
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include "turing/tu102/dev_fb.h"
#include "mmu/mmucmn.h"
#include "gpu/utility/sec2rtossub.h"
#include "mods_reg_hal.h"

#define VPR_REGION_SIZE                         4
#define VPR_CARVEOUT_REGION_SIZE_MB(size)       (size << 20)

/*!
 * Class95a1Sec2VprTest
 *
 */
/*!
 * Construction and desctructors
 */
Class95a1Sec2VprTest::Class95a1Sec2VprTest() :
    m_hObj(0)
{
    SetName("Class95a1Sec2VprTest");
}

Class95a1Sec2VprTest::~Class95a1Sec2VprTest()
{
}

/*!
 * IsTestSupported
 *
 * Need class 95A1 to be supported
 */
string Class95a1Sec2VprTest::IsTestSupported()
{
    // TODO: Check fuse as well.
    if (!IsClassSupported(LW95A1_TSEC))
        return "LW95A1 class is not supported on current platform";

    return RUN_RMTEST_TRUE;
}

/*!
 * AllocateMem
 *
 * allocates surface
 */
RC Class95a1Sec2VprTest::AllocateMem(Sec2MemDesc *pMemDesc, LwU32 size)
{
    Surface2D *pVidMemSurface = &(pMemDesc->surf2d);
    pVidMemSurface->SetForceSizeAlloc(1);
    pVidMemSurface->SetLayout(Surface2D::Pitch);
    pVidMemSurface->SetColorFormat(ColorUtils::Y8);
    pVidMemSurface->SetLocation(Memory::Coherent);
    pVidMemSurface->SetArrayPitchAlignment(256);
    pVidMemSurface->SetArrayPitch(1);
    pVidMemSurface->SetArraySize(size);
    pVidMemSurface->SetAddressModel(Memory::Paging);
    pVidMemSurface->SetLayout(Surface2D::Pitch);

    pVidMemSurface->Alloc(GetBoundGpuDevice());
    pVidMemSurface->Map();

    pMemDesc->gpuAddrSysMem = pVidMemSurface->GetCtxDmaOffsetGpu();
    pMemDesc->pCpuAddrSysMem = (LwU32*)pVidMemSurface->GetAddress();
    pMemDesc->size = size;

    return OK;
}

/*!
 * @brief: Allocate VPR surface memdesc
 *
 * @param[in] pMemDesc:     memory desc
 * @param[in] size          size of memory desc
 * @param[in] addrHint      Address hint
 */
RC Class95a1Sec2VprTest::AllocateVprMemdesc(Sec2MemDesc *pMemDesc, LwU32 size, LwU64 addrHint, LwU64 addrHintMax)
{
    RC rc;

    Surface2D *pVidMemSurface = &(pMemDesc->surf2d);
    pVidMemSurface->SetForceSizeAlloc(1);
    pVidMemSurface->SetPageSize(4);
    pVidMemSurface->SetLayout(Surface2D::Pitch);
    pVidMemSurface->SetColorFormat(ColorUtils::VOID32);
    pVidMemSurface->SetLocation(Memory::Fb);
    pVidMemSurface->SetArrayPitchAlignment(256);
    pVidMemSurface->SetArrayPitch(1);
    pVidMemSurface->SetArraySize(size);
    pVidMemSurface->SetAddressModel(Memory::Paging);
    pVidMemSurface->SetAlignment(1<20);
    pVidMemSurface->SetPhysContig(true);
    pVidMemSurface->SetLayout(Surface2D::Pitch);
    pVidMemSurface->SetGpuPhysAddrHint(addrHint);
    pVidMemSurface->SetGpuPhysAddrHintMax(addrHintMax);
    CHECK_RC(pVidMemSurface->Alloc(GetBoundGpuDevice()));
    CHECK_RC(pVidMemSurface->Map());

    pMemDesc->gpuAddrSysMem = pVidMemSurface->GetCtxDmaOffsetGpu();
    pMemDesc->pCpuAddrSysMem = (LwU32*)pVidMemSurface->GetAddress();
    pMemDesc->size = size;

    return OK;
}

/*!
 * FreeMem
 *
 * Frees the above allocated memory
 */
RC Class95a1Sec2VprTest::FreeMem(Sec2MemDesc *pMemDesc)
{
    (pMemDesc->surf2d).Free();
    return OK;
}

/*!
 * Setup
 *
 * Setup function for Class95a1Sec2VprTest
 */
RC Class95a1Sec2VprTest::Setup()
{

    LwRmPtr pLwRm;
    RC      rc;
    LwU64   vprMinAddrInBytes    = 0;
    LwU64   vprEndAddressInBytes = 0;
    LwU64   vprSizeInBytes       = 0;

    CHECK_RC(InitFromJs());

    m_pSubdev     = GetBoundGpuSubdevice();
    RegHal &regs  = GetBoundGpuSubdevice()->Regs();
    m_TimeoutMs   = m_TestConfig.TimeoutMs();

    GetBoundGpuSubdevice()->GetSec2Rtos(&m_pSec2Rtos);

    // Allocate Memory for Main class. Allocate big enough to support 1K/2K for both READ/GEN mode
    AllocateMem(&m_semMemDesc, 0x4);
    vprMinAddrInBytes    = ((LwU64)regs.Read32(MODS_PGC6_BSI_VPR_SELWRE_SCRATCH_15_VPR_MAX_RANGE_START_ADDR_MB_ALIGNED)) << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;
    vprSizeInBytes       = ((LwU64)regs.Read32(MODS_PGC6_BSI_VPR_SELWRE_SCRATCH_13_MAX_VPR_SIZE_MB))  << VPR_ADDR_ALIGN_IN_BSI_1MB_SHIFT;
    vprEndAddressInBytes = vprMinAddrInBytes + vprSizeInBytes - 1;

    AllocateVprMemdesc(&m_resMemDesc, VPR_CARVEOUT_REGION_SIZE_MB(VPR_REGION_SIZE), vprMinAddrInBytes, vprEndAddressInBytes);
    AllocateMem(&m_methodMemDesc, sizeof(vpr_program_region_param)<<2);

    return rc;
}

LwU64 Class95a1Sec2VprTest::GetPhysicalAddress(const Surface& surf, LwU64 offset)
{
    LwRmPtr pLwRm;
    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};

    params.memOffset = offset;
    RC rc = pLwRm->Control(
            surf.GetMemHandle(),
            LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
            &params, sizeof(params));

    MASSERT(rc == OK);
    if (rc != OK)
        Printf(Tee::PriHigh, "GetPhysicalAddress failed: %s\n", rc.Message());

    return params.memOffset;
}

/*!
 * AllocateProtected
 *
 * sends method to SEC engine to program the VPR region
 */
RC Class95a1Sec2VprTest::AllocateProtected(Sec2MemDesc *pMemDesc)
{
    RC       rc = OK;
    PHYSADDR physAddr;
    LwU32    startAddr;
    LwU32    size;

    Surface2D *pVidMemSurface = &(pMemDesc->surf2d);
    pVidMemSurface->GetDevicePhysAddress(0, Surface2D::GPUVASpace, &physAddr);

    // SEC2 expects start address in 4K and VPR size in MB
    startAddr = (LwU32)(((LwU64)physAddr)>> LW_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT);
    size      = VPR_REGION_SIZE;

    Printf(Tee::PriHigh, "VPR: Allocating region, start address 0x%x, size: 0x%x MB \n",
            startAddr, size);

    Sec2RtosVprTestCmd(startAddr, size);

    return rc;
}

/*!
 * DestroyProtected
 *
 * Clears the VPR region registers
 */
RC Class95a1Sec2VprTest::DestroyProtected()
{
    RC       rc = OK;
    LwU32    startAddr;
    LwU32    size;
    PHYSADDR physAddr;

    startAddr = 0;
    size      = 0;

    Surface2D *pVidMemSurface = &(m_resMemDesc.surf2d);
    pVidMemSurface->GetDevicePhysAddress(0, Surface2D::GPUVASpace, &physAddr);
    startAddr = (LwU32)(((LwU64)physAddr)>> LW_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT);

    Printf(Tee::PriHigh, "VPR: Destroying region, start address 0x%x, size: 0x%x MB \n",
            startAddr, size);

    Sec2RtosVprTestCmd(startAddr, 0);

    return rc;
}

/*!
 * VerifyProtected
 *
 * Verifies the allocated region with VPR registers
 */
RC Class95a1Sec2VprTest::VerifyProtected(Sec2MemDesc *pMemDesc)
{
    LwU64    cmd = 0;
    LwU64    addrLo;
    LwU64    addrHi;
    LwU64    startAddr;
    LwU32    allocSizeMB;
    PHYSADDR physAddr;
    RegHal   &regs  = GetBoundGpuSubdevice()->Regs();

    if (GetBoundGpuSubdevice()->DeviceId() >= Gpu::TU102)
    {
        cmd = regs.Read32(MODS_PFB_PRI_MMU_VPR_ADDR_LO_VAL);
        addrLo = cmd << LW_PFB_PRI_MMU_VPR_ADDR_LO_ALIGNMENT; // Halify this constant

        cmd = regs.Read32(MODS_PFB_PRI_MMU_VPR_ADDR_HI_VAL);
        addrHi = cmd << LW_PFB_PRI_MMU_VPR_ADDR_HI_ALIGNMENT;
    }
    else
    {
        regs.SetField((LwU32 *)&cmd, MODS_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_LO);
        regs.Write32(MODS_PFB_PRI_MMU_VPR_INFO, (LwU32) cmd);
        cmd = regs.Read32(MODS_PFB_PRI_MMU_VPR_INFO_DATA);

        addrLo = cmd << LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT; // Halify this constant

        cmd = 0;
        regs.SetField((LwU32 *)&cmd, MODS_PFB_PRI_MMU_VPR_INFO_INDEX_ADDR_HI);
        regs.Write32(MODS_PFB_PRI_MMU_VPR_INFO, (LwU32)cmd);
        cmd = regs.Read32(MODS_PFB_PRI_MMU_VPR_INFO_DATA);

        addrHi = cmd << LW_PFB_PRI_MMU_VPR_WPR_WRITE_ADDR_ALIGNMENT;
    }

    allocSizeMB = (LwU32)((addrHi - addrLo) >> 20);

    Surface2D *pVidMemSurface = &(pMemDesc->surf2d);
    pVidMemSurface->GetDevicePhysAddress(0, Surface2D::GPUVASpace, &physAddr);
    startAddr = (LwU64)physAddr;

    if (addrLo == startAddr && allocSizeMB == (VPR_REGION_SIZE - 1))
    {
        return OK;
    }

   return !OK;
}
//------------------------------------------------------------------------
RC Class95a1Sec2VprTest::Run()
{
    RC rc = OK;

    CHECK_RC(AllocateProtected(&m_resMemDesc));

    CHECK_RC(VerifyProtected(&m_resMemDesc));

    return rc;
}

//------------------------------------------------------------------------------
RC Class95a1Sec2VprTest::Cleanup()
{
    RC rc, firstRc;
    LwRmPtr pLwRm;

    DestroyProtected();

    FreeMem(&m_semMemDesc);
    FreeMem(&m_resMemDesc);
    FreeMem(&m_methodMemDesc);
    m_pSec2Rtos = NULL;
    return firstRc;
}

RC Class95a1Sec2VprTest::Sec2RtosVprTestCmd(LwU32 startAddr, LwU32 size)
{
    RC                      rc = OK;
    RM_FLCN_CMD_SEC2        sec2Cmd;
    RM_FLCN_MSG_SEC2        sec2Msg;
    UINT32                  seqNum;

    memset(&sec2Cmd, 0, sizeof(RM_FLCN_CMD_SEC2));
    memset(&sec2Msg, 0, sizeof(RM_FLCN_MSG_SEC2));

    // Send an VPR command to SEC2. 
    sec2Cmd.hdr.unitId               = RM_SEC2_UNIT_VPR;
    sec2Cmd.hdr.size                 = RM_SEC2_CMD_SIZE(VPR,SETUP_VPR);
    sec2Cmd.cmd.vpr.cmdType          = RM_SEC2_VPR_CMD_ID_SETUP_VPR;
    sec2Cmd.cmd.vpr.vprCmd.startAddr = startAddr;
    sec2Cmd.cmd.vpr.vprCmd.size      = size;

    Printf(Tee::PriHigh,
           "%d:Sec2RtosVprTest::%s: Submit a VPR command to SEC2...\n",
            __LINE__, __FUNCTION__);

    // submit the command
    rc = m_pSec2Rtos->Command(&sec2Cmd,
                              &sec2Msg,
                              &seqNum);
    CHECK_RC(rc);

    Printf(Tee::PriHigh,
           "%d:Sec2RtosVprTest::%s: CMD submission success, got seqNum = %u\n",
            __LINE__, __FUNCTION__, seqNum);

    //
    // In function SEC2::Command(), it waits message back if argument pMsg is
    // not NULL
    //

    // response message received, print out the details
    Printf(Tee::PriHigh,
           "%d:Sec2RtosVprTest::%s: Received command response from SEC2\n",
           __LINE__, __FUNCTION__);
    Printf(Tee::PriHigh,
           "%d:Sec2RtosVprTest::%s: Printing Header :-\n",
           __LINE__, __FUNCTION__);
    Printf(Tee::PriHigh,
           "unitID = 0x%x ,size = 0x%x ,ctrlFlags  = 0x%x ,seqNumID = 0x%x\n",
            sec2Msg.hdr.unitId, sec2Msg.hdr.size,
            sec2Msg.hdr.ctrlFlags, sec2Msg.hdr.seqNumId);

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ Class95a1Sec2VprTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(Class95a1Sec2VprTest, RmTest,
                 "Class95a1Sec2VprTest RM test.");

