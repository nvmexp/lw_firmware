/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2013,2016-2021 by LWPU Corporation.  All rights 
 * reserved. All information contained herein is proprietary and confidential 
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the 
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "fb_hub_mmu_ilwalidate.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"
#include "kepler/gk104/dev_flush.h"
#include "sim/IChip.h"
#include "mdiag/utils/lwgpu_classes.h"
#include "class/cl906f.h"
#include "gpu/include/gpumgr.h"

/*
  1) Alloc&Map surf A, B on FB.
  2) Write 'A' to surf A, 'B' to surf B
  3) Unmap& Free surf B, A
  4) Ilwalidate (Hub MMU Only for ctc coverage)
  5) Reallocate&Map surf A,B on FB. (interchanging their gpu address by setting AddressHint)
  6) Read surf A and surf B
  7) Error if surf A =='A' andr surf B == 'B'.
*/

extern const ParamDecl fb_hub_mmu_ilwalidate_params[] = {
    PARAM_SUBDECL(lwgpu_single_params),
    SIMPLE_PARAM("-noIlwalidate", "do not ilwalidate myself (however RM will do it anyway)"),
    SIMPLE_PARAM("-IlwalidateCoh", "Ilwalidate pte on sysmem (must be used with -pte_coh only"),
    LAST_PARAM
};

fb_hub_mmu_ilwalidateTest::fb_hub_mmu_ilwalidateTest(ArgReader *reader) :
    LWGpuSingleChannelTest(reader)
{
    params = reader;

    no_ilwalidate = params->ParamPresent("-noIlwalidate") != 0;
    ilwal_coh = params->ParamPresent("-IlwalidateCoh") != 0;
}

fb_hub_mmu_ilwalidateTest::~fb_hub_mmu_ilwalidateTest(void)
{
}

STD_TEST_FACTORY(fb_hub_mmu_ilwalidate, fb_hub_mmu_ilwalidateTest)

// a little extra setup to be done
int fb_hub_mmu_ilwalidateTest::Setup(void)
{
    //_DMA_TARGET target;
    local_status = 1;

    DebugPrintf("fb_hub_mmu_ilwalidateTest::Setup() starts\n");

    getStateReport()->enable();
    subChannel = SUB_CHANNEL;

    GpuDevice* pGpuDevice = ((GpuDevMgr*)DevMgrMgr::d_GraphDevMgr)->GetFirstGpuDevice();
    GpuSubdevice* pGpuSubdevice = pGpuDevice->GetSubdevice(0);

    if (!SetupLWGpuResource(EngineClasses::GetClassVec("Ce").size(),
            EngineClasses::GetClassVec("Ce").data()))
    {
        return 0;
    }

    UINT32 engineId = LW2080_ENGINE_TYPE_NULL;
    if (pGpuSubdevice->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
    {
        vector<UINT32> supportedCEs;
        if (OK != lwgpu->GetSupportedCeNum(pGpuSubdevice, &supportedCEs, LwRmPtr().Get()))
        {
            ErrPrintf("Error in retrieving supported CEs\n");
            SetStatus(Test::TEST_NO_RESOURCES);
            return(0);
        }
        MASSERT(supportedCEs.size() != 0);
        engineId = MDiagUtils::GetCopyEngineId(supportedCEs[0]);
    }

    // call parent's Setup first
    if (!SetupSingleChanTest(engineId))
    {
        return 0;
    }

    // we need a destination surface in FB, let's allocate it.
    DebugPrintf("Create SurfA\n");
    surfA.SetArrayPitch(32);
    surfA.SetColorFormat(ColorUtils::Y8);
    surfA.SetForceSizeAlloc(true);
    surfA.SetLocation(Memory::Fb);          // Destination Vidmem
    surfA.SetGpuCacheMode(Surface::GpuCacheOn);         // L2 Caching
    if (OK != surfA.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(surfA);

    if (OK != surfA.Map()) {
        ErrPrintf("can't map the destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    /*
    m_gpu_addrA_old = surfA.GetCtxDmaOffsetGpu();
    InfoPrintf("SurfA = 0x%x\n",m_gpu_addrA_old);
    */
    m_gpu_addrA_old = surfA.GetPhysAddress();
    InfoPrintf("SurfA = 0x%x\n",m_gpu_addrA_old);

    // we need a destination surface in FB, let's allocate it.
    DebugPrintf("Create SurfB\n");
    surfB.SetArrayPitch(32);
    surfB.SetColorFormat(ColorUtils::Y8);
    surfB.SetForceSizeAlloc(true);
    surfB.SetLocation(Memory::Fb);          // Destination Vidmem
    surfB.SetGpuCacheMode(Surface::GpuCacheOn);         // L2 Caching
    if (OK != surfB.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    PrintDmaBufferParams(surfB);

    if (OK != surfB.Map()) {
        ErrPrintf("can't map the destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return(0);
    }
    /*
    m_gpu_addrB_old = surfB.GetCtxDmaOffsetGpu();
    */
    m_gpu_addrB_old = surfB.GetPhysAddress();
    InfoPrintf("SurfB = 0x%x\n",m_gpu_addrB_old);

    // wait for GR idle
    local_status = local_status && ch->WaitForIdle();
    SetStatus(local_status?(Test::TEST_SUCCEEDED):(Test::TEST_FAILED));

    // if unsuccessful, clean up and exit
    if(!local_status) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return(0);
    }

    DebugPrintf("fb_hub_mmu_ilwalidateTest::Setup() done succesfully\n");
    return(1);
}

// a little extra cleanup to be done
void fb_hub_mmu_ilwalidateTest::CleanUp(void)
{
    DebugPrintf("fb_hub_mmu_ilwalidateTest::CleanUp() starts\n");

    // free the surface and objects we used for drawing
    if (surfA.GetSize()) {
        surfA.Unmap();
        surfA.Free();
    }
    if (surfB.GetSize()) {
        surfB.Unmap();
        surfB.Free();
    }

    // call parent's cleanup too
    LWGpuSingleChannelTest::CleanUp();
    DebugPrintf("fb_hub_mmu_ilwalidateTest::CleanUp() done\n");
}

// the real test
void fb_hub_mmu_ilwalidateTest::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("fb_hub_mmu_ilwalidateTest::Run() starts\n");

    DisableBackdoor();

    // Write surface A, B
    MEM_WR32(reinterpret_cast<uintptr_t>(surfA.GetAddress()), 'A');
    MEM_WR32(reinterpret_cast<uintptr_t>(surfB.GetAddress()), 'B');

    
    // Send down an FB_FLUSH, MODS will use the correct register based on
    // PCIe GEN of HW.
    lwgpu->GetGpuSubdevice()->FbFlush(Tasker::NO_TIMEOUT);

    // Unmap
    surfA.Unmap();
    surfA.Free();
    surfB.Unmap();
    surfB.Free();

    // Ilwalidate
    if (!no_ilwalidate) {
    /*
            lwgpu->RegWr32(
                LW_PFB_PRI_MMU_ILWALIDATE,
                DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _ALL_VA, _TRUE) |
                DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _ALL_PDB, _TRUE) |
                DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _HUBTLB_ONLY, _TRUE) |
                DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _TRIGGER, _TRUE));
    */

    UINT32 methodData[4];

        //methodData[0] =  DRF_NUM(_UDMA, _MEM_OP_A, _TLB_ILWALIDATE_CLEAR_FAULT, 0);
        if (ilwal_coh) {
            methodData[0] =
                             DRF_DEF(906F, _MEM_OP_A, _TLB_ILWALIDATE_TARGET, _SYS_MEM_COHERENT) ;
        } else {
            methodData[0] =
                             DRF_DEF(906F, _MEM_OP_A, _TLB_ILWALIDATE_TARGET, _VID_MEM) ;

        }
        methodData[1] =  (
                             DRF_DEF(906F, _MEM_OP_B,
                                     _MMU_TLB_ILWALIDATE_PDB, _ALL) |
                             DRF_DEF(906F, _MEM_OP_B,
                                     _MMU_TLB_ILWALIDATE_GPC, _ENABLE) |
                             DRF_DEF(906F, _MEM_OP_B,
                                     _OPERATION, _MMU_TLB_ILWALIDATE)
                );

        ch->MethodWriteN( 0, LW906F_MEM_OP_A, 2, methodData);
        ch->ScheduleChannel(true); // need an explicit schedule
    }

    // Remap with old address swapped
    //surfA.SetGpuVirtAddrHint(m_gpu_addrB_old);
    //surfB.SetGpuVirtAddrHint(m_gpu_addrA_old);
    //surfA.SetGpuPhysAddrHint(m_gpu_addrB_old);
    //surfB.SetGpuPhysAddrHint(m_gpu_addrA_old);

    surfB.SetLocation(Memory::Fb);          // Destination Vidmem
    surfB.SetArrayPitch(32);
    surfB.SetColorFormat(ColorUtils::Y8);
    surfB.SetForceSizeAlloc(true);
    if (OK != surfB.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return;
    }
    if (OK != surfB.Map()) {
        ErrPrintf("can't map the destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
    return;
    }
    //m_gpu_addrB_new = surfB.GetCtxDmaOffsetGpu();
    m_gpu_addrB_new = surfB.GetPhysAddress();

    surfA.SetLocation(Memory::Fb);          // Destination Vidmem
    surfA.SetArrayPitch(32);
    surfA.SetColorFormat(ColorUtils::Y8);
    surfA.SetForceSizeAlloc(true);
    if (OK != surfA.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return;
    }
    if (OK != surfA.Map()) {
        ErrPrintf("can't map the destination buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
    return;
    }
    //m_gpu_addrA_new = surfA.GetCtxDmaOffsetGpu();
    m_gpu_addrA_new = surfA.GetPhysAddress();

    InfoPrintf("New SurfA = 0x%x\n",m_gpu_addrA_new);
    InfoPrintf("New SurfB = 0x%x\n",m_gpu_addrB_new);

    // Read Surface A, B & Check
    unsigned read_a = MEM_RD32(reinterpret_cast<uintptr_t>(surfA.GetAddress()));
    unsigned read_b = MEM_RD32(reinterpret_cast<uintptr_t>(surfB.GetAddress()));

    InfoPrintf("VAL A = %c\n",read_a);
    InfoPrintf("VAL B = %c\n",read_b);

    if ((read_a == 'A') || (read_b == 'B'))
    {
    ErrPrintf("Ilwalidation did not work\n");
    local_status = 0;
    }

    if(local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(Test::TEST_FAILED_CRC);
   }

    EnableBackdoor();
}

//! \brief Enables the backdoor access
void fb_hub_mmu_ilwalidateTest::EnableBackdoor(void)
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 1);
       // Turn off backdoor accesses
       // bug 527528 - Comments by Joseph Harwood:
       //   DUT in the fermi fullchip testbench is the GPU. Only backdoor sysmem accesses will work in this testbench,
       //   because only the GPU is on the PCI, and the GPU (the real chip) does not process memory accesses
       //   for other chips, only for itself.
       //
       // Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_SYS, 4, 1);
    }
}

//! \brief Disable the backdoor access
void fb_hub_mmu_ilwalidateTest::DisableBackdoor(void)
{
    // Turn off backdoor accesses
    if(Platform::GetSimulationMode() != Platform::Hardware)
    {
       Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
       // Turn off backdoor accesses
       // bug 527528 - Comments by Joseph Harwood:
       //   DUT in the fermi fullchip testbench is the GPU. Only backdoor sysmem accesses will work in this testbench,
       //   because only the GPU is on the PCI, and the GPU (the real chip) does not process memory accesses
       //   for other chips, only for itself.
       //
       // Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_SYS, 4, 0);
    }
}

