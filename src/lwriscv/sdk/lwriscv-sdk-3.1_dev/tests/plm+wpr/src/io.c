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
 * @file        io.c
 * @brief       Input/output primitives.
 */

// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// Manual includes.
#include LWRISCV64_MANUAL_ADDRESS_MAP

// LWRISCV includes.
#include <lwriscv/status.h>

// LIBLWRISCV includes.
#include <liblwriscv/dma.h>
#include <liblwriscv/io.h>

// Local includes.
#include "engine.h"
#include "hal.h"
#include "io.h"

// Conditional includes.
#if ENGINE_TEST_GDMA
#include <liblwriscv/gdma.h>
#endif // ENGINE_TEST_GDMA


/*!
 * @brief Allocates a buffer of the requested size and alignment.
 *
 * @param[in] size  The desired size for the new buffer in bytes.
 * @param[in] align The desired minimum alignment for the new buffer in bytes.
 *
 * @return The address of the new buffer as an integer.
 */
#define IO_CREATE_BUFFER(size, alignment) (                         \
    (uintptr_t)LW_ALIGN_UP64(                                       \
        (uintptr_t)(uint8_t[(size) + (alignment) - 1]){0},          \
        (alignment)                                                 \
    )                                                               \
)

//
// The smallest valid DMA transfer size capable of transferring at least one
// 32-bit integer.
//
#define IO_DMA_TRANSFER_SIZE_32 LW_ALIGN_UP64(                      \
    sizeof(uint32_t), DMA_BLOCK_SIZE_MIN                            \
)

//
// The smallest valid GDMA transfer size capable of transferring at least one
// 32-bit integer.
//
#define IO_GDMA_TRANSFER_SIZE_32 LW_ALIGN_UP64(                     \
    sizeof(uint32_t), GDMA_ADDR_ALIGN                               \
)


#if ENGINE_TEST_GDMA
// Prototypes for local helper functions. See definitions for details.
static bool _gdmaRead(uint64_t address, uint8_t channel, uint8_t subChannel,
    GDMA_ADDR_CFG *config, uint32_t *pValue);
#endif // ENGINE_TEST_GDMA


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
bool
dmaRead
(
    uint64_t offset,
    uint8_t dmaIndex,
    uint32_t *pValue
)
{
    // Allocate an interim TCM buffer of appropriate size and alignment.
    const uintptr_t tcmBuffer = IO_CREATE_BUFFER(IO_DMA_TRANSFER_SIZE_32,
        DMA_BLOCK_SIZE_MIN);
    
    // Read the external-memory contents into the interim TCM buffer.
    LWRV_STATUS status = dmaPa(tcmBuffer, offset, IO_DMA_TRANSFER_SIZE_32,
        dmaIndex, true);

    // Check for failure.
    if (status != LWRV_OK)
    {
        *pValue = status;
        return false;
    }

    // Extract the final value from the TCM buffer.
    *pValue = memRead(tcmBuffer);

    // Transfer successful.
    return true;
}

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
bool
fbGdma
(
    uint64_t offset,
    uint8_t channel,
    uint8_t subChannel,
    uint8_t wprId,
    uint32_t *pValue
)
{
    // Prepare the configuration settings for the transfer.
    GDMA_ADDR_CFG addrCfg = 
    {
        // Increment after each byte.
        .srcAddrMode = GDMA_ADDR_MODE_INC,
        .destAddrMode = GDMA_ADDR_MODE_INC,

        // Set external source configuration.
        .srcExtAddrCfg = 
        {
            // Enable coherent access.
            .fbCoherent = true,
            
            // Normal L2-eviction class.
            .fbL2cRd = GDMA_L2C_L2_EVICT_NORMAL,
            
            // Set WPR ID.
            .fbWpr = wprId,
        },
    };

    // Attempt GDMA.
    return _gdmaRead(LW_RISCV_AMAP_FBGPA_START + offset, channel, subChannel,
        &addrCfg, pValue);
}

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
bool
priGdma
(
    uint64_t offset,
    uint8_t channel,
    uint8_t subChannel,
    uint32_t *pValue
)
{   
    // Prepare the configuration settings for the transfer.
    GDMA_ADDR_CFG addrCfg = 
    {
        // Fixed mode for PRI source.
        .srcAddrMode = GDMA_ADDR_MODE_FIX,

        // Standard increment for DMEM destination.
        .destAddrMode = GDMA_ADDR_MODE_INC,
    };

    // Attempt GDMA.
    return _gdmaRead(LW_RISCV_AMAP_PRIV_START + offset, channel, subChannel,
        &addrCfg, pValue);
}
#endif // ENGINE_TEST_GDMA

///////////////////////////////////////////////////////////////////////////////

#if ENGINE_TEST_GDMA
/*!
 * @brief Reads a 32-bit value from external memory using GDMA.
 *
 * @param[in]  address    The physical address of the external memory location
 *                        to read from. Must meet GDMA alignment requirements.
 *
 * @param[in]  channel    The GDMA channel to use for the transfer.
 *
 * @param[in]  subChannel The GDMA sub-channel to use for the transfer.
 *
 * @param[in]  config     The addressing-mode configuration settings for the
 *                        transfer.
 *
 * @param[out] pValue     A pointer to a variable to receive the contents of
 *                        the specified external-memory location (32-bits). On
 *                        error, receives the LWRV_STATUS code from the GDMA
 *                        driver.
 *
 * @return
 *    true  if the GDMA transfer was successful.
 *    false if an error oclwrred.
 *
 * @note Assumes that the indicated GDMA channel has been properly configured
 *       beforehand.
 */
static bool
_gdmaRead
(
    uint64_t address,
    uint8_t channel,
    uint8_t subChannel,
    GDMA_ADDR_CFG *config,
    uint32_t *pValue
)
{
    // Allocate an interim TCM buffer of appropriate size and alignment.
    const uintptr_t tcmBuffer = IO_CREATE_BUFFER(IO_GDMA_TRANSFER_SIZE_32,
        GDMA_ADDR_ALIGN);

    // Read the external-memory contents into the interim TCM buffer.
    LWRV_STATUS status = gdmaXferReg(address, tcmBuffer,
        IO_GDMA_TRANSFER_SIZE_32, config, channel, subChannel, true, false);

    // Issue a channel-reset to clear any error state.
    halResetGDMAChannel(channel);

    // Report any failures.
    if(status != LWRV_OK)
    {
        *pValue = status;
        return false;
    }

    // Extract the final value from the TCM buffer.
    *pValue = memRead(tcmBuffer);

    // Transfer successful.
    return true;
}
#endif // ENGINE_TEST_GDMA
