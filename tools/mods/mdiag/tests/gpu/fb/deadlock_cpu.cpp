/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007,2019-2021 by LWPU Corporation.  All rights reserved. 
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "sim/IChip.h"
#include "core/include/channel.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"
#include "sim/simpage.h"
#include "deadlock_cpu.h"

#include "fermi/gf100/dev_fb.h"

#include "class/cla097.h"
#include "class/cl906f.h"
#include "class/cl9097.h"

#include "mdiag/utils/surfutil.h"

//#define LW_UDMA_MEM_OP_A_TLB_ILWALIDATE_CLEAR_FAULT           31:31 /* -W-VF */
#define LW_UDMA_MEM_OP_A_TLB_ILWALIDATE_TARGET                          31:30 /* -W-VF */
#define LW_UDMA_MEM_OP_A_TLB_ILWALIDATE_TARGET_VID_MEM             0x00000000 /* RW--V */
#define LW_UDMA_MEM_OP_A_TLB_ILWALIDATE_TARGET_SYS_MEM_COHERENT    0x00000002 /* RW--V */
#define LW_UDMA_MEM_OP_A_TLB_ILWALIDATE_TARGET_SYS_MEM_NONCOHERENT 0x00000003 /* RW--V */

#define LW_UDMA_MEM_OP_B_TLB_ILWALIDATE_ENG_ID                  7:3 /* -W-VF */
#define LW_UDMA_MEM_OP_B_TLB_ILWALIDATE_ALL_VA                  2:2 /* -W-VF */
#define LW_UDMA_MEM_OP_B_TLB_ILWALIDATE_ALL_ENG                 1:1 /* -W-VF */
#define LW_UDMA_MEM_OP_B_TLB_ILWALIDATE_ALL_PDB                 0:0 /* -W-VF */

/*
  This test do basic reads and writes for FB PA.
*/

extern const ParamDecl deadlock_cpu_params[] = {
    PARAM_SUBDECL(FBTest::fbtest_common_params),
    UNSIGNED_PARAM("-size",           "size in bytes of the dma buffer to read/write"),
    UNSIGNED_PARAM("-write_number", "total number of write request to generate"),
    SIMPLE_PARAM("-inband", "Use in band method to ilwalidate TLB"),
    LAST_PARAM
};

deadlock_cpuTest::deadlock_cpuTest(ArgReader *reader) :
    FBTest(reader),
    m_pGpuChannel(NULL),
    m_hClassFermi(0)
{
    params = reader;

    m_size = params->ParamUnsigned("-size", 4096*8);
    m_writeNum = params->ParamUnsigned("-write_number", 1000);
    m_inband = params->ParamPresent("-inband") != 0;
}

deadlock_cpuTest::~deadlock_cpuTest(void)
{
}

STD_TEST_FACTORY(deadlock_cpu, deadlock_cpuTest)

// a little extra setup to be done
int deadlock_cpuTest::Setup(void)
{
    local_status = 1;

    InfoPrintf("deadlock_cpuTest::Setup() starts\n");

    getStateReport()->enable();

    if (!FBTest::Setup()) {
        ErrPrintf("enhanced_read_writeTest::Setup() Fail\n");
        return 0;
    }

    // Set to Blocklinear to get RM do reflected mapping
    m_testBuffer.SetLayout(Surface2D::BlockLinear);
    m_testBuffer.SetLocation(Memory::NonCoherent);
    m_testBuffer.SetArrayPitch(m_size);
    m_testBuffer.SetColorFormat(ColorUtils::Y8);
    m_testBuffer.SetProtect(Memory::ReadWrite);
    if (OK != m_testBuffer.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create src buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        CleanUp();
        return 0;
    }
    if (OK != m_testBuffer.Map()) {
        ErrPrintf("can't map the source buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return 0;
    }
// m_offset = m_testBuffer.GetExtraAllocSize();

    InfoPrintf("Created Surface at VidMemOffset 0x%lx Mapped at %p\n",
               m_testBuffer.GetVidMemOffset(), m_testBuffer.GetAddress());
    InfoPrintf("Surface physical address = %llx\n",
               m_testBuffer.GetPhysAddress());

    // if unsuccessful, clean up and exit
    if(!local_status) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return(0);
    }
    InfoPrintf("deadlock_cpuTest::Setup() done\n");

    return (1);
}

// a little extra cleanup to be done
void deadlock_cpuTest::CleanUp(void)
{
    DebugPrintf("deadlock_cpuTest::CleanUp() starts\n");

    if (m_testBuffer.GetSize() != 0) {
        m_testBuffer.Unmap();
        m_testBuffer.Free();
    }

    if (m_pGpuChannel != NULL) {
        if (m_hClassFermi != 0) {
            m_pGpuChannel->DestroyObject(m_hClassFermi);
            m_hClassFermi = 0;
        }
        chHelper->release_channel();
        m_pGpuChannel = NULL;
    }

    FBTest::CleanUp();

    DebugPrintf("deadlock_cpuTest::CleanUp() done\n");
}

void deadlock_cpuTest::Run(void)
{

    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("deadlock_cpuTest::Run() starts\n");

    InfoPrintf("deadlock_cpuTest: Physical Address of FB Start: 0x%08x.\n", lwgpu->PhysFbBase());
    InfoPrintf("deadlock_cpuTest: BAR1 Buffer Start: 0x%08x.\n", m_testBuffer.GetAddress());

    TestMemoryRW();

    InfoPrintf("Done with deadlock_cpu test\n");

    if(local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(Test::TEST_FAILED_CRC);
    }
}

void deadlock_cpuTest::DisableBackdoor()
{
    if(Platform::GetSimulationMode() != Platform::Hardware) {
        Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_FB, 4, 0);
// Commenting out the sysmem accesses from frontdoor since it's not applicable in
// DGPU sims, see bug 764038.
//Platform::EscapeWrite("BACKDOOR_ACCESS", IChip::EBACKDOOR_SYS, 4, 0);
    }
}

int deadlock_cpuTest::IlwalidTLB(bool inband)
{
    InfoPrintf("Ilwalidating TLB entries...\n");

    if (inband) {
        if (!chHelper->acquire_channel() ) {
            ErrPrintf("Channel Create failed\n");
            //chHelper->release_gpu_resource();
            return 1;
        }
        ch = chHelper->channel();
        m_pGpuChannel = ch;//lwgpu->CreateChannel();
        MASSERT(m_pGpuChannel && "Getting channel failed!");

        GpuSubdevice* pGpuSubdevice = lwgpu->GetGpuSubdevice();
        UINT32 engineId = LW2080_ENGINE_TYPE_NULL;
        if (pGpuSubdevice->HasFeature(Device::GPUSUB_HOST_REQUIRES_ENGINE_ID))
        {
            engineId = MDiagUtils::GetGrEngineId(0);
        }

        if (!chHelper->alloc_channel(engineId))
        {
            ErrPrintf("Channel Alloc failed\n");
            chHelper->release_channel();
            chHelper->release_gpu_resource();
            return 1;
        }

        Gpu::LwDeviceId devId = lwgpu->GetGpuSubdevice()->DeviceId();
        InfoPrintf("    Got the deviceID %lx\n",devId);

        if (devId >= Gpu::GM107){
            InfoPrintf("Setting     Kepler class \n");
            m_hClassFermi = ch->CreateObject(KEPLER_A);

            if (!m_hClassFermi) {
                ErrPrintf("Couldn't CreateObject(KEPLER_A)\n");
                return 0;
            }
            DebugPrintf("KEPLER_A created \n");
        } else{
            InfoPrintf("Setting Fermi class \n");
            m_hClassFermi = ch->CreateObject(FERMI_A);

            if (!m_hClassFermi) {
                ErrPrintf("Couldn't CreateObject(FERMI_A)\n");
                return 0;
            }
            DebugPrintf("FERMI_A created \n");

        }

        MASSERT(m_hClassFermi);

        ch->SetObject(0, m_hClassFermi);

//        Channel* pCh = m_pGpuChannel->GetModsChannel();
        //       MASSERT(pCh);

        // now diff from the method in policy manager
        // need verify

        UINT32 data[] = {
            DRF_NUM(_UDMA, _MEM_OP_A, _TLB_ILWALIDATE_TARGET,
                    LW_UDMA_MEM_OP_A_TLB_ILWALIDATE_TARGET_SYS_MEM_NONCOHERENT),
//            DRF_NUM(_UDMA, _MEM_OP_B, _TLB_ILWALIDATE_ALL_VA, 1) |
            DRF_NUM(_UDMA, _MEM_OP_B, _TLB_ILWALIDATE_ALL_ENG, 0) |
            DRF_NUM(_UDMA, _MEM_OP_B, _TLB_ILWALIDATE_ALL_PDB, 1) |
            DRF_DEF(906F, _MEM_OP_B, _OPERATION, _MMU_TLB_ILWALIDATE)
        };

        ch->MethodWriteN(0, LW906F_MEM_OP_A, 2, data);

        ch->MethodFlush();
        ch->WaitForIdle();
    }
    else {
        UINT32 value = DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _ALL_VA, _TRUE) |
            DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _ALL_PDB, _TRUE) |
            DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _PDB_APERTURE, _SYS_MEM) |
            DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _TRIGGER, _TRUE);

        GpuDevice* pGpuDev = lwgpu->GetGpuDevice();
        int numSubDev = pGpuDev->GetNumSubdevices();

        for (int i = 0; i < numSubDev; ++i)
        {
            GpuSubdevice* pSubDev = lwgpu->GetGpuSubdevice(i);
            if (pSubDev != NULL) {
                InfoPrintf("Ilwalidating on Sub Device #%ld...\n", i);
                pSubDev->RegWr32(LW_PFB_PRI_MMU_ILWALIDATE, value);
            }
        }
    }
//    _TRIGGER is write-only
//     unsigned status;
//     do {
//         status = lwgpu->RegRd32(LW_PFB_PRI_MMU_ILWALIDATE);
//     } while (status & DRF_DEF(_PFB_PRI, _MMU_ILWALIDATE, _TRIGGER, _TRUE));

    InfoPrintf("TLB ilwalidation done.\n");
    return 0;
}

void deadlock_cpuTest::TestMemoryRW()
{
    DisableBackdoor();
    IlwalidTLB(m_inband);

    UINT32 testData32B[] = {
        0x12345678, 0xdeadbeaf, 0x87654321, 0xbeefdead,
        0x11111111, 0x33333333, 0x66666666, 0xaaaaaaaa,
    };
    UINT32 readData32B[] = {
        0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0
    };

    InfoPrintf("deadlock_cpuTest: Sending %lu write requests...\n", m_writeNum);
    UINT32 addr = 0;
    for (unsigned int i = 0; i < m_writeNum; ++i, addr += 4096*2, addr %= m_size) {
        Platform::VirtualWr(m_testBuffer.GetAddress(), testData32B, 32);
    }

    InfoPrintf("deadlock_cpuTest: Reading back data from BAR1...\n");
    Platform::VirtualRd(m_testBuffer.GetAddress(), readData32B, 32);

    for (int j = 0; j < 32; ++j) {
        unsigned char write = ((unsigned char*)testData32B)[j];
        unsigned char read  = ((unsigned char*)readData32B)[j];

        if (write != read) {
            ErrPrintf("FAILED! at addr 0x%x[offset %x]: 0x%x != 0x%x\n", addr, j, write, read);
            local_status = 0;
        }
    }
}
