/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_SCP_GENERAL_H
#define LIBLWRISCV_SCP_GENERAL_H

/*!
 * @file        scp_general.h
 * @brief       General SCP operations.
 *
 * @note        Client applications should include scp.h instead of this file.
 */

// Standard includes.
#include <stdint.h>

// SDK includes.
#include <lwmisc.h>

// LWRISCV includes.
#include <lwriscv/status.h>

// SCP includes.
#include <liblwriscv/scp/scp_common.h>


/*!
 * @brief Allocates a buffer whose base is aligned to SCP_BUFFER_ALIGNMENT.
 *
 * @param[in] size The desired size for the new buffer in bytes. This should
 *                 generally be a nonzero multiple of one of the provided
 *                 SCP_*_SIZE constants.
 *
 * @return A pointer to the newly-allocated buffer.
 *
 * Allocates a buffer of size at least size whose base address is aligned to
 * SCP_BUFFER_ALIGNMENT. The buffer has automatic storage duration.
 *
 * Note that the pointer returned by this macro may require virtual-to-physical
 * colwerion before it can be used in an SCP interface call, depending on the
 * application's current operating mode. Furthermore, it is up to the
 * application to ensure that the alignment properties of the returned pointer
 * are preserved through the virtual-to-physical colwersion process, if
 * applicable.
 *
 * This macro is safe to call from direct mode.
 */
#define SCP_ALIGNED_BUFFER(size) (                                  \
    (uint8_t*)LW_ALIGN_UP64(                                        \
        (uintptr_t)(uint8_t[(size) + SCP_BUFFER_ALIGNMENT - 1]){0}, \
        SCP_BUFFER_ALIGNMENT                                        \
    )                                                               \
)

/*!
 * @brief Configures the shortlwt-DMA path for an external destination buffer.
 *
 * @param[in] destPa   The physical address of the destination buffer.
 * @param[in] dmaIndex The index of the DMA aperture to be used when
 *                     transferring to the destination buffer.
 *
 * @return
 *    destPa if the shortlwt-DMA path was configured successfully.
 *
 *    0      if dmaIndex does not refer to a valid DMA aperture, or the
 *           application does not have shortlwt-DMA support enabled, or
 *           the SCP driver has not been initialized.
 *
 * Configures the shortlwt path for transfers to external memory as if by
 * calling scpConfigureShortlwt(dmaIndex) and then returns destPa. Returns
 * zero/NULL if configuration fails.
 *
 * This helper is meant to simplify the process of passing external buffers to
 * applicable SCP interfaces by allowing for the shortlwt-DMA path to be
 * configured in-line. It also aims to lessen the risk of misconfigured
 * shortlwt transfers by encouraging applications to explicitly re-configure
 * for each operation.
 *
 * Note that shortlwt-DMA support must be enabled during build in order for
 * this macro to succeed.
 */
#define SCP_EXTERNAL_DEST(destPa, dmaIndex) (       \
    (scpConfigureShortlwt(dmaIndex) == LWRV_OK) ?   \
    (destPa) : (uintptr_t)0                         \
)


//
// A collection of single-bit flags that can be used to lwstomize certain
// behaviours of the SCP driver. Passed to scpInit().
//
typedef enum SCP_INIT_FLAG
{
    //
    // Use the recommended default configuration settings.
    // Ignored if combined with other flags.
    //
    SCP_INIT_FLAG_DEFAULT = 0,

} SCP_INIT_FLAG;


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
LWRV_STATUS scpInit(SCP_INIT_FLAG flags);

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
LWRV_STATUS scpShutdown(void);

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
bool scpIsDebug(void);

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
LWRV_STATUS scpClearReg(SCP_REGISTER_INDEX regIndex);

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
LWRV_STATUS scpClearAll(void);

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
LWRV_STATUS scpConfigureShortlwt(uint8_t dmaIndex);

#endif // LIBLWRISCV_SCP_GENERAL_H
