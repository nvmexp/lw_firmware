/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file ucodefuzzdriver.cpp
//! \brief ucode fuzz driver base classes (for all ucodes)
//!

#include <stdlib.h>

#include "ucodefuzzdriver.h"

#include "random.h"
#include "mqdatainterface.h"

//! Random data source used to test isolated from MQ system
//!
class RandomDataSource : public FuzzDataInterface
{
public:
    RandomDataSource(LwU32 seed, LwU32 maxIter);
    virtual RC Initialize(LwU32 maxSize) override;
    virtual RC NextBytes(std::vector<LwU8>& bytes) override;
    virtual RC SubmitResult(const TestcaseResult& result) override { return RC::OK; }
private:
    Random m_Random;
    LwU32  m_MaxSize;
    LwU32  m_RemIter;
};

RandomDataSource::RandomDataSource(LwU32 seed, LwU32 maxIter)
{
    m_RemIter = maxIter;
    m_Random.SeedRandom(seed);
}

RC RandomDataSource::Initialize(LwU32 maxSize)
{
    m_MaxSize = maxSize;
    return RC::OK;
}

RC RandomDataSource::NextBytes(std::vector<LwU8>& bytes)
{
    if (m_RemIter-- == 0)
        return RC::EXIT_OK;

    bytes.resize(m_Random.GetRandom(0, m_MaxSize));
    for (auto& byte : bytes)
    {
        byte = m_Random.GetRandom(LW_U8_MIN, LW_U8_MAX);
    }

    return RC::OK;
}

RC UcodeFuzzDriver::Setup()
{
    RC rc;

    CHECK_RC(InitFromJs());

    m_LogVerbose = (getelw("UCODEFUZZ_MODS_DRIVER_LOG_VERBOSE") != nullptr);

    m_pUcodeFuzzer = make_unique<UcodeFuzzerSub>(GetBoundGpuSubdevice());

    switch (m_DataInterface)
    {
        case RANDOM_IF:
            m_pDataInterface = make_unique<RandomDataSource>(m_RngSeed, m_MaxIter);
            break;
        case MQ_IF:
            m_pDataInterface = make_unique<MqDataInterface>();
            break;
        default:
            Printf(Tee::PriError, "Invalid data interface!\n");
            CHECK_RC(RC::LWRM_ILWALID_ARGUMENT);
    }

    return rc;
}

RC UcodeFuzzDriver::Cleanup()
{
    m_pUcodeFuzzer.reset();
    m_pDataInterface.reset();

    return RC::OK;
}

RC UcodeFuzzDriver::Run()
{
    RC                 rc;
    std::vector<LwU8>  fuzzBytes;
    auto               *pDataTemplate = GetDataTemplate();
    TestcaseResult     result;

    if (pDataTemplate == nullptr)
    {
        Printf(Tee::PriError, "UcodeFuzzDriver: Null data template instance\n");
        return RC::SOFTWARE_ERROR;
    }
    // This needs to happen after Setup() since the data template may not be
    //  ready before the child's Setup()
    CHECK_RC(m_pDataInterface->Initialize(pDataTemplate->MaxInputSize()));

    CHECK_RC(ErrorLogger::StartingTest());
    //ErrorLogger::IgnoreErrorsForThisTest();

    // Run the fuzzing loop
    while (1)
    {
        rc = m_pDataInterface->NextBytes(fuzzBytes);
        if (rc == RC::EXIT_OK)
        {
            rc = RC::OK;
            break;
        }
        CHECK_RC(rc);

        auto *pData = pDataTemplate->Fill(fuzzBytes);

        // Let the ucode do any last-minute, per-cmd op (i.e. wait for "ready")
        CHECK_RC(PreRun(pData));

        if (m_DryRun)
        {
            Printf(Tee::PriNormal, "UcodeFuzzDriver: Dry run, command not sent\n");
        }
        else
        {
            // Try to enable coverage on the ucode
            rc = CoverageEnable();
            switch (rc)
            {
                case RC::OK:
                    if (m_LogVerbose)
                    {
                        Printf(Tee::PriNormal, "UcodeFuzzDriver: Coverage enabled successfully\n");
                    }
                    break;
                case RC::LWRM_NOT_SUPPORTED:
                    break;
                default:
                    Printf(Tee::PriError,
                           "UcodeFuzzDriver: Could not enable coverage (%d: %s)\n",
                           rc.Get(), rc.Message());
                    break;
            }

            if (m_LogVerbose)
            {
                Printf(Tee::PriNormal, "UcodeFuzzDriver: Sending command...\n");
            }

            rc = SubmitFuzzedData(pData);
            result.rc = rc;
            switch (result.rc)
            {
                case RC::OK:
                    if (m_LogVerbose)
                    {
                        Printf(Tee::PriNormal, "UcodeFuzzDriver: Command sent\n");
                    }
                    break;
                default:
                    Printf(Tee::PriError,
                        "UcodeFuzzDriver: Error sending command (%d: %s)\n",
                            rc.Get(), rc.Message());
                    break;
            }

            // Try and get coverage from the ucode (but skip for timeout and
            // insufficient resource results where we're unlikely to get anything)
            if (result.rc != RC::LWRM_TIMEOUT &&
                result.rc != RC::LWRM_INSUFFICIENT_RESOURCES)
            {
                rc = CoverageDisable();
                switch (rc)
                {
                    case RC::OK:
                        if (m_LogVerbose)
                        {
                            Printf(Tee::PriNormal, "UcodeFuzzDriver: Coverage disabled successfully\n");
                        }
                        break;
                    case RC::LWRM_NOT_SUPPORTED:
                        break;
                    default:
                        Printf(Tee::PriError,
                            "UcodeFuzzDriver: Could not disable coverage (%d: %s)\n",
                            rc.Get(), rc.Message());
                        break;
                }

                rc = CoverageCollect(result);
                switch (rc)
                {
                    case RC::OK:
                        if (result.covType == UCODEFUZZ_MSG_COV_SANITIZER_COVERAGE)
                        {
                            if (m_LogVerbose)
                            {
                                Printf(Tee::PriNormal,
                                    "UcodeFuzzDriver: Coverage collected (%lu SanitizerCoverage elements)\n",
                                    result.covData.sanitizerCoverage.size());
                            }
                        }
                        else
                        {
                            Printf(Tee::PriNormal, "UcodeFuzzDriver: Coverage collected\n");
                        }
                        break;

                    case RC::LWRM_NOT_SUPPORTED:
                        if (m_LogVerbose)
                        {
                            Printf(Tee::PriNormal, "UcodeFuzzDriver: Coverage not supported\n");
                        }
                        result.covType = UCODEFUZZ_MSG_COV_NONE;  // indicate no coverage
                        break;

                    default:
                        Printf(Tee::PriError,
                            "UcodeFuzzDriver: Could not collect coverage (%d: %s)\n",
                            rc.Get(), rc.Message());
                        result.covType = UCODEFUZZ_MSG_COV_NONE;  // indicate no coverage
                        break;
                }
            }
        }

        CHECK_RC(m_pDataInterface->SubmitResult(result));

        // clear coverage data
        switch (result.covType)
        {
            case UCODEFUZZ_MSG_COV_SANITIZER_COVERAGE:
                result.covData.sanitizerCoverage.clear();
                break;
            case UCODEFUZZ_MSG_COV_NONE:
                break;  // nothing to be done
        }

        // delete the structured data since its polymorphic/heap-allocated
        delete pData;
    }

    Printf(Tee::PriNormal, "UcodeFuzzDriver: Fuzzing done, exiting normally\n");
    return rc;
}

RC UcodeFuzzDriver::CoverageEnable()
{
    return RC::LWRM_NOT_SUPPORTED;
}

RC UcodeFuzzDriver::CoverageDisable()
{
    return RC::LWRM_NOT_SUPPORTED;
}

RC UcodeFuzzDriver::CoverageCollect(TestcaseResult& result)
{
    return RC::LWRM_NOT_SUPPORTED;
}
