/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_TARGETS_H
#define PLM_WPR_TARGETS_H

/*!
 * @file        targets.h
 * @brief       Target management.
 */

// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// Local includes.
#include "engine.h"
#include "hal.h"


//
// Symbolic names for supported test targets. Note that short names are used
// here to improve the readbility of calls to testSingleAccess().
//
typedef enum TARGET_TYPE
{
    PRI,    // External PRI register.
#if ENGINE_TEST_CSB
    CSB,    // External CSB register.
#endif // ENGINE_TEST_CSB
    WPR,    // Write-protected region (FB).
}TARGET_TYPE;

//
// Supported methods by which test targets can be accessed. Note that some
// targets may not support all access types. Short names are used here to
// improve the readability of calls to testSingleAccess().
//
typedef enum TARGET_ACCESS_TYPE
{
    LOAD,   // Direct read.
    FETCH,  // Instruction fetch.
    DMA,    // DMA transfer.
#if ENGINE_TEST_GDMA
    GDMA,   // GDMA transfer.
#endif // ENGINE_TEST_GDMA
}TARGET_ACCESS_TYPE;

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Performs initialization of all supported test targets.
 *
 * @return
 *    true  if initialization was successful.
 *    false if an error oclwrred.
 */
bool targetInitializeAll(void);

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Attempts to access the contents of the specified test target.
 *
 * @param[in]  targetType  Which test target to attempt access to.
 *
 * @param[in]  accessType  The method by which the target should be accessed.
 *
 * @param[out] pResult     A pointer to a variable to receive the result of the
 *                         access attempt (32-bits). If successful, this result
 *                         should match the value returned by a prior call to
 *                         targetPrepareAccess(). If unsuccessful, the result
 *                         will contain some other target- and access-type-
 *                         -specific value.
 *
 * @return
 *    true  if access was attempted (but did not necessarily succeed).
 *    false if access was unable to be attempted.
 *
 * @note GDMA accesses to WPR use the WPR ID specified in the most recent call
 *       to targetSetGDMAWPRID().
 */
bool targetAttemptAccess(TARGET_TYPE targetType, TARGET_ACCESS_TYPE accessType,
    uint32_t *pResult);

/*!
 * @brief Clears the contents of the specified test target.
 *
 * @param[in] targetType  Which test target to clear.
 *
 * @return
 *    true  if target was cleared successfully.
 *    false if an error oclwrred.
 */
bool targetClearAccess(TARGET_TYPE targetType);

/*!
 * @brief Prepares the specified test target for an access attempt.
 *
 * @param[in]  targetType  Which test target to prepare.
 *
 * @param[in]  accessType  The method by which the target will be accessed.
 *
 * @param[out] pExpected   A pointer to a variable to receive the expected
 *                         result of a future access attempt. Subsequent calls
 *                         to targetAttemptAccess() will return this result if
 *                         the access attempt succeeds.
 *
 * @return
 *    true  if the target was prepared successfully.
 *    false if the target could not be prepared.
 */
bool targetPrepareAccess(TARGET_TYPE targetType, TARGET_ACCESS_TYPE accessType,
    uint32_t *pExpected);

/*!
 * @brief Configures the PLM for the specified test target.
 *
 * @param[in] targetType  Which test target to configure.
 * @param[in] plm         The PLM setting to apply (read and write).
 *
 * @return
 *    true  if the target's PLM was configured successfully.
 *    false if an error oclwrred.
 *
 * @note Caller must have sufficient external privileges in order to modify the
 *       target PLM.
 */
bool targetSetPLM(TARGET_TYPE targetType, HAL_PRIVILEGE_MASK plm);

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Configures the WPR ID to use when accessing WPR via GDMA.
 *
 * @param[in] wprId  The desired WPR ID.
 *
 * @note Only affects calls to targetAttemptAccess().
 */
void targetSetGDMAWPRID(HAL_REGION_ID wprId);

#endif // PLM_WPR_TARGETS_H
