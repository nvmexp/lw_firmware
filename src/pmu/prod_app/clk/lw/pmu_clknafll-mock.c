/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file pmu_clknafll-mock.c
 * @brief Mock implementation for pmu_clknafll public interface.
 */

#include "test-macros.h"
#include "pmu_objclk.h"
#include "clk/pmu_clknafll.h"
#include "clk/pmu_clknafll-mock.h"

#define QUANTIZE_MAX_ENTRIES                                                (1U)
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
        clkNafllFreqMHzQuantize_MOCK_CALLBACK *pCallback;
    } entries[QUANTIZE_MAX_ENTRIES];
} QUANTIZE_MOCK_CONFIG;
static QUANTIZE_MOCK_CONFIG quantize_MOCK_CONFIG;

/*!
 * @brief Initializes the QUANTAIZE_MOCK_CONFIG data.
 */
void clkNafllFreqMHzQuantizeMockInit(void)
{
    memset(&quantize_MOCK_CONFIG, 0x00, sizeof(quantize_MOCK_CONFIG));
    for (LwU8 i = 0; i < QUANTIZE_MAX_ENTRIES; ++i)
    {
        quantize_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
        quantize_MOCK_CONFIG.entries[i].pCallback = NULL;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param[in]  entry      The entry (call number) of the entry to populate.
 * @param[in]  status     The return value.
 */
void clkNafllFreqMHzQuantizeMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < QUANTIZE_MAX_ENTRIES);
    quantize_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Adds an entry to the mock data for clkNafllFreqMHzQuantize().
 *
 * This is the approach to use when the tests wants to either use the production
 * function or have a function return a value via a parameter.
 *
 * @param entry      The entry (or call number) for the test.
 * @param pCallback  Function to call.
 */
void clkNafllFreqMHzQuantize_StubWithCallback(LwU8 entry, clkNafllFreqMHzQuantize_MOCK_CALLBACK *pCallback)
{
    UT_ASSERT_TRUE(entry < QUANTIZE_MAX_ENTRIES);
    quantize_MOCK_CONFIG.entries[entry].pCallback = pCallback;
}

/*!
 * @brief Mock implementation of clkNafllFreqMHzQuantize().
 *
 * The return type and values are dictated by quantize_MOCK_CONFIG.
 */
FLCN_STATUS
clkNafllFreqMHzQuantize_MOCK
(
    LwU32   clkDomain,
    LwU16  *pFreqMHz,
    LwBool  bFloor
)
{
    LwU8 entry = quantize_MOCK_CONFIG.numCalls;
    ++quantize_MOCK_CONFIG.numCalls;

    if (quantize_MOCK_CONFIG.entries[entry].pCallback != NULL)
    {
        return quantize_MOCK_CONFIG.entries[entry].pCallback(clkDomain, pFreqMHz, bFloor);
    }
    else
    {
        return quantize_MOCK_CONFIG.entries[entry].status;
    }
}


#define FREQ_BY_INDEX_MAX_ENTRIES                                           (1U)
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
        clkNafllGetFreqMHzByIndex_MOCK_CALLBACK *pCallback;
    } entries[FREQ_BY_INDEX_MAX_ENTRIES];
} FREQ_BY_INDEX_MOCK_CONFIG;
static FREQ_BY_INDEX_MOCK_CONFIG freqByIndex_MOCK_CONFIG;

/*!
 * @brief Initializes the FREQ_BY_INDEX_MOCK_CONFIG data.
 */
void clkNafllGetFreqMHzByIndexMockInit(void)
{
    memset(&freqByIndex_MOCK_CONFIG, 0x00, sizeof(&freqByIndex_MOCK_CONFIG));
    for (LwU8 i = 0; i < FREQ_BY_INDEX_MAX_ENTRIES; ++i)
    {
        freqByIndex_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
        freqByIndex_MOCK_CONFIG.entries[i].pCallback = NULL;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param[in]  entry      The entry (call number) of the entry to populate.
 * @param[in]  status     The return value.
 */
void clkNafllGetFreqMHzByIndexMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < FREQ_BY_INDEX_MAX_ENTRIES);
    freqByIndex_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Adds an entry to the mock data for clkNafllGetFreqMHzByIndex().
 *
 * This is the approach to use when the tests wants to either use the production
 * function or have a function return a value via a parameter.
 *
 * @param entry      The entry (or call number) for the test.
 * @param pCallback  Function to call.
 */
void clkNafllGetFreqMHzByIndex_StubWithCallback(LwU8 entry, clkNafllGetFreqMHzByIndex_MOCK_CALLBACK *pCallback)
{
    UT_ASSERT_TRUE(entry < FREQ_BY_INDEX_MAX_ENTRIES);
    freqByIndex_MOCK_CONFIG.entries[entry].pCallback = pCallback;
}

/*!
 * @brief Mock implementation of clkNafllGetFreqMHzByIndex().
 */
FLCN_STATUS
clkNafllGetFreqMHzByIndex_MOCK
(
    LwU32   clkDomain,
    LwU16   idx,
    LwU16  *pFreqMHz
)
{
    LwU8 entry = freqByIndex_MOCK_CONFIG.numCalls;
    ++freqByIndex_MOCK_CONFIG.numCalls;

    if (freqByIndex_MOCK_CONFIG.entries[entry].pCallback != NULL)
    {
        return freqByIndex_MOCK_CONFIG.entries[entry].pCallback(clkDomain, idx, pFreqMHz);
    }
    else
    {
        return freqByIndex_MOCK_CONFIG.entries[entry].status;
    }
}


#define INDEX_BY_FREQ_MAX_ENTRIES                                           (1U)
typedef struct
{
    LwU8 numCalls;
    struct
    {
        FLCN_STATUS status;
        clkNafllGetIndexByFreqMHz_MOCK_CALLBACK *pCallback;
    } entries[INDEX_BY_FREQ_MAX_ENTRIES];
} INDEX_BY_FREQ_MOCK_CONFIG;
static INDEX_BY_FREQ_MOCK_CONFIG indexByFreq_MOCK_CONFIG;

/*!
 * @brief Initializes the INDEX_BY_FREQ_MOCK_CONFIG data.
 */
void clkNafllGetIndexByFreqMHzMockInit()
{
    memset(&indexByFreq_MOCK_CONFIG, 0x00, sizeof(indexByFreq_MOCK_CONFIG));
    for (LwU8 i = 0; i < INDEX_BY_FREQ_MAX_ENTRIES; ++i)
    {
        indexByFreq_MOCK_CONFIG.entries[i].status = FLCN_ERROR;
        indexByFreq_MOCK_CONFIG.entries[i].pCallback = NULL;
    }
}

/*!
 * @brief Add entry into the mock configuration data.
 *
 * @param[in]  entry      The entry (call number) of the entry to populate.
 * @param[in]  status     The return value.
 */
void clkNafllGetIndexByFreqMHzMockAddEntry(LwU8 entry, FLCN_STATUS status)
{
    UT_ASSERT_TRUE(entry < INDEX_BY_FREQ_MAX_ENTRIES);
    indexByFreq_MOCK_CONFIG.entries[entry].status = status;
}

/*!
 * @brief Adds an entry to the mock data for clkNafllGetIndexByFreqMHz().
 *
 * This is the approach to use when the tests wants to either use the production
 * function or have a function return a value via a parameter.
 *
 * @param entry      The entry (or call number) for the test.
 * @param pCallback  Function to call.
 */
void clkNafllGetIndexByFreqMHz_StubWithCallback(LwU8 entry, clkNafllGetIndexByFreqMHz_MOCK_CALLBACK *pCallback)
{
    UT_ASSERT_TRUE(entry < INDEX_BY_FREQ_MAX_ENTRIES);
    indexByFreq_MOCK_CONFIG.entries[entry].pCallback = pCallback;
}

/*!
 * @brief Mock implementation of clkNafllGetFreqMHzByIndex().
 */
FLCN_STATUS
clkNafllGetIndexByFreqMHz_MOCK
(
    LwU32   clkDomain,
    LwU16   freqMHz,
    LwBool  bFloor,
    LwU16  *pIdx
)
{
    LwU8 entry = indexByFreq_MOCK_CONFIG.numCalls;
    ++indexByFreq_MOCK_CONFIG.numCalls;

    if (indexByFreq_MOCK_CONFIG.entries[entry].pCallback != NULL)
    {
        return indexByFreq_MOCK_CONFIG.entries[entry].pCallback(clkDomain, freqMHz, bFloor, pIdx);
    }
    else
    {
        return indexByFreq_MOCK_CONFIG.entries[entry].status;
    }
}
