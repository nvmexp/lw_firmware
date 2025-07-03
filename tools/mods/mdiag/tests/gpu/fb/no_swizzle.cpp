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

#include "no_swizzle.h"
#include "sim/IChip.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "lwmisc.h"
#include "mdiag/utils/randstrm.h"

#include "mdiag/tests/gpu/fb/amap/FermiAddressMapping.h"
#include "mdiag/tests/gpu/fb/amap/PhysicalAddress.h"

extern const ParamDecl no_swizzle_params[] = {
    PARAM_SUBDECL(FBTest::fbtest_common_params),
    MEMORY_SPACE_PARAMS("_vid", ""),
    LAST_PARAM
};

no_swizzleTest::no_swizzleTest(ArgReader *reader) :
    FBTest(reader),
    local_status(1),
    m_params(reader)
{
    ParseDmaBufferArgs(m_dmaBuffer, reader, "vid", &m_pteKindName, 0);

    FBTest::DebugPrintArgs();
}

no_swizzleTest::~no_swizzleTest(void)
{
    // delete RndStream;
    m_params = NULL;
}

STD_TEST_FACTORY(no_swizzle, no_swizzleTest)

// a little extra setup to be done
int no_swizzleTest::Setup(void)
{
    local_status = 1;

    InfoPrintf("no_swizzleTest::Setup() starts\n");

    getStateReport()->enable();

    // call parent's Setup first
    if (!FBTest::Setup()) {
        ErrPrintf("no_swizzleTest::Setup() Fail\n");
        return 0;
    }

    m_dmaBuffer.SetArrayPitch(4096);
    m_dmaBuffer.SetColorFormat(ColorUtils::Y8);
    m_dmaBuffer.SetForceSizeAlloc(true);
    if (OK != SetPteKind(m_dmaBuffer, m_pteKindName, lwgpu->GetGpuDevice())) {
        return 0;
    }
    if (OK != m_dmaBuffer.Alloc(lwgpu->GetGpuDevice())) {
        ErrPrintf("can't create src buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        CleanUp();
        return 0;
    }
    if (OK != m_dmaBuffer.Map()) {
        ErrPrintf("can't map the source buffer, line %d\n", __LINE__);
        SetStatus(Test::TEST_NO_RESOURCES);
        return 0;
    }
    InfoPrintf("partition_white_boxTest::Setup() Dma buffer at 0x%012x\n", m_dmaBuffer.GetVidMemOffset());

    // if unsuccessful, clean up and exit
    if (!local_status) {
        SetStatus(Test::TEST_NO_RESOURCES);
        ErrPrintf("Something wrong with local_status 0x%x\n",local_status);
        CleanUp();
        return 0;
    }

    return 1;
}

// a little extra cleanup to be done
void no_swizzleTest::CleanUp(void)
{
    DebugPrintf("no_swizzleTest::CleanUp() starts\n");

    if (m_dmaBuffer.GetSize()) {
        m_dmaBuffer.Unmap();
        m_dmaBuffer.Free();
    }

    // call parent's cleanup too
    FBTest::CleanUp();
    DebugPrintf("no_swizzleTest::CleanUp() done\n");
}

// the real test
void no_swizzleTest::Run(void)
{
    SetStatus(Test::TEST_INCOMPLETE);

    InfoPrintf("no_swizzleTest::Run() starts\n");

    local_status = 1;

    fbHelper->SetBackdoor(false);

    // fbHelper->SetBar1Block(0x1f0f0000, 0);

//     FermiPhysicalAddress::KIND kind = FermiPhysicalAddress::PITCHLINEAR;
//     FermiPhysicalAddress::KIND kind_no_swizzle = FermiPhysicalAddress::PITCH_NO_SWIZZLE;
//     FermiPhysicalAddress::APERTURE aperture = FermiPhysicalAddress::LOCAL;
//     FermiPhysicalAddress::PAGE_SIZE pagesize = FermiPhysicalAddress::PS4K;

    // UINT64 addr = 0x3fffc;

//     FBAddress pa_pitch_no_swizzle(FermiFBAddress(addr, kind_no_swizzle, pagesize, aperture));

//     lwgpu->MemWr32(reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()) + 0x3fffc, 0xdeadbeef);
//     lwgpu->MemRd32(reinterpret_cast<uintptr_t>(m_dmaBuffer.GetAddress()) + 0x3fffc);

//     for (UINT64 a = 0x0; a < 0xfffffff; a += 64) {
//         FBAddress pa_pitch(FermiFBAddress(a, kind, pagesize, aperture));
//         InfoPrintf("no_swizzleTest::Run() fb_adr: 0x%010llx by 0x%010llx\n", pa_pitch.fb_addr(), a);
//     }

//     FBAddress pa_0(FermiFBAddress(0x1fffffff54llu, kind, pagesize, aperture));
//     FBAddress pa_1(FermiFBAddress(0x1fffffed54llu, kind, pagesize, aperture));
//     FBAddress ltc2ramif(FermiFBAddress(0x1fffffed54llu, kind, pagesize, aperture));

//     fbHelper->PhysWr32_Bar0Window(0x1fffffff54llu, 0xdeadbeef);
//     fbHelper->PhysRd32_Bar0Window(0x1fffffed54llu);

//     InfoPrintf("part %d : subpart: %d row: %d bank: %d col: %d fb_addr %010llx\n",
//                pa_0.partition(), pa_0.subPartition(), pa_0.row(), pa_0.bank(), pa_0.column(), pa_0.fb_addr());
//     InfoPrintf("part %d : subpart: %d row: %d bank: %d col: %d fb_addr %010llx\n",
//                pa_1.partition(), pa_1.subPartition(), pa_1.row(), pa_1.bank(), pa_1.column(), pa_1.fb_addr());

    if (local_status) {
        SetStatus(Test::TEST_SUCCEEDED);
        getStateReport()->crcCheckPassedGold();
    } else {
        SetStatus(Test::TEST_FAILED_CRC);
    }

    return;
}
