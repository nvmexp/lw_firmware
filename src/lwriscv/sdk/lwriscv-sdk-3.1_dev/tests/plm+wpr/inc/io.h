/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_IO_H
#define PLM_WPR_IO_H

/*!
 * @file        io.h
 * @brief       Input/output primitives.
 */

// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// Manual includes.
#include LWRISCV64_MANUAL_ADDRESS_MAP

// Local includes.
#include "engine.h"
#include "fetch.h"


/*!
 * @brief Reads a 32-bit value from external memory using DMA.
 *
 * @param[in]  offset   The aperture-offset of the external memory location to
 *                      read from. Must meet DMA alignment requirements.
 *
 * @param[in]  dmaIndex The index of the FBIF aperture to use for the transfer.
 *
 * @param[out] pValue   A pointer to a variable to receive the contents of the
 *                      specified external-memory location (32-bits). On error,
 *                      receives the LWRV_STATUS code from the DMA driver.
 *
 * @return
 *    true  if the DMA transfer was successful.
 *    false if an error oclwrred.
 *
 * @note Assumes that the indicated FBIF aperture has been properly configured
 *       beforehand.
 */
bool dmaRead(uint64_t offset, uint8_t dmaIndex, uint32_t *pValue);

#if ENGINE_TEST_GDMA
/*!
 * @brief Reads a 32-bit value from FB using GDMA.
 *
 * @param[in]  offset     The FB offset to read from. Must meet GDMA alignment
 *                        requirements.
 *
 * @param[in]  channel    The GDMA channel to use for the transfer.
 *
 * @param[in]  subChannel The GDMA sub-channel to use for the transfer.
 *
 * @param[in]  wprId      The WPR ID to assign the transfer.
 *
 * @param[out] pValue     A pointer to a variable to receive the contents of
 *                        the specified FB location (32-bits). On error,
 *                        receives the LWRV_STATUS code from the GDMA driver.
 *
 * @return
 *    true  if the GDMA transfer was successful.
 *    false if an error oclwrred.
 *
 * @note Assumes that the indicated GDMA channel has been properly configured
 *       beforehand.
 */
bool fbGdma(uint64_t offset, uint8_t channel, uint8_t subChannel,
    uint8_t wprId, uint32_t *pValue);

/*!
 * @brief Reads a 32-bit value from PRI using GDMA.
 *
 * @param[in]  offset     The PRI offset to read from. Must meet GDMA alignment
 *                        requirements.
 *
 * @param[in]  channel    The GDMA channel to use for the transfer.
 *
 * @param[in]  subChannel The GDMA sub-channel to use for the transfer.
 *
 * @param[out] pValue     A pointer to a variable to receive the contents of
 *                        the specified PRI location (32-bits). On error,
 *                        receives the LWRV_STATUS code from the GDMA driver.
 *
 * @return
 *    true  if the GDMA transfer was successful.
 *    false if an error oclwrred.
 *
 * @note Assumes that the indicated GDMA channel has been properly configured
 *       beforehand.
 */
bool priGdma(uint64_t offset, uint8_t channel, uint8_t subChannel,
    uint32_t *pValue);
#endif // ENGINE_TEST_GDMA

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Reads a 32-bit value from FB.
 *
 * @param[in] offset  The FB offset to read from.
 *
 * @return
 *    The contents of the specified FB location (32-bits).
 */
static inline uint32_t
fbRead
(
    uint64_t offset
)
{
    volatile uint32_t *pFb = (volatile uint32_t*)(LW_RISCV_AMAP_FBGPA_START + offset);

    return *pFb;
}

/*!
 * @brief Writes a 32-bit value to FB.
 *
 * @param[out] offset  The FB offset to write to.
 * @param[in]  value   The 32-bit value to write.
 */
static inline void
fbWrite
(
    uint64_t offset,
    uint32_t value
)
{
    volatile uint32_t *pFb = (volatile uint32_t*)(LW_RISCV_AMAP_FBGPA_START + offset);

    *pFb = value;
}

/*!
 * @brief Fetches an instruction from FB.
 *
 * @param[in] offset  The FB offset to fetch from.
 *
 * @return
 *    FETCH_RET_CODE.
 *
 * @note Will not return unless the code at the target location also returns.
 */
static inline uint32_t
fbFetch
(
    uint64_t offset
)
{
    return fetchAddress((uintptr_t)(LW_RISCV_AMAP_FBGPA_START + offset));
}

/*!
 * @brief Reads a 32-bit value from an arbitrary memory location.
 *
 * @param[in] address  The physical memory address to read from.
 *
 * @return
 *    The contents of the specified memory location (32-bits).
 */
static inline uint32_t
memRead
(
    uintptr_t address
)
{
    volatile uint32_t *pMem = (volatile uint32_t*)address;

    return *pMem;
}

#endif // PLM_WPR_IO_H
