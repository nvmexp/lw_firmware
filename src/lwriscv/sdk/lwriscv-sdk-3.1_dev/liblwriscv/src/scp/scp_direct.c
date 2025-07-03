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
 * @file        scp_direct.c
 * @brief       Low-level SCP primitives.
 */

#if !LWRISCV_FEATURE_SCP
#error "Attempting to build scp_direct.c but the SCP driver is disabled!"
#endif // LWRISCV_FEATURE_SCP


// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// SDK includes.
#include <lwmisc.h>

// LWRISCV includes.
#include <lwriscv/status.h>

// LIBLWRISCV includes.
#include <liblwriscv/io.h>
#include <liblwriscv/iopmp.h>
#include <liblwriscv/memutils.h>

// SCP includes.
#include <liblwriscv/scp/scp_common.h>
#include <liblwriscv/scp/scp_direct.h>
#include <liblwriscv/scp/scp_private.h>


/*!
 * @brief Initiates a single SCPDMA transfer.
 *
 * @param[inout] offset    The IMEM/DMEM offset for the transfer. Ignored for
 *                         shortlwt transfers (32-bit).
 *
 * @param[in]    suppress  Whether to avoid issuing a corresponding push/fetch
 *                         instruction for this transfer (ENABLE / DISABLE).
 *
 * @param[in]    shortlwt  Whether to enable the shortlwt path for direct
 *                         transfer to FB/SYSMEM. Only valid for writes (ENABLE
 *                         / DISABLE).
 *
 * @param[in]    imem      Whether to target IMEM rather than DMEM. Ignored for
 *                         shortlwt transfers (TRUE / FALSE).
 *
 * @param[in]    write     Indicates transfer direction. Here "write" means a
 *                         transfer from memory to the SCP unit (TRUE / FALSE).
 *
 * @param[in]    size      The number of bytes to transfer. Must be a valid
 *                         SCP_TRANSFER_SIZE value, or zero for non-suppressed
 *                         transfers.
 *
 * @param[in]    gpr       The SCP register to target in the corresponding push
 *                         /fetch instruction. Must be a valid
 *                         SCP_REGISTER_INDEX value, or zero for suppressed
 *                         transfers.
 */
#define SCP_TRANSFER(offset, suppress, shortlwt, imem, write, size, gpr) ( \
    localWrite(LW_PRGNLCL_RISCV_SCPDMATRFMOFFS,                            \
        DRF_NUM(_PRGNLCL_RISCV, _SCPDMATRFMOFFS, _OFFS, offset)            \
    ),                                                                     \
                                                                           \
    localWrite(LW_PRGNLCL_RISCV_SCPDMATRFCMD,                              \
        DRF_DEF(_PRGNLCL_RISCV, _SCPDMATRFCMD, _SUPPRESS, _##suppress)  |  \
        DRF_DEF(_PRGNLCL_RISCV, _SCPDMATRFCMD, _SHORTLWT, _##shortlwt)  |  \
        DRF_DEF(_PRGNLCL_RISCV, _SCPDMATRFCMD, _IMEM,     _##imem    )  |  \
        DRF_DEF(_PRGNLCL_RISCV, _SCPDMATRFCMD, _WRITE,    _##write   )  |  \
        DRF_NUM(_PRGNLCL_RISCV, _SCPDMATRFCMD, _SIZE,     size       )  |  \
        DRF_NUM(_PRGNLCL_RISCV, _SCPDMATRFCMD, _GPR,      gpr        )  |  \
        DRF_DEF(_PRGNLCL_RISCV, _SCPDMATRFCMD, _CCI_EX,   _SCPDMA    )     \
    )                                                                      \
)


//
// Prototypes for local helper functions.
// See definitions for details.
//
static inline LWRV_STATUS _scpGetError(void);
static inline void        _scpClearError(void);


// Variable to track load/store failures for scp_check_dma() to report later.
static LWRV_STATUS s_transferStatus = LWRV_OK;


/*!
 * @brief Prepares the SCP driver for low-level operations.
 *
 * @return LWRV_
 *    OK                            if direct mode was started successfully.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    WARN_NOTHING_TO_DO            if the SCP driver is already operating in
 *                                  direct mode (no-op).
 *
 * Prepares the SCP driver for low-level operations by entering direct mode.
 * Applications must call scpStartDirect() before ilwoking any low-level SCP
 * interfaces.
 */
LWRV_STATUS
scpStartDirect(void)
{
    // Verify driver state before proceeding further.
    if (!g_scpState.bInitialized)
    {
        return LWRV_ERR_NOT_READY;
    }

    // Skip entry if already in direct mode.
    if (g_scpState.bDirectModeActive)
    {
        return LWRV_WARN_NOTHING_TO_DO;
    }

    //
    // The high-level SCP interfaces are written such that the SCP GPRs should
    // already be cleared and unlocked, the sequencers should already be empty,
    // any SCPDMA errors should already be handled, and the SCP unit should
    // already be idle before this function is even called. Therefore, we have
    // nothing to do here besides updating the internal driver state.
    //
    g_scpState.bDirectModeActive = true;

    // Direct mode was started successfully.
    return LWRV_OK;
}

/*!
 * @brief Prepares the SCP driver for high-level operations.
 *
 * @return LWRV_
 *    OK                            if direct mode was stopped successfully.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_ILWALID_LOCK_STATE        if access to certain necessary registers is
 *                                  being blocked by SCP lockdown.
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 *    WARN_NOTHING_TO_DO            if the SCP driver is not operating in
 *                                  direct mode (no-op).
 *
 * Prepares the SCP driver for high-level operations by exiting direct mode.
 * Applications must call scpStopDirect() before ilwoking any high-level SCP
 * interfaces, unless otherwise noted.
 */
LWRV_STATUS
scpStopDirect(void)
{
    // Verify driver state before proceeding further.
    if (!g_scpState.bInitialized)
    {
        return LWRV_ERR_NOT_READY;
    }

    // Skip exit if not in direct mode in the first place.
    if (!g_scpState.bDirectModeActive)
    {
        return LWRV_WARN_NOTHING_TO_DO;
    }

    // Verify access to lockable SCP registers/fields.
    if (_scpIsAccessRestricted())
    {
        return LWRV_ERR_ILWALID_LOCK_STATE;
    }

    //
    // Wait for idle before issuing the pipe reset. Otherwise, we may end up
    // corrupting the result of some prior operation that's still pending.
    //
    scp_wait_dma();

    //
    // Do a full pipe reset. This ensures that the SCP unit isn't in some weird
    // state (e.g. stalled, in the middle of a sequencer load, etc.).
    //
    _scpResetPipeline();

    //
    // Clear the sequencers to prevent anything accidentally carrying forward
    // into higher-level operations.
    //
    _scpResetSequencers();

    //
    // Clear any pending SCPDMA errors the application may have neglected to
    // deal with (or else we might get false failures from _scpClearAll()).
    // Ignore result explicitly to avoid coverity errors.
    //
    (void) scp_check_dma(true);

    // Clear and unlock the GPRs.
    LWRV_STATUS clearStatus = _scpClearAll();
    if (clearStatus != LWRV_OK)
    {
        return clearStatus;
    }

    // Update internal state.
    g_scpState.bDirectModeActive = false;

    // Direct mode was stopped successfully.
    return LWRV_OK;
}

/*!
 * @brief Loads data from a buffer into a general-purpose SCP register.
 *
 * @param[in] sourcePa  The physical address of the input buffer containing the
 *                      data to be loaded. Must be aligned to and contain at
 *                      least SCP_REGISTER_SIZE bytes. Supports IMEM and DMEM
 *                      locations only.
 *
 * @param[in] regIndex  The index of the SCP register to which the data is to
 *                      be written.
 *
 * Loads the first SCP_REGISTER_SIZE bytes from sourcePa into the register
 * specified by regIndex. Assumes that the destination register is writable.
 *
 * Applications can check the status of the underlying SCPDMA transfer using
 * the provided scp_*_dma() interfaces.
 */
void
scp_load
(
    uintptr_t sourcePa,
    SCP_REGISTER_INDEX regIndex
)
{
    riscv_mem_target_t memTarget;
    uint64_t memOffset;

    //
    // Determine which memory region sourcePa points to and obtain the offset
    // of sourcePa into said region.
    //
    if (memutils_riscv_pa_to_target_offset(sourcePa, &memTarget, &memOffset) != LWRV_OK)
    {
        // Track for scp_check_dma() to report later.
        s_transferStatus = LWRV_ERR_ILWALID_BASE;
    }
    // Issue a transfer from IMEM.
    else if (memTarget == RISCV_MEM_TARGET_IMEM)
    {
        SCP_TRANSFER((uint32_t)memOffset, DISABLE, DISABLE, TRUE, TRUE, 0, regIndex);
    }
    // Issue a transfer from DMEM.
    else if (memTarget == RISCV_MEM_TARGET_DMEM)
    {
        SCP_TRANSFER((uint32_t)memOffset, DISABLE, DISABLE, FALSE, TRUE, 0, regIndex);
    }
    // Invalid memory region.
    else
    {
        // Track for scp_check_dma() to report later.
        s_transferStatus = LWRV_ERR_ILWALID_BASE;
    }
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
 * Writes the contents of the register specified by regIndex to the first
 * SCP_REGISTER_SIZE bytes of destPa. Assumes that the source register is
 * fetchable.
 *
 * Applications can check the status of the underlying SCPDMA transfer using
 * the provided scp_*_dma() interfaces.
 *
 * In the event that destPa refers to an external memory location, the
 * application is responsible for completing the corresponding shortlwt-DMA
 * transfer accordingly.
 */
void
scp_store
(
    SCP_REGISTER_INDEX regIndex,
    uintptr_t destPa
)
{
    riscv_mem_target_t memTarget;
    uint64_t memOffset;

    //
    // Determine which memory region destPa points to and obtain the offset
    // of destPa into said region.
    //
    if (memutils_riscv_pa_to_target_offset(destPa, &memTarget, &memOffset) != LWRV_OK)
    {
        // Track for scp_check_dma() to report later.
        s_transferStatus = LWRV_ERR_ILWALID_BASE;
    }
    // Issue a transfer to IMEM.
    else if (memTarget == RISCV_MEM_TARGET_IMEM)
    {
        SCP_TRANSFER((uint32_t)memOffset, DISABLE, DISABLE, TRUE, FALSE, 0, regIndex);
    }
    // Issue a transfer to DMEM.
    else if (memTarget == RISCV_MEM_TARGET_DMEM)
    {
        SCP_TRANSFER((uint32_t)memOffset, DISABLE, DISABLE, FALSE, FALSE, 0, regIndex);
    }
    // Issue a transfer to external memory (shortlwt).
    else if ((memTarget == RISCV_MEM_TARGET_FBGPA) ||
             (memTarget == RISCV_MEM_TARGET_SYSGPA))
    {
        SCP_TRANSFER(0, DISABLE, ENABLE, FALSE, FALSE, 0, regIndex);
    }
    // Invalid memory region.
    else
    {
        // Track for scp_check_dma() to report later.
        s_transferStatus = LWRV_ERR_ILWALID_BASE;
    }
}

/*!
 * @brief Queues an SCPDMA transfer without issuing any push instructions.
 *
 * @param[in] sourcePa  The physical address of the input buffer containing the
 *                      data to be read. Must be aligned to and contain at
 *                      least size bytes. Supports IMEM and DMEM locations
 *                      only.
 *
 * @param[in] size      The number of bytes to read from sourcePa. See the
 *                      documentation for SCP_TRANSFER_SIZE for supported
 *                      values.
 *
 * Queues a transfer of size bytes from sourcePa but does not read any data
 * until one or more subsequent push instructions are received. Generally used
 * to feed sequencer programs.
 *
 * Applications can check the status of the underlying suppressed SCPDMA
 * transfer using the provided scp_*_dma() interfaces.
 */
void scp_queue_read
(
    uintptr_t sourcePa,
    SCP_TRANSFER_SIZE size
)
{
    riscv_mem_target_t memTarget;
    uint64_t memOffset;

    //
    // Determine which memory region sourcePa points to and obtain the offset
    // of sourcePa into said region.
    //
    if (memutils_riscv_pa_to_target_offset(sourcePa, &memTarget, &memOffset) != LWRV_OK)
    {
        // Track for scp_check_dma() to report later.
        s_transferStatus = LWRV_ERR_ILWALID_BASE;
    }
    // Queue a transfer from IMEM.
    else if (memTarget == RISCV_MEM_TARGET_IMEM)
    {
        SCP_TRANSFER((uint32_t)memOffset, ENABLE, DISABLE, TRUE, TRUE, size, 0);
    }
    // Queue a transfer from DMEM.
    else if (memTarget == RISCV_MEM_TARGET_DMEM)
    {
        SCP_TRANSFER((uint32_t)memOffset, ENABLE, DISABLE, FALSE, TRUE, size, 0);
    }
    // Invalid memory region.
    else
    {
        // Track for scp_check_dma() to report later.
        s_transferStatus = LWRV_ERR_ILWALID_BASE;
    }
}

/*!
 * @brief Queues an SCPDMA transfer without issuing any fetch instructions.
 *
 * @param[out] destPa   The physical address of the output buffer to which the
 *                      data is to be written. Must be aligned to and have
 *                      capacity for at least size bytes. Supports IMEM, DMEM,
 *                      and external locations.
 *
 * @param[in] size      The number of bytes to write to destPa. See the
 *                      documentation for SCP_TRANSFER_SIZE for supported
 *                      values.
 *
 * Queues a transfer of size bytes to destPa but does not write any data until
 * one or more subsequent fetch instructions are received. Generally used to
 * feed sequencer programs.
 *
 * Applications can check the status of the underlying suppressed SCPDMA
 * transfer using the provided scp_*_dma() interfaces.
 *
 * In the event that destPa refers to an external memory location, the
 * application is responsible for queuing the corresponding shortlwt-DMA
 * transfer accordingly.
 */
void scp_queue_write
(
    uintptr_t destPa,
    SCP_TRANSFER_SIZE size
)
{
    riscv_mem_target_t memTarget;
    uint64_t memOffset;

    //
    // Determine which memory region destPa points to and obtain the offset
    // of destPa into said region.
    //
    if (memutils_riscv_pa_to_target_offset(destPa, &memTarget, &memOffset) != LWRV_OK)
    {
        // Track for scp_check_dma() to report later.
        s_transferStatus = LWRV_ERR_ILWALID_BASE;
    }
    // Queue a transfer to IMEM.
    else if (memTarget == RISCV_MEM_TARGET_IMEM)
    {
        SCP_TRANSFER((uint32_t)memOffset, ENABLE, DISABLE, TRUE, FALSE, size, 0);
    }
    // Queue a transfer to DMEM.
    else if (memTarget == RISCV_MEM_TARGET_DMEM)
    {
        SCP_TRANSFER((uint32_t)memOffset, ENABLE, DISABLE, FALSE, FALSE, size, 0);
    }
    // Queue a transfer to external memory (shortlwt).
    else if ((memTarget == RISCV_MEM_TARGET_FBGPA) ||
             (memTarget == RISCV_MEM_TARGET_SYSGPA))
    {
        SCP_TRANSFER(0, ENABLE, ENABLE, FALSE, FALSE, size, 0);
    }
    // Invalid memory region.
    else
    {
        // Track for scp_check_dma() to report later.
        s_transferStatus = LWRV_ERR_ILWALID_BASE;
    }
}

/*!
 * @brief Checks for pending SCPDMA errors and then clears them if requested.
 *
 * @param[in] bClear    A boolean value indicating whether the SCPDMA error
 *                      status should be cleared before returning.
 *
 * @return LWRV_
 *    OK                            if no errors were reported.
 *
 *    ERR_ILWALID_ADDRESS           if an SCPDMA transfer was initiated with a
 *                                  physical address whose offset is not
 *                                  aligned to the transfer size.
 *
 *    ERR_ILWALID_BASE              if an SCPDMA transfer was initiated with a
 *                                  physical address that points to an
 *                                  unsupported memory region (e.g. EMEM).
 *
 *    ERR_INSUFFICIENT_PERMISSIONS  if a CCI secret instruction was initiated
 *                                  with a hardware-secret index that the
 *                                  application does not have permission to
 *                                  use.
 *
 *    ERR_PROTECTION_FAULT          if an IO-PMP fault was detected.
 *
 * Checks for pending SCPDMA errors. Clears them after reading if bClear is
 * true (IO-PMP errors are not cleared).
 *
 * Applications can use scp_check_dma() to determine whether a load, store, or
 * CCI transfer has encountered problems.
 */
LWRV_STATUS
scp_check_dma
(
    bool bClear
)
{
    LWRV_STATUS finalStatus;

    // Give first precedence to errors cached from prior load/store operations.
    if (s_transferStatus != LWRV_OK)
    {
        finalStatus = s_transferStatus;
    }
    else
    {
        // Next, check for errors flagged by the SCPDMA hardware.
        finalStatus = _scpGetError();

        // Finally, check for general IO-PMP errors.
        if ((finalStatus == LWRV_OK) && iopmpHasError())
        {
            finalStatus = LWRV_ERR_PROTECTION_FAULT;
        }
    }

    //
    // Clear any errors if requested. Note that we don't clear IO-PMP errors as
    // the user may want to investigate them in more detail.
    //
    if (bClear)
    {
        s_transferStatus = LWRV_OK;
        _scpClearError();
    }

    return finalStatus;
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Checks for and returns any pending SCPDMA errors.
 *
 * @return LWRV_
 *    OK                            if no errors were reported.
 *
 *    ERR_ILWALID_ADDRESS           if an SCPDMA transfer was initiated with an
 *                                  offset that is not aligned to its size.
 *
 *    ERR_INSUFFICIENT_PERMISSIONS  if a CCI secret instruction was initiated
 *                                  with a hardware-secret index that the
 *                                  application does not have permission to
 *                                  use.
 */
static inline LWRV_STATUS
_scpGetError(void)
{
    uint32_t reg = localRead(LW_PRGNLCL_RISCV_SCPDMAPOLL);

    // Check for bad alignment.
    if (FLD_TEST_DRF(_PRGNLCL_RISCV, _SCPDMAPOLL, _ERROR_CODE,
        _NOT_SIZE_ALIGNED, reg))
    {
        return LWRV_ERR_ILWALID_ADDRESS;
    }
    // Check for bad secret index.
    else if (FLD_TEST_DRF(_PRGNLCL_RISCV, _SCPDMAPOLL, _ERROR_CODE,
        _SECRET_NOT_ALLOWED, reg))
    {
        return LWRV_ERR_INSUFFICIENT_PERMISSIONS;
    }
    // No errors reported.
    else
    {
        return LWRV_OK;
    }
}

/*!
 * @brief Clears any pending SCPDMA errors.
 */
static inline void
_scpClearError(void)
{
    localWrite(LW_PRGNLCL_RISCV_SCPDMAPOLL,
        FLD_SET_DRF_NUM(_PRGNLCL_RISCV, _SCPDMAPOLL, _ERROR_CLR, 1,
            localRead(LW_PRGNLCL_RISCV_SCPDMAPOLL)));
}
