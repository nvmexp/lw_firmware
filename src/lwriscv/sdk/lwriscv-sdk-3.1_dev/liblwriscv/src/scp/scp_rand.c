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
 * @file        scp_rand.c
 * @brief       SCP features for random-number generation.
 */

#if !LWRISCV_FEATURE_SCP
#error "Attempting to build scp_rand.c but the SCP driver is disabled!"
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
#include <liblwriscv/io.h>

// SCP includes.
#include <liblwriscv/scp/scp_direct.h>
#include <liblwriscv/scp/scp_general.h>
#include <liblwriscv/scp/scp_private.h>
#include <liblwriscv/scp/scp_rand.h>


//
// Default tap selections for the two non-SCC ring-oscillators used as noise
// sources for RNG. Values taken from https://confluence.lwpu.com/x/MO1QGw.
//
#define SCP_DEFAULT_RO_TAP_A 0x7
#define SCP_DEFAULT_RO_TAP_B 0x5


//
// Prototypes for local helper functions.
// See definitions for details.
//
static LWRV_STATUS _scpLoadRandSequence(void);

static inline bool _scpIsRandEnabled(void);

static inline void _scpConfigureOscillators(void);

static inline void _scpDisableFakeRand(void);
static inline void _scpEnableFakeRand(void);


/*!
 * @brief Configures the SCP RNG hardware for use.
 *
 * @param[in]  config           The hardware configuration to use. See the
 *                              documentation for SCP_RAND_CONFIG for a list of
 *                              supported values.
 *
 * @return LWRV_
 *    OK                        if configuration settings were applied
 *                              successfully.
 *
 *    ERR_NOT_READY             if the SCP driver has not been initialized.
 *
 *    ERR_IN_USE                if the RNG hardware is lwrrently running and
 *                              cannot be reconfigured.
 *
 *    ERR_ILWALID_ARGUMENT      if config is not a valid SCP_RAND_CONFIG value.
 *
 *    ERR_ILWALID_REQUEST       if config is SCP_RAND_CONFIG_FAKE but the SCP
 *                              unit is not in debug mode.
 *
 *    ERR_FEATURE_NOT_ENABLED   if config is SCP_RAND_CONFIG_FAKE but the
 *                              application does not have fake-RNG support
 *                              enabled.
 *
 *    ERR_ILWALID_STATE         if calls to this function are prohibited
 *                              because the SCP driver is operating in direct
 *                              mode.
 *
 *    ERR_ILWALID_LOCK_STATE    if access to certain necessary registers is
 *                              being blocked by SCP lockdown.
 *
 * Configures the SCP RNG hardware according to the value of config.
 *
 * Applications must call scpConfigureRand() before attempting to start the RNG
 * hardware and must stop the hardware again before attempting to reconfigure,
 * should the need arise.
 */
LWRV_STATUS scpConfigureRand
(
    SCP_RAND_CONFIG config
)
{
    // Verify driver state before proceeding further.
    if (!g_scpState.bInitialized)
    {
        return LWRV_ERR_NOT_READY;
    }

    // Ilwalidate previous configuration settings, if any.
    g_scpState.bRandConfigured = false;

    // This function cannot be called from direct mode.
    if (g_scpState.bDirectModeActive)
    {
        return LWRV_ERR_ILWALID_STATE;
    }

    // Verify access to lockable SCP registers/fields.
    if (_scpIsAccessRestricted())
    {
        return LWRV_ERR_ILWALID_LOCK_STATE;
    }

    // Ensure that the RNG hardware is not lwrrently active.
    if (_scpIsRandEnabled())
    {
        return LWRV_ERR_IN_USE;
    }

    // Apply the desired configuration settings.
    switch (config)
    {
        case SCP_RAND_CONFIG_DEFAULT:
            // Tap the ring-oscillators at their recommended lengths.
            _scpConfigureOscillators();

            // Ensure that deterministic mode is off.
            _scpDisableFakeRand();
            break;

        case SCP_RAND_CONFIG_FAKE:
            // Build-time configuration is mandatory per security-bar requirements.
            if (!LWRISCV_DRIVER_SCP_FAKE_RNG_ENABLED)
            {
                return LWRV_ERR_FEATURE_NOT_ENABLED;
            }

            // Deterministic mode is not permitted in production.
            if (!scpIsDebug())
            {
                return LWRV_ERR_ILWALID_REQUEST;
            }

            //
            // Enable deterministic mode as requested (no further configuration
            // is needed as of SCP RNG v2.0).
            //
            _scpEnableFakeRand();
            break;

        default:
            // Not a recognized configuration.
            return LWRV_ERR_ILWALID_ARGUMENT;
    }

    // Perform a pipe reset to clear any residual RNG data.
    _scpResetPipeline();

    // Update internal state.
    g_scpState.bRandConfigured = true;

    // Configuration settings were applied successfully.
    return LWRV_OK;
}

/*!
 * @brief Starts the SCP RNG hardware.
 *
 * @return LWRV_
 *    OK                        if RNG hardware was started successfully.
 *
 *    ERR_NOT_READY             if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_STATE         if the application has not called
 *                              scpConfigureRand() to configure the RNG
 *                              hardware for use.
 *
 *    ERR_ILWALID_LOCK_STATE    if access to certain necessary registers is
 *                              being blocked by SCP lockdown.
 *
 *    WARN_NOTHING_TO_DO        if the RNG hardware is already running (no-op).
 *
 * Starts the SCP unit's RNG hardware. Applications must call scpStartRand()
 * before attempting to acquire random data from scpGetRand() (or scp_rand()).
 *
 * Note that there is some delay between starting the RNG hardware and
 * acquiring the first RNG data from it. The length of this delay is dictated
 * by the specific hardware configuration set by scpConfigureRand() and
 * manifests as increased latency in the exelwtion of scpGetRand().
 *
 * This function is safe to call from direct mode.
 */
LWRV_STATUS scpStartRand(void)
{
    // Verify driver state before proceeding further.
    if (!g_scpState.bInitialized)
    {
        return LWRV_ERR_NOT_READY;
    }

    // Ensure that the RNG hardware has been properly configured.
    if (!g_scpState.bRandConfigured)
    {
        return LWRV_ERR_ILWALID_STATE;
    }

    // Verify access to lockable SCP registers/fields.
    if (_scpIsAccessRestricted())
    {
        return LWRV_ERR_ILWALID_LOCK_STATE;
    }

    // Check whether the RNG hardware is already running.
    if (_scpIsRandEnabled())
    {
        return LWRV_WARN_NOTHING_TO_DO;
    }

    // Start the RNG hardware.
    _scpEnableRand();

    // RNG hardware was started successfully.
    return LWRV_OK;
}

/*!
 * @brief Stops the SCP RNG hardware.
 *
 * @return LWRV_
 *    OK                        if RNG hardware was stopped successfully.
 *
 *    ERR_NOT_READY             if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_LOCK_STATE    if access to certain necessary registers is
 *                              being blocked by SCP lockdown.
 *
 *    WARN_NOTHING_TO_DO        if the RNG hardware is already stopped (no-op).
 *
 * Stops the SCP unit's RNG hardware. Applications must call scpStopRand()
 * before attempting to reconfigure the RNG hardware via scpConfigureRand().
 *
 * This function is safe to call from direct mode.
 */
LWRV_STATUS scpStopRand(void)
{
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

    // Check whether the RNG hardware is already stopped.
    if (!_scpIsRandEnabled())
    {
        return LWRV_WARN_NOTHING_TO_DO;
    }

    //
    // Make sure the SCP unit is idle before disabling RNG. Otherwise, we may
    // cause some pending RNG operation to hang.
    //
    scp_wait_dma();

    // Stop the RNG hardware.
    _scpDisableRand();

    // RNG hardware was stopped successfully.
    return LWRV_OK;
}

/*!
 * @brief Generates an arbitrary string of true-random data.
 *
 * @param[out] destPa       The physical address of the output buffer to which
 *                          the generated data is to be written. Must be
 *                          aligned to SCP_BUFFER_ALIGNMENT and have at least
 *                          size bytes of capacity. Supports IMEM, DMEM, and
 *                          external locations.
 *
 * @param[in]  size         The number of bytes of data to generate. Must be a
 *                          nonzero multiple of SCP_RAND_SIZE.
 *
 * @param[in]  contextPa    The physical address of the input buffer containing
 *                          the initial context value to be factored into the
 *                          generated data (zero/NULL if none). Must be aligned
 *                          to SCP_BUFFER_ALIGNMENT and contain at least
 *                          SCP_RAND_SIZE bytes. Permitted to alias to destPa.
 *                          Supports IMEM and DMEM locations only.
 *
 * @return LWRV_
 *    OK                            if data was generated successfully.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_OPERATION         if the RNG hardware is not running or has
 *                                  not been properly configured.
 *
 *    ERR_ILWALID_POINTER           if destPa is zero/NULL.
 *
 *    ERR_ILWALID_ADDRESS           if one or more of destPa or contextPa is
 *                                  incorrectly aligned.
 *
 *    ERR_ILWALID_BASE              if one or more of destPa or contextPa
 *                                  points to an unsupported memory region
 *                                  (e.g. EMEM).
 *
 *    ERR_ILWALID_REQUEST           if size is zero or is not a multiple of
 *                                  SCP_RAND_SIZE.
 *
 *    ERR_FEATURE_NOT_ENABLED       if destPa is located in external memory but
 *                                  the application does not have shortlwt-DMA
 *                                  support enabled.
 *
 *    ERR_ILWALID_DMA_SPECIFIER     if destPa is located in external memory but
 *                                  the application has not called
 *                                  scpConfigureShortlwt() to configure the
 *                                  shortlwt-DMA path.
 *
 *    ERR_OUT_OF_RANGE              if one or more of the buffers referenced by
 *                                  destPa or contextPa overflows the memory
 *                                  region in which it resides.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_ILWALID_STATE             if calls to this function are prohibited
 *                                  because the SCP driver is operating in
 *                                  direct mode.
 *
 *    ERR_ILWALID_LOCK_STATE        if access to certain necessary registers is
 *                                  being blocked by SCP lockdown.
 *
 *    ERR_PROTECTION_FAULT          if an IO-PMP fault was detected.
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Generates size bytes of true-random data and writes the result to destPa.
 * Uses the first SCP_RAND_SIZE bytes from contextPa as a contextual input to
 * the generation process.
 *
 * Applications must call scpStartRand() to ensure that the RNG hardware is
 * running before attempting to acquire data from scpGetRand().
 *
 * Keep in mind that the quality of the data generated by scpGetRand() is
 * heavily influenced by the RNG hardware's specific configuration settings,
 * which also affect the rate at which the random data is produced. These
 * settings can be tuned via scpConfigureRand().
 *
 * In any case, the data produced by scpGetRand() is not NIST-compliant and
 * should not be used to generate encryption keys or initialization vectors.
 *
 * Note that it is permissible for destPa and contextPa to alias to the same
 * memory location.
 *
 * In the event that destPa refers to an external memory location, the
 * application must ensure that it has first called scpConfigureShortlwt() (or
 * used the SCP_EXTERNAL_DEST() helper macro) to configure the shortlwt-DMA
 * path accordingly.
 */
LWRV_STATUS scpGetRand
(
    uintptr_t destPa,
    size_t size,
    uintptr_t contextPa
)
{
    LWRV_STATUS status = LWRV_OK;

    // Verify driver state before proceeding further.
    if (!g_scpState.bInitialized)
    {
        // Early exit as clean-up code may not be able to execute safely.
        return LWRV_ERR_NOT_READY;
    }

    // This function cannot be called from direct mode.
    if (g_scpState.bDirectModeActive)
    {
        // Early exit as clean-up code may not be able to execute safely.
        return LWRV_ERR_ILWALID_STATE;
    }

    // Verify access to lockable SCP registers/fields.
    if (_scpIsAccessRestricted())
    {
        // Early exit as clean-up code may not be able to execute safely.
        return LWRV_ERR_ILWALID_LOCK_STATE;
    }

    // Make sure the RNG hardware is active and properly configured.
    if (!_scpIsRandEnabled() || !g_scpState.bRandConfigured)
    {
        status = LWRV_ERR_ILWALID_OPERATION;
        goto exit;
    }

    // Verify destination buffer.
    status = _scpValidateBuffer(destPa, size, true);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Verify byte count.
    if ((size == 0) || (size % SCP_RAND_SIZE != 0))
    {
        status = LWRV_ERR_ILWALID_REQUEST;
        goto exit;
    }

    // Handle context buffer, if provided.
    if (contextPa != (uintptr_t)NULL)
    {
        // Verify context buffer.
        status = _scpValidateBuffer(contextPa, SCP_RAND_SIZE, false);
        if (status != LWRV_OK)
        {
            goto exit;
        }

        // Load initial context information into register-index one.
        status = _scpLoadBuffer(contextPa, SCP_REGISTER_INDEX_1);
        if (status != LWRV_OK)
        {
            goto exit;
        }
    }

    // Load sequencer zero with the instructions for RNG and update Ku.
    status = _scpLoadRandSequence();
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Run the sequence we loaded above to generate the output data.
    status = _scpRunSequence((uintptr_t)NULL, destPa, size);
    if (status != LWRV_OK)
    {
        goto exit;
    }

exit:
    //
    // Clear the sequencers to free them up for future use and prevent
    // anything accidentally carrying forward into other interfaces.
    //
    _scpResetSequencers();

    //
    // Clear the SCP GPRs to scrub any potentially sensitive data from them and
    // unlock their associated ACLs. Report failures only if no higher-priority
    // error is already pending.
    //
    LWRV_STATUS clearStatus = _scpClearAll();
    if (status == LWRV_OK)
    {
        status = clearStatus;
    }

    // Done.
    return status;
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Loads the SCP instructions for RNG into sequencer zero.
 *
 * @return LWRV_
 *    OK                            if instructions were loaded successfully.
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Loads sequencer zero with the set of SCP instructions required to generate a
 * single block of true-random data. Also updates the key register (Ku).
 *
 * The loaded algorithm assumes that the initial context value, if any, has
 * been loaded to register-index one and that register-indices two and three
 * are available for use as intermediate storage.
 */
static LWRV_STATUS
_scpLoadRandSequence(void)
{
    // Prepare to write five instructions to sequencer zero.
    scp_load_trace0(5);
    {
        //
        // Improve RNG quality by collecting entropy across two conselwtive
        // random numbers.
        //
        scp_rand(SCP_REGISTER_INDEX_2);
        scp_rand(SCP_REGISTER_INDEX_3);

        // Mix the current context value into the first number.
        scp_xor(SCP_REGISTER_INDEX_1, SCP_REGISTER_INDEX_2);

        //
        // Whiten the second number with an encryption step using the above
        // value as the key. The result will be carried forward as the new
        // context value to be used in the next iteration.
        //
        scp_encrypt(SCP_REGISTER_INDEX_3, SCP_REGISTER_INDEX_1);

        // Store the random data we produced.
        scp_fetch(SCP_REGISTER_INDEX_1);
    }

    //
    // Select the register from which the encrypt stage of each sequencer
    // iteration will source its key. This only needs to be specified once,
    // hence its exclusion from the above sequencer load.
    //
    scp_key(SCP_REGISTER_INDEX_2);

    // Wait for completion.
    scp_wait_dma();

    // Check for any unexpected failures.
    if (scp_check_dma(true) != LWRV_OK)
    {
        return LWRV_ERR_GENERIC;
    }

    // Sequence loaded successfully.
    return LWRV_OK;
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Checks whether the SCP unit's RNG hardware is enabled.
 *
 * @return
 *    true  if the RNG hardware is enabled.
 *    false if the RNG hardware is disabled.
 */
static inline bool
_scpIsRandEnabled(void)
{
    return FLD_TEST_DRF(_PRGNLCL_SCP, _CTL1, _RNG_EN, _ENABLED,
                        localRead(LW_PRGNLCL_SCP_CTL1));
}

/*!
 * @brief Configures the ring-oscillators for the SCP unit's RNG hardware.
 */
static inline void
_scpConfigureOscillators(void)
{
    uint32_t reg = localRead(LW_PRGNLCL_SCP_RNDCTL11);

    reg = FLD_SET_DRF_NUM(_PRGNLCL_SCP, _RNDCTL11, _AUTOCAL_STATIC_TAP_A,
                          SCP_DEFAULT_RO_TAP_A, reg);
    reg = FLD_SET_DRF_NUM(_PRGNLCL_SCP, _RNDCTL11, _AUTOCAL_STATIC_TAP_B,
                          SCP_DEFAULT_RO_TAP_B, reg);

    localWrite(LW_PRGNLCL_SCP_RNDCTL11, reg);
}

/*!
 * @brief Disables deterministic mode for the SCP unit's RNG hardware.
 */
static inline void
_scpDisableFakeRand(void)
{
    localWrite(LW_PRGNLCL_SCP_CTL1,
        FLD_SET_DRF(_PRGNLCL_SCP, _CTL1, _RNG_FAKE_MODE, _DISABLED,
            localRead(LW_PRGNLCL_SCP_CTL1)));
}

/*!
 * @brief Enables deterministic mode for the SCP unit's RNG hardware.
 */
static inline void
_scpEnableFakeRand(void)
{
    localWrite(LW_PRGNLCL_SCP_CTL1,
        FLD_SET_DRF(_PRGNLCL_SCP, _CTL1, _RNG_FAKE_MODE, _ENABLED,
            localRead(LW_PRGNLCL_SCP_CTL1)));
}
