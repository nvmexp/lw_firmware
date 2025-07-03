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
 * @file        scp_private.c
 * @brief       Private interfaces for internal use by the SCP driver.
 */

#if !LWRISCV_FEATURE_SCP
#error "Attempting to build scp_private.c but the SCP driver is disabled!"
#endif // LWRISCV_FEATURE_SCP


// Standard includes.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SDK includes.
#include <lwmisc.h>

// LWRISCV includes.
#include <lwriscv/status.h>

// LIBLWRISCV includes.
#include <liblwriscv/memutils.h>

// SCP includes.
#include <liblwriscv/scp/scp_common.h>
#include <liblwriscv/scp/scp_direct.h>
#include <liblwriscv/scp/scp_private.h>

// Conditional includes.
#if LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED
#include <liblwriscv/dma.h>
#endif // LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED


// The chmod value for a fully-open register ACL.
#define SCP_REGISTER_ACL_ALL 0x1F

// The maximum supported size for a single SCPDMA transfer, in bytes.
#define SCP_TRANSFER_LIMIT 256


//
// Prototypes for local helper functions.
// See definitions for details.
//
static LWRV_STATUS _scpUnlockReg(SCP_REGISTER_INDEX regIndex);
static LWRV_STATUS _scpZeroReg(SCP_REGISTER_INDEX regIndex);


/*!
 * @brief Clears any data from the specified SCP GPR and then unlocks its ACL.
 *
 * @param[in] regIndex The index of the SCP register to clear.
 *
 * @return LWRV_
 *    OK                            if the register was cleared successfully.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * @note This is an internal-only version of scpClearReg() that skips certain
 *       redundant checks.
 */
LWRV_STATUS
_scpClearReg
(
    SCP_REGISTER_INDEX regIndex
)
{
    LWRV_STATUS status;

    // Clear the register's contents.
    status = _scpZeroReg(regIndex);
    if (status != LWRV_OK)
    {
        return status;
    }

    //
    // In secure contexts, _scpZeroReg() will have locked out inselwre access
    // to the target register, so restore it here. In inselwre contexts, the
    // register is assumed to already be accessible (or else _scpZeroReg()
    // would have triggered an ACL violation).
    //
    if (_scpIsSelwre())
    {
        status = _scpUnlockReg(regIndex);
        if (status != LWRV_OK)
        {
            return status;
        }
    }

    // Register cleared successfully.
    return LWRV_OK;
}

/*!
 * @brief Clears any data from all SCP GPRs and then unlocks their ACLs.
 *
 * @return LWRV_
 *    OK                            if the registers were cleared successfully.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * @note This is an internal-only version of scpClearAll() that skips certain
 *       redundant checks.
 */
LWRV_STATUS
_scpClearAll(void)
{
    LWRV_STATUS lwrrentStatus, finalStatus = LWRV_OK;

    //
    // Clear each register in turn. To ensure we always clear as much data as
    // possible, we avoid exiting immediately on failure here and instead
    // simply track any errors for later.
    //
    if ((lwrrentStatus = _scpClearReg(SCP_REGISTER_INDEX_0)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpClearReg(SCP_REGISTER_INDEX_1)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpClearReg(SCP_REGISTER_INDEX_2)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpClearReg(SCP_REGISTER_INDEX_3)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpClearReg(SCP_REGISTER_INDEX_4)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpClearReg(SCP_REGISTER_INDEX_5)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpClearReg(SCP_REGISTER_INDEX_6)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpClearReg(SCP_REGISTER_INDEX_7)) != LWRV_OK)
        finalStatus = lwrrentStatus;

    // Registers cleared.
    return finalStatus;
}

/*!
 * @brief Resets the ACLs of all SCP GPRs to a fully-open state.
 *
 * @return LWRV_
 *    OK            if the registers were unlocked successfully.
 *    ERR_GENERIC   if an unexpected error oclwrred.
 *
 * @note This function must only be called from secure contexts and requires
 *       that both secure permissions (Fs and Ks) are already present for all
 *       registers.
 *
 * @note As a precaution, the contents of each register are cleared after
 *       unlocking. It is recommended that callers also clear the registers
 *       before unlocking by calling _scpZeroAll() and checking that it
 *       succeeded.
 */
LWRV_STATUS
_scpUnlockAll(void)
{
    LWRV_STATUS lwrrentStatus, finalStatus = LWRV_OK;

    //
    // Unlock each register in turn. To ensure we always unlock as many GPRs as
    // possible, we avoid exiting immediately on failure here and instead
    // simply track any errors for later.
    //
    if ((lwrrentStatus = _scpUnlockReg(SCP_REGISTER_INDEX_0)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpUnlockReg(SCP_REGISTER_INDEX_1)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpUnlockReg(SCP_REGISTER_INDEX_2)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpUnlockReg(SCP_REGISTER_INDEX_3)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpUnlockReg(SCP_REGISTER_INDEX_4)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpUnlockReg(SCP_REGISTER_INDEX_5)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpUnlockReg(SCP_REGISTER_INDEX_6)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpUnlockReg(SCP_REGISTER_INDEX_7)) != LWRV_OK)
        finalStatus = lwrrentStatus;

    // Registers unlocked.
    return finalStatus;
}

/*!
 * @brief Zeroes all SCP GPRs.
 *
 * @return LWRV_
 *    OK                            if the registers were zeroed successfully.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * @note Calling this function from a secure context will leave the SCP GPRs
 *       inaccessible from inselwre contexts until explicitly unlocked later.
 */
LWRV_STATUS
_scpZeroAll(void)
{
    LWRV_STATUS lwrrentStatus, finalStatus = LWRV_OK;

    //
    // Zero each register in turn. To ensure we always zero as many GPRs as
    // possible, we avoid exiting immediately on failure here and instead
    // simply track any errors for later.
    //
    if ((lwrrentStatus = _scpZeroReg(SCP_REGISTER_INDEX_0)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpZeroReg(SCP_REGISTER_INDEX_1)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpZeroReg(SCP_REGISTER_INDEX_2)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpZeroReg(SCP_REGISTER_INDEX_3)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpZeroReg(SCP_REGISTER_INDEX_4)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpZeroReg(SCP_REGISTER_INDEX_5)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpZeroReg(SCP_REGISTER_INDEX_6)) != LWRV_OK)
        finalStatus = lwrrentStatus;
    if ((lwrrentStatus = _scpZeroReg(SCP_REGISTER_INDEX_7)) != LWRV_OK)
        finalStatus = lwrrentStatus;

    // Registers zeroed.
    return finalStatus;
}

/*!
 * @brief Loads data from a buffer into a general-purpose SCP register.
 *
 * @param[in]  sourcePa     The physical address of the input buffer containing
 *                          the data to be loaded. Must be aligned to and
 *                          contain at least SCP_REGISTER_SIZE bytes. Supports
 *                          IMEM and DMEM locations only.
 *
 * @param[in]  regIndex     The index of the SCP register to which the data is
 *                          to be written.
 *
 * @return LWRV_
 *    OK                            if data was loaded successfully.
 *    ERR_PROTECTION_FAULT          if an IO-PMP fault was detected.
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Loads the first SCP_REGISTER_SIZE bytes from sourcePa into the register
 * specified by regIndex. Assumes that the destination register is writable
 * and that the caller has performed all necessary validation on sourcePa and
 * regIndex.
 *
 * Intended as an internal-only colwenience wrapper for scp_load().
 */
LWRV_STATUS _scpLoadBuffer
(
    uintptr_t sourcePa,
    SCP_REGISTER_INDEX regIndex
)
{
    LWRV_STATUS status;

    // Load into the selected register.
    scp_load(sourcePa, regIndex);

    // Wait for completion.
    scp_wait_dma();

    // Check for any failures.
    status = scp_check_dma(true);
    if (status == LWRV_ERR_PROTECTION_FAULT)
    {
        // Bubble up IO-PMP errors.
        return status;
    }
    else if (status != LWRV_OK)
    {
        // No other errors are expected.
        return LWRV_ERR_GENERIC;
    }

    // Data was loaded successfully.
    return LWRV_OK;
}

/*!
 * @brief Stores data from a general-purpose SCP register into a buffer.
 *
 * @param[in]  regIndex The index of the SCP register containing the data to
 *                      be stored.
 *
 * @param[out] destPa   The physical address of the output buffer to which the
 *                      data is to be written. Must be aligned to and have
 *                      capacity for at least SCP_REGISTER_SIZE bytes. Supports
 *                      IMEM, DMEM, and external locations.
 *
 * @return LWRV_
 *    OK                            if data was stored successfully.
 *    ERR_PROTECTION_FAULT          if an IO-PMP fault was detected.
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Writes the contents of the register specified by regIndex to the first
 * SCP_REGISTER_SIZE bytes of destPa. Assumes that the source register is
 * fetchable and that the caller has performed all necessary validation on
 * destPa and regIndex.
 *
 * In the event that destPa refers to an external memory location, the
 * caller is also responsible for ensuring that the shortlwt-DMA path has been
 * properly enabled and configured accordingly.
 *
 * Intended as an internal-only colwenience wrapper for scp_store().
 */
LWRV_STATUS _scpStoreBuffer
(
    SCP_REGISTER_INDEX regIndex,
    uintptr_t destPa
)
{
    LWRV_STATUS status;

    // Write to the destination buffer.
    scp_store(regIndex, destPa);

#if LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED
    riscv_mem_target_t destTarget;
    uint64_t destOffset;

    // Obtain the global target/offset pair for the destination buffer.
    if (memutils_riscv_pa_to_target_offset(destPa, &destTarget, &destOffset) != LWRV_OK)
    {
        // This should have been validated by the caller.
        return LWRV_ERR_GENERIC;
    }

    // Complete the follow-up transfer to external memory, if applicable.
    if ((destTarget == RISCV_MEM_TARGET_FBGPA) ||
        (destTarget == RISCV_MEM_TARGET_SYSGPA))
    {
        if (fbdma_scp_to_extmem(destOffset, SCP_REGISTER_SIZE, g_scpState.dmaIndex)
            != LWRV_OK)
        {
            //
            // This should never happen so long as the caller has properly
            // sanitized destPa and ensured that the shortlwt-DMA path has
            // been configured.
            //
            return LWRV_ERR_GENERIC;
        }
    }
#endif // LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED

    // Wait for completion.
    scp_wait_dma();

    // Check for any failures.
    status = scp_check_dma(true);
    if (status == LWRV_ERR_PROTECTION_FAULT)
    {
        // Bubble up IO-PMP errors.
        return status;
    }
    else if (status != LWRV_OK)
    {
        // No other errors are expected.
        return LWRV_ERR_GENERIC;
    }

    // Data was stored successfully.
    return LWRV_OK;
}

/*!
 * @brief Validates an input/output buffer for use by a higher-level interface.
 *
 * @param[in]  basePa           The physical address of the buffer to validate.
 *
 * @param[in]  size             The size of the buffer to validate, in bytes.
 *
 * @param[in]  bAllowExternal   Whether to permit external memory locations.
 *
 * @return LWRV_
 *    OK                        if the buffer was validated successfully.
 *
 *    ERR_ILWALID_POINTER       if basePa is zero (NULL).
 *
 *    ERR_ILWALID_ADDRESS       if basePa is incorrectly aligned.
 *
 *    ERR_ILWALID_BASE          if basePa points to an unsupported memory
 *                              region (e.g. EMEM).
 *
 *    ERR_FEATURE_NOT_ENABLED   if basePa is located in external memory but
 *                              the application does not have shortlwt-DMA
 *                              support enabled.
 *
 *    ERR_ILWALID_DMA_SPECIFIER if basePa is located in external memory but the
 *                              application has not called
 *                              scpConfigureShortlwt() to configure the
 *                              shortlwt-DMA path.
 *
 *    ERR_OUT_OF_RANGE          if the buffer referenced by basePa overflows
 *                              the memory region in which it resides.
 *
 * Verifies that the input or output buffer pointed to by basePa is located at
 * a valid address within a supported memory region and is correctly aligned.
 * Also ensures that shortlwt-DMA support has been properly enabled and
 * configured when bAllowExternal is true.
 */
LWRV_STATUS
_scpValidateBuffer
(
    uintptr_t basePa,
    size_t size,
    bool bAllowExternal
)
{
    riscv_mem_target_t baseTarget, endTarget;
    uint64_t baseOffset;

    //
    // Obtain the physical address of the final byte in the buffer (accounting
    // for the edge case of a zero-sized buffer).
    //
    const uintptr_t endPa = basePa + size - (size > 0);

    // Check base address for zero/NULL.
    if (basePa == (uintptr_t)NULL)
    {
        return LWRV_ERR_ILWALID_POINTER;
    }

    // Colwert to global target/offset pair.
    if (memutils_riscv_pa_to_target_offset(basePa, &baseTarget, &baseOffset) != LWRV_OK)
    {
        return LWRV_ERR_ILWALID_BASE;
    }

    // Ensure the target memory region is supported.
    switch (baseTarget)
    {
        // IMEM and DMEM are always supported.
        case RISCV_MEM_TARGET_IMEM:
        case RISCV_MEM_TARGET_DMEM:
            break;

        //
        // FB and SYSMEM are supported only for output buffers and only if the
        // shortlwt-DMA path has been enabled and configured.
        //
        case RISCV_MEM_TARGET_FBGPA:
        case RISCV_MEM_TARGET_SYSGPA:
            {
                if (!bAllowExternal)
                {
                    return LWRV_ERR_ILWALID_BASE;
                }

                if (!LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED)
                {
                    return LWRV_ERR_FEATURE_NOT_ENABLED;
                }

                if (!g_scpState.bShortlwtConfigured)
                {
                    return LWRV_ERR_ILWALID_DMA_SPECIFIER;
                }
            }
            break;

        // No other locations are supported.
        default:
            return LWRV_ERR_ILWALID_BASE;
    }

    // Ensure the buffer does not overflow the target memory region.
    if ((memutils_riscv_pa_to_target_offset(endPa, &endTarget, NULL) != LWRV_OK) ||
        (endTarget != baseTarget) || (endPa < basePa))
    {
        return LWRV_ERR_OUT_OF_RANGE;
    }

    // Ensure the offset into the target memory region is correctly aligned.
    if (!LW_IS_ALIGNED64(baseOffset, SCP_BUFFER_ALIGNMENT))
    {
        return LWRV_ERR_ILWALID_ADDRESS;
    }

    // Buffer validated successfully.
    return LWRV_OK;
}

/*!
 * @brief Exelwtes the program lwrrently loaded in SCP sequencer zero.
 *
 * @param[in]  sourcePa     The physical address of the input buffer containing
 *                          the data to be fed into the sequencer program (zero
 *                          /NULL if none). Must be aligned to
 *                          SCP_REGISTER_SIZE and contain at least size bytes.
 *                          Supports IMEM and DMEM locations only.
 *
 * @param[out] destPa       The physical address of the output buffer to which
 *                          the data produced by the sequencer program is to be
 *                          written (zero/NULL if none). Must be aligned to
 *                          SCP_REGISTER_SIZE and have at least size bytes of
 *                          capacity. Supports IMEM, DMEM, and external
 *                          locations.
 *
 * @param[in]  size         The number of bytes to process from sourcePa and/or
 *                          output to destPa. Must be a multiple of
 *                          SCP_REGISTER_SIZE.
 *
 * @return LWRV_
 *    OK                            if data was processed successfully.
 *    ERR_PROTECTION_FAULT          if an IO-PMP fault was detected.
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Repeatedly exelwtes the sequencer program lwrrently loaded into SCP
 * sequencer zero until size bytes of data have been read from sourcePa and/or
 * written to destPa. Automatically breaks the input and/or output data into
 * optimally-sized batches while processing.
 *
 * Note that the caller is assumed to have performed all necessary validation
 * on sourcePa, destPa, and size before passing them to this function. This
 * includes verifying proper enablement and configuration of the shortlwt-DMA
 * path in the event that destPa refers to an external memory location.
 *
 * It is also assumed that the number of push/fetch instructions issued per
 * iteration of the lwrrently-loaded sequencer program exactly matches the
 * values in the table below.
 *
 *       sourcePa | destPa  | pushes | fetches
 *      ----------|---------|--------|---------
 *       Present  | Present |   1    |    1
 *       Present  | Absent  |   1    |    0
 *       Absent   | Present |   0    |    1
 *       Absent   | Absent  |   0    |    0
 *
 * While it is permissible to call this function with neither an input buffer
 * nor an output buffer, doing so is likely to be less efficient than simply
 * ilwoking the required number of sequencer iterations directly.
 */
LWRV_STATUS
_scpRunSequence
(
    uintptr_t sourcePa,
    uintptr_t destPa,
    size_t size
)
{
    riscv_mem_target_t destTarget;
    uint64_t sourceOffset, destOffset;
    bool bHasInput, bHasOutput;

    // Track which parameters are present.
    bHasInput = (sourcePa != (uintptr_t)NULL);
    bHasOutput = (destPa != (uintptr_t)NULL);

    // Obtain the aperture offset of the source buffer, if any.
    if (bHasInput)
    {
        if (memutils_riscv_pa_to_target_offset(sourcePa, NULL, &sourceOffset) != LWRV_OK)
        {
            // This should have been validated by the caller.
            return LWRV_ERR_GENERIC;
        }
    }

    // Obtain the global target/offset pair for the destination buffer, if any.
    if (bHasOutput)
    {
        if (memutils_riscv_pa_to_target_offset(destPa, &destTarget, &destOffset) != LWRV_OK)
        {
            // This should have been validated by the caller.
            return LWRV_ERR_GENERIC;
        }
    }

#if LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED
    //
    // Track whether the destination buffer (if any) is located in an external
    // memory region.
    //
    bool bExternalDest = bHasOutput && ((destTarget == RISCV_MEM_TARGET_FBGPA) ||
                                        (destTarget == RISCV_MEM_TARGET_SYSGPA));
#endif // LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED

    // Continue processing until no data remains.
    while (size > 0)
    {
        SCP_TRANSFER_SIZE transferSize;
        uint8_t numIterations;

        //
        // Determine the maximum number of bytes we can process this iteration
        // based on the alignment of the source and destination buffers (as
        // applicable), and the constraints of the SCPDMA unit.
        //
        uint32_t batchSize = (uint32_t)LOWESTBIT(
                                 (bHasInput  ? sourceOffset : 0) |
                                 (bHasOutput ? destOffset   : 0) |
                                 SCP_TRANSFER_LIMIT);

        //
        // Ensure that the computed size does not exceed the quantity of data
        // remaining to be processed.
        //
        while (batchSize > size)
        {
            batchSize >>= 1;
        }

        //
        // Colwert to an equivalent SCPDMA transfer size and corresponding
        // number of sequencer iterations (assuming at most one push and/or
        // fetch instruction per loop).
        //
        switch (batchSize)
        {
            case (16 * SCP_REGISTER_SIZE):
                transferSize = SCP_TRANSFER_SIZE_16R;
                numIterations = 16;
                break;

            case (8 * SCP_REGISTER_SIZE):
                transferSize = SCP_TRANSFER_SIZE_8R;
                numIterations = 8;
                break;

            case (4 * SCP_REGISTER_SIZE):
                transferSize = SCP_TRANSFER_SIZE_4R;
                numIterations = 4;
                break;

            case (2 * SCP_REGISTER_SIZE):
                transferSize = SCP_TRANSFER_SIZE_2R;
                numIterations = 2;
                break;

            case (1 * SCP_REGISTER_SIZE):
                transferSize = SCP_TRANSFER_SIZE_1R;
                numIterations = 1;
                break;

            default:
                //
                // This should never happen unless the caller has failed to
                // ensure that sourcePa, destPa, and size are aligned properly.
                //
                return LWRV_ERR_GENERIC;
        }

        // Queue the next batch of data to read from the source (if any).
        if (bHasInput)
        {
            scp_queue_read(sourcePa, transferSize);
        }

        // Queue the next batch of data to write to the destination (if any).
        if (bHasOutput)
        {
            scp_queue_write(destPa, transferSize);
        }

        // Execute the computed number of sequencer iterations.
        scp_loop_trace0(numIterations);

#if LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED
        // Complete the follow-up transfer to external memory, if applicable.
        if (bExternalDest)
        {
            if (fbdma_scp_to_extmem(destOffset, batchSize, g_scpState.dmaIndex) !=
                LWRV_OK)
            {
                //
                // This should never happen so long as the caller has properly
                // sanitized destPa and ensured that the shortlwt-DMA path has
                // been configured.
                //
                return LWRV_ERR_GENERIC;
            }
        }
#endif // LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED

        // Wait for the current batch to finish processing.
        scp_wait_dma();

        // Check for any failures.
        LWRV_STATUS status = scp_check_dma(true);
        if (status == LWRV_ERR_PROTECTION_FAULT)
        {
            // Bubble up IO-PMP errors.
            return status;
        }
        else if (status != LWRV_OK)
        {
            // No other errors are expected.
            return LWRV_ERR_GENERIC;
        }

        // Update pointers.
        sourcePa += batchSize;
        destPa += batchSize;

        // Update aperture offsets.
        if (bHasInput)
        {
            sourceOffset += batchSize;
        }

        if (bHasOutput)
        {
            destOffset += batchSize;
        }

        // Update bytes remaining.
        size -= batchSize;
    }

    // Data was processed successfully.
    return LWRV_OK;
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Resets the ACL of the specified SCP GPR to a fully-open state.
 *
 * @param[in] regIndex The index of the SCP register to unlock.
 *
 * @return LWRV_
 *    OK            if the register was unlocked successfully.
 *    ERR_GENERIC   if an unexpected error oclwrred.
 *
 * @note This function must only be called from secure contexts and requires
 *       that both secure permissions (Fs and Ks) are already present for the
 *       register being unlocked.
 *
 * @note As a precaution, the target register's contents are cleared after
 *       unlocking. It is recommended that callers also clear the register
 *       before unlocking by calling _scpZeroReg() and checking that it
 *       succeeded.
 */
static LWRV_STATUS
_scpUnlockReg
(
    SCP_REGISTER_INDEX regIndex
)
{
    //
    // Enable all permissions in the ACL of the specified register.
    //
    // Some concerns were raised that this creates a dangerous potential ROP
    // gadget for an attacker to leverage, but we don't have any other options
    // for recovering the inselwre-write (Wi) permission at this point.
    //
    // We did consider reducing the number of permissions we grant here, but
    // that would require sprinkling additional code throughout other parts of
    // the SCP driver in order to recover the remaining inselwre-fetch (Fi) and
    // inselwre-key (Ki) permissions on-demand. It's also unlikely that such an
    // alternative approach would buy us much addditional security as any
    // attacker with ROP capabilities can likely also manipulate function
    // arguments anyway.
    //
    scp_chmod(SCP_REGISTER_ACL_ALL, regIndex);

    //
    // Clear the register's contents. This gives us some protection against the
    // aforementioned ROP threat as it makes leaking sensitive information a
    // bit harder.
    //
    // Note that we don't need to go through the extra steps that _scpZeroReg()
    // follows here since the target register is expected to be fetchable
    // already.
    //
    scp_xor(regIndex, regIndex);

    // Wait for completion.
    scp_wait_dma();

    //
    // Check for failure. The SCPDMA itself should never fail here (we'd get
    // an interrupt instead if the actual chmod or xor instructions failed),
    // but we check anyway to be safe.
    //
    if (scp_check_dma(true) != LWRV_OK)
    {
        return LWRV_ERR_GENERIC;
    }

    // Register unlocked successfully.
    return LWRV_OK;
}

/*!
 * @brief Zeroes the specified SCP GPR.
 *
 * @param[in] regIndex The index of the SCP register to zero.
 *
 * @return LWRV_
 *    OK                            if the register was zeroed successfully.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * @note Calling this function from a secure context will leave the specified
 *       SCP GPR inaccessible from inselwre contexts until explicitly unlocked
 *       later.
 */
static LWRV_STATUS
_scpZeroReg
(
    SCP_REGISTER_INDEX regIndex
)
{
    //
    // In order for the xor operation below to succeed, the target register
    // must have its fetchable attribute set in its associated ACL. We can
    // ensure this in secure contexts by first loading a fetchable hardware
    // secret into the register, which we do here.
    //
    // Inselwre contexts do not have this option but are also not expected to
    // ever encounter a non-fetchable register unless the application has been
    // misusing certain direct-mode interfaces (in which case we'll simply end
    // up triggering an ACL violation).
    //
    if (_scpIsSelwre())
    {
        // Load secret-index zero to ensure the target register is fetchable.
        scp_secret(SCP_SECRET_INDEX_CLEAR, regIndex);

        // Wait for completion.
        scp_wait_dma();

        // Check for failures.
        LWRV_STATUS status = scp_check_dma(true);

        //
        // Catch denied access to the hardware secret. We return a unique
        // error code here to help differentiate this failure from other
        // permissions errors that higher-level interfaces using this function
        // may return.
        //
        if (status == LWRV_ERR_INSUFFICIENT_PERMISSIONS)
        {
            return LWRV_ERR_INSUFFICIENT_RESOURCES;
        }
        //
        // Other failures shouldn't be possible here so just return a generic
        // error on the off chance we hit one somehow.
        //
        else if (status != LWRV_OK)
        {
            return LWRV_ERR_GENERIC;
        }
    }

    // Clear the register's contents.
    scp_xor(regIndex, regIndex);

    // Wait for completion.
    scp_wait_dma();

    //
    // Check for failure. The SCPDMA itself should never fail here (we'd get
    // an interrupt instead if the actual xor instruction failed), but we
    // check anyway to be safe.
    //
    if (scp_check_dma(true) != LWRV_OK)
    {
        return LWRV_ERR_GENERIC;
    }

    // Register was zeroed successfully.
    return LWRV_OK;
}

///////////////////////////////////////////////////////////////////////////////

//
// Global internal state for the SCP driver.
// Initialize to reasonable defaults.
//
SCP_STATE g_scpState =
{
    // Not initialized until scpInit() is called.
    .bInitialized = false,

    // Not shutdown until scpShutdown() is called.
    .bShutdown = false,

    // Direct mode must be requested by the application.
    .bDirectModeActive = false,

    // RNG hardware must be configured explicitly.
    .bRandConfigured = false,

    // Shortlwt path must be configured explicitly.
    .bShortlwtConfigured = false,

};
