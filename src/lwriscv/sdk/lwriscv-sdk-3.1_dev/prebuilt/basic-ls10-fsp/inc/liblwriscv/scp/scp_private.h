/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_SCP_PRIVATE_H
#define LIBLWRISCV_SCP_PRIVATE_H

/*!
 * @file        scp_private.h
 * @brief       Private interfaces for internal use by the SCP driver.
 *
 * @note        Not intended for use by client applications.
 */

// Standard includes.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SDK includes.
#include <lwmisc.h>

// LWRISCV includes.
#include <lwriscv/csr.h>
#include <lwriscv/status.h>

// LIBLWRISCV includes.
#include <liblwriscv/io.h>
#include <liblwriscv/memutils.h>

// SCP includes.
#include <liblwriscv/scp/scp_common.h>


//
// The index of the SCP hardware secret to use when clearing registers from a
// secure context. Not to be used for any other purpose, per the security bar.
//
#define SCP_SECRET_INDEX_CLEAR 0


// Structure describing the internal state of the SCP driver.
typedef struct SCP_STATE
{
    //
    // Tracks whether the driver has been initialized correctly. Most
    // interfaces will early-out if this flag is not set.
    //
    bool bInitialized : 1;

    //
    // Tracks whether the driver has been shut down after prior initialization.
    // Mostly a courtesy feature to detect duplicate calls to scpShutdown().
    //
    bool bShutdown : 1;

    //
    // Tracks whether the driver is lwrrently operating in "direct mode", which
    // affects whether certain higher-level interfaces are available.
    //
    bool bDirectModeActive : 1;

    //
    // Tracks whether the SCP unit's RNG hardware has been configured. Requests
    // for random-number generation will be declined unless this flag is set.
    //
    bool bRandConfigured : 1;

    //
    // Tracks whether the shortlwt-DMA path has been configured. dmaIndex below
    // is only valid if this flag is set and any reqests for transfers to
    // external memory will be declined otherwise.
    //
    bool bShortlwtConfigured : 1;

    //
    // Stores the index of the DMA aperture to be used for shortlwt transfers
    // to external memory. Valid only if bShortlwtConfigured is set.
    //
    uint8_t dmaIndex;

} SCP_STATE;


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
LWRV_STATUS _scpClearReg(SCP_REGISTER_INDEX regIndex);

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
LWRV_STATUS _scpClearAll(void);

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
LWRV_STATUS _scpUnlockAll(void);

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
 *       inaccessible from inselwre contexts unless explicitly unlocked later.
 */
LWRV_STATUS _scpZeroAll(void);

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
LWRV_STATUS _scpLoadBuffer(uintptr_t sourcePa, SCP_REGISTER_INDEX regIndex);

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
LWRV_STATUS _scpStoreBuffer(SCP_REGISTER_INDEX regIndex, uintptr_t destPa);

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
LWRV_STATUS _scpValidateBuffer(uintptr_t basePa,
                               size_t size,
                               bool bAllowExternal);

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
LWRV_STATUS _scpRunSequence(uintptr_t sourcePa, uintptr_t destPa, size_t size);

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Disables the SCP unit's RNG hardware.
 */
static inline void
_scpDisableRand(void)
{
    localWrite(LW_PRGNLCL_SCP_CTL1,
        FLD_SET_DRF(_PRGNLCL_SCP, _CTL1, _RNG_EN, _DISABLED,
            localRead(LW_PRGNLCL_SCP_CTL1)));
}

/*!
 * @brief Enables the SCP unit's RNG hardware.
 */
static inline void
_scpEnableRand(void)
{
    localWrite(LW_PRGNLCL_SCP_CTL1,
        FLD_SET_DRF(_PRGNLCL_SCP, _CTL1, _RNG_EN, _ENABLED,
            localRead(LW_PRGNLCL_SCP_CTL1)));
}

/*!
 * @brief Checks whether SCP lockdown is enabled.
 *
 * @return
 *    true  if SCP lockdown is enabled.
 *    false if SCP lockdown is disabled.
 */
static inline bool
_scpIsLockdownEnabled(void)
{
    return FLD_TEST_DRF(_PRGNLCL_SCP, _CTL_CFG, _LOCKDOWN_SCP, _ENABLE,
        localRead(LW_PRGNLCL_SCP_CTL_CFG));
}

/*!
 * @brief Checks whether the host engine is operating in a secure context.
 *
 * @return
 *    true  if the host engine is operating in a secure context.
 *    false if the host engine is operating in an inselwre context.
 */
static inline bool
_scpIsSelwre(void)
{
    return FLD_TEST_DRF64(_RISCV_CSR, _XRSP, _XRSEC, _SEC,
                          csr_read(LW_RISCV_CSR_XRSP));
}

/*!
 * @brief Checks whether access to lockable SCP registers/fields is restricted.
 *
 * @return
 *    true  if access is restricted.
 *    false if access is unrestricted.
 */
static inline bool
_scpIsAccessRestricted(void)
{
    return (_scpIsLockdownEnabled() && !_scpIsSelwre());
}

/*!
 * @brief Requests a pipe reset. Waits for completion before returning.
 */
static inline void
_scpResetPipeline(void)
{
    // Request a pipe reset.
    localWrite(LW_PRGNLCL_SCP_CTL1,
        FLD_SET_DRF(_PRGNLCL_SCP, _CTL1, _PIPE_RESET, _TASK,
            localRead(LW_PRGNLCL_SCP_CTL1)));

    // Wait for the pipe reset to complete.
    while (FLD_TEST_DRF(_PRGNLCL_SCP, _CTL1, _PIPE_RESET, _PENDING,
        localRead(LW_PRGNLCL_SCP_CTL1)));
}

/*!
 * @brief Clears both SCP sequencers.
 */
static inline void
_scpResetSequencers(void)
{
    // Request a reset of both sequencers.
    localWrite(LW_PRGNLCL_SCP_CTL1,
        FLD_SET_DRF(_PRGNLCL_SCP, _CTL1, _SEQ_CLEAR, _TASK,
            localRead(LW_PRGNLCL_SCP_CTL1)));

    // Wait for the sequencer reset to complete.
    while (FLD_TEST_DRF(_PRGNLCL_SCP, _CTL1, _SEQ_CLEAR, _PENDING,
        localRead(LW_PRGNLCL_SCP_CTL1)));
}

///////////////////////////////////////////////////////////////////////////////

// Global internal state variable.
extern SCP_STATE g_scpState;

#endif // LIBLWRISCV_SCP_PRIVATE_H
