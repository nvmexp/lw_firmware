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
 * @file        scp_general.c
 * @brief       General SCP operations.
 */

#if !LWRISCV_FEATURE_SCP
#error "Attempting to build scp_general.c but the SCP driver is disabled!"
#endif // LWRISCV_FEATURE_SCP


// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// SDK includes.
#include <lwmisc.h>

// LWRISCV includes.
#include <lwriscv/status.h>

// LIBLWRISCV includes.
#include <liblwriscv/devicemap.h>
#include <liblwriscv/io.h>
#include <liblwriscv/iopmp.h>

// SCP includes.
#include <liblwriscv/scp/scp_common.h>
#include <liblwriscv/scp/scp_direct.h>
#include <liblwriscv/scp/scp_general.h>
#include <liblwriscv/scp/scp_private.h>


//
// Prototypes for local helper functions.
// See definitions for details.
//
static LWRV_STATUS _scpForgetSignature(void);
static LWRV_STATUS _scpRecoverInselwre(void);

static bool        _scpHasInterrupt(void);

static inline bool _scpIsActive(void);
static inline bool _scpIsPresent(void);
static inline bool _scpIsSelwreCapable(void);

static inline void _scpClearInterrupts(void);
static inline void _scpDisableInterrupts(void);
static inline void _scpEnableInterrupts(void);

static inline void _scpDisableProcessing(void);
static inline void _scpEnableProcessing(void);

static inline void _scpResetConfiguration(void);
static inline void _scpResetControls(void);
static inline void _scpResetSecret63(void);


/*!
 * @brief Initializes the SCP driver and underlying hardware for use.
 *
 * @param[in] flags A bitmask specifying the desired configuration settings for
 *                  the SCP driver.
 *
 * @return LWRV_
 *    OK                             if initialization was successful.
 *
 *    ERR_ILWALID_FLAGS              if flags is not a valid combination of
 *                                   SCP_INIT_FLAG values.
 *
 *    ERR_NOT_SUPPORTED              if the host engine does not contain an
 *                                   SCP unit.
 *
 *    ERR_INSUFFICIENT_PERMISSIONS   if the SCP unit or IO-PMP registers are
 *                                   blocked in the host engine's device map.
 *
 *    ERR_INSUFFICIENT_RESOURCES     if the application lacks access to secret-
 *                                   index zero (required in secure contexts).
 *
 *    ERR_IN_USE                     if the SCP unit is in use and cannot be
 *                                   safely reset.
 *
 *    ERR_ILWALID_STATE              if IO-PMP error-capture is disabled for
 *                                   the CPDMA master.
 *
 *    ERR_ILWALID_LOCK_STATE         if access to certain necessary registers
 *                                   is being blocked by SCP lockdown, or the
 *                                   host engine is secure-capable but SCP
 *                                   lockdown is disabled.
 *
 *    ERR_GENERIC                    if an unexpected error oclwrred.
 *
 *    WARN_NOTHING_TO_DO             if the SCP driver has already been
 *                                   initialized (no-op).
 *
 * Initializes the SCP driver for use. Applications must not call into any
 * other SCP interfaces before exelwting scpInit(), unless otherwise noted.
 *
 * After scpInit() completes successfully, the SCP unit is guaranteed to be in
 * a clean, safe state for the application to use.
 */
LWRV_STATUS
scpInit
(
    SCP_INIT_FLAG flags
)
{
    LWRV_STATUS status;

    // Skip initialization if already done.
    if (g_scpState.bInitialized)
    {
        return LWRV_WARN_NOTHING_TO_DO;
    }

    // Verify that the host Peregrine actually contains an SCP unit.
    if (!_scpIsPresent())
    {
        return LWRV_ERR_NOT_SUPPORTED;
    }

    //
    // Check that we have sufficient access rights to the necessary device-map
    // groups. We do this just as a courtesy check at init and not in every
    // interface.
    //
    if (!DEVICEMAP_HAS_ACCESS(MMODE, SCP,   READ)  ||
        !DEVICEMAP_HAS_ACCESS(MMODE, SCP,   WRITE) ||
        !DEVICEMAP_HAS_ACCESS(MMODE, IOPMP, READ))
    {
        return LWRV_ERR_INSUFFICIENT_PERMISSIONS;
    }

#if LWRISCV_CONFIG_CPU_MODE != 3
    // Not operating in M-mode so check S/U as well.
    if(!DEVICEMAP_HAS_ACCESS(SUBMMODE, SCP,   READ)  ||
       !DEVICEMAP_HAS_ACCESS(SUBMMODE, SCP,   WRITE) ||
       !DEVICEMAP_HAS_ACCESS(SUBMMODE, IOPMP, READ))
    {
        return LWRV_ERR_INSUFFICIENT_PERMISSIONS;
    }
#endif // LWRISCV_CONFIG_CPU_MODE != 3

    // Verify that IO-PMP error-capture is enabled for CPDMA transfers.
    if (!IOPMP_IS_CAPTURE_ENABLED(CPDMA))
    {
        return LWRV_ERR_ILWALID_STATE;
    }

    // Verify that SCP lockdown is correctly configured.
    if (_scpIsSelwreCapable() != _scpIsLockdownEnabled())
    {
        return LWRV_ERR_ILWALID_LOCK_STATE;
    }

    // Verify access to lockable SCP registers/fields.
    if (_scpIsAccessRestricted())
    {
        return LWRV_ERR_ILWALID_LOCK_STATE;
    }

    //
    // Ensure the SCP unit is idle before trying to reset it. Otherwise, we
    // may corrupt the result of some prior operation that's still pending.
    //
    if (scp_poll_dma())
    {
        return LWRV_ERR_IN_USE;
    }

    //
    // Hardware state has been verified. Check that the configuration flags
    // are valid as well before initiating any changes.
    //
    if (flags != SCP_INIT_FLAG_DEFAULT)
    {
        // No other flags are supported lwrrently.
        return LWRV_ERR_ILWALID_FLAGS;
    }

    //
    // Conlwrrent SCP usage is not supported, so the unit is expected to remain
    // idle throughout initialization regardless, but disable processing anyway
    // as a precautionary measure.
    //
    _scpDisableProcessing();

    // Disable any interrupts that could cause us to stall later.
    _scpDisableInterrupts();

    //
    // Clear any already-pending interrupts or SCPDMA errors so we can check
    // later that we didn't unexpectedly trigger any new ones ourselves.
    //
    _scpClearInterrupts();
    (void) scp_check_dma(true);

    //
    // Perform a pipe reset and clear the sequencers. We also disable RNG at
    // this point (applications can explicitly re-enable later if needed).
    //
    _scpResetControls();

    //
    // Reset the carry-chain, endianness, stall-timeout, etc. to their
    // default values.
    //
    _scpResetConfiguration();

    //
    // Re-enable processing now that configuration is complete. Needs to be
    // done before attempting to clear registers or we'll hang.
    //
    _scpEnableProcessing();

    if (!_scpIsSelwre())
    {
        //
        // Ensure we have fetchable access to the general-purpose registers
        // before trying to clear them. Secure code can do this via the
        // scp_secret() primitive (which _scpZeroAll() issues automatically if
        // needed) but inselwre code requires the alternative approach ilwoked
        // here.
        //
        // Note that any subsequent driver code that causes these permissions
        // to be revoked again will also make sure to restore them before
        // returning, thus bypassing the need for this special procedure
        // beyond first-time initialization.
        //
        // In the event that one or more of the general-purpose registers lack
        // inselwre-writable access (Wi), we will have no means of recovering
        // access whatsoever and will simply error out when we catch the
        // ensuing write-violation interrupt below.
        //
        status = _scpRecoverInselwre();
        if (status != LWRV_OK)
        {
            return status;
        }
    }

    // Clear the contents of all general-purpose registers.
    status = _scpZeroAll();
    if (status != LWRV_OK)
    {
        return status;
    }

    if (_scpIsSelwre())
    {
        //
        // Relax the ACLs of the general-purpose registers so inselwre code is
        // not locked out later on. It is critical that we only do this if
        // _scpZeroAll() succeeded entirely, hence the interrupt check.
        //
        if (!_scpHasInterrupt())
        {
            status = _scpUnlockAll();
            if (status != LWRV_OK)
            {
                return status;
            }
        }

        //
        // Ilwalidate the engine's signature/hash. This will generate an
        // interrupt if the signature register is already invalid, hence doing
        // it while they're still disabled.
        //
        // Note that the RISC-V BootROM normally performs this step itself but
        // we repeat it here anyway as a defense-in-depth measure.
        //
        status = _scpForgetSignature();
        if (status != LWRV_OK)
        {
            return status;
        }
    }

    //
    // Check for any unexpected interrupts. There are very few realistic
    // scenarios in which we would ever hit this condition, so we just return
    // a generic error code here without ilwestigating further.
    //
    if (_scpHasInterrupt())
    {
        return LWRV_ERR_GENERIC;
    }

    //
    // Clear any non-fatal interrupts we may have triggered and then enable a
    // reasonable subset of them for the application.
    //
    _scpClearInterrupts();
    _scpEnableInterrupts();

    // Update internal state to reflect completed initialization.
    g_scpState.bInitialized = true;
    g_scpState.bShutdown = false;
    g_scpState.bDirectModeActive = false;
    g_scpState.bRandConfigured = false;
    g_scpState.bShortlwtConfigured = false;

    // Initialization was successful.
    return LWRV_OK;
}

/*!
 * @brief Clears all data from the SCP unit and disables further processing.
 *
 * @return LWRV_
 *    OK                            if shutdown was successful.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_IN_USE                    if the SCP unit is in use and cannot be
 *                                  safely reset.
 *
 *    ERR_ILWALID_LOCK_STATE        if access to certain necessary registers is
 *                                  being blocked by SCP lockdown.
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 *    WARN_NOTHING_TO_DO            if the SCP driver has already been shut
 *                                  down (no-op).
 *
 * Clears all data from the SCP unit and disables further processing of
 * commands. Applications should ilwoke scpShutdown() as soon as SCP support is
 * no longer needed (or at least before exiting).
 *
 * Should an application require SCP functionality again after issuing
 * scpShutdown(), it must first ilwoke scpInit() to initialize the driver
 * again, unless otherwise noted.
 */
LWRV_STATUS
scpShutdown(void)
{
    LWRV_STATUS lwrrentStatus, finalStatus = LWRV_OK;

    // Skip shutdown if already done.
    if (g_scpState.bShutdown)
    {
        return LWRV_WARN_NOTHING_TO_DO;
    }

    // Verify driver state before proceeding further.
    if (!g_scpState.bInitialized)
    {
        return LWRV_ERR_NOT_READY;
    }

    // Verify access to lockable SCP registers/fields.
    if (_scpIsAccessRestricted())
    {
        return LWRV_ERR_ILWALID_LOCK_STATE;
    }

    //
    // Ensure the SCP unit is idle before trying to shut it down. Otherwise,
    // we may corrupt the result of some prior operation that's still pending.
    //
    if (_scpIsActive())
    {
        return LWRV_ERR_IN_USE;
    }

    // Disable any interrupts that could cause us to stall later.
    _scpDisableInterrupts();

    //
    // Clear any already-pending interrupts or SCPDMA errors so we can check
    // later that we didn't unexpectedly trigger any new ones.
    //
    _scpClearInterrupts();
    (void) scp_check_dma(true);

    //
    // Perform a pipe reset and clear the sequencers. It's important to do this
    // before attempting to clear the registers in case the application forgot
    // to properly exit direct mode, or encountered a stall. We also disable
    // RNG at this point.
    //
    _scpResetControls();

    //
    // Ensure processing is enabled in case, e.g., the application is calling
    // this function in response to a fatal error.
    //
    _scpEnableProcessing();

    if (!_scpIsSelwre())
    {
        //
        // Try to ensure that we have fetchable access to the general-purpose
        // registers before attempting to clear them. _scpZeroAll() will handle
        // this automatically for secure contexts, but inselwre contexts
        // require the alternative procedure used here.
        //
        // Realistically, this should only be necessary if the application has
        // been misusing direct mode, but we do it anyway to maximize our
        // chances of a clean shutdown.
        //
        lwrrentStatus = _scpRecoverInselwre();
        if (finalStatus == LWRV_OK)
        {
            finalStatus = lwrrentStatus;
        }
    }

    // Clear the contents of all general-purpose registers.
    lwrrentStatus = _scpZeroAll();
    if (finalStatus == LWRV_OK)
    {
        finalStatus = lwrrentStatus;
    }

    //
    // Relax the ACLs of the general-purpose registers so inselwre code can
    // freely use them afterwards. It is critical that we only do this if
    // _scpZeroAll() succeeded entirely, hence the extra check.
    //
    if (_scpIsSelwre() && !_scpHasInterrupt() && (finalStatus == LWRV_OK))
    {
        finalStatus = _scpUnlockAll();
    }

    //
    // Check for any unexpected interrupts. There are very few realistic
    // scenarios in which we would ever hit this condition, so we just return
    // a generic error code here without ilwestigating further.
    //
    if(_scpHasInterrupt() && (finalStatus == LWRV_OK))
    {
        finalStatus = LWRV_ERR_GENERIC;
    }

    // Disable further processing until the SCP unit is needed again.
    _scpDisableProcessing();

    // Clear any interrupts we may have triggered.
    _scpClearInterrupts();

    // Reset the source for secret-index 63 (hardware recommendation).
    _scpResetSecret63();

    //
    // Update internal state to reflect completed shutdown. The driver will no
    // longer be in a usable state by this point, even if some prior operation
    // failed, so we update these unconditionally.
    //
    g_scpState.bInitialized = false;
    g_scpState.bShutdown = true;

    // Shutdown complete.
    return finalStatus;
}

/*!
 * @brief Returns the debug status of the SCP unit.
 *
 * @return
 *    true  if the SCP unit is in debug mode.
 *    false if the SCP unit is not in debug mode.
 *
 * This function can be called regardless of whether the SCP driver has been
 * initialized or shut down and is safe to call from direct mode.
 */
bool
scpIsDebug(void)
{
    return !FLD_TEST_DRF(_PRGNLCL_SCP, _CTL_STAT, _DEBUG_MODE, _DISABLED,
                         localRead(LW_PRGNLCL_SCP_CTL_STAT));
}

/*!
 * @brief Clears any data from the specified SCP GPR.
 *
 * @param[in] regIndex The index of the SCP register to clear.
 *
 * @return LWRV_
 *    OK                            if the register was cleared successfully.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_INDEX             if regIndex is not a valid
 *                                  SCP_REGISTER_INDEX value.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * This function is safe to call from direct mode.
 */
LWRV_STATUS
scpClearReg
(
    SCP_REGISTER_INDEX regIndex
)
{
    // Verify driver state before proceeding further.
    if (!g_scpState.bInitialized)
    {
        return LWRV_ERR_NOT_READY;
    }

    // Ensure the provided register index is valid.
    if (regIndex > SCP_REGISTER_INDEX_7)
    {
        return LWRV_ERR_ILWALID_INDEX;
    }

    // Clear the selected register and unlock its associated ACL if needed.
    return _scpClearReg(regIndex);
}

/*!
 * @brief Clears any data from all SCP GPRs.
 *
 * @return LWRV_
 *    OK                            if the registers were cleared successfully.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * This function is safe to call from direct mode.
 */
LWRV_STATUS
scpClearAll(void)
{
    // Verify driver state before proceeding further.
    if (!g_scpState.bInitialized)
    {
        return LWRV_ERR_NOT_READY;
    }

    // Clear all GPRs and unlock their associated ACLs if needed.
    return _scpClearAll();
}

/*!
 * @brief Configures the shortlwt-DMA path for transfers to external memory.
 *
 * @param[in] dmaIndex The index of the DMA aperture to be used for shortlwt
 *                     transfers to external memory.
 *
 * @return LWRV_
 *    OK                      if the shortlwt-DMA path was configured
 *                            successfully.
 *
 *    ERR_NOT_READY           if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_INDEX       if dmaIndex does not refer to a valid DMA
 *                            aperture.
 *
 *    ERR_FEATURE_NOT_ENABLED if the application does not have shortlwt-DMA
 *                            support enabled.
 *
 * Configures the shortlwt path to be used by applicable interfaces when
 * transferring data to external memory. Applications must call
 * scpConfigureShortlwt() (or the SCP_EXTERNAL_DEST() helper macro) before
 * passing external destination buffers to any SCP interfaces.
 *
 * Once configured, the selected DMA aperture will be used for all future
 * shortlwt transfers until reconfigured by a subsequent call to
 * scpConfigureShortlwt().
 *
 * Note that the application is responsible for ensuring that the provided DMA
 * aperture has itself been properly configured before use. Additionally,
 * shortlwt-DMA support must be enabled during build in order to leverage this
 * feature.
 */
LWRV_STATUS
scpConfigureShortlwt
(
    uint8_t dmaIndex
)
{
    // Verify driver state before proceeding further.
    if (!g_scpState.bInitialized)
    {
        return LWRV_ERR_NOT_READY;
    }

    // Ilwalidate previous setting, if any.
    g_scpState.bShortlwtConfigured = false;

    // Check that shortlwt-DMA support is enabled.
    if (!LWRISCV_DRIVER_SCP_SHORTLWT_ENABLED)
    {
        return LWRV_ERR_FEATURE_NOT_ENABLED;
    }

    // Ensure the provided DMA index is valid.
    if (dmaIndex > DRF_MASK(LW_PRGNLCL_FALCON_DMATRFCMD_CTXDMA))
    {
        return LWRV_ERR_ILWALID_INDEX;
    }

    // Update internal state.
    g_scpState.bShortlwtConfigured = true;
    g_scpState.dmaIndex = dmaIndex;

    // Shortlwt-DMA path was configured successfully.
    return LWRV_OK;
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Ilwalidates the engine's signature/hash.
 *
 * @return LWRV_
 *    OK            if signature/hash was ilwalidated successfully.
 *    ERR_GENERIC   if an unexpected error oclwrred.
 *
 * Intended as an internal-only colwenience wrapper for scp_forget_sig().
 */
static LWRV_STATUS
_scpForgetSignature(void)
{
    // Ilwalidate the engine's signature/hash.
    scp_forget_sig();

    // Wait for completion.
    scp_wait_dma();

    // Check for any unexpected failures.
    if (scp_check_dma(true) != LWRV_OK)
    {
        return LWRV_ERR_GENERIC;
    }

    // Signature/hash ilwalidated successfully.
    return LWRV_OK;
}

/*!
 * @brief Recovers inselwre access to the SCP GPRs.
 *
 * @return LWRV_
 *    OK            if access was recovered successfully.
 *    ERR_GENERIC   if an unexpected error oclwrred.
 *
 * Recovers inselwre-fetchable and inselwre-keyable access to the SCP GPRs.
 *
 * Requires that inselwre-writable access (Wi) is already present for all
 * registers. Only intended to be called from inselwre contexts.
 *
 * Note that this function makes use of the SCP unit's RNG hardware and does
 * NOT preserve its configuration settings, or the contents of the affected
 * GPRs.
 */
static LWRV_STATUS
_scpRecoverInselwre(void)
{
    //
    // The only way an inselwre context can recover access to an SCP GPR
    // previously locked by a secure context is by loading data of some kind
    // into it, which will overwrite the GPR's previous contents and reset its
    // associated ACL to a fully-open state.
    //
    // Unfortunately, declaring a temporary buffer to load data from is out of
    // the question here as we have no means of obtaining said buffer's
    // physical address in order to initiate an SCPDMA transfer. That leaves
    // the "rand" instruction as the only other practical means of getting
    // inselwre data into the GPRs.
    //
    // Of course, for even this to work, we need to already have inselwre-
    // writable (Wi) access to the target GPRs or we'll trigger a write-
    // violation interrupt, hence the pre-condition listed in the function
    // description.
    //
    // As of SCP RNG v2.0, the RNG hardware should be usable "out of the box",
    // so we don't bother with any configuration here (since the actual entropy
    // of the values we produce is irrelevant).
    //

    // Ensure that the RNG hardware is enabled.
    _scpEnableRand();

    //
    // Load a true-random number into register-index zero. This will
    // overwrite any (potentially sensitive) prior contents and unlock the
    // register's associated ACL to a fully-open state.
    //
    scp_rand(SCP_REGISTER_INDEX_0);

    //
    // Copy the true-random number from register-index zero into the remaining
    // GPRs, thus effectively copying its fully-open ACL to each of them as
    // well.
    //
    scp_mov(SCP_REGISTER_INDEX_0, SCP_REGISTER_INDEX_1);
    scp_mov(SCP_REGISTER_INDEX_0, SCP_REGISTER_INDEX_2);
    scp_mov(SCP_REGISTER_INDEX_0, SCP_REGISTER_INDEX_3);
    scp_mov(SCP_REGISTER_INDEX_0, SCP_REGISTER_INDEX_4);
    scp_mov(SCP_REGISTER_INDEX_0, SCP_REGISTER_INDEX_5);
    scp_mov(SCP_REGISTER_INDEX_0, SCP_REGISTER_INDEX_6);
    scp_mov(SCP_REGISTER_INDEX_0, SCP_REGISTER_INDEX_7);

    // Wait for completion.
    scp_wait_dma();

    // Disable the RNG hardware.
    _scpDisableRand();

    // Check for any unexpected failures.
    if (scp_check_dma(true) != LWRV_OK)
    {
        return LWRV_ERR_GENERIC;
    }

    // Registers unlocked successfully.
    return LWRV_OK;
}

/*!
 * @brief Checks for pending SCP interrupts.
 *
 * @return
 *    true  if there are interrupts pending.
 *    false if there are no interrupts pending.
 *
 * @note This function only checks for interrupts that would represent
 *       genuine error conditions in the context of initialization or
 *       shutdown and ignores all others.
 */
static bool
_scpHasInterrupt(void)
{
    uint32_t reg;

    // Read interrupt status.
    reg = localRead(LW_PRGNLCL_SCP_INTR);

    // ACL violations and security violations are always relevant.
    if (FLD_TEST_DRF(_PRGNLCL_SCP, _INTR, _ACL_VIO,      _PENDING, reg) ||
        FLD_TEST_DRF(_PRGNLCL_SCP, _INTR, _SELWRITY_VIO, _PENDING, reg))
    {
        return true;
    }

    // Only certain command errors are relevant so investigate further.
    if (FLD_TEST_DRF(_PRGNLCL_SCP, _INTR, _CMD_ERROR, _PENDING, reg))
    {
        // Obtain error details.
        reg = localRead(LW_PRGNLCL_SCP_CMD_ERROR);

        // Check for relevant errors.
        if (FLD_TEST_DRF(
                _PRGNLCL_SCP, _CMD_ERROR, _UNDEFINED,              _TRUE, reg) ||
            FLD_TEST_DRF(
                _PRGNLCL_SCP, _CMD_ERROR, _SEQ_EMPTY,              _TRUE, reg) ||
            FLD_TEST_DRF(
                _PRGNLCL_SCP, _CMD_ERROR, _SEQ_OVERFLOW,           _TRUE, reg) ||
            FLD_TEST_DRF(
                _PRGNLCL_SCP, _CMD_ERROR, _SIG_INSELWRE,           _TRUE, reg) ||
            FLD_TEST_DRF(
                _PRGNLCL_SCP, _CMD_ERROR, _CHMOD_INSELWRE,         _TRUE, reg) ||
            FLD_TEST_DRF(
                _PRGNLCL_SCP, _CMD_ERROR, _SECRET_INSELWRE,        _TRUE, reg) ||
            FLD_TEST_DRF(
                _PRGNLCL_SCP, _CMD_ERROR, _SEQ_INST_WHILE_LOADING, _TRUE, reg))
        {
            return true;
        }
    }

    // No relevant pending interrupts.
    return false;
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Checks the SCP unit for activity.
 *
 * @return
 *    true  if the SCP unit is busy.
 *    false if the SCP unit is idle.
 *
 * Checks whether the SCP unit is lwrrently active. Does not check for pending
 * activity (i.e. CCI transfers queued while processing is disabled).
 */
static inline bool
_scpIsActive(void)
{
    return FLD_TEST_DRF(_PRGNLCL_SCP, _STATUS, _SCP_ACTIVE, _TRUE,
                        localRead(LW_PRGNLCL_SCP_STATUS));
}

/*!
 * @brief Checks whether the host engine contains an SCP unit.
 *
 * @return
 *    true  if the SCP unit is present.
 *    false if the SCP unit is absent.
 */
static inline bool
_scpIsPresent(void)
{
    return FLD_TEST_DRF(_PRGNLCL_FALCON, _HWCFG2, _SCP, _ENABLE,
                        localRead(LW_PRGNLCL_FALCON_HWCFG2));
}

/*!
 * @brief Checks whether the host engine is capable of operating in a secure context.
 *
 * @return
 *    true  if the host engine is capable of operating in a secure context.
 *    false if the host engine is not capable of operating in a secure context.
 *
 * @note To check whether the host engine is lwrrently operating in a secure
 *       context, use _scpIsSelwre() instead.
 */
static inline bool
_scpIsSelwreCapable(void)
{
    return FLD_TEST_DRF64(_RISCV_CSR, _XSPM, _XSECM, _SEC,
                          csr_read(LW_RISCV_CSR_XSPM));
}

/*!
 * @brief Clears any pending SCP interrupts.
 */
static inline void
_scpClearInterrupts(void)
{
    // Clear the main interrupt register first.
    localWrite(LW_PRGNLCL_SCP_INTR,
        DRF_DEF(_PRGNLCL_SCP, _INTR, _RAND_READY,           _RESET) |
        DRF_DEF(_PRGNLCL_SCP, _INTR, _ACL_VIO,              _RESET) |
        DRF_DEF(_PRGNLCL_SCP, _INTR, _SELWRITY_VIO,         _RESET) |
        DRF_DEF(_PRGNLCL_SCP, _INTR, _CMD_ERROR,            _RESET) |
        DRF_DEF(_PRGNLCL_SCP, _INTR, _STEP,                 _RESET) |
        DRF_DEF(_PRGNLCL_SCP, _INTR, _RNDHOLDOFF_REQUESTED, _RESET) |
        DRF_DEF(_PRGNLCL_SCP, _INTR, _STALL_TIMEOUT,        _RESET));

    // Clear any ACL violations.
    localWrite(LW_PRGNLCL_SCP_ACL_VIO,
        DRF_DEF(_PRGNLCL_SCP, _ACL_VIO, _CHMOD_RELAX_VIO, _CLEAR) |
        DRF_DEF(_PRGNLCL_SCP, _ACL_VIO, _FETCH_VIO,       _CLEAR) |
        DRF_DEF(_PRGNLCL_SCP, _ACL_VIO, _WR_VIO,          _CLEAR));

    // Can't clear security violations.

    // Clear any command errors.
    localWrite(LW_PRGNLCL_SCP_CMD_ERROR,
        DRF_DEF(_PRGNLCL_SCP, _CMD_ERROR, _UNDEFINED,              _CLEAR) |
        DRF_DEF(_PRGNLCL_SCP, _CMD_ERROR, _SEQ_EMPTY,              _CLEAR) |
        DRF_DEF(_PRGNLCL_SCP, _CMD_ERROR, _SEQ_OVERFLOW,           _CLEAR) |
        DRF_DEF(_PRGNLCL_SCP, _CMD_ERROR, _SEQ_INST_WHILE_LOADING, _CLEAR) |
        DRF_DEF(_PRGNLCL_SCP, _CMD_ERROR, _SIG_INSELWRE,           _CLEAR) |
        DRF_DEF(_PRGNLCL_SCP, _CMD_ERROR, _SIG_ILWALID,            _CLEAR) |
        DRF_DEF(_PRGNLCL_SCP, _CMD_ERROR, _CHMOD_INSELWRE,         _CLEAR) |
        DRF_DEF(_PRGNLCL_SCP, _CMD_ERROR, _SECRET_INSELWRE,        _CLEAR));
}

/*!
 * @brief Disables all SCP interrupts.
 */
static inline void
_scpDisableInterrupts(void)
{
    // Disable all interrupts.
    localWrite(LW_PRGNLCL_SCP_INTR_EN,
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _RAND_READY,            _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _ACL_VIO,               _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _SELWRITY_VIO,          _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _CMD_ERROR,             _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _STEP,                  _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _RNDHOLDOFF_REQUESTED,  _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _STALL_TIMEOUT,         _DISABLED));
}

/*!
 * @brief Enables a critical subset of SCP interrupts.
 */
static inline void
_scpEnableInterrupts(void)
{
    // Enable only critical interrupts that shouldn't be ignored.
    localWrite(LW_PRGNLCL_SCP_INTR_EN,
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _RAND_READY,            _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _ACL_VIO,               _ENABLED)  |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _SELWRITY_VIO,          _ENABLED)  |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _CMD_ERROR,             _ENABLED)  |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _STEP,                  _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _RNDHOLDOFF_REQUESTED,  _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _INTR_EN, _STALL_TIMEOUT,         _DISABLED));
}

/*!
 * @brief Disables all processing of SCP commands.
 */
static inline void
_scpDisableProcessing(void)
{
    localWrite(LW_PRGNLCL_SCP_CTL0,
        DRF_DEF(_PRGNLCL_SCP, _CTL0, _CTL_EN,            _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL0, _SEQ_EN,            _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL0, _SF_CMD_IFACE_EN,   _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL0, _SF_PUSH_IFACE_EN,  _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL0, _SF_FETCH_IFACE_EN, _DISABLED));
}

/*!
 * @brief Enables processing of SCP commands.
 */
static inline void
_scpEnableProcessing(void)
{
    localWrite(LW_PRGNLCL_SCP_CTL0,
        DRF_DEF(_PRGNLCL_SCP, _CTL0, _CTL_EN,            _ENABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL0, _SEQ_EN,            _ENABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL0, _SF_CMD_IFACE_EN,   _ENABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL0, _SF_PUSH_IFACE_EN,  _ENABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL0, _SF_FETCH_IFACE_EN, _ENABLED));
}

/*!
 * @brief Resets the SCP hardware to its default configuration settings.
 *
 * These values have been confirmed by the hardware team.
 */
static inline void
_scpResetConfiguration(void)
{
    localWrite(LW_PRGNLCL_SCP_CFG0, (uint32_t)
        DRF_DEF(_PRGNLCL_SCP, _CFG0, _STALL_TIMEOUT, _INIT)   |
        DRF_DEF(_PRGNLCL_SCP, _CFG0, _CARRY_CHAIN,   _64)     |
        DRF_DEF(_PRGNLCL_SCP, _CFG0, _CMD_FLUSH,     _FALSE)  |
        DRF_DEF(_PRGNLCL_SCP, _CFG0, _AES_ENDIAN,    _BIG)    |
        DRF_DEF(_PRGNLCL_SCP, _CFG0, _PUSH_ENDIAN,   _LITTLE) |
        DRF_DEF(_PRGNLCL_SCP, _CFG0, _FETCH_ENDIAN,  _LITTLE) |
        DRF_DEF(_PRGNLCL_SCP, _CFG0, _CMAC_ENDIAN,   _BIG)    |
        DRF_DEF(_PRGNLCL_SCP, _CFG0, _ADD_ENDIAN,    _BIG));
}

/*!
 * @brief Resets the SCP pipeline and sequencers. Also disables RNG.
 */
static inline void
_scpResetControls(void)
{
    uint32_t reg;

    //
    // Disable RNG and request pipe and sequencer resets. We also disable some
    // debug-only features here that must not be active in production.
    //
    localWrite(LW_PRGNLCL_SCP_CTL1,
        DRF_DEF(_PRGNLCL_SCP, _CTL1, _SF_PUSH_BYPASS,   _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL1, _SF_FETCH_BYPASS,  _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL1, _SF_FETCH_AS_ZERO, _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL1, _RNG_EN,           _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL1, _RNG_FAKE_MODE,    _DISABLED) |
        DRF_DEF(_PRGNLCL_SCP, _CTL1, _PIPE_RESET,       _TASK)     |
        DRF_DEF(_PRGNLCL_SCP, _CTL1, _SEQ_CLEAR,        _TASK));

    // Wait for the pipe and sequencer resets to complete.
    do
    {
        reg = localRead(LW_PRGNLCL_SCP_CTL1);
    }while(FLD_TEST_DRF(_PRGNLCL_SCP, _CTL1, _PIPE_RESET, _PENDING, reg) ||
           FLD_TEST_DRF(_PRGNLCL_SCP, _CTL1, _SEQ_CLEAR,  _PENDING, reg));
}

/*!
 * @brief Resets the source of secret-index 63 per hardware recommendation.
 */
static inline void
_scpResetSecret63(void)
{
    localWrite(LW_PRGNLCL_SCP_SECRET63_CTL,
        DRF_DEF(_PRGNLCL_SCP, _SECRET63_CTL, _SEL, _PKEY));
}
