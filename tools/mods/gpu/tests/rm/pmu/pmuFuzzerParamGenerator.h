/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_PMU_FUZZER_PARAM_GEN_H
#define INCLUDED_PMU_FUZZER_PARAM_GEN_H

#include "rmpmucmdif.h"
#include "flcnifcmn.h"
#include "random.h"
#include "pmu/pmuifcmn.h"
#include "lwtypes.h"

#define PMU_EXCLUDE_GEN_FILE 1

#ifndef PMU_EXCLUDE_GEN_FILE
#include "g_pmuifrpc.h"
#endif

using namespace std;

#define PMU_BUG_ID_11234                                  11234
#define PMU_BUG_ID_1970804                                1970804
#define PMU_BUG_ID_1998074                                1998074
#define PMU_BUG_ID_1970555                                1970555
#define PMU_BUG_ID_1970872                                1970872
#define PMU_BUG_ID_1971827                                1971827
#define PMU_BUG_ID_1971827_1                              19718271
#define PMU_BUG_ID_1971827_2                              19718272

#define TEST_CASE(testCaseId)                             \
    static RM_PMU_RPC_HEADER *testCase##testCaseId        \
    (                                                     \
        RM_PMU_RPC_HEADER  *pRpc,                         \
        LwU32              *pSizeRpc                      \
    )                                                     \

#define TEST_CASE_STR(testCaseId)                         TEST_CASE(testCaseId)

#define TEST_CASE_NAME(name)                              &testCase##name
#define TEST_CASE_NAME_STR(name)                          TEST_CASE_NAME(name)

typedef RM_PMU_RPC_HEADER* (*TestCaseFunction) (RM_PMU_RPC_HEADER *pRpc, LwU32 *pSizeRpc);

class PmuFuzzerParamGenerator
{
private:

    static RM_PMU_RPC_HEADER *generateRandomPayload(LwU32 size, Random m_random)
    {
        LwU32  i;
        LwU8  *payload = new LwU8[size];

        for (i = 0; i < size; i++)
        {
            payload[i] = m_random.GetRandom(LW_U8_MIN, LW_U8_MAX);
        }
        return (RM_PMU_RPC_HEADER *)payload;
    }

public:

    map<LwU32, TestCaseFunction> testCaseIdMap;

    PmuFuzzerParamGenerator()
    {
        testCaseIdMap[PMU_BUG_ID_11234]   = TEST_CASE_NAME_STR(PMU_BUG_ID_11234);

#ifndef PMU_EXCLUDE_GEN_FILE
        testCaseIdMap[PMU_BUG_ID_1970804] = TEST_CASE_NAME_STR(PMU_BUG_ID_1970804);
        testCaseIdMap[PMU_BUG_ID_1998074] = TEST_CASE_NAME_STR(PMU_BUG_ID_1998074);
        testCaseIdMap[PMU_BUG_ID_1970555] = TEST_CASE_NAME_STR(PMU_BUG_ID_1970555);
        testCaseIdMap[PMU_BUG_ID_1970872] = TEST_CASE_NAME_STR(PMU_BUG_ID_1970872);
        testCaseIdMap[PMU_BUG_ID_1971827] = TEST_CASE_NAME_STR(PMU_BUG_ID_1971827);
        testCaseIdMap[PMU_BUG_ID_1971827_1] = TEST_CASE_NAME_STR(PMU_BUG_ID_1971827_1);
        testCaseIdMap[PMU_BUG_ID_1971827_2] = TEST_CASE_NAME_STR(PMU_BUG_ID_1971827_2);
#endif
    }

    TestCaseFunction findTestCaseFunction(LwU32 testCaseId)
    {
        map<LwU32, TestCaseFunction>::iterator it;

        it = testCaseIdMap.find(testCaseId);
        if (it == testCaseIdMap.end())
        {
            return NULL;
        }
        return it->second;
    }

    TestCaseFunction* getAllTestCaseFunctions()
    {
        map<LwU32, TestCaseFunction>::iterator it;
        TestCaseFunction* functions = new TestCaseFunction[testCaseIdMap.size()];
        LwU8 i;

        for (i = 0, it = testCaseIdMap.begin(); it != testCaseIdMap.end(); it++)
        {
            functions[i++] = it->second;
        }

        return functions;
    }

    RM_PMU_RPC_HEADER *randomFuzzing
    (
        RM_PMU_RPC_HEADER  *pRpc,
        LwU32              *pSizeRpc,
        LwU32               unitId,
        LwU32               secId,
        PMU                *m_pPmu,
        Random              m_random
    )
    {
        *pSizeRpc   = m_random.GetRandom(64, 256) * sizeof(LwU32);
        pRpc        = generateRandomPayload(*pSizeRpc, m_random);

        pRpc->unitId   = (LwU8)unitId;
        pRpc->function = (LwU8)secId;

        return pRpc;
    }

    TEST_CASE_STR(PMU_BUG_ID_11234)
    {
        Random m_random;

        m_random.SeedRandom(2809);

        *pSizeRpc   = m_random.GetRandom(64, 256) * sizeof(LwU32);
        pRpc        = generateRandomPayload(*pSizeRpc, m_random);

        pRpc->unitId = RM_PMU_UNIT_SPI;

        return pRpc;
    }

#ifndef PMU_EXCLUDE_GEN_FILE
    TEST_CASE_STR(PMU_BUG_ID_1970804)
    {
        Random m_random;

        m_random.SeedRandom(2809);

        *pSizeRpc   = m_random.GetRandom(32, 64) * sizeof(LwU32);
        pRpc        = generateRandomPayload(*pSizeRpc, m_random);

        pRpc->unitId   = RM_PMU_UNIT_PERFMON;
        pRpc->function = RM_PMU_RPC_ID_PERFMON_START;
        ((RM_PMU_RPC_STRUCT_PERFMON_START *)pRpc)->groupId = 35;

        return pRpc;
    }

    TEST_CASE_STR(PMU_BUG_ID_1998074)
    {
        Random m_random;

        m_random.SeedRandom(2809);

        *pSizeRpc   = m_random.GetRandom(32, 64) * sizeof(LwU32);
        pRpc        = generateRandomPayload(*pSizeRpc, m_random);

        pRpc->unitId   = RM_PMU_UNIT_CMDMGMT;

        return pRpc;
    }

    TEST_CASE_STR(PMU_BUG_ID_1970555)
    {
        Random m_random;

        m_random.SeedRandom(2809);

        *pSizeRpc   = sizeof(RM_PMU_RPC_STRUCT_PMGR_GPIO_INIT_CFG);
        pRpc        = generateRandomPayload(*pSizeRpc, m_random);

        pRpc->unitId   = RM_PMU_UNIT_PMGR;
        pRpc->function = RM_PMU_RPC_ID_PMGR_GPIO_INIT_CFG;

        //if you pass 35 as a gpioPinCnt test crashes with exterr
        ((RM_PMU_RPC_STRUCT_PMGR_GPIO_INIT_CFG *)pRpc)->gpioPinCnt = 100;

        return pRpc;
    }

    TEST_CASE_STR(PMU_BUG_ID_1970872)
    {
        Random m_random;

        m_random.SeedRandom(2809);

        *pSizeRpc   = sizeof(RM_PMU_RPC_STRUCT_PMGR_GPIO_INIT_CFG);
        pRpc        = generateRandomPayload(*pSizeRpc, m_random);

        pRpc->unitId   = RM_PMU_UNIT_THERM;
        pRpc->function = RM_PMU_RPC_ID_THERM_SW_SLOW_SET;

        //if you pass 1 or 3 as a value test crashes at the end. For 0 it passes.
        ((RM_PMU_RPC_STRUCT_THERM_SW_SLOW_SET *)pRpc)->clkIndex = 13;

        return pRpc;
    }

    /*
     *  This repro test is WIP. It requires some code refactoring.
     *  Submitting the code as-is to document the order of sequence to repro the bug
     */
    TEST_CASE_STR(PMU_BUG_ID_1971827)
    {
        Random m_random;

        m_random.SeedRandom(2809);

        *pSizeRpc   = sizeof(RM_PMU_RPC_STRUCT_PMGR_GPIO_INIT_CFG);
        pRpc        = generateRandomPayload(*pSizeRpc, m_random);

        pRpc->unitId   = RM_PMU_UNIT_DISP;
        pRpc->function = RM_PMU_RPC_ID_DISP_BC_UPDATE_HEADINFO;

        ((RM_PMU_RPC_STRUCT_DISP_BC_UPDATE_HEADINFO *)pRpc)->pwmIndexHead = 35;

        //
        // The dependency for this is to call RM_PMU_RPC_ID_DISP_BC_UPDATE_FPINFO,
        // (PMU_BUG_ID_1971827_1) followed by RM_PMU_RPC_ID_DISP_BC_SET_BRIGHTNESS
        // (PMU_BUG_ID_1971827_2).
        //
        // However, this infra does not yet support multiple command submission.
        //

        return pRpc;
    }

    TEST_CASE_STR(PMU_BUG_ID_1971827_1)
    {
        Random m_random;
        LwU32  sizeTemp = 1024;
        LwU64  temp     = (LwU64) new LwU8 (sizeTemp);

        m_random.SeedRandom(2809);

        *pSizeRpc   = sizeof(RM_PMU_RPC_STRUCT_PMGR_GPIO_INIT_CFG);
        pRpc        = generateRandomPayload(*pSizeRpc, m_random);

        pRpc->unitId   = RM_PMU_UNIT_DISP;
        pRpc->function = RM_PMU_RPC_ID_DISP_BC_UPDATE_FPINFO;

        ((RM_PMU_RPC_STRUCT_DISP_BC_UPDATE_FPINFO *)pRpc)->dmaDesc.params =
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _SIZE, sizeTemp) |
            DRF_NUM(_RM_FLCN_MEM_DESC, _PARAMS, _DMA_IDX, RM_PMU_DMAIDX_PHYS_SYS_COH);

        ((RM_PMU_RPC_STRUCT_DISP_BC_UPDATE_FPINFO *)pRpc)->dmaDesc.address.lo =
            ((temp << 32) >> 32);
        ((RM_PMU_RPC_STRUCT_DISP_BC_UPDATE_FPINFO *)pRpc)->dmaDesc.address.hi =
            (temp >> 32);

        // once the infra is ready, free the temp buffer
        return pRpc;
    }

    TEST_CASE_STR(PMU_BUG_ID_1971827_2)
    {
        Random m_random;

        m_random.SeedRandom(2809);

        *pSizeRpc   = sizeof(RM_PMU_RPC_STRUCT_PMGR_GPIO_INIT_CFG);
        pRpc        = generateRandomPayload(*pSizeRpc, m_random);

        pRpc->unitId   = RM_PMU_UNIT_DISP;
        pRpc->function = RM_PMU_RPC_ID_DISP_BC_SET_BRIGHTNESS;

        ((RM_PMU_RPC_STRUCT_DISP_BC_SET_BRIGHTNESS *)pRpc)->brightnessLevel = 900;

        return pRpc;
    }

#endif

};

#endif
