/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file        targets.c
 * @brief       Target management.
 */

// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// LWRISCV includes.
#include <lwriscv/fence.h>
#include <lwriscv/status.h>

// LIBLWRISCV includes.
#include <liblwriscv/fbif.h>
#include <liblwriscv/io.h>

// Local includes.
#include "config.h"
#include "engine.h"
#include "hal.h"
#include "io.h"
#include "targets.h"

// Conditional includes.
#if ENGINE_TEST_GDMA
#include <liblwriscv/gdma.h>
#endif // ENGINE_TEST_GDMA


//
// The WPR ID to use for GDMA transfers to/from FB. Used by
// targetAttemptAccess() and configured by targetSetGDMAWPRID().
//
static HAL_REGION_ID s_gdmaRegion = NONWPR;

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Performs initialization of all supported test targets.
 *
 * @return
 *    true  if initialization was successful.
 *    false if an error oclwrred.
 */
bool
targetInitializeAll(void)
{
    // Configure WPR1 and the host engine's subWPR.
    halConfigureWPR(CONFIG_TARGET_OFFSET_WPR, CONFIG_TARGET_SIZE_WPR);

    // Prepare an FBIF aperture for DMA transfers.
    if (fbifConfigureAperture(&g_configAccessDmaAperture, 1) != LWRV_OK)
    {
        return false;
    }

#if ENGINE_TEST_GDMA
    // Prepare a channel for GDMA transfers.
    if (gdmaCfgChannelPriv(CONFIG_ACCESS_GDMA_CHANNEL,
            &g_configAccessGdmaChannelPrivate) != LWRV_OK ||
        gdmaCfgChannel(CONFIG_ACCESS_GDMA_CHANNEL,
            &g_configAccessGdmaChannel)        != LWRV_OK)
    {
        return false;
    }
#endif // ENGINE_TEST_GDMA

    // Initialization was successful.
    return true;
}

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
bool
targetAttemptAccess
(
    TARGET_TYPE targetType,
    TARGET_ACCESS_TYPE accessType,
    uint32_t *pResult
)
{   
    switch (targetType)
    {
        case PRI:
            if (accessType == LOAD)
            {
                // Direct read.
                *pResult = priRead(CONFIG_TARGET_OFFSET_PRI);
                return true;
            }

#if ENGINE_TEST_GDMA
            else if (accessType == GDMA)
            {
                // GDMA transfer.
                if (!priGdma(CONFIG_TARGET_OFFSET_PRI,
                    CONFIG_ACCESS_GDMA_CHANNEL, CONFIG_ACCESS_GDMA_SUBCHANNEL,
                    pResult))
                {
                    // Differentiate a negative access from a failed attempt.
                    return (*pResult == CONFIG_ACCESS_GDMA_NACK);
                }

                return true;
            }
#endif // ENGINE_TEST_GDMA
            break;

#if ENGINE_TEST_CSB
        case CSB:
            if (accessType == LOAD)
            {
                // Direct read.
                *pResult = csbRead(CONFIG_TARGET_OFFSET_CSB);
                return true;
            }
            break;
#endif // ENGINE_TEST_CSB

        case WPR:
            if (accessType == LOAD)
            {
                // Direct read.
                *pResult = fbRead(CONFIG_TARGET_OFFSET_WPR);
                return true;
            }

            else if (accessType == FETCH)
            {
                // Instruction fetch.
                *pResult = fbFetch(CONFIG_TARGET_OFFSET_WPR);
                return true;
            }

            else if (accessType == DMA)
            {
                // DMA tranfser.
                if (!dmaRead(CONFIG_TARGET_OFFSET_WPR, CONFIG_ACCESS_DMA_INDEX,
                    pResult))
                {
                    // Differentiate a negative access from a failed attempt.
                    return (*pResult == CONFIG_ACCESS_DMA_NACK);
                }

                return true;
            }

#if ENGINE_TEST_GDMA
            else if (accessType == GDMA)
            {
                // GDMA transfer.
                if (!fbGdma(CONFIG_TARGET_OFFSET_WPR,
                    CONFIG_ACCESS_GDMA_CHANNEL, CONFIG_ACCESS_GDMA_SUBCHANNEL,
                    s_gdmaRegion, pResult))
                {
                    // Differentiate a negative access from a failed attempt.
                    return (*pResult == CONFIG_ACCESS_GDMA_NACK);
                }

                return true;
            }
#endif // ENGINE_TEST_GDMA
            break;
    }

    // Unrecognized target or access type.
    return false;
}

/*!
 * @brief Clears the contents of the specified test target.
 *
 * @param[in] targetType  Which test target to clear.
 *
 * @return
 *    true  if target was cleared successfully.
 *    false if an error oclwrred.
 */
bool
targetClearAccess
(
    TARGET_TYPE targetType
)
{
    volatile uint32_t readback = ~CONFIG_TARGET_SENTINEL_ALL_CLEAR;
    
    //
    // Write a special sentinel value to the indicated test target and then
    // read back to verify that the write went through.
    //
    switch (targetType)
    {
        case PRI:
            priWrite(CONFIG_TARGET_OFFSET_PRI, CONFIG_TARGET_SENTINEL_ALL_CLEAR);
            readback = priRead(CONFIG_TARGET_OFFSET_PRI);
            break;

#if ENGINE_TEST_CSB
        case CSB:
            csbWrite(CONFIG_TARGET_OFFSET_CSB, CONFIG_TARGET_SENTINEL_ALL_CLEAR);
            readback = csbRead(CONFIG_TARGET_OFFSET_CSB);
            break;
#endif // ENGINE_TEST_CSB

        case WPR:
            fbWrite(CONFIG_TARGET_OFFSET_WPR, CONFIG_TARGET_SENTINEL_ALL_CLEAR);
            readback = fbRead(CONFIG_TARGET_OFFSET_WPR);
            break;
    }

    // Return success/failure.
    return (readback == CONFIG_TARGET_SENTINEL_ALL_CLEAR);
}

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
bool
targetPrepareAccess
(
    TARGET_TYPE targetType,
    TARGET_ACCESS_TYPE accessType,
    uint32_t *pExpected
)
{   
    switch (targetType)
    {
        case PRI:
        {
            switch (accessType)
            {
                case LOAD: *pExpected = CONFIG_TARGET_SENTINEL_PRI_LOAD; break;
#if ENGINE_TEST_GDMA
                case GDMA: *pExpected = CONFIG_TARGET_SENTINEL_PRI_GDMA; break;
#endif // ENGINE_TEST_GDMA
                default:   return false;
            }

            priWrite(CONFIG_TARGET_OFFSET_PRI, *pExpected);
            return true;
        }

#if ENGINE_TEST_CSB
        case CSB:
        {
            switch (accessType)
            {
                case LOAD: *pExpected = CONFIG_TARGET_SENTINEL_CSB_LOAD; break;
                default:   return false;
            }

            csbWrite(CONFIG_TARGET_OFFSET_CSB, *pExpected);
            return true;
        }
#endif // ENGINE_TEST_CSB

        case WPR:
        {
            switch(accessType)
            {
                case LOAD:  *pExpected = CONFIG_TARGET_SENTINEL_WPR_LOAD;  break;
                case FETCH: *pExpected = CONFIG_TARGET_SENTINEL_WPR_FETCH; break;
                case DMA:   *pExpected = CONFIG_TARGET_SENTINEL_WPR_DMA;   break;
#if ENGINE_TEST_GDMA
                case GDMA:  *pExpected = CONFIG_TARGET_SENTINEL_WPR_GDMA;  break;
#endif // ENGINE_TEST_GDMA
                default:    return false;
            }

            fbWrite(CONFIG_TARGET_OFFSET_WPR, *pExpected);
            return true;
        }
    }

    // Unrecognized target.
    return false;
}

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
bool
targetSetPLM
(
    TARGET_TYPE targetType,
    HAL_PRIVILEGE_MASK plm
)
{
    switch (targetType)
    {
        case PRI:
        {
            // Obtain correct hardware-level PLM setting.
            const uint32_t setting = halConstructPLM(plm);

            // Apply setting.
            priWrite(CONFIG_TARGET_PLM_PRI, setting);

            // Check whether setting went through.
            return (priRead(CONFIG_TARGET_PLM_PRI) == setting);
        }

#if ENGINE_TEST_CSB
        case CSB:
        {
            // Obtain correct hardware-level PLM setting.
            const uint32_t setting = halConstructPLM(plm);

            // Apply setting.
            csbWrite(CONFIG_TARGET_PLM_CSB, setting);

            // Check whether setting went through.
            return (csbRead(CONFIG_TARGET_PLM_CSB) == setting);
        }
#endif // ENGINE_TEST_CSB

        case WPR:
        {
            HAL_PRIVILEGE_MASK rReadback, wReadback;
            
            // Apply setting (read and write).
            halSetWPRPLM(plm, plm);

            // Wait for setting to apply.
            riscvLwfenceRWIO();

            // Read setting back.
            halGetWPRPLM(&rReadback, &wReadback);

            // Check whether setting went through.
            return ((rReadback == plm) && (wReadback == plm));
        }
    }

    // Unrecognized target.
    return false;
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Configures the WPR ID to use when accessing WPR via GDMA.
 *
 * @param[in] wprId  The desired WPR ID.
 *
 * @note Only affects calls to targetAttemptAccess().
 */
void
targetSetGDMAWPRID
(
    HAL_REGION_ID wprId
)
{
    //
    // Needs to be specified at the time the transfer is initiated, so cache
    // locally until then.
    //
    s_gdmaRegion = wprId;
}
