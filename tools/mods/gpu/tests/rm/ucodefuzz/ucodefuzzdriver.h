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

#ifndef UCODEFUZZDRIVER_H_INCLUDED
#define UCODEFUZZDRIVER_H_INCLUDED

//!
//! \file ucodefuzzdriver.h
//! \brief ucode fuzz driver base classes (for all ucodes)
//!

#include "gpu/tests/rmtest.h"
#include "gpu/tests/rm/utility/dtiutils.h"
#include "rmlsfm.h"
#include "mqmsgfmt.h"
#include "ucodefuzzersub.h"

// Macro for defining standard test parameters
#define DEFINE_UCODE_FUZZ_PROPS(CLASS_NAME) \
CLASS_PROP_READWRITE(CLASS_NAME, DataInterface, LwU32, \
                     "The data source (0 = Random, 1 = libfuzzer)"); \
CLASS_PROP_READWRITE(CLASS_NAME, RngSeed, LwU32, \
                     "Seed for RNG if DataInterface=Random"); \
CLASS_PROP_READWRITE(CLASS_NAME, MaxIter, LwU32, \
                     "Maximum RNG iterations for this configuration before exiting"); \
CLASS_PROP_READWRITE(CLASS_NAME, DryRun, LwBool, \
                     "Run the fuzzing loop without submitting to the ucode"); \
CLASS_PROP_READWRITE(CLASS_NAME, TimeoutMs, LwU32, \
                     "The timeout for submiting cmds (default 10000ms)");

typedef struct
{
    RC                rc;
    UcodeFuzzCovType  covType;

    // union of possible coverage data formats
    // (declared as struct for ease of using non-trivial members)
    struct
    {
        std::vector<LwU64> sanitizerCoverage;
    } covData;
} TestcaseResult;

//! An abstract interface between MODs and a fuzzed data source (libfuzzer)
//!
class FuzzDataInterface
{
public:
    virtual ~FuzzDataInterface() = default;

    //! Initialize the data source
    //!
    //! \param maxSize the max number of bytes the data source should generate
    //! \return RC::OK on success
    //!         error otherwise
    //!
    virtual RC Initialize(LwU32 maxSize) = 0;

    //! Receive the next fuzzed data
    //!
    //! \param bytes The fuzzed data
    //! \return RC::OK on success
    //!         RC::EXIT_OK to exit gracefully
    //!         error otherwise
    //!
    virtual RC NextBytes(std::vector<LwU8>& bytes) = 0;

    //! Submits the result / coverage data of the command to the data source
    //! Note: the coverage data will be consumed (i.e. emptied following call)
    //!
    //! \param result the ucode command result
    //! \return RC::OK on success
    //!         error otherwise
    //!
    virtual RC SubmitResult(const TestcaseResult& result) = 0;
};

//! \brief An empty base class for ucode data templates
//!
class FilledDataTemplate
{
};

//! \brief A template for filling fuzzer data interspersed with preset values
//!
class DataTemplate
{
public:
    virtual ~DataTemplate() {}

    //! Gets the max number of bytes that can be used to fill the template
    virtual LwU32               MaxInputSize() const = 0;

    //! Fills the template with the fuzzed data
    //!
    //! \param data the fuzzed data
    //! \return A heap-allocated instance of the uCode-specific structured data
    //!
    virtual FilledDataTemplate* Fill(const std::vector<LwU8>& data) const = 0;

    //! Helper function for filling a masked memory buffer with fuzzed data
    //!
    //! Be very careful with memory in this function.  If there are new memory
    //! errors it is most likely because of incorrect usage of this function.
    //!
    //! \param tgt an iterator to the target data
    //! \param data_it the begin iterator for the source data
    //! \param data_end the end iterator for the source data
    //! \param mask_it the begin iterator for the data mask (0xff=hide)
    //! \param mask_end the end iterator for the data mask
    //! \return the next iterator into the target data
    //!
    template<typename it1, typename it2, typename it3>
    static it1 FillMask(it1 tgt,
                        it2 data_it, it2 data_end,
                        it3 mask_it, it3 mask_end)
     {
        for (; mask_it != mask_end && data_it != data_end; ++mask_it, ++tgt)
        {
            if (*mask_it != 0)
                continue;
            *tgt = *data_it++;
        }
        return std::copy(data_it, data_end, tgt);
     }
};

//! \brief The base class for ucode fuzzing
//!
//! Defines the control flow for fuzzing and IPC with libfuzzer
//!
class UcodeFuzzDriver : public RmTest
{
public:
    virtual ~UcodeFuzzDriver() = default;

    virtual string              IsTestSupported() = 0;
    virtual RC Setup();
    virtual RC Cleanup();
    virtual RC Run() final;

    //! Gets the DataTemplate for the ucode.
    //!
    //! Called once before the fuzzing loop
    //!
    virtual const DataTemplate* GetDataTemplate() const = 0;

    //! Optional per-ucode behavior to run immediately before submitting
    //!
    virtual RC                  PreRun(FilledDataTemplate* pData) { return RC::OK; }

    //! Submit the structured fuzzed data to the ucode
    //!
    virtual RC                  SubmitFuzzedData(const FilledDataTemplate* pData) = 0;

    //! Enable coverage data collection on ucode (if supported)
    //!
    virtual RC                  CoverageEnable();

    //! Disable coverage data collection on ucode (if supported)
    //!
    virtual RC                  CoverageDisable();

    //! Collect coverage data from ucode (if supported)
    //!
    virtual RC                  CoverageCollect(TestcaseResult& result);

    typedef enum
    {
        RANDOM_IF,
        MQ_IF
    } DataInterface;

    SETGET_PROP(DataInterface, LwU32);

    SETGET_PROP(DryRun, LwBool);
    SETGET_PROP(TimeoutMs, LwU32);

    // These two are only relevent for RANDOM_IF
    SETGET_PROP(RngSeed, LwU32);
    SETGET_PROP(MaxIter, LwU32);
private:
    unique_ptr<UcodeFuzzerSub>    m_pUcodeFuzzer;
    unique_ptr<FuzzDataInterface> m_pDataInterface;

    LwU32  m_DataInterface;

    LwBool m_DryRun = LW_FALSE;
    LwU32  m_TimeoutMs = 10000;

    LwU32  m_RngSeed;
    LwU32  m_MaxIter;

protected:
    UcodeFuzzerSub& UcodeFuzzer() { return *m_pUcodeFuzzer; }

    LwBool m_LogVerbose = LW_FALSE;
};

#endif // UCODEFUZZDRIVER_H_INCLUDED
