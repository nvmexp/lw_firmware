/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//
// TurboCipher Test : This test is intended to check the Encryption and decryption
// functinality of XVE layer.
// To test that, Allocate some Turbocipher Encryption enable surface and wite
// some known data so that it passes through XVE layer. For verfication of
// whether encryption is done or not check the written data , so that reading
// doesn't cross through XVE or simulate the things so.
//

#include "gpu/tests/rmtest.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "lwos.h"
#include "core/include/gpu.h"
#include "gpu/include/gpudev.h"
#include "core/include/channel.h"
#include "ctrl/ctrl0080.h"
#include "class/cl906f.h"
#include "class/cl007d.h" // LW04_SOFTWARE_TEST
#include "core/include/memcheck.h"

#define RM_PAGE_SIZE   1024

class ClassTurboCipherTest: public RmTest
{
public:
    ClassTurboCipherTest();
    virtual ~ClassTurboCipherTest();

    virtual string IsTestSupported();

    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

private:

    LwRm::Handle m_hMemory;
    UINT32 *m_cpuAddr;
    UINT32 *m_dirCpuAddr;
};

//------------------------------------------------------------------------------
ClassTurboCipherTest::ClassTurboCipherTest()
{
    SetName("ClassTurboCipherTest");
    m_pCh = NULL;
    m_cpuAddr = NULL;
    m_dirCpuAddr = NULL;
    m_hCh = 0;
    m_hMemory = 0;
}

//------------------------------------------------------------------------------
ClassTurboCipherTest::~ClassTurboCipherTest()
{
}

//------------------------------------------------------------------------------
string ClassTurboCipherTest::IsTestSupported()
{
    if (Platform::GetSimulationMode() == Platform::Hardware)
    {
        if (GetBoundGpuDevice()->
            CheckCapsBit(GpuDevice::TURBOCIPHER_SUPPORTED))
        {
            return RUN_RMTEST_TRUE;
        }
        else
        {
            return "Invalid caps bit sent to GpuDevice::CheckCapsBit";
        }
    }
    else
    {
        Printf(Tee::PriHigh,
            "%d:TurboCipher test is supported only on HW \n",__LINE__);
        Printf(Tee::PriHigh,"%d:Returns from non HW platform\n",
               __LINE__);
        return "Supported only on HW";
    }
}

//------------------------------------------------------------------------------
RC ClassTurboCipherTest::Setup()
{
    RC rc;
    LwRmPtr pLwRm;
    UINT32 attr,attr2,flags;

    CHECK_RC(InitFromJs());

    // Allocate TurboCipher Enabled Surface
    attr = DRF_DEF(OS32, _ATTR, _LOCATION, _PCI)|
           DRF_DEF(OS32, _ATTR, _FORMAT, _BLOCK_LINEAR)|
           DRF_DEF(OS32, _ATTR, _PHYSICALITY, _CONTIGUOUS);
    attr2 = LWOS32_ATTR2_NONE;
    CHECK_RC(pLwRm->VidHeapAllocSizeEx(
        LWOS32_TYPE_IMAGE,
        LWOS32_ALLOC_FLAGS_IGNORE_BANK_PLACEMENT |
        LWOS32_ALLOC_FLAGS_TURBO_CIPHER_ENCRYPTED,
        &attr,
        &attr2,
        RM_PAGE_SIZE,
        0,
        NULL,
        NULL,
        NULL,
        &m_hMemory,
        NULL,
        NULL,
        NULL,
        NULL,
        0, 0,
        GetBoundGpuDevice()
        ));

    CHECK_RC(pLwRm->MapMemory(m_hMemory, 0, RM_PAGE_SIZE, (void **)&m_cpuAddr, 0, GetBoundGpuSubdevice()));

    flags = DRF_DEF(OS33,_FLAGS,_MAPPING,_DIRECT);

    // Map it to get Cpu virtual address map without BAR ( or at least simulate it)
    CHECK_RC(pLwRm->MapMemory(m_hMemory, 0, RM_PAGE_SIZE, (void **)&m_dirCpuAddr,flags, GetBoundGpuSubdevice()));

    return rc;
}

//------------------------------------------------------------------------------

RC ClassTurboCipherTest::Run()
{
    RC rc;
    UINT32 numData, index;
    UINT32 direMatchCount = 0;
    UINT32 barDiffCount = 0;
    UINT32 *tempCpuAddr = NULL;
    UINT32 *tempDirCpuAddr = NULL;

    // Number of UINT32 data possible in RM_PAGE_SIZE
    numData = (UINT32)(RM_PAGE_SIZE / sizeof(UINT32));

    tempCpuAddr = m_cpuAddr;
    //
    // Write the data using BAR mapped cpu virtual address
    // Through this, Data will get encryted for TCipher enabled
    // surface as it pass through XVE layer
    //
    for (index = 0; index < numData; index++)
    {
        MEM_WR32(tempCpuAddr++, index);
    }

    //
    // Do a read through the GPU to force pending writes through, otherwise the
    // first read below may get wrong data.
    //
    index =  MEM_RD32(m_cpuAddr);

    //
    // 1)While Turbocipher is enabled, data writen to cipher enabled
    // surface gets encrypted by XVE layer , so we get the
    // encrypted data if we directly read sysmemdata without crossing XVE.
    // 2)Data at cipher enabled surface gets decrypted when reading it
    // from Gpu side as this access goes through XVE.
    //

    tempCpuAddr = m_cpuAddr;
    tempDirCpuAddr = m_dirCpuAddr;

    for(index = 0; index < numData; tempDirCpuAddr++, tempCpuAddr++ ,index++)
    {
        if (MEM_RD32(tempDirCpuAddr) == index)
        {
            Printf(Tee::PriLow,
            "%d: System Memory Data %d = original data %d as it is not encrypted while Turbocipher enabled,\n",__LINE__, MEM_RD32(tempDirCpuAddr), index);
            direMatchCount++;
            rc = RC::DATA_MISMATCH;
        }
        else
        {
            Printf(Tee::PriDebug,
            "%d: System Memory Data %d, Original Data %d,\n",__LINE__, MEM_RD32(tempDirCpuAddr), index);
        }
        if (MEM_RD32(tempCpuAddr) != index)
        {
            Printf(Tee::PriLow,
            "%d: Data read through XVE %d is not same as Orignal data %d while Turbocipher enabled,\n",__LINE__, MEM_RD32(tempCpuAddr), index);
            barDiffCount++;
            rc = RC::DATA_MISMATCH;
        }
        else
        {
            Printf(Tee::PriLow,
            "%d: System Memory Data through XVE %d, Original Data %d,\n",__LINE__, MEM_RD32(tempCpuAddr), index);
        }

    }
    if (direMatchCount || barDiffCount)
    {
        Printf(Tee::PriHigh,
                "System mem data = Original data for %d times,\n "
                "Bar map read != Original data for %d times,\n",
                direMatchCount, barDiffCount);
        return rc;
    }

    return rc;
}

//------------------------------------------------------------------------------
RC ClassTurboCipherTest::Cleanup()
{
    RC rc;
    LwRmPtr pLwRm;

    pLwRm->UnmapMemory(m_hMemory, m_dirCpuAddr, 0, GetBoundGpuSubdevice());
    pLwRm->UnmapMemory(m_hMemory, m_cpuAddr, 0, GetBoundGpuSubdevice());
    pLwRm->Free(m_hMemory);
    m_pCh = NULL;

    return rc;
}

//------------------------------------------------------------------------------
// Set up a JavaScript class that creates and owns a C++ ClassTurboCipherTest object.
//------------------------------------------------------------------------------
JS_CLASS_INHERIT(ClassTurboCipherTest, RmTest,
                 "TurboCipher system memory allocations");
