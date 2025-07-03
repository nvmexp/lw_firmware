/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <bitset>
#include "core/include/mgrmgr.h"
#include "core/include/script.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/lwdastst.h"
#include "cbu/cbu.h"

// -----------------------------------------------------------------------------
class LwdaCbu : public LwdaStreamTest
{
public:
    LwdaCbu();
    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;

    static UINT32 GetBrxJmxTestExpectedVal(UINT32 numThreads);

    SETGET_PROP(StressFactor,            UINT32);
    SETGET_PROP(SubtestMask,             UINT32);
private:

    enum Subtest
    {
        BRA_64_X1_DIVERGENCE,
        BRA_LINEAR_DIVERGENCE,
        BRA_U_BINARY_DIVERGENCE_HIGH_EXIT,
        BRA_U_BINARY_DIVERGENCE_LOW_EXIT,
        BRA_COLW_EXIT,
        BRA_COLW_WARP_SYNC,
        BRA_DIV_WARP_SYNC,
        BRA_DIV_EXIT,
        BRX_32X_MULTIWAY,
        JMX_32X_MULTIWAY,
        EXIT_VERIFY_AT_EXIT,
        S_PRODUCER_M_CONSUMER_BSSY_BSYN,
        S_PRODUCER_M_CONSUMER_WARP_SYNC,
        PAIR_WISE_PRODUCER_CONSUMER,
    };

    RC RunSubtest(Subtest subtest);
    RC RunBraDivergence(Subtest subtest);
    RC RunBraDivergenceExit(Subtest subtest);
    RC RunBraColwDiv(Subtest subtest);
    RC RunExitVerifyAtExit();
    RC RunMultiway(Subtest subtest);
    RC RunProducerConsumer(Subtest subtest);

    RC AllocMemory();
    RC InitCbuFunc(Subtest subtest);
    RC CallCbuFunc(UINT32 blockDim, Subtest subtest);

    const char* GetCbuFuncName(Subtest subtest) const;

    CbuParams           m_Params = {};
    UINT32              m_ArraySize = 0;
    UINT32              m_ThreadStepArrSize = 0;
    UINT32              m_ThreadsPerWarp = 0;
    UINT32              m_NumBlocks = 0;
    UINT32              m_NumFlagAccesses = 0;

    vector<Subtest>     m_SubtestsToRun;

    Lwca::Module        m_CbuMod;
    Lwca::Function      m_CbuFunc;
    Lwca::DeviceMemory  m_TestMem;
    Lwca::DeviceMemory  m_FlagArrMem;
    Lwca::DeviceMemory  m_ThreadStepArrMem;

    vector<UINT08> m_HostData;
    vector<UINT32> m_HostData32;

    //! Variables set from JS
    UINT32 m_StressFactor = 128;
    UINT32 m_SubtestMask = 0x3FFF;
};

//------------------------------------------------------------------------------
//! \brief Constructor
//!
LwdaCbu::LwdaCbu()
{
    SetName("LwdaCbu");
}

//------------------------------------------------------------------------------
//! \brief Check if the test is supported.
//!
//! \return true if supported, false otherwise
bool LwdaCbu::IsSupported()
{
    // Check if compute is supported at all
    if (!LwdaStreamTest::IsSupported())
    {
        return false;
    }

    // The kernel is compiled only for compute 7.x+
    const Lwca::Capability cap = GetBoundLwdaDevice().GetCapability();
    if (cap < Lwca::Capability::SM_70)
        return false;

    return true;
}

//------------------------------------------------------------------------------
//! \brief Allocate the required memory for all the tests
//!
//! \return OK if memory is allocated properly, not OK otherwise
RC LwdaCbu::AllocMemory()
{
    RC rc;
    // Size the block/grid to ensure 100% thread oclwpancy without violating
    // dimension requirements
    m_NumBlocks         = m_StressFactor * GetBoundLwdaDevice().GetShaderCount();
    m_ThreadsPerWarp    = GetBoundLwdaDevice().GetAttribute(LW_DEVICE_ATTRIBUTE_WARP_SIZE);
    m_ArraySize         = m_NumBlocks * m_ThreadsPerWarp;
    m_ThreadStepArrSize = m_ArraySize * sizeof(UINT32);
    UINT32 flagArrSize  = m_ThreadStepArrSize / 2;
    m_NumFlagAccesses   = 16 * m_StressFactor;

    m_HostData.resize(m_ArraySize, UINT08());
    m_HostData32.resize(m_ThreadStepArrSize, UINT32());

    CHECK_RC(GetLwdaInstance().AllocDeviceMem(GetBoundLwdaDevice(), m_ArraySize, &m_TestMem));
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(GetBoundLwdaDevice(), flagArrSize, &m_FlagArrMem));
    CHECK_RC(GetLwdaInstance().AllocDeviceMem(GetBoundLwdaDevice(), m_ThreadStepArrSize, 
                                              &m_ThreadStepArrMem));

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Returns the name of the CBU function
//!
//! \return Name of the CBU Function
const char* LwdaCbu::GetCbuFuncName(Subtest subtest) const
{
    const char* str = "";
    switch (subtest)
    {
        case BRA_64_X1_DIVERGENCE:
            str = "Bra64x1Divergence";
            break;

        case BRA_LINEAR_DIVERGENCE:
            str = "BraLinearDivergence";
            break;

        case BRA_U_BINARY_DIVERGENCE_HIGH_EXIT:
            str = "BraUBinaryDivergenceHighExit";
            break;

        case BRA_U_BINARY_DIVERGENCE_LOW_EXIT:
            str = "BraUBinaryDivergenceLowExit";
            break;

        case BRA_COLW_EXIT:
            str = "BraColwExit";
            break;

        case BRA_COLW_WARP_SYNC:
            str = "BraColwWarpSync";
            break;

        case BRA_DIV_WARP_SYNC:
            str = "BraDivWarpSync";
            break;

        case BRA_DIV_EXIT:
            str = "BraDivExit";
            break;

        case EXIT_VERIFY_AT_EXIT:
            str = "ExitVerifyAtExit";
            break;

        case BRX_32X_MULTIWAY:
            str = "Brx32xMultiway";
            break;

        case JMX_32X_MULTIWAY:
            str = "Jmx32xMultiway";
            break;

        case S_PRODUCER_M_CONSUMER_BSSY_BSYN:
            str = "SProducerMConsumerBssyBsyn";
            break;

        case S_PRODUCER_M_CONSUMER_WARP_SYNC:
            str = "SProducerMConsumerWarpSync";
            break;

        case PAIR_WISE_PRODUCER_CONSUMER:
            str = "PairWiseProducerConsumer";
            break;
    }
    return str;
}

//------------------------------------------------------------------------------
//! \brief Initialises the CBU function
//!
//!
RC LwdaCbu::InitCbuFunc(Subtest subtest)
{
    RC rc;

    m_CbuFunc = m_CbuMod.GetFunction(GetCbuFuncName(subtest));
    CHECK_RC(m_CbuFunc.InitCheck());
    m_CbuFunc.SetGridDim(m_NumBlocks);

    return rc;
}
//------------------------------------------------------------------------------
//! \brief Calls the CBU Function and gets the data into m_HostData
//!
//! \param blockDim : block dimension of the CBU function
RC LwdaCbu::CallCbuFunc(UINT32 blockDim, Subtest subtest)
{
    RC rc;

    m_CbuFunc.SetBlockDim(blockDim);

    switch (subtest)
    {
        case BRA_64_X1_DIVERGENCE:
        case BRA_LINEAR_DIVERGENCE:
        case BRA_U_BINARY_DIVERGENCE_HIGH_EXIT:
        case BRA_U_BINARY_DIVERGENCE_LOW_EXIT:
        case BRA_COLW_EXIT:
        case BRA_COLW_WARP_SYNC:
        case BRA_DIV_WARP_SYNC:
        case BRA_DIV_EXIT:
        case EXIT_VERIFY_AT_EXIT:
            CHECK_RC(m_TestMem.Fill(0));

            CHECK_RC(m_CbuFunc.Launch(m_TestMem.GetDevicePtr()));
            CHECK_RC(m_TestMem.Get(&m_HostData[0], m_ArraySize));
            CHECK_RC(GetLwdaInstance().Synchronize(GetBoundLwdaDevice()));
            break;

        case BRX_32X_MULTIWAY:
        case JMX_32X_MULTIWAY:
            CHECK_RC(m_TestMem.Fill(0));

            CHECK_RC(m_CbuFunc.Launch(m_TestMem.GetDevicePtr(),
                                      static_cast<UINT64>((0x1ULL << blockDim) - 1)));
            CHECK_RC(m_TestMem.Get(&(m_HostData32)[0], m_ArraySize));
            CHECK_RC(GetLwdaInstance().Synchronize(GetBoundLwdaDevice()));
            break;

        case S_PRODUCER_M_CONSUMER_BSSY_BSYN:
        case S_PRODUCER_M_CONSUMER_WARP_SYNC:
        case PAIR_WISE_PRODUCER_CONSUMER:

            CHECK_RC(m_FlagArrMem.Fill(0));
            CHECK_RC(m_ThreadStepArrMem.Fill(0));
            
            // Temporary workaround for merlwry multiple unpacked argument issue
            if (GetBoundLwdaDevice().GetCapability() == Lwca::Capability::SM_90) 
            {
                m_Params.flagArrMem = m_FlagArrMem.GetDevicePtr();
                m_Params.threadStepArrMem = m_ThreadStepArrMem.GetDevicePtr();
                m_Params.numFlagAccesses = m_NumFlagAccesses;
                CHECK_RC(m_CbuFunc.Launch(m_Params));
            }
            else
            {
                CHECK_RC(m_CbuFunc.Launch(m_FlagArrMem.GetDevicePtr(),
                                          m_ThreadStepArrMem.GetDevicePtr(),
                                          m_NumFlagAccesses));
            }
            CHECK_RC(m_ThreadStepArrMem.Get(&(m_HostData32)[0], m_ThreadStepArrSize));
            CHECK_RC(GetLwdaInstance().Synchronize(GetBoundLwdaDevice()));
            break;
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Setup the test
//!
//! \return OK if setup succeeds, not OK otherwise
/* virtual */ RC LwdaCbu::Setup()
{
    RC rc;
    CHECK_RC(LwdaStreamTest::Setup());

    CHECK_RC(GetLwdaInstance().LoadNewestSupportedModule(GetBoundLwdaDevice(), "cbu", &m_CbuMod));

    bitset<sizeof(UINT32)*8> subtestBitset(m_SubtestMask);
    for (UINT32 i = 0; i < subtestBitset.size(); i++)
    {
        if (subtestBitset[i])
            m_SubtestsToRun.push_back(static_cast<Subtest>(i));
    }

    if (m_SubtestsToRun.empty())
        return RC::NO_TESTS_RUN;

    CHECK_RC(AllocMemory());
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Callwlates and returns the expected value for the Brx and Jmx sub-tests
//!
//! \return Expected Value for the Brx and Jmx sub-tests
/* static */ UINT32 LwdaCbu::GetBrxJmxTestExpectedVal(UINT32 numThreads)
{
    return numThreads * (numThreads - 1) / 2;
}

//------------------------------------------------------------------------------
//! \brief Common run function for Bra Divergence subtests
//!
//! \return OK if the subtest passes, not OK otherwise
RC LwdaCbu::RunBraDivergence(Subtest subtest)
{
    RC rc;
    CHECK_RC(InitCbuFunc(subtest));

    UINT32 numCellsToVerify = 0;
    UINT32 numCellsVerified = 0;
    // Launch the kernel in various test configurations
    for (UINT32 pivot = 1; pivot <= m_ThreadsPerWarp; pivot++)
    {
        CHECK_RC(CallCbuFunc(pivot, subtest));
        // Verify that each warp behaved amicably and that verification logic touched
        // all the cells that were supposed to exist since we are allocating the array
        // only once and reusing it
        UINT08 expData = 0x1;
        for (UINT32 blockIdx = 0; blockIdx < m_NumBlocks; blockIdx++)
        {
            numCellsToVerify += pivot;
            for (UINT32 threadIdx = 0; threadIdx < pivot; threadIdx++)
            {
                UINT32 linearThreadIdx = blockIdx * pivot + threadIdx;
                if (subtest == BRA_LINEAR_DIVERGENCE)
                {
                    expData = 0x1 + threadIdx;
                }
                if (m_HostData[linearThreadIdx] != expData)
                {
                    Printf(Tee::PriError, "%s subtest failed\n", GetCbuFuncName(subtest));
                    Printf(Tee::PriError,
                           "[ P: %u => Expected Data [%u:%u] Mismatch - "
                                "E: 0x%hhx, A: 0x%hhx ] !!!\n",
                           pivot,
                           blockIdx,
                           threadIdx,
                           expData,
                           m_HostData[linearThreadIdx]);
                    return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
                }
                numCellsVerified += 1;
            }
        }
    }

    if (numCellsToVerify != numCellsVerified)
    {
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Common run function for Bra Divergence Exit subtests
//!
//! \return OK if the subtest passes, not OK otherwise
RC LwdaCbu::RunBraDivergenceExit(Subtest subtest)
{
    RC rc;

    CHECK_RC(InitCbuFunc(subtest));

    UINT32 numCellsToVerify = 0;
    UINT32 numCellsVerified = 0;
    for (UINT32 nThreadsPerBlock = 1; nThreadsPerBlock <= m_ThreadsPerWarp; nThreadsPerBlock++)
    {
        CHECK_RC(CallCbuFunc(nThreadsPerBlock, subtest));
        // Verify that each warp behaved amicably and that verification logic touched
        // all the cells that were supposed to exist since we are allocating the array
        // only once and reusing it
        for (UINT32 blockIdx = 0; blockIdx < m_NumBlocks; blockIdx++)
        {
            numCellsToVerify += nThreadsPerBlock;
            UINT32 crossOver = 16;
            UINT32 rExpect = (subtest == BRA_U_BINARY_DIVERGENCE_HIGH_EXIT) ? 1 : 16;
            UINT32 expData = (subtest == BRA_U_BINARY_DIVERGENCE_HIGH_EXIT) ? 1 :
                             (nThreadsPerBlock <= rExpect) ? nThreadsPerBlock : rExpect;
            for (UINT32 threadIdx = 0; threadIdx < nThreadsPerBlock; threadIdx++)
            {
                if (subtest == BRA_U_BINARY_DIVERGENCE_HIGH_EXIT)
                {
                    if (threadIdx == rExpect << 1)
                    {
                        expData = (nThreadsPerBlock - threadIdx) < (rExpect << 1) ?
                                  (nThreadsPerBlock - threadIdx) : (rExpect << 1);
                        rExpect <<= 1;
                    }
                }
                else
                {
                    if (threadIdx == crossOver && (rExpect >> 1))
                    {
                        UINT32 nextCrossOver = crossOver + (rExpect >> 1);
                        expData = (nThreadsPerBlock < nextCrossOver) ?
                                  (nThreadsPerBlock - crossOver) : (rExpect >>= 1);
                        crossOver = nextCrossOver;
                    }
                }
                UINT32 linearThreadIdx = blockIdx * nThreadsPerBlock + threadIdx;
                if (m_HostData[linearThreadIdx] != expData)
                {
                    Printf(Tee::PriError, "BraUBinaryDivergenceHighExit subtest failed\n");
                    Printf(Tee::PriError,
                           "[ P: %u => Expected Data [%u:%u] Mismatch - E: %u, A: %u ] !!!\n",
                           nThreadsPerBlock,
                           blockIdx,
                           threadIdx,
                           expData,
                           m_HostData[linearThreadIdx]);
                    return RC::MEM_TO_MEM_RESULT_NOT_MATCH;

                }
                numCellsVerified += 1;
            }
        }
    }
    if (numCellsToVerify != numCellsVerified)
    {
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Common run function for Div and Colw subtests
//!
//! \return OK if the subtest passes, not OK otherwise
RC LwdaCbu::RunBraColwDiv(Subtest subtest)
{
    RC rc;

    CHECK_RC(InitCbuFunc(subtest));
    UINT32 numCellsToVerify = 0;
    UINT32 numCellsVerified = 0;
    for (UINT32 nThreadsPerBlock = 1; nThreadsPerBlock <= m_ThreadsPerWarp; nThreadsPerBlock++)
    {
        // Binary tree pruning configuration for this test
        CHECK_RC(CallCbuFunc(nThreadsPerBlock, subtest));

        // Verify that each warp behaved amicably
        for (UINT32 blockIdx = 0; blockIdx < m_NumBlocks; blockIdx++)
        {
            numCellsToVerify += nThreadsPerBlock;
            numCellsVerified += 1; // Cell[0] is verified multiple times
            UINT32 pTidScale = 1;
            UINT32 pTidRange = nThreadsPerBlock;
            while (pTidRange != 0)
            {
                for (UINT32 pTid = 0; pTid < pTidRange; pTid++)
                {
                    UINT32 threadIdx = pTid * pTidScale;
                    UINT32 linearThreadIdx = blockIdx * nThreadsPerBlock + (pTid * pTidScale);
                    numCellsVerified += (pTid % 2);
                    if (subtest == BRA_COLW_EXIT || subtest == BRA_DIV_EXIT)
                    {
                        if ((pTid % 2 && m_HostData[linearThreadIdx] != pTidRange >> 1) ||
                            (pTid == 0 && m_HostData[linearThreadIdx] != 1))
                        {
                            Printf(Tee::PriError, "%s subtest failed\n", GetCbuFuncName(subtest));
                            Printf(Tee::PriError,
                                   "[ P: %u => Expected Data [%u:%u] Mismatch - "
                                        "E: 0x%x, A: %hhx ] !!!\n",
                                   nThreadsPerBlock,
                                   blockIdx,
                                   threadIdx,
                                   pTidRange >> 1,
                                   m_HostData[linearThreadIdx]);
                            return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
                        }
                    }
                    else
                    {
                        UINT08 expData = nThreadsPerBlock + threadIdx;
                        if ((pTid % 2 && m_HostData[linearThreadIdx] != expData) ||
                            (pTid == 0 && m_HostData[linearThreadIdx] != expData))
                        {
                            Printf(Tee::PriError, "%s subtest failed\n", GetCbuFuncName(subtest));
                            Printf(Tee::PriError,
                                   "[ P: %u => Expected Data [%u:%u] Mismatch - "
                                        "E: 0x%hhx, A: %hhx ] !!!\n",
                                   nThreadsPerBlock,
                                   blockIdx,
                                   threadIdx,
                                   expData,
                                   m_HostData[linearThreadIdx]);
                            return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
                        }
                    }
                }
                pTidScale <<= 1;
                pTidRange -= ((pTidRange == 1) ? 1 : (pTidRange >> 1));
            }
        }
    }
    if (numCellsToVerify != numCellsVerified)
    {
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Common run function for the Multiway Subtests
//!
//! \return OK if the subtest passes, not OK otherwise
RC LwdaCbu::RunMultiway(Subtest subtest)
{
    RC rc;
    CHECK_RC(InitCbuFunc(subtest));
    for (UINT32 nThreadsPerBlock = 1; nThreadsPerBlock <= m_ThreadsPerWarp; nThreadsPerBlock++)
    {
        // Binary tree pruning configuration for this test
        CHECK_RC(CallCbuFunc(nThreadsPerBlock, subtest));

        UINT32 expData = GetBrxJmxTestExpectedVal(nThreadsPerBlock);
        for (UINT32 blockIdx = 0; blockIdx < m_NumBlocks; blockIdx++)
        {
            if ((m_HostData32)[blockIdx] != expData)
            {
                Printf(Tee::PriError, "%s subtest failed\n", GetCbuFuncName(subtest));
                Printf(Tee::PriError,
                       "[ P: %u => result [%u] Mismatch - E: %u, A: %u ] !!!\n",
                       nThreadsPerBlock,
                       blockIdx,
                       expData,
                       (m_HostData32)[blockIdx]);
                return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Common run function for the Producer Consumer subtests
//!
//! \return OK if test runs successfully
RC LwdaCbu::RunProducerConsumer(Subtest subtest)
{
    RC rc;
    CHECK_RC(InitCbuFunc(subtest));
    CHECK_RC(CallCbuFunc(m_ThreadsPerWarp, subtest));

    // Verification:
    // 1. m_HostData[0] for thread 0 will do m_NumFlagAccesses * 16
    // 2. m_HostData[1..15] for threads 1..15 will each do 1 flag access as specified by test

    UINT32 expData;

    for (UINT32 blockIdx = 0; blockIdx < m_NumBlocks; blockIdx++)
    {
        for (UINT32 threadIdx = 0; threadIdx < m_ThreadsPerWarp; threadIdx++)
        {
            UINT32 linearThreadIdx = blockIdx * m_ThreadsPerWarp + threadIdx;
            if (subtest == PAIR_WISE_PRODUCER_CONSUMER)
            {
                expData = m_NumFlagAccesses;
            }
            else
            {
                expData = (threadIdx == 0) ? (m_NumFlagAccesses * 16) :
                          (threadIdx < 16) ? m_NumFlagAccesses : 0;
            }
            if ((m_HostData32)[linearThreadIdx] != expData)
            {
                Printf(Tee::PriError, "%s subtest failed\n", GetCbuFuncName(subtest));
                Printf(Tee::PriError,
                       "[ P: %u => Thread Access Array [%u:%u] Mismatch - E: %u, A: %u ] !!!\n",
                       m_ThreadsPerWarp,
                       blockIdx,
                       threadIdx,
                       expData,
                       (m_HostData32)[linearThreadIdx]);
                return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
//! \brief Run the ExitVerifyAtExit subtest
//!
//! \return OK if the subtest passes, not OK otherwise
RC LwdaCbu::RunExitVerifyAtExit()
{
    RC rc;

    CHECK_RC(InitCbuFunc(EXIT_VERIFY_AT_EXIT));
    CHECK_RC(CallCbuFunc(m_ThreadsPerWarp, EXIT_VERIFY_AT_EXIT));

    const UINT08 expData = 0x1;

    for (UINT32 blockIdx = 0; blockIdx < m_NumBlocks; blockIdx++)
    {
        for (UINT32 threadIdx = 0; threadIdx < m_ThreadsPerWarp; threadIdx++)
        {
            UINT32 linearThreadIdx = blockIdx * m_ThreadsPerWarp + threadIdx;
            if (m_HostData[linearThreadIdx] != expData)
            {
                Printf(Tee::PriError, "%s subtest failed", GetCbuFuncName(EXIT_VERIFY_AT_EXIT));
                Printf(Tee::PriError,
                       "[ P: %u => Expected Data [%u:%u] Mismatch - E: 0x%hhx, A: 0x%hhx ] !!!\n",
                       m_ThreadsPerWarp,
                       blockIdx,
                       threadIdx,
                       expData,
                       m_HostData[linearThreadIdx]);
                return RC::MEM_TO_MEM_RESULT_NOT_MATCH;
            }
        }
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Run the chosen subtest
//!
//! \param subtest The subtest chosen to be run
//! \return OK if the chosen CBU subtest passes, not OK otherwise
RC LwdaCbu::RunSubtest(Subtest subtest)
{
    RC rc;
    VerbosePrintf("Running %s(0x%x)\n", GetCbuFuncName(subtest), (1 << subtest));
    switch (subtest)
    {
        case BRA_64_X1_DIVERGENCE:
        case BRA_LINEAR_DIVERGENCE:
            rc = RunBraDivergence(subtest);
            break;

        case BRA_U_BINARY_DIVERGENCE_HIGH_EXIT:
        case BRA_U_BINARY_DIVERGENCE_LOW_EXIT:
            rc = RunBraDivergenceExit(subtest);
            break;

        case BRA_COLW_EXIT:
        case BRA_COLW_WARP_SYNC:
        case BRA_DIV_WARP_SYNC:
        case BRA_DIV_EXIT:
            rc = RunBraColwDiv(subtest);
            break;

        case EXIT_VERIFY_AT_EXIT:
            rc = RunExitVerifyAtExit();
            break;

        case BRX_32X_MULTIWAY:
        case JMX_32X_MULTIWAY:
            rc = RunMultiway(subtest);
            break;

        case S_PRODUCER_M_CONSUMER_BSSY_BSYN:
        case S_PRODUCER_M_CONSUMER_WARP_SYNC:
        case PAIR_WISE_PRODUCER_CONSUMER:
            rc = RunProducerConsumer(subtest);
            break;
    }

    return rc;
}

//------------------------------------------------------------------------------
//! \brief Run the test
//!
//! \return OK if all the CBU subtests pass, not OK otherwise
RC LwdaCbu::Run()
{
    StickyRC rc;
    // Run all the sub-tests
    for (UINT32 i = 0; i < m_SubtestsToRun.size(); i++)
    {
        rc = RunSubtest(m_SubtestsToRun[i]);
        if (GetGoldelwalues()->GetStopOnError())
            CHECK_RC(rc);
    }
    return rc;
}

//-----------------------------------------------------------------------------
//! \brief Print the test properties
//!
//! \param pri : Priority to print the test properties at
void LwdaCbu::PrintJsProperties(Tee::Priority pri)
{
    Printf(pri, "%s Js Properties:\n", GetName().c_str());
    Printf(pri, "\tStressFactor:             %u\n", m_StressFactor);
    Printf(pri, "\tSubtestMask:            0x%x\n", m_SubtestMask);

    LwdaStreamTest::PrintJsProperties(pri);
}

//-----------------------------------------------------------------------------
//! \brief Perform Cleanup
//!
//! \return OK if the cleanup was successfull, not OK otherwise
RC LwdaCbu::Cleanup()
{
    m_TestMem.Free();
    m_FlagArrMem.Free();
    m_ThreadStepArrMem.Free();

    m_CbuMod.Unload();

    return LwdaStreamTest::Cleanup();
}

JS_CLASS_INHERIT(LwdaCbu, LwdaStreamTest, "LWCA CBU test");
CLASS_PROP_READWRITE(LwdaCbu, StressFactor, UINT32,
                     "Stress factor for the test (default = 128)");
CLASS_PROP_READWRITE(LwdaCbu, SubtestMask, UINT32,
                     "Subtest Mask for running the subtest (default = 0x3FFF)");
