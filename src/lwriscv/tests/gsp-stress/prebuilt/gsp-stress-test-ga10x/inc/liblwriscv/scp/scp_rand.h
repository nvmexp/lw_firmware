/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_SCP_RAND_H
#define LIBLWRISCV_SCP_RAND_H

/*!
 * @file    scp_rand.h
 * @brief   SCP features for random-number generation.
 *
 * @note    Client applications should include scp.h instead of this file.
 */

// Standard includes.
#include <stddef.h>
#include <stdint.h>

// LWRISCV includes.
#include <lwriscv/status.h>


//
// The size, in bytes, of a single block of data in the context of RNG
// operations.
//
#define SCP_RAND_SIZE 16


//
// A collection of supported configurations for the SCP unit's RNG hardware.
// Passed to scpConfigureRand().
//
typedef enum SCP_RAND_CONFIG
{
    //
    // The default configuration recommended by hardware for optimal RNG
    // quality.
    //
    SCP_RAND_CONFIG_DEFAULT,

    //
    // A configuration that generates fake, deterministic data for testing and
    // debugging purposes. Note that this configuration is only supported when
    // the SCP unit is in debug mode and also requires that fake-RNG support be
    // enabled during build.
    //
    SCP_RAND_CONFIG_FAKE,

}SCP_RAND_CONFIG;


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
 * Configures the SCP RNG hardware according to the value of config.
 *
 * Applications must call scpConfigureRand() before attempting to start the RNG
 * hardware and must stop the hardware again before attempting to reconfigure,
 * should the need arise.
 */
LWRV_STATUS scpConfigureRand(SCP_RAND_CONFIG config);

/*!
 * @brief Starts the SCP RNG hardware.
 *
 * @return LWRV_
 *    OK                    if RNG hardware was started successfully.
 *
 *    ERR_NOT_READY         if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_STATE     if the application has not called
 *                          scpConfigureRand() to configure the RNG hardware
 *                          for use.
 *
 *    WARN_NOTHING_TO_DO    if the RNG hardware is already running (no-op).
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
LWRV_STATUS scpStartRand(void);

/*!
 * @brief Stops the SCP RNG hardware.
 *
 * @return LWRV_
 *    OK                    if RNG hardware was stopped successfully.
 *    ERR_NOT_READY         if the SCP driver has not been initialized.
 *    WARN_NOTHING_TO_DO    if the RNG hardware is already stopped (no-op).
 *
 * Stops the SCP unit's RNG hardware. Applications must call scpStopRand()
 * before attempting to reconfigure the RNG hardware via scpConfigureRand().
 *
 * Note that applications can also call scpStopRand() to conserve power
 * whenever RNG facilities are not needed.
 *
 * This function is safe to call from direct mode.
 */
LWRV_STATUS scpStopRand(void);

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
LWRV_STATUS scpGetRand(uintptr_t destPa, size_t size, uintptr_t contextPa);

#endif // LIBLWRISCV_SCP_RAND_H
