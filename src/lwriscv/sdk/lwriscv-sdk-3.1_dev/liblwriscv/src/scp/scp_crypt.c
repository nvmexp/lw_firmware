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
 * @file        scp_crypt.c
 * @brief       Cryptographic SCP features.
 */

#if !LWRISCV_FEATURE_SCP
#error "Attempting to build scp_crypt.c but the SCP driver is disabled!"
#endif // LWRISCV_FEATURE_SCP


// Standard includes.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// LWRISCV includes.
#include <lwriscv/status.h>

// LIBLWRISCV includes.
#include <liblwriscv/memutils.h>

// SCP includes.
#include <liblwriscv/scp/scp_common.h>
#include <liblwriscv/scp/scp_crypt.h>
#include <liblwriscv/scp/scp_direct.h>
#include <liblwriscv/scp/scp_private.h>


//
// The chmod value for a secure-keyable-only register ACL. Intended to protect
// encryption/decryption keys in secure contexts.
//
#define SCP_REGISTER_ACL_KEY 0x1


//
// Prototypes for local helper functions.
// See definitions for details.
//
static LWRV_STATUS _scpCrypt(const SCP_KEY_DESC *pKeyDesc,
                             SCP_CIPHER_MODE cipherMode, uintptr_t sourcePa,
                             uintptr_t destPa, size_t size,
                             uintptr_t contextPa, bool bDecrypt);

static LWRV_STATUS _scpLoadKey(const SCP_KEY_DESC *pKeyDesc);

static LWRV_STATUS _scpLoadBufferKey(uintptr_t keyPa);

static LWRV_STATUS _scpLoadRegisterKey(SCP_REGISTER_INDEX regIndex);

static LWRV_STATUS _scpLoadEcbKey(uint8_t secretIndex, uintptr_t saltPa);

static LWRV_STATUS _scpLoadCbcKey(uint8_t secretIndex, uintptr_t saltPa,
                                  uintptr_t ivPa);

static LWRV_STATUS _scpLoadCryptSequence(SCP_CIPHER_MODE cipherMode,
                                         bool bDecrypt);

static LWRV_STATUS _scpLoadEcbSequence(bool bDecrypt);

static LWRV_STATUS _scpLoadCbcSequence(bool bDecrypt);

static LWRV_STATUS _scpLoadCtrSequence(void);

static LWRV_STATUS _scpLoadHashSequence(void);

static LWRV_STATUS _scpLoadCmacSequence(void);

static LWRV_STATUS _scpCompleteCmac(uintptr_t finalPa, bool bPadded);


/*!
 * @brief Encrypts an arbitary buffer of data using an AES-128 block cipher.
 *
 * @param[in]  pKeyDesc     A pointer to the key-descriptor to be used for the
 *                          encryption operation. Applications should acquire
 *                          this parameter through one of the provided
 *                          SCP_KEY_* macros.
 *
 * @param[in]  cipherMode   The block-cipher mode of operation to use. See the
 *                          documentation for SCP_CIPHER_MODE for a list of
 *                          supported values.
 *
 * @param[in]  sourcePa     The physical address of the input buffer to be
 *                          encrypted. Must be aligned to SCP_BUFFER_ALIGNMENT
 *                          and contain at least size bytes. Supports IMEM and
 *                          DMEM locations only.
 *
 * @param[out] destPa       The physical address of the output buffer to which
 *                          the encrypted data is to be written. Must be
 *                          aligned to SCP_BUFFER_ALIGNMENT and have at least
 *                          size bytes of capacity. Permitted to overlap with
 *                          sourcePa only if exactly equal. Supports IMEM,
 *                          DMEM, and external locations.
 *
 * @param[in]  size         The number of bytes to process from sourcePa. Must
 *                          be a nonzero multiple of SCP_BLOCK_SIZE.
 *
 * @param[in]  contextPa    The physical address of the input buffer containing
 *                          the initialization vector (CBC) or initial counter
 *                          value with nonce (CTR) to be used for the
 *                          encryption operation. Must be aligned to
 *                          SCP_BUFFER_ALIGNMENT and contain at least
 *                          SCP_BLOCK_SIZE bytes. Required to be zero (NULL)
 *                          for ECB mode. Supports IMEM and DMEM locations
 *                          only.
 *
 * @return LWRV_
 *    OK                            if data was encrypted successfully.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_OBJECT            if pKeyDesc does not point to a valid key-
 *                                  descriptor.
 *
 *    ERR_ILWALID_OBJECT_BUFFER     if pKeyDesc was constructed with a zero
 *                                  (NULL) or incorrectly-aligned address, or a
 *                                  pointer to an unsupported memory location
 *                                  (e.g. EMEM), or a buffer that overflows the
 *                                  memory region in which it resides.
 *
 *    ERR_ILWALID_INDEX             if pKeyDesc was constructed with an invalid
 *                                  register or hardware-secret index, or
 *                                  references secret-index zero.
 *
 *    ERR_INSUFFICIENT_PERMISSIONS  if pKeyDesc was constructed with a
 *                                  hardware-secret index that the application
 *                                  does not have permission to use.
 *
 *    ERR_ILWALID_POINTER           if one or more of pKeyDesc, sourcePa, or
 *                                  destPa is zero/NULL, or contextPa is zero
 *                                  (NULL) and cipherMode is not
 *                                  SCP_CIPHER_MODE_ECB.
 *
 *    ERR_ILWALID_ADDRESS           if one or more of sourcePa, destPa, or
 *                                  contextPa is incorrectly aligned, or the
 *                                  buffers referenced by sourcePa and destPa
 *                                  overlap without being equal.
 *
 *    ERR_ILWALID_BASE              if one or more of sourcePa, destPa, or
 *                                  contextPa points to an unsupported memory
 *                                  region (e.g. EMEM).
 *
 *    ERR_ILWALID_REQUEST           if size is zero or is not a multiple of
 *                                  SCP_BLOCK_SIZE.
 *
 *    ERR_ILWALID_OPERATION         if cipherMode is not a valid
 *                                  SCP_CIPHER_MODE value, or cipherMode is
 *                                  SCP_CIPHER_MODE_ECB and contextPa is
 *                                  nonzero (non-NULL).
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
 *                                  sourcePa, destPa, or contextPa overflows
 *                                  the memory region in which it resides.
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
 * Encrypts the first size bytes of data from sourcePa using the key described
 * by pKeyDesc and writes the result to destPa. Uses an AES-128 block cipher
 * operating in the mode indicated by cipherMode as the encryption algorithm.
 * If applicable, SCP_BLOCK_SIZE bytes from contextPa are used as the initial
 * contextual input to the cipher.
 *
 * Should pKeyDesc describe a derived key, the scpEncrypt() function will
 * perform the indicated key-derivation operation automatically. See the
 * relevant SCP_KEY_* macros for details.
 *
 * Note that it is permissible for sourcePa and destPa to reference the same
 * memory location (i.e. to facilitate encrypting data in-place). However, the
 * use of buffers that only partially overlap is prohibited and will result in
 * an error.
 *
 * In the event that destPa refers to an external memory location, the
 * application must ensure that it has first called scpConfigureShortlwt() (or
 * used the SCP_EXTERNAL_DEST() helper macro) to configure the shortlwt-DMA
 * path accordingly.
 */
LWRV_STATUS
scpEncrypt
(
    const SCP_KEY_DESC *pKeyDesc,
    SCP_CIPHER_MODE cipherMode,
    uintptr_t sourcePa,
    uintptr_t destPa,
    size_t size,
    uintptr_t contextPa
)
{
    return _scpCrypt(pKeyDesc, cipherMode, sourcePa, destPa, size, contextPa,
                     false);
}

/*!
 * @brief Decrypts an arbitary buffer of data using an AES-128 block cipher.
 *
 * @param[in]  pKeyDesc     A pointer to the key-descriptor to be used for the
 *                          decryption operation. Applications should acquire
 *                          this parameter through one of the provided
 *                          SCP_KEY_* macros.
 *
 * @param[in]  cipherMode   The block-cipher mode of operation to use. See the
 *                          documentation for SCP_CIPHER_MODE for a list of
 *                          supported values.
 *
 * @param[in]  sourcePa     The physical address of the input buffer to be
 *                          decrypted. Must be aligned to SCP_BUFFER_ALIGNMENT
 *                          and contain at least size bytes. Supports IMEM and
 *                          DMEM locations only.
 *
 * @param[out] destPa       The physical address of the output buffer to which
 *                          the decrypted data is to be written. Must be
 *                          aligned to SCP_BUFFER_ALIGNMENT and have at least
 *                          size bytes of capacity. Permitted to overlap with
 *                          sourcePa only if exactly equal. Supports IMEM,
 *                          DMEM, and external locations.
 *
 * @param[in]  size         The number of bytes to process from sourcePa. Must
 *                          be a nonzero multiple of SCP_BLOCK_SIZE.
 *
 * @param[in]  contextPa    The physical address of the input buffer containing
 *                          the initialization vector (CBC) or initial counter
 *                          value with nonce (CTR) to be used for the
 *                          decryption operation. Must be aligned to
 *                          SCP_BUFFER_ALIGNMENT and contain at least
 *                          SCP_BLOCK_SIZE bytes. Required to be zero (NULL)
 *                          for ECB mode. Supports IMEM and DMEM locations
 *                          only.
 *
 * @return LWRV_
 *    OK                            if data was decrypted successfully.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_OBJECT            if pKeyDesc does not point to a valid key-
 *                                  descriptor.
 *
 *    ERR_ILWALID_OBJECT_BUFFER     if pKeyDesc was constructed with a zero
 *                                  (NULL) or incorrectly-aligned address, or a
 *                                  pointer to an unsupported memory location
 *                                  (e.g. EMEM), or a buffer that overflows the
 *                                  memory region in which it resides.
 *
 *    ERR_ILWALID_INDEX             if pKeyDesc was constructed with an invalid
 *                                  register or hardware-secret index, or
 *                                  references secret-index zero.
 *
 *    ERR_INSUFFICIENT_PERMISSIONS  if pKeyDesc was constructed with a
 *                                  hardware-secret index that the application
 *                                  does not have permission to use.
 *
 *    ERR_ILWALID_POINTER           if one or more of pKeyDesc, sourcePa, or
 *                                  destPa is zero/NULL, or contextPa is zero
 *                                  (NULL) and cipherMode is not
 *                                  SCP_CIPHER_MODE_ECB.
 *
 *    ERR_ILWALID_ADDRESS           if one or more of sourcePa, destPa, or
 *                                  contextPa is incorrectly aligned, or the
 *                                  buffers referenced by sourcePa and destPa
 *                                  overlap without being equal.
 *
 *    ERR_ILWALID_BASE              if one or more of sourcePa, destPa, or
 *                                  contextPa points to an unsupported memory
 *                                  region (e.g. EMEM).
 *
 *    ERR_ILWALID_REQUEST           if size is zero or is not a multiple of
 *                                  SCP_BLOCK_SIZE.
 *
 *    ERR_ILWALID_OPERATION         if cipherMode is not a valid
 *                                  SCP_CIPHER_MODE value, or cipherMode is
 *                                  SCP_CIPHER_MODE_ECB and contextPa is
 *                                  nonzero (non-NULL).
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
 *                                  sourcePa, destPa, or contextPa overflows
 *                                  the memory region in which it resides.
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
 * Decrypts the first size bytes of data from sourcePa using the key described
 * by pKeyDesc and writes the result to destPa. Uses an AES-128 block cipher
 * operating in the mode indicated by cipherMode as the decryption algorithm.
 * If applicable, SCP_BLOCK_SIZE bytes from contextPa are used as the initial
 * contextual input to the cipher.
 *
 * Should pKeyDesc describe a derived key, the scpDecrypt() function will
 * perform the indicated key-derivation operation automatically. See the
 * relevant SCP_KEY_* macros for details.
 *
 * Note that it is permissible for sourcePa and destPa to reference the same
 * memory locations (i.e. to facilitate decrypting data in-place). However, the
 * use of buffers that only partially overlap is prohibited and will result in
 * an error.
 *
 * In the event that destPa refers to an external memory location, the
 * application must ensure that it has first called scpConfigureShortlwt() (or
 * used the SCP_EXTERNAL_DEST() helper macro) to configure the shortlwt-DMA
 * path accordingly.
 */
LWRV_STATUS
scpDecrypt
(
    const SCP_KEY_DESC *pKeyDesc,
    SCP_CIPHER_MODE cipherMode,
    uintptr_t sourcePa,
    uintptr_t destPa,
    size_t size,
    uintptr_t contextPa
)
{
    return _scpCrypt(pKeyDesc, cipherMode, sourcePa, destPa, size, contextPa,
                     true);
}

/*!
 * @brief Computes the Davies-Meyer hash of an arbitrary buffer of data.
 *
 * @param[in]  sourcePa The physical address of the input buffer whose hash
 *                      will be computed. Must be aligned to
 *                      SCP_BUFFER_ALIGNMENT and contain at least size bytes.
 *                      Supports IMEM and DMEM locations only.
 *
 * @param[in]  size     The number of bytes to process from sourcePa. Must be a
 *                      nonzero multiple of SCP_KEY_SIZE.
 *
 * @param[out] destPa   The physical address of the output buffer to which the
 *                      computed hash value is to be written. Must be aligned
 *                      to SCP_BUFFER_ALIGNMENT and have at least
 *                      SCP_BLOCK_SIZE bytes of capacity. Supports IMEM, DMEM,
 *                      and external locations.
 *
 * @param[in]  initPa   The physical address of the input buffer containing the
 *                      initial hash value H0 to be used in the computation.
 *                      Must be aligned to SCP_BUFFER_ALIGNMENT and contain at
 *                      least SCP_BLOCK_SIZE bytes. Permitted to alias to
 *                      destPa. Supports IMEM and DMEM locations only.
 *
 * @return LWRV_
 *    OK                            if hash was computed successfully.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_POINTER           if one or more of sourcePa, destPa, or
 *                                  initPa is zero/NULL.
 *
 *    ERR_ILWALID_ADDRESS           if one or more of sourcePa, destPa, or
 *                                  initPa is incorrectly aligned.
 *
 *    ERR_ILWALID_BASE              if one or more of sourcePa, destPa, or
 *                                  initPa points to an unsupported memory
 *                                  location (e.g. EMEM).
 *
 *    ERR_ILWALID_REQUEST           if size is zero or is not a multiple of
 *                                  SCP_KEY_SIZE.
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
 *                                  sourcePa, destPa, or initPa overflows
 *                                  the memory region in which it resides.
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
 * Computes the Davies-Meyer hash of the first size bytes of data from sourcePa
 * and writes the result to destPa. Uses SCP_BLOCK_SIZE bytes from initPa as
 * the initial hash value.
 *
 * Note that it is permissible for destPa and initPa to alias to the same
 * memory location.
 *
 * In the event that destPa refers to an external memory location, the
 * application must ensure that it has first called scpConfigureShortlwt() (or
 * used the SCP_EXTERNAL_DEST() helper macro) to configure the shortlwt-DMA
 * path accordingly.
 */
LWRV_STATUS
scpDmhash
(
    uintptr_t sourcePa,
    size_t size,
    uintptr_t destPa,
    uintptr_t initPa
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

    // Verify source buffer.
    status = _scpValidateBuffer(sourcePa, size, false);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Verify byte count.
    if ((size == 0) || (size % SCP_KEY_SIZE != 0))
    {
        status = LWRV_ERR_ILWALID_REQUEST;
        goto exit;
    }

    // Verify destination buffer.
    status = _scpValidateBuffer(destPa, SCP_BLOCK_SIZE, true);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Verify initial hash buffer.
    status = _scpValidateBuffer(initPa, SCP_BLOCK_SIZE, false);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Load initial hash value into register-index one.
    status = _scpLoadBuffer(initPa, SCP_REGISTER_INDEX_1);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Load sequencer zero with the instructions for DmHash and update Ku.
    status = _scpLoadHashSequence();
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Run the input data through the sequence we loaded above.
    status = _scpRunSequence(sourcePa, (uintptr_t)NULL, size);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Read the final hash value out of register-index one.
    status = _scpStoreBuffer(SCP_REGISTER_INDEX_1, destPa);
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

/*!
 * @brief Computes the CMAC of an arbitrary buffer of data.
 *
 * @param[in]  pKeyDesc     A pointer to the key-descriptor to be used for the
 *                          CMAC computation. Applications should acquire this
 *                          parameter through one of the provided SCP_KEY_*
 *                          macros.
 *
 * @param[in]  cmacMode     The CMAC mode of operation to use. See the
 *                          documentation for SCP_CMAC_MODE for a list of
 *                          supported values.
 *
 * @param[in]  sourcePa     The physical address of the input buffer for which
 *                          the CMAC will be computed. Must be aligned to
 *                          SCP_BUFFER_ALIGNMENT and contain at least size
 *                          bytes. Supports IMEM and DMEM locations only.
 *
 * @param[in]  size         The number of bytes to process from sourcePa. Must
 *                          be a nonzero multiple of SCP_BLOCK_SIZE.
 *
 * @param[out] destPa       The physical address of the output buffer to which
 *                          the computed CMAC value is to be written. Must be
 *                          aligned to SCP_BUFFER_ALIGNMENT and have at least
 *                          SCP_BLOCK_SIZE bytes of capacity. Supports IMEM,
 *                          DMEM, and external locations.
 *
 * @param[in]  contextPa    The physical address of the input buffer containing
 *                          the result of a prior partial CMAC computation, if
 *                          applicable. Must be aligned to SCP_BUFFER_ALIGNMENT
 *                          and contain at least SCP_BLOCK_SIZE bytes. Should
 *                          be zero (NULL) or should point to an all-zero
 *                          buffer if not continuing a prior computation.
 *                          Permitted to alias to destPa. Supports IMEM and
 *                          DMEM locations only.
 *
 * @return LWRV_
 *    OK                            if CMAC was computed successfully.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_OBJECT            if pKeyDesc does not point to a valid key-
 *                                  descriptor.
 *
 *    ERR_ILWALID_OBJECT_BUFFER     if pKeyDesc was constructed with a zero
 *                                  (NULL) or incorrectly-aligned address, or a
 *                                  pointer to an unsupported memory location
 *                                  (e.g. EMEM), or a buffer that overflows the
 *                                  memory region in which it resides.
 *
 *    ERR_ILWALID_INDEX             if pKeyDesc was constructed with an invalid
 *                                  register or hardware-secret index, or
 *                                  references secret-index zero.
 *
 *    ERR_INSUFFICIENT_PERMISSIONS  if pKeyDesc was constructed with a
 *                                  hardware-secret index that the application
 *                                  does not have permission to use.
 *
 *    ERR_ILWALID_POINTER           if one or more of pKeyDesc, sourcePa, or
 *                                  destPa is zero/NULL.
 *
 *    ERR_ILWALID_ADDRESS           if one or more of sourcePa, destPa, or
 *                                  contextPa is incorrectly aligned, or the
 *                                  buffers referenced by sourcePa and
 *                                  contextPa overlap.
 *
 *    ERR_ILWALID_BASE              if one or more of sourcePa, destPa, or
 *                                  contextPa points to an unsupported memory
 *                                  region (e.g. EMEM).
 *
 *    ERR_ILWALID_REQUEST           if size is zero or is not a multiple of
 *                                  SCP_BLOCK_SIZE.
 *
 *    ERR_ILWALID_OPERATION         if cmacMode is not a valid SCP_CMAC_MODE
 *                                  value.
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
 *                                  sourcePa, destPa, or contextPa overflows
 *                                  the memory region in which it resides.
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
 * Computes the CMAC of the first size bytes of data from sourcePa using the
 * key described by pKeyDesc and writes the result to destPa. Uses an AES-128
 * block cipher for the operation. If provided, SCP_BLOCK_SIZE bytes from
 * contextPa are used as the initial contextual input (IV) for the computation.
 *
 * When cmacMode is SCP_CMAC_MODE_PARTIAL, scpCmac() skips the final step of
 * the CMAC computation, thus allowing the result to be chained into a future
 * call via contextPa. The full CMAC result can then be obtained by passing
 * SCP_CMAC_MODE_COMPLETE, as is done for singular, unchained calls.
 *
 * Notice that it is the application's responsibility to pad the source message
 * if needed (by appending 0b10...0 per the CMAC specification) and to signal
 * that padding has oclwrred by setting cmacMode to SCP_CMAC_MODE_PADDED in
 * place of SCP_CMAC_MODE_COMPLETE.
 *
 * Should pKeyDesc describe a derived key, the scpCmac() function will perform
 * the indicated key-derivation operation automatically. See the relevant
 * SCP_KEY_* macros for details.
 *
 * Note that it is permissible for destPa and contextPa to reference the same
 * memory location (e.g. for easy chaining of partial computations - see
 * above).
 *
 * In the event that destPa refers to an external memory location, the
 * application must ensure that it has first called scpConfigureShortlwt() (or
 * used the SCP_EXTERNAL_DEST() helper macro) to configure the shortlwt-DMA
 * path accordingly.
 */
LWRV_STATUS
scpCmac
(
    const SCP_KEY_DESC *pKeyDesc,
    SCP_CMAC_MODE cmacMode,
    uintptr_t sourcePa,
    size_t size,
    uintptr_t destPa,
    uintptr_t contextPa
)
{
    LWRV_STATUS status = LWRV_OK;
    size_t batchSize = size;
    bool bPadded = false;

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

    // Verify source buffer.
    status = _scpValidateBuffer(sourcePa, size, false);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Verify byte count.
    if ((size == 0) || (size % SCP_BLOCK_SIZE != 0))
    {
        status = LWRV_ERR_ILWALID_REQUEST;
        goto exit;
    }

    // Verify destination buffer.
    status = _scpValidateBuffer(destPa, SCP_BLOCK_SIZE, true);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Verify context buffer, if provided.
    if (contextPa != (uintptr_t)NULL)
    {
        status = _scpValidateBuffer(contextPa, SCP_BLOCK_SIZE, false);
        if (status != LWRV_OK)
        {
            goto exit;
        }

        // Check for overlap with source buffer.
        if (memutils_mem_has_overlap(sourcePa, size, contextPa, SCP_BLOCK_SIZE))
        {
            status = LWRV_ERR_ILWALID_ADDRESS;
            goto exit;
        }
    }

    // Verify mode of operation.
    switch (cmacMode)
    {
        case SCP_CMAC_MODE_PADDED:
            bPadded = true;
            // Fall-through.

        case SCP_CMAC_MODE_COMPLETE:
            //
            // The final block in non-partial messages requires special
            // handling, so exclude it from the inital batch.
            //
            batchSize -= SCP_BLOCK_SIZE;
            break;

        case SCP_CMAC_MODE_PARTIAL:
            // Nothing to do.
            break;

        default:
            // Not a recognized operating mode.
            status = LWRV_ERR_ILWALID_OPERATION;
            goto exit;
    }

    // Verify key-descriptor, load key into register-index zero, and update Ku.
    status = _scpLoadKey(pKeyDesc);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Load initial context information if provided.
    if (contextPa != (uintptr_t)NULL)
    {
        status = _scpLoadBuffer(contextPa, SCP_REGISTER_INDEX_1);
        if (status != LWRV_OK)
        {
            goto exit;
        }
    }

    // Otherwise, use zero.
    else
    {
        status = _scpClearReg(SCP_REGISTER_INDEX_1);
        if (status != LWRV_OK)
        {
            goto exit;
        }
    }

    // Load sequencer zero with the instructions for CMAC.
    status = _scpLoadCmacSequence();
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Run the input data through the sequence we loaded above.
    status = _scpRunSequence(sourcePa, (uintptr_t)NULL, batchSize);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Handle the final block if needed.
    if (batchSize != size)
    {
        status = _scpCompleteCmac((sourcePa + batchSize), bPadded);
        if (status != LWRV_OK)
        {
            goto exit;
        }
    }

    // Read the final CMAC value out of register-index one.
    status = _scpStoreBuffer(SCP_REGISTER_INDEX_1, destPa);
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
 * @brief Encrypts or decrypts an arbitary buffer of data using AES-128.
 *
 * @param[in]  pKeyDesc     A pointer to the key-descriptor to be used for the
 *                          encryption or decryption operation. Applications
 *                          should acquire this parameter through one of the
 *                          provided SCP_KEY_* macros.
 *
 * @param[in]  cipherMode   The block-cipher mode of operation to use.
 *
 * @param[in]  sourcePa     The physical address of the input buffer to be
 *                          encrypted or decrypted. Must be aligned to
 *                          SCP_BUFFER_ALIGNMENT and contain at least size
 *                          bytes. Supports IMEM and DMEM locations only.
 *
 * @param[out] destPa       The physical address of the output buffer to which
 *                          the encrypted or decrypted data is to be written.
 *                          Must be aligned to SCP_BUFFER_ALIGNMENT and have at
 *                          least size bytes of capacity. Permitted to overlap
 *                          with sourcePa only if exactly equal. Supports IMEM,
 *                          DMEM, and external locations.
 *
 * @param[in]  size         The number of bytes to process from sourcePa. Must
 *                          be a nonzero multiple of SCP_BLOCK_SIZE.
 *
 * @param[in]  contextPa    The physical address of the input buffer containing
 *                          the initialization vector (CBC) or initial counter
 *                          value with nonce (CTR) to be used for the
 *                          encryption or decryption operation. Must be aligned
 *                          to SCP_BUFFER_ALIGNMENT and contain at least
 *                          SCP_BLOCK_SIZE bytes. Required to be zero (NULL)
 *                          for ECB mode. Supports IMEM and DMEM locations
 *                          only.
 *
 * @param[in]  bDecrypt     A boolean value indicating whether to decrypt the
 *                          data from sourcePa rather than encrypting it.
 *
 * @return LWRV_
 *    OK                            if data was encrypted or decrypted
 *                                  successfully.
 *
 *    ERR_NOT_READY                 if the SCP driver has not been initialized.
 *
 *    ERR_ILWALID_OBJECT            if pKeyDesc does not point to a valid key-
 *                                  descriptor.
 *
 *    ERR_ILWALID_OBJECT_BUFFER     if pKeyDesc was constructed with a zero
 *                                  (NULL) or incorrectly-aligned address, or a
 *                                  pointer to an unsupported memory location
 *                                  (e.g. EMEM), or a buffer that overflows the
 *                                  memory region in which it resides.
 *
 *    ERR_ILWALID_INDEX             if pKeyDesc was constructed with an invalid
 *                                  register or hardware-secret index, or
 *                                  references secret-index zero.
 *
 *    ERR_INSUFFICIENT_PERMISSIONS  if pKeyDesc was constructed with a
 *                                  hardware-secret index that the application
 *                                  does not have permission to use.
 *
 *    ERR_ILWALID_POINTER           if one or more of pKeyDesc, sourcePa, or
 *                                  destPa is zero/NULL, or contextPa is zero
 *                                  (NULL) and cipherMode is not
 *                                  SCP_CIPHER_MODE_ECB.
 *
 *    ERR_ILWALID_ADDRESS           if one or more of sourcePa, destPa, or
 *                                  contextPa is incorrectly aligned, or the
 *                                  buffers referenced by sourcePa and destPa
 *                                  overlap without being equal.
 *
 *    ERR_ILWALID_BASE              if one or more of sourcePa, destPa, or
 *                                  contextPa points to an unsupported memory
 *                                  region (e.g. EMEM).
 *
 *    ERR_ILWALID_REQUEST           if size is zero or is not a multiple of
 *                                  SCP_BLOCK_SIZE.
 *
 *    ERR_ILWALID_OPERATION         if cipherMode is not a valid
 *                                  SCP_CIPHER_MODE value, or cipherMode is
 *                                  SCP_CIPHER_MODE_ECB and contextPa is
 *                                  nonzero (non-NULL).
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
 *                                  sourcePa, destPa, or contextPa overflows
 *                                  the memory region in which it resides.
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
 * Encrypts or decrypts the first size bytes of data from sourcePa using the
 * key described by pKeyDesc and writes the result to destPa. Uses an AES-128
 * block cipher operating in the mode indicated by cipherMode as the encryption
 * or decryption algorithm. If applicable, SCP_BLOCK_SIZE bytes from contextPa
 * are used as the initial contextual input to the cipher. Performs a
 * decryption operation if bDecrypt is true and an encryption operation
 * otherwise.
 *
 * Should pKeyDesc describe a derived key, the _scpCrypt() function will
 * perform the indicated key-derivation operation automatically. See the
 * relevant SCP_KEY_* macros for details.
 *
 * Note that it is permissible for sourcePa and destPa to reference the same
 * memory locations (i.e. to facilitate encrypting or decrypting data in-
 * place). However, the use of buffers that only partially overlap is
 * prohibited and will result in an error.
 *
 * In the event that destPa refers to an external memory location, the
 * application must ensure that it has first called scpConfigureShortlwt() (or
 * used the SCP_EXTERNAL_DEST() helper macro) to configure the shortlwt-DMA
 * path accordingly.
 */
static LWRV_STATUS
_scpCrypt
(
    const SCP_KEY_DESC *pKeyDesc,
    SCP_CIPHER_MODE cipherMode,
    uintptr_t sourcePa,
    uintptr_t destPa,
    size_t size,
    uintptr_t contextPa,
    bool bDecrypt
)
{
    LWRV_STATUS clearStatus, status = LWRV_OK;

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

    // Verify source buffer.
    status = _scpValidateBuffer(sourcePa, size, false);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Verify destination buffer.
    status = _scpValidateBuffer(destPa, size, true);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Check for partial overlap (exact aliasing is fine).
    if ((sourcePa != destPa) && memutils_mem_has_overlap(sourcePa, size, destPa, size))
    {
        status = LWRV_ERR_ILWALID_ADDRESS;
        goto exit;
    }

    // Verify byte count.
    if ((size == 0) || (size % SCP_BLOCK_SIZE != 0))
    {
        status = LWRV_ERR_ILWALID_REQUEST;
        goto exit;
    }

    // Verify mode of operation.
    switch (cipherMode)
    {
        case SCP_CIPHER_MODE_ECB:
            // No context in ECB mode.
            if (contextPa != (uintptr_t)NULL)
            {
                status = LWRV_ERR_ILWALID_OPERATION;
                goto exit;
            }
            break;

        case SCP_CIPHER_MODE_CBC:
        case SCP_CIPHER_MODE_CTR:
            // Verify context buffer for other modes.
            status = _scpValidateBuffer(contextPa, SCP_BLOCK_SIZE, false);
            if (status != LWRV_OK)
            {
                goto exit;
            }
            break;

        default:
            // Not a recognized block-cipher mode.
            status = LWRV_ERR_ILWALID_OPERATION;
            goto exit;
    }

    // Verify key-descriptor, load key into register-index zero, and update Ku.
    status = _scpLoadKey(pKeyDesc);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Load initial context information (if any) into register-index one.
    if (contextPa != (uintptr_t)NULL)
    {
        status = _scpLoadBuffer(contextPa, SCP_REGISTER_INDEX_1);
        if (status != LWRV_OK)
        {
            goto exit;
        }
    }

    // Load sequencer zero with the instructions for the selected cipher mode.
    status = _scpLoadCryptSequence(cipherMode, bDecrypt);
    if (status != LWRV_OK)
    {
        goto exit;
    }

    // Run the input data through the sequence we loaded above.
    status = _scpRunSequence(sourcePa, destPa, size);
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
    clearStatus = _scpClearAll();
    if (status == LWRV_OK)
    {
        status = clearStatus;
    }

    // Done.
    return status;
}

/*!
 * @brief Validates a key-descriptor and then loads the key it describes to R0.
 *
 * @param[in]  pKeyDesc     A pointer to the key-descriptor to be processed.
 *
 * @return LWRV_
 *    OK                            if key was loaded successfully.
 *
 *    ERR_ILWALID_OBJECT            if pKeyDesc does not point to a valid key-
 *                                  descriptor.
 *
 *    ERR_ILWALID_OBJECT_BUFFER     if pKeyDesc was constructed with a zero
 *                                  (NULL) or incorrectly-aligned address, or a
 *                                  pointer to an unsupported memory location
 *                                  (e.g. EMEM), or a buffer that overflows the
 *                                  memory region in which it resides.
 *
 *    ERR_ILWALID_INDEX             if pKeyDesc was constructed with an invalid
 *                                  register or hardware-secret index, or
 *                                  references secret-index zero.
 *
 *    ERR_INSUFFICIENT_PERMISSIONS  if pKeyDesc was constructed with a
 *                                  hardware-secret index that the application
 *                                  does not have permission to use.
 *
 *    ERR_ILWALID_POINTER           if pKeyDesc is NULL.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_PROTECTION_FAULT          if an IO-PMP fault was detected.
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Checks the provided key-descriptor for errors and then loads the key it
 * describes into register-index zero. Automatically performs any key-
 * derivation operations as required and updates the key register (Ku) to
 * point to the newly-loaded key.
 *
 * Note that this function uses register-index one for intermediate values.
 */
static LWRV_STATUS
_scpLoadKey
(
    const SCP_KEY_DESC *pKeyDesc
)
{
    LWRV_STATUS status;

    // Check for valid pointer.
    if (pKeyDesc == NULL)
    {
        return LWRV_ERR_ILWALID_POINTER;
    }

    // Load the requested key based on its type.
    switch (pKeyDesc->keyType)
    {
        case SCP_KEY_TYPE_RAW_BUFFER:
            status = _scpLoadBufferKey(pKeyDesc->rawBufferPa);
            break;

        case SCP_KEY_TYPE_RAW_REGISTER:
            status = _scpLoadRegisterKey(pKeyDesc->rawRegisterIndex);
            break;

        case SCP_KEY_TYPE_DERIVED_ECB:
            status = _scpLoadEcbKey(pKeyDesc->ecbSecretIndex,
                                    pKeyDesc->ecbSaltPa);
            break;

        case SCP_KEY_TYPE_DERIVED_CBC:
            status = _scpLoadCbcKey(pKeyDesc->cbcSecretIndex,
                                    pKeyDesc->cbcSaltPa,
                                    pKeyDesc->cbcIvPa);
            break;

        default:
            // Not a recognized key type.
            return LWRV_ERR_ILWALID_OBJECT;
    }

    // Report any errors to the caller.
    if (status != LWRV_OK)
    {
        return status;
    }

    //
    // Restrict the key's associated ACL to the bare minimum required for
    // encryption and decryption operations.
    //
    if (_scpIsSelwre())
    {
        scp_chmod(SCP_REGISTER_ACL_KEY, SCP_REGISTER_INDEX_0);
    }

    // Update the key register to point to the newly-loaded key.
    scp_key(SCP_REGISTER_INDEX_0);

    // Wait for the above operations to complete.
    scp_wait_dma();

    // Check for any unexpected failures.
    if (scp_check_dma(true) != LWRV_OK)
    {
        return LWRV_ERR_GENERIC;
    }

    // Key was loaded successfully.
    return LWRV_OK;
}

/*!
 * @brief Loads a raw key from the provided buffer into R0.
 *
 * @param[in]  keyPa    The physical address of the input buffer containing the
 *                      raw key to be loaded.
 *
 * @return LWRV_
 *    OK                            if key was loaded successfully.
 *
 *    ERR_ILWALID_OBJECT_BUFFER     if keyPa is zero (NULL) or incorrectly-
 *                                  aligned, or points to an unsupported memory
 *                                  location (e.g. EMEM), or references a
 *                                  buffer that overflows the memory region in
 *                                  which it resides.
 *
 *    ERR_PROTECTION_FAULT          if an IO-PMP fault was detected.
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Validates the buffer referenced by keyPa and then loads the first
 * SCP_KEY_SIZE bytes from it into register-index zero.
 */
static LWRV_STATUS
_scpLoadBufferKey
(
    uintptr_t keyPa
)
{
    // Validate the source buffer.
    if(_scpValidateBuffer(keyPa, SCP_KEY_SIZE, false) != LWRV_OK)
    {
        return LWRV_ERR_ILWALID_OBJECT_BUFFER;
    }

    // Load into register-index zero.
    return _scpLoadBuffer(keyPa, SCP_REGISTER_INDEX_0);
}

/*!
 * @brief Loads a raw key from the indicated SCP GPR into R0.
 *
 * @param[in]  regIndex     The index of the SCP register containing the raw
 *                          key to be loaded.
 *
 * @return LWRV_
 *    OK                            if key was loaded successfully.
 *
 *    ERR_ILWALID_INDEX             if regIndex is not a valid
 *                                  SCP_REGISTER_INDEX value.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Copies the contents of the register specified by regIndex into register-
 * index zero. Clears the source register after copying.
 */
static LWRV_STATUS
_scpLoadRegisterKey
(
    SCP_REGISTER_INDEX regIndex
)
{
    // Validate the register index.
    if (regIndex > SCP_REGISTER_INDEX_7)
    {
        return LWRV_ERR_ILWALID_INDEX;
    }

    // No-op if already in R0.
    if (regIndex == SCP_REGISTER_INDEX_0)
    {
        return LWRV_OK;
    }

    // Copy into register-index zero.
    scp_mov(regIndex, SCP_REGISTER_INDEX_0);
    scp_wait_dma();

    // Check for any unexpected failures.
    if (scp_check_dma(true) != LWRV_OK)
    {
        return LWRV_ERR_GENERIC;
    }

    // Clear the source register.
    return _scpClearReg(regIndex);
}

/*!
 * @brief Loads a key derived via AES-ECB of a provided salt value into R0.
 *
 * @param[in]  secretIndex  The index of the hardware-secret to be used in the
 *                          key-derivation process.
 *
 * @param[in]  saltPa       The physical address of the input buffer containing
 *                          the salt value to be used in the key-derivation
 *                          process.
 *
 * @return LWRV_
 *    OK                            if key was loaded successfully.
 *
 *    ERR_ILWALID_OBJECT_BUFFER     if saltPa is zero (NULL) or incorrectly-
 *                                  aligned, or points to an unsupported memory
 *                                  location (e.g. EMEM), or references a
 *                                  buffer that overflows the memory region in
 *                                  which it resides.
 *
 *    ERR_ILWALID_INDEX             if secretIndex is not a valid SCP secret-
 *                                  index, or references secret-index zero.
 *
 *    ERR_INSUFFICIENT_PERMISSIONS  if secretIndex references a hardware-secret
 *                                  that the application does not have
 *                                  permission to use.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_PROTECTION_FAULT          if an IO-PMP fault was detected.
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Performs AES-ECB encryption on the first SCP_BLOCK_SIZE bytes from saltPa,
 * using the hardware-secret indicated by secretIndex as the key, and stores
 * the result in register-index zero.
 *
 * Note that this function uses register-index one for intermediate values.
 */
static LWRV_STATUS
_scpLoadEcbKey
(
    uint8_t secretIndex,
    uintptr_t saltPa
)
{
    LWRV_STATUS status;

    // Verify context (hardware secrets cannot be used in inselwre contexts).
    if (!_scpIsSelwre())
    {
        return LWRV_ERR_INSUFFICIENT_PERMISSIONS;
    }

    // Validate the secret index.
    // TODO(COREUCODES-1846): Re-enable this check for Blackwell.
    if (/*(secretIndex <= SCP_SECRET_INDEX_CLEAR) ||*/
        (secretIndex >= SCP_SECRET_COUNT))
    {
        return LWRV_ERR_ILWALID_INDEX;
    }

    // Validate the salt buffer.
    if(_scpValidateBuffer(saltPa, SCP_BLOCK_SIZE, false) != LWRV_OK)
    {
        return LWRV_ERR_ILWALID_OBJECT_BUFFER;
    }

    // Load the salt value into a temporary register.
    scp_load(saltPa, SCP_REGISTER_INDEX_1);
    scp_wait_dma();

    // Load the hardware secret into the destination register.
    scp_secret(secretIndex, SCP_REGISTER_INDEX_0);

    // Encrypt the salt with the hardware secret.
    scp_key(SCP_REGISTER_INDEX_0);
    scp_encrypt(SCP_REGISTER_INDEX_1, SCP_REGISTER_INDEX_0);

    // Wait for completion.
    scp_wait_dma();

    // Check for any failures.
    status = scp_check_dma(true);
    if ((status == LWRV_ERR_INSUFFICIENT_PERMISSIONS) ||
        (status == LWRV_ERR_PROTECTION_FAULT))
    {
        // Bubble up permissions and IO-PMP errors.
        return status;
    }
    else if (status != LWRV_OK)
    {
        // No other errors are expected.
        return LWRV_ERR_GENERIC;
    }

    // Clear the temporary register.
    return _scpClearReg(SCP_REGISTER_INDEX_1);
}

/*!
 * @brief Loads a key derived via AES-CBC of a provided salt value into R0.
 *
 * @param[in]  secretIndex  The index of the hardware-secret to be used in the
 *                          key-derivation process.
 *
 * @param[in]  saltPa       The physical address of the input buffer containing
 *                          the salt value to be used in the key-derivation
 *                          process.
 *
 * @param[in]  ivPa         The physical address of the input buffer containing
 *                          the initialization vector to be used in the key-
 *                          derivation process.
 *
 * @return LWRV_
 *    OK                            if key was loaded successfully.
 *
 *    ERR_ILWALID_OBJECT_BUFFER     if one or more of saltPa or ivPa is zero
 *                                  (NULL) or incorrectly-aligned, or points to
 *                                  an unsupported memory location (e.g. EMEM),
 *                                  or references a buffer that overflows the
 *                                  memory region in which it resides.
 *
 *    ERR_ILWALID_INDEX             if secretIndex is not a valid SCP secret-
 *                                  index, or references secret-index zero.
 *
 *    ERR_INSUFFICIENT_PERMISSIONS  if secretIndex references a hardware-secret
 *                                  that the application does not have
 *                                  permission to use.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_PROTECTION_FAULT          if an IO-PMP fault was detected.
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Performs AES-CBC encryption on the first SCP_BLOCK_SIZE bytes from saltPa,
 * using the hardware-secret indicated by secretIndex as the key and the first
 * SCP_BLOCK_SIZE bytes from ivPa as the initialization vector, and stores
 * the result in register-index zero.
 *
 * Note that this function uses register-index one for intermediate values.
 */
static LWRV_STATUS
_scpLoadCbcKey
(
    uint8_t secretIndex,
    uintptr_t saltPa,
    uintptr_t ivPa
)
{
    LWRV_STATUS status;

    // Verify context (hardware secrets cannot be used in inselwre contexts).
    if (!_scpIsSelwre())
    {
        return LWRV_ERR_INSUFFICIENT_PERMISSIONS;
    }

    // Validate the secret index.
    // TODO(COREUCODES-1846): Re-enable this check for Blackwell.
    if (/*(secretIndex <= SCP_SECRET_INDEX_CLEAR) ||*/
        (secretIndex >= SCP_SECRET_COUNT))
    {
        return LWRV_ERR_ILWALID_INDEX;
    }

    // Validate the salt buffer.
    if(_scpValidateBuffer(saltPa, SCP_BLOCK_SIZE, false) != LWRV_OK)
    {
        return LWRV_ERR_ILWALID_OBJECT_BUFFER;
    }

    // Validate the IV buffer.
    if (_scpValidateBuffer(ivPa, SCP_BLOCK_SIZE, false) != LWRV_OK)
    {
        return LWRV_ERR_ILWALID_OBJECT_BUFFER;
    }

    // Load the IV and salt values into registers.
    scp_load(ivPa, SCP_REGISTER_INDEX_0);
    scp_load(saltPa, SCP_REGISTER_INDEX_1);
    scp_wait_dma();

    // XOR the IV and salt as the first step of CBC encryption.
    scp_xor(SCP_REGISTER_INDEX_0, SCP_REGISTER_INDEX_1);

    // Load the hardware secret into the destination register.
    scp_secret(secretIndex, SCP_REGISTER_INDEX_0);

    // Encrypt the XORed salt with the hardware secret.
    scp_key(SCP_REGISTER_INDEX_0);
    scp_encrypt(SCP_REGISTER_INDEX_1, SCP_REGISTER_INDEX_0);

    // Wait for completion.
    scp_wait_dma();

    // Check for any failures.
    status = scp_check_dma(true);
    if ((status == LWRV_ERR_INSUFFICIENT_PERMISSIONS) ||
        (status == LWRV_ERR_PROTECTION_FAULT))
    {
        // Bubble up permissions and IO-PMP errors.
        return status;
    }
    else if (status != LWRV_OK)
    {
        // No other errors are expected.
        return LWRV_ERR_GENERIC;
    }

    // Clear the temporary register.
    return _scpClearReg(SCP_REGISTER_INDEX_1);
}

/*!
 * @brief Loads the requested cryptographic algorithm into sequencer zero.
 *
 * @param[in]  cipherMode   The block-cipher algorithm to load.
 *
 * @param[in]  bDecrypt     A boolean value indicating whether to load the
 *                          decryption sequence for the selected algorithm
 *                          rather than the encryption sequence.
 *
 * @return LWRV_
 *    OK                            if algorithm was loaded successfully.
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Loads sequencer zero with the set of SCP instructions corresponding to a
 * single iteration of the requested block-cipher algorithm. Uses the
 * decryption form if bDecrypt is true and the encryption form otherwise.
 *
 * The loaded algorithm assumes that key material has already been loaded to
 * register-index zero and has also been selected via the scp_key()
 * instruction. It further assumes that any context information has been loaded
 * to register-index one and that register-indices two and three are available
 * for use as intermediate storage.
 */
static LWRV_STATUS
_scpLoadCryptSequence
(
    SCP_CIPHER_MODE cipherMode,
    bool bDecrypt
)
{
    // Load the correct sequence for the requested block-cipher mode.
    switch (cipherMode)
    {
        case SCP_CIPHER_MODE_ECB:
            return _scpLoadEcbSequence(bDecrypt);

        case SCP_CIPHER_MODE_CBC:
            return _scpLoadCbcSequence(bDecrypt);

        case SCP_CIPHER_MODE_CTR:
            return _scpLoadCtrSequence();

        default:
            //
            // Not a recognized block-cipher mode. This should have been
            // validated already by the caller so treat as unexpected.
            //
            return LWRV_ERR_GENERIC;
    }
}

/*!
 * @brief Loads the SCP instructions for AES-ECB into sequencer zero.
 *
 * @param[in]  bDecrypt     A boolean value indicating whether to load the
 *                          decryption sequence rather than the encryption
 *                          sequence.
 *
 * @return LWRV_
 *    OK                            if instructions were loaded successfully.
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Loads sequencer zero with the set of SCP instructions corresponding to a
 * single iteration of AES-ECB encryption/decryption. Uses the decryption
 * algorithm if bDecrypt is true and the encryption algorithm otherwise.
 *
 * The loaded algorithm assumes that key material has already been loaded to
 * register-index zero and has also been selected via the scp_key()
 * instruction (but not yet colwerted via scp_rkey10(), if applicable). It
 * further assumes that register-index two is available for use as intermediate
 * storage.
 */
static LWRV_STATUS
_scpLoadEcbSequence
(
    bool bDecrypt
)
{
    // Prepare to write three instructions to sequencer zero.
    scp_load_trace0(3);
    {
        // Load the next block of plaintext/ciphertext to process.
        scp_push(SCP_REGISTER_INDEX_2);

        if (bDecrypt)
        {
            // Decrypt the ciphertext in-place.
            scp_decrypt(SCP_REGISTER_INDEX_2, SCP_REGISTER_INDEX_2);
        }
        else
        {
            // Encrypt the plaintext in-place.
            scp_encrypt(SCP_REGISTER_INDEX_2, SCP_REGISTER_INDEX_2);
        }

        // Store the resulting ciphertext/plaintext.
        scp_fetch(SCP_REGISTER_INDEX_2);
    }

    //
    // Colwert the key loaded in register-index zero to the form required for
    // decryption operations, if applicable. This only needs to be done once,
    // hence its exclusion from the above sequencer load.
    //
    if (bDecrypt)
    {
        scp_rkey10(SCP_REGISTER_INDEX_0, SCP_REGISTER_INDEX_0);
    }

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

/*!
 * @brief Loads the SCP instructions for AES-CBC into sequencer zero.
 *
 * @param[in]  bDecrypt     A boolean value indicating whether to load the
 *                          decryption sequence rather than the encryption
 *                          sequence.
 *
 * @return LWRV_
 *    OK                            if instructions were loaded successfully.
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Loads sequencer zero with the set of SCP instructions corresponding to a
 * single iteration of AES-CBC encryption/decryption. Uses the decryption
 * algorithm if bDecrypt is true and the encryption algorithm otherwise.
 *
 * The loaded algorithm assumes that key material has already been loaded to
 * register-index zero and has also been selected via the scp_key()
 * instruction (but not yet colwerted via scp_rkey10(), if applicable). It
 * further assumes that the initialization vector has been loaded to register-
 * index one and that register-indices two and three are available for use as
 * intermediate storage.
 */
static LWRV_STATUS
_scpLoadCbcSequence
(
    bool bDecrypt
)
{
    // Split because decryption requires an extra instruction over encryption.
    if (bDecrypt)
    {
        // Prepare to write five instructions to sequencer zero.
        scp_load_trace0(5);
        {
            // Load the next block of ciphertext to process.
            scp_push(SCP_REGISTER_INDEX_2);

            // Decrypt non-destructively.
            scp_decrypt(SCP_REGISTER_INDEX_2, SCP_REGISTER_INDEX_3);

            // XOR with the current "IV" to obtain the plaintext.
            scp_xor(SCP_REGISTER_INDEX_1, SCP_REGISTER_INDEX_3);

            // Carry the original ciphertext forward as the new "IV".
            scp_mov(SCP_REGISTER_INDEX_2, SCP_REGISTER_INDEX_1);

            // Store the resulting plaintext.
            scp_fetch(SCP_REGISTER_INDEX_3);
        }

        //
        // Colwert the key loaded in register-index zero to the form required for
        // decryption operations. This only needs to be done once, hence its
        // its exclusion from the above sequencer load.
        //
        scp_rkey10(SCP_REGISTER_INDEX_0, SCP_REGISTER_INDEX_0);
    }
    else
    {
        // Prepare to write four instructions to sequencer zero.
        scp_load_trace0(4);
        {
            // Load the next block of plaintext to process.
            scp_push(SCP_REGISTER_INDEX_2);

            // XOR with the current "IV".
            scp_xor(SCP_REGISTER_INDEX_1, SCP_REGISTER_INDEX_2);

            // Encrypt to obtain the ciphertext and save as the new "IV".
            scp_encrypt(SCP_REGISTER_INDEX_2, SCP_REGISTER_INDEX_1);

            // Store the resulting ciphertext.
            scp_fetch(SCP_REGISTER_INDEX_1);
        }
    }

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

/*!
 * @brief Loads the SCP instructions for AES-CTR into sequencer zero.
 *
 * @return LWRV_
 *    OK                            if instructions were loaded successfully.
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Loads sequencer zero with the set of SCP instructions corresponding to a
 * single iteration of AES-CTR encryption/decryption.
 *
 * The loaded algorithm assumes that key material has already been loaded to
 * register-index zero and has also been selected via the scp_key()
 * instruction. It further assumes that the initial counter value (with nonce)
 * has been loaded to register-index one and that register-indices two and
 * three are available for use as intermediate storage.
 */
static LWRV_STATUS
_scpLoadCtrSequence(void)
{
    // Prepare to write five instructions to sequencer zero.
    scp_load_trace0(5);
    {
        // Load the next block of plaintext/ciphertext to process.
        scp_push(SCP_REGISTER_INDEX_3);

        // Encrypt the current counter value (with nonce) non-destructively.
        scp_encrypt(SCP_REGISTER_INDEX_1, SCP_REGISTER_INDEX_2);

        // XOR with the current block to obtain the ciphertext/plaintext.
        scp_xor(SCP_REGISTER_INDEX_2, SCP_REGISTER_INDEX_3);

        // Update the current counter value.
        scp_add(1, SCP_REGISTER_INDEX_1);

        // Store the resulting ciphertext/plaintext.
        scp_fetch(SCP_REGISTER_INDEX_3);
    }

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

/*!
 * @brief Loads the SCP instructions for DmHash into sequencer zero.
 *
 * @return LWRV_
 *    OK                            if instructions were loaded successfully.
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Loads sequencer zero with the set of SCP instructions required to compute
 * the Davies-Meyer hash of a single block of data. Also updates the key
 * register (Ku).
 *
 * The loaded algorithm assumes that the initial hash value (H0) has been
 * loaded to register-index one and that register-indices two and three are
 * available for use as intermediate storage.
 */
static LWRV_STATUS
_scpLoadHashSequence(void)
{
    // Prepare to write three instructions to sequencer zero.
    scp_load_trace0(3);
    {
        // Load the next block of the message to process.
        scp_push(SCP_REGISTER_INDEX_2);

        // Encrypt the previous hash value with the message.
        scp_encrypt(SCP_REGISTER_INDEX_1, SCP_REGISTER_INDEX_3);

        // XOR with the previous hash value to obtain the new hash value.
        scp_xor(SCP_REGISTER_INDEX_3, SCP_REGISTER_INDEX_1);
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

/*!
 * @brief Loads the SCP instructions for CMAC into sequencer zero.
 *
 * @return LWRV_
 *    OK                            if instructions were loaded successfully.
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Loads sequencer zero with the set of SCP instructions required to compute
 * the CMAC of a single intermediate block of data.
 *
 * The loaded algorithm assumes that key material has already been loaded to
 * register-index zero and has also been selected via the scp_key()
 * instruction. It further assumes that the initialization vector, if any, has
 * been loaded to register-index one and that register-index two is available
 * for use as intermediate storage.
 */
static LWRV_STATUS
_scpLoadCmacSequence(void)
{
    // Prepare to write three instructions to sequencer zero.
    scp_load_trace0(3);
    {
        // Load the next block of the message to process.
        scp_push(SCP_REGISTER_INDEX_2);

        // XOR with the current MAC value (or IV).
        scp_xor(SCP_REGISTER_INDEX_1, SCP_REGISTER_INDEX_2);

        // Encrypt to obtain the new MAC value.
        scp_encrypt(SCP_REGISTER_INDEX_2, SCP_REGISTER_INDEX_1);
    }

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

/*!
 * @brief Completes a CMAC computation by handling a message's final block.
 *
 * @param[in]  finalPa      The physical address of the final block of the
 *                          input message. Must be aligned to and contain at
 *                          least SCP_BLOCK_SIZE bytes. Supports IMEM and DMEM
 *                          locations only.
 *
 * @param[in]  bPadded      A boolean value indicating whether the input
 *                          message has been padded per the CMAC specification.
 *
 * @return LWRV_
 *    OK                            if CMAC was completed successfully.
 *
 *    ERR_INSUFFICIENT_RESOURCES    if the application lacks access to secret-
 *                                  index zero (required in secure contexts).
 *
 *    ERR_PROTECTION_FAULT          if an IO-PMP fault was detected.
 *
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 * Generates the correct CMAC subkey per the value of bPadded and uses it to
 * tweak the first SCP_BLOCK_SIZE bytes from finalPa before computing the final
 * CMAC result from it and the intermediate value in SCP register-index one.
 *
 * Assumes that key material has already been loaded to register-index zero and
 * has also been selected via the scp_key() instruction. Further assumes that
 * register-indices two and three are available for use as intermediate storage.
 *
 * Note that the caller is responsible for validating finalPa before passing it
 * to this function, and is also responsible for clearing the intermediate
 * registers when finished.
 */
static LWRV_STATUS
_scpCompleteCmac
(
    uintptr_t finalPa,
    bool bPadded
)
{
    LWRV_STATUS status;

    // Load the final message block.
    status = _scpLoadBuffer(finalPa, SCP_REGISTER_INDEX_2);
    if (status != LWRV_OK)
    {
        return status;
    }

    // Ensure that register-index three is zero.
    status = _scpClearReg(SCP_REGISTER_INDEX_3);
    if (status != LWRV_OK)
    {
        return status;
    }

    // Generate the first CMAC subkey.
    scp_encrypt(SCP_REGISTER_INDEX_3, SCP_REGISTER_INDEX_3);
    scp_cmac_sk(SCP_REGISTER_INDEX_3, SCP_REGISTER_INDEX_3);

    // Generate the second subkey if needed.
    if (bPadded)
    {
        scp_cmac_sk(SCP_REGISTER_INDEX_3, SCP_REGISTER_INDEX_3);
    }

    // Tweak the final message block using the relevant subkey.
    scp_xor(SCP_REGISTER_INDEX_3, SCP_REGISTER_INDEX_2);

    // XOR with the current MAC value.
    scp_xor(SCP_REGISTER_INDEX_1, SCP_REGISTER_INDEX_2);

    // Encrypt to obtain the final MAC value.
    scp_encrypt(SCP_REGISTER_INDEX_2, SCP_REGISTER_INDEX_1);

    // Wait for completion.
    scp_wait_dma();

    // Check for any unexpected failures.
    if (scp_check_dma(true) != LWRV_OK)
    {
        return LWRV_ERR_GENERIC;
    }

    // Done.
    return status;
}
