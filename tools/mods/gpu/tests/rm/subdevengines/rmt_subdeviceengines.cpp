/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file rmt_subdeviceengine.cpp
//! \brief Subdevice mapping Tests
//!

#include "gpu/tests/rmtest.h"
#include "core/include/utility.h"
#include "core/include/platform.h"
#include "core/include/massert.h"
#include "lwos.h"

#include "class/cl90e0.h" // GF100_SUBDEVICE_GRAPHICS
#include "class/cl90e1.h" // GF100_SUBDEVICE_FB
#include "class/cl90e3.h" // GF100_SUBDEVICE_FLUSH
#include "class/cl90e4.h" // GF100_SUBDEVICE_LTCG
#include "class/cl90e5.h" // GF100_SUBDEVICE_TOP
#include "class/cl90e6.h" // GF100_SUBDEVICE_MASTER

#include "ctrl/ctrl90e0.h"  // GF100_SUBDEVICE_GRAPHICS
#include "ctrl/ctrl90e1.h"  // GF100_SUBDEVICE_FB
#include "ctrl/ctrl90e6.h"  // GF100_SUBDEVICE_MASTER
#include "core/include/memcheck.h"

class SubdeviceEngineTest;

typedef RC (SubdeviceEngineTest::*RunClassRmCtrlFn)();

typedef struct class_desc
{
    LwU32 classNum;
    bool  isSupported;              // override by SubdeviceEngineTest::IsSupported after checking hw
    LwU32 protect;
    LwU32 mappingSize;
    RunClassRmCtrlFn runRmCtrl;
} classDesc;

class SubdeviceEngineTest: public RmTest
{
public:
    SubdeviceEngineTest();
    virtual ~SubdeviceEngineTest();

    RC Run90E0RmCtrls();
    RC Run90E1RmCtrls();
    RC Run90E6RmCtrls();

    virtual string     IsTestSupported();

    virtual RC       Setup();
    virtual RC       Run();
    virtual RC       Cleanup();
private:
    LwRm::Handle     m_hSubDevEngine;
    void *           m_cpuAddr;
    int              m_numOfClasses;

    classDesc *      m_subdevEngineClasses;
};

RC SubdeviceEngineTest::Run90E0RmCtrls()
{
    LW90E0_CTRL_GR_GET_ECC_COUNTS_PARAMS    eccCountsParams;
    LwRmPtr pLwRm;
    RC rc;

    memset(&eccCountsParams, 0, sizeof(eccCountsParams));

    eccCountsParams.tpcCount = LW90E0_CTRL_GR_ECC_TPC_COUNT;
    eccCountsParams.gpcCount = LW90E0_CTRL_GR_ECC_GPC_COUNT;

    CHECK_RC(pLwRm->Control(m_hSubDevEngine, LW90E0_CTRL_CMD_GR_GET_ECC_COUNTS,
                      &eccCountsParams, sizeof(eccCountsParams)));

    Printf(Tee::PriLow, "  testing LW90E0_CTRL_CMD_GR_GET_ECC_COUNTS: passed\n");

    return rc;
}

RC SubdeviceEngineTest::Run90E1RmCtrls()
{
    LW90E1_CTRL_FB_GET_ECC_COUNTS_PARAMS    eccCountsParams;
    LW90E1_CTRL_FB_GET_EDC_COUNTS_PARAMS    edcCountsParams;
    LwRmPtr pLwRm;
    RC rc;

    memset(&eccCountsParams, 0, sizeof(eccCountsParams));
    memset(&edcCountsParams, 0, sizeof(edcCountsParams));

    eccCountsParams.sliceCount = LW90E1_CTRL_FB_ECC_SLICE_COUNT;
    eccCountsParams.partitionCount = LW90E1_CTRL_FB_ECC_PARTITION_COUNT;

    CHECK_RC(pLwRm->Control(m_hSubDevEngine, LW90E1_CTRL_CMD_FB_GET_ECC_COUNTS,
                      &eccCountsParams, sizeof(eccCountsParams)));

    Printf(Tee::PriLow, "    testing LW90E1_CTRL_CMD_FB_GET_ECC_COUNTS: passed\n");

    CHECK_RC(pLwRm->Control(m_hSubDevEngine, LW90E1_CTRL_CMD_FB_GET_EDC_COUNTS,
                      &edcCountsParams, sizeof(edcCountsParams)));

    Printf(Tee::PriLow, "    testing LW90E1_CTRL_CMD_FB_GET_EDC_COUNTS: passed\n");

    return rc;
}

RC SubdeviceEngineTest::Run90E6RmCtrls()
{
    LW90E6_CTRL_MASTER_GET_ECC_INTR_OFFSET_MASK_PARAMS params = {0};
    LwRmPtr pLwRm;
    LwU32 val;
    RC rc;

    CHECK_RC(pLwRm->Control(m_hSubDevEngine, LW90E6_CTRL_CMD_MASTER_GET_ECC_INTR_OFFSET_MASK,
                      &params, sizeof(params)));
    Printf(Tee::PriLow, "%s Offset: 0x%08x Mask: 0x%08x\n", __FUNCTION__, params.offset, params.mask);
    val = MEM_RD32(((LwU8*)m_cpuAddr) + params.offset) & params.mask;
    Printf(Tee::PriLow, "%s val 0x%08x\n", __FUNCTION__, val);
    return rc;
}

//! \brief SubdeviceEngineTest constructor.
//!
//! \sa Setup
//-----------------------------------------------------------------------------
SubdeviceEngineTest::SubdeviceEngineTest() :
    m_hSubDevEngine(0),
    m_cpuAddr(nullptr)
{
    SetName("SubdeviceEngineTest");

    static classDesc SubdeviceEngineClasses[] =
    {
        // classNum                 isSupported  protect                                             mappingSize             runRmCtrl
        { GF100_SUBDEVICE_GRAPHICS, false,       DRF_DEF(OS33, _FLAGS, _ACCESS, _READ_WRITE),         sizeof(GF100GRMap),     &SubdeviceEngineTest::Run90E0RmCtrls },
        { GF100_SUBDEVICE_FB,       false,       DRF_DEF(OS33, _FLAGS, _ACCESS, _READ_WRITE),         0,                      &SubdeviceEngineTest::Run90E1RmCtrls },
        { GF100_SUBDEVICE_FLUSH,    false,       DRF_DEF(OS33, _FLAGS, _ACCESS, _READ_WRITE),         sizeof(GF100FLUSHMap),  NULL },
        { GF100_SUBDEVICE_LTCG,     false,       DRF_DEF(OS33, _FLAGS, _ACCESS, _READ_WRITE),         sizeof(GF100LTCGMap),   NULL },
        { GF100_SUBDEVICE_TOP,      false,       DRF_DEF(OS33, _FLAGS, _ACCESS, _READ_WRITE),         sizeof(GF100TOPMap),    NULL },
        { GF100_SUBDEVICE_MASTER,   false,       DRF_DEF(OS33, _FLAGS, _ACCESS, _READ_ONLY),          sizeof(GF100MASTERMap), &SubdeviceEngineTest::Run90E6RmCtrls },
    };

    m_numOfClasses = sizeof(SubdeviceEngineClasses) / sizeof(SubdeviceEngineClasses[0]);
    m_subdevEngineClasses = SubdeviceEngineClasses;
}

//! \brief SubdeviceEngineTest destructor.
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
SubdeviceEngineTest::~SubdeviceEngineTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//!
//! \return True
//-----------------------------------------------------------------------------
string SubdeviceEngineTest::IsTestSupported()
{
    LwRmPtr pLwRm;
    bool hasSupportedClass = false;

    for (int i = 0; i < m_numOfClasses; i++)
    {
        if (pLwRm->IsClassSupported(m_subdevEngineClasses[i].classNum, GetBoundGpuDevice()))
        {
            m_subdevEngineClasses[i].isSupported = true;
            hasSupportedClass = true;
        }
    }

    if( hasSupportedClass )
        return RUN_RMTEST_TRUE;
    return "None of these GF100_SUBDEVICE_GRAPHICS, GF100_SUBDEVICE_FB, GF100_SUBDEVICE_FIFO, GF100_SUBDEVICE_FLUSH, GF100_SUBDEVICE_LTCG,\
            \nGF100_SUBDEVICE_TOP, GF100_SUBDEVICE_MASTER, classes is supported on current platform";
}

//! \brief Setup all necessary state before running the test.
//!
//! \sa IsTestSupported
//-----------------------------------------------------------------------------
RC SubdeviceEngineTest::Setup()
{
    return OK;
}

//! \brief Run the test!
//!
//! \return OK if the test passed, test-specific RC if it failed
//-----------------------------------------------------------------------------
RC SubdeviceEngineTest::Run()
{
    LwRmPtr pLwRm;
    RC      rc = OK;
    GpuSubdevice * pSubdev = GetBoundGpuSubdevice();

    for (int i = 0; i < m_numOfClasses; i++)
    {
        Printf(Tee::PriLow, "SubdeviceEngineTest: class 0x%x test begin\n",
               m_subdevEngineClasses[i].classNum);

        rc = pLwRm->Alloc(pLwRm->GetSubdeviceHandle(pSubdev),
                              &m_hSubDevEngine, m_subdevEngineClasses[i].classNum, NULL);
        if (!m_subdevEngineClasses[i].isSupported && rc == OK)
        {
            Printf(Tee::PriHigh, "SubdeviceEngineTest: Class 0x%x reported as unsupported, but was allocated\n",
                   m_subdevEngineClasses[i].classNum);
            CHECK_RC(RC::SOFTWARE_ERROR);
        }
        else
            rc.Clear();

        if (!m_subdevEngineClasses[i].isSupported)
            continue;

        // test mapping
        if (m_subdevEngineClasses[i].mappingSize != 0)
        {
            void *tempCpuAddr;
            Printf(Tee::PriLow, "  testing user mapping: ");
            rc.Clear();
            CHECK_RC(pLwRm->MapMemory(m_hSubDevEngine, 0, m_subdevEngineClasses[i].mappingSize,
                                      (void **)&m_cpuAddr, m_subdevEngineClasses[i].protect, pSubdev));
            Printf(Tee::PriLow, "passed\n");

            // Test other ACCESS settings
            for (LwU32 j = 0; j < LWOS33_FLAGS_ACCESS_WRITE_ONLY; j++)
            {
                LwU32 flags = DRF_NUM(OS33, _FLAGS, _ACCESS, j);
                if (m_subdevEngineClasses[i].protect == flags)
                    continue;

                Printf(Tee::PriLow, "  testing protection flags %d: ", j);
                rc = pLwRm->MapMemory(m_hSubDevEngine, 0,
                        m_subdevEngineClasses[i].mappingSize,
                        &tempCpuAddr, flags, pSubdev);
                pLwRm->UnmapMemory(m_hSubDevEngine, tempCpuAddr, 0, pSubdev);

                if (m_subdevEngineClasses[i].protect != DRF_DEF(OS33, _FLAGS, _ACCESS, _READ_WRITE))
                {
                    if (rc == OK)
                    {
                        Printf(Tee::PriHigh, "%s: Unsupported protection flags allowed by RM: %d\n", __FUNCTION__, i);
                        CHECK_RC(RC::SOFTWARE_ERROR);
                    }
                    rc.Clear();
                }
                else
                {
                    CHECK_RC(rc);
                }
                Printf(Tee::PriLow, "passed\n");

            }
        }

        // test rmctrls
        if (m_subdevEngineClasses[i].runRmCtrl)
        {
            Printf(Tee::PriLow, "  testing rmctrls\n");
            (this->*m_subdevEngineClasses[i].runRmCtrl)();
        }

        if (m_subdevEngineClasses[i].mappingSize != 0)
        {
            pLwRm->UnmapMemory(m_hSubDevEngine, m_cpuAddr, 0, pSubdev);
        }

        pLwRm->Free(m_hSubDevEngine);

        Printf(Tee::PriLow, "SubdeviceEngineTest: class 0x%x test passed\n",
               m_subdevEngineClasses[i].classNum);
    }

    return rc;
}

//! \brief Free any resources that this test selwred
//!
//! \sa Cleanup
//-----------------------------------------------------------------------------
RC SubdeviceEngineTest::Cleanup()
{
    return OK;
}

//-----------------------------------------------------------------------------
// JS Linkage
//-----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ SubdeviceEngineTest
//! object.
//-----------------------------------------------------------------------------
JS_CLASS_INHERIT(SubdeviceEngineTest, RmTest,
                 "SubdeviceEngineTest RM test.");

