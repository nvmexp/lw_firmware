/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_SCP_CRYPT_H
#define LIBLWRISCV_SCP_CRYPT_H

/*!
 * @file    scp_crypt.h
 * @brief   Cryptographic SCP features.
 *
 * @note    Client applications should include scp.h instead of this file.
 */

// Standard includes.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// LWRISCV includes.
#include <lwriscv/status.h>

// SCP includes.
#include <liblwriscv/scp/scp_common.h>


/*!
 * @brief Constructs a new key-descriptor with automatic storage duration.
 *
 * @param[in] keyPa The physical address of the input buffer containing the
 *                  desired raw key. Must be aligned to SCP_BUFFER_ALIGNMENT
 *                  and contain at least SCP_KEY_SIZE bytes. Supports IMEM and
 *                  DMEM locations only.
 *
 * @return A pointer to the new key-descriptor.
 *
 * When a descriptor returned by this macro is passed to scpEncrypt(),
 * scpDecrypt(), or scpCmac(), it instructs the callee to use the first
 * SCP_KEY_SIZE bytes from keyPa as the encryption key for the requested
 * operation.
 *
 * Note that the contents of keyPa are never copied or modified and must remain
 * valid for the lifetime of the returned key-descriptor.
 */
#define SCP_KEY_RAW_BUFFER(keyPa) (         \
    (const SCP_KEY_DESC *)&(SCP_KEY_DESC)   \
    {                                       \
        .keyType = SCP_KEY_TYPE_RAW_BUFFER, \
        .rawBufferPa = (uintptr_t)(keyPa),  \
    }                                       \
)

/*!
 * @brief Constructs a new key-descriptor with automatic storage duration.
 *
 * @param[in] regIndex  The index of the SCP register containing the desired
 *                      raw key. Must be a valid SCP_REGISTER_INDEX value.
 *
 * @return A pointer to the new key-descriptor.
 *
 * When a descriptor returned by this macro is passed to scpEncrypt(),
 * scpDecrypt(), or scpCmac(), it instructs the callee to use the data located
 * in the register specified by regIndex as the encryption key for the
 * requested operation. This mode is intended to support keys pulled in from
 * KMEM.
 *
 * Note that the contents of the register will be cleared following use of the
 * returned key-descriptor in an encryption, decryption, or CMAC operation.
 * Additionally, the register's access-control list must permit keying of its
 * contents in secure contexts, or full access in inselwre contexts.
 */
#define SCP_KEY_RAW_REGISTER(regIndex) (        \
    (const SCP_KEY_DESC *)&(SCP_KEY_DESC)       \
    {                                           \
        .keyType = SCP_KEY_TYPE_RAW_REGISTER,   \
        .rawRegisterIndex = (regIndex),         \
    }                                           \
)

/*!
 * @brief Constructs a new key-descriptor with automatic storage duration.
 *
 * @param[in] secretIndex   The index of the hardware-secret to be used in the
 *                          key-derivation process. Must be a positive integer
 *                          less than SCP_SECRET_COUNT.
 *
 * @param[in] saltPa        The physical address of the input buffer containing
 *                          the salt value to be used in the key-derivation
 *                          process. Must be aligned to SCP_BUFFER_ALIGNMENT
 *                          and contain at least SCP_BLOCK_SIZE bytes. Supports
 *                          IMEM and DMEM locations only.
 *
 * @return A pointer to the new key-descriptor.
 *
 * When a descriptor returned by this macro is passed to scpEncrypt(),
 * scpDecrypt(), or scpCmac(), it instructs the callee to perform AES-ECB
 * encryption on the first SCP_BLOCK_SIZE bytes from saltPa, using the
 * hardware-secret indicated by secretIndex as the key. The result of this
 * procedure is then used as the encryption key for the requested encryption,
 * decryption, or CMAC operation.
 *
 * Note that the contents of saltPa are never copied or modified and must
 * remain valid for the lifetime of the returned key-descriptor. Additionally,
 * the hardware-secret referred to by secretIndex must have an access-control
 * list that permits keying of its contents for the current operating context
 * (secure or inselwre).
 */
#define SCP_KEY_DERIVED_ECB(secretIndex, saltPa) (  \
    (const SCP_KEY_DESC *)&(SCP_KEY_DESC)           \
    {                                               \
        .keyType = SCP_KEY_TYPE_DERIVED_ECB,        \
        .ecbSecretIndex = (secretIndex),            \
        .ecbSaltPa = (uintptr_t)(saltPa),           \
    }                                               \
)

/*!
 * @brief Constructs a new key-descriptor with automatic storage duration.
 *
 * @param[in] secretIndex   The index of the hardware-secret to be used in the
 *                          key-derivation process. Must be a positive integer
 *                          less than SCP_SECRET_COUNT.
 *
 * @param[in] saltPa        The physical address of the input buffer containing
 *                          the salt value to be used in the key-derivation
 *                          process. Must be aligned to SCP_BUFFER_ALIGNMENT
 *                          and contain at least SCP_BLOCK_SIZE bytes. Supports
 *                          IMEM and DMEM locations only.
 *
 * @param[in] ivPa          The physical address of the input buffer containing
 *                          the initialization vector to be used in the key-
 *                          derivation process. Must be aligned to
 *                          SCP_BUFFER_ALIGNMENT and contain at least
 *                          SCP_BLOCK_SIZE bytes. Supports IMEM and DMEM
 *                          locations only.
 *
 * @return A pointer to the new key-descriptor.
 *
 * When a descriptor returned by this macro is passed to scpEncrypt(),
 * scpDecrypt(), or scpCmac(), it instructs the callee to perform AES-CBC
 * encryption on the first SCP_BLOCK_SIZE bytes from saltPa, using the
 * hardware-secret indicated by secretIndex as the key and the first
 * SCP_BLOCK_SIZE bytes from ivPa as the initialization vector. The result of
 * this procedure is then used as the encryption key for the requested
 * encryption, decryption, or CMAC operation.
 *
 * Note that the contents of saltPa and ivPa are never copied or modified and
 * must remain valid for the lifetime of the returned key-descriptor.
 * Additionally, the hardware-secret referred to by secretIndex must have an
 * access-control list that permits keying of its contents for the current
 * operating context (secure or inselwre).
 */
#define SCP_KEY_DERIVED_CBC(secretIndex, saltPa, ivPa) (    \
    (const SCP_KEY_DESC *)&(SCP_KEY_DESC)                   \
    {                                                       \
        .keyType = SCP_KEY_TYPE_DERIVED_CBC,                \
        .cbcSecretIndex = (secretIndex),                    \
        .cbcSaltPa = (uintptr_t)(saltPa),                   \
        .cbcIvPa = (uintptr_t)(ivPa),                       \
    }                                                       \
)

//
// The size, in bytes, of a single block of data in the context of
// cryptographic operations.
//
#define SCP_BLOCK_SIZE 16

//
// The size, in bytes, of an encryption key in the context of cryptographic
// operations.
//
#define SCP_KEY_SIZE 16

//
// The number of hardware secrets supported by the SCP unit.
//
#define SCP_SECRET_COUNT 64


//
// A collection of supported block-cipher modes for encryption and decryption
// operations.
//
typedef enum SCP_CIPHER_MODE
{
    SCP_CIPHER_MODE_ECB,
    SCP_CIPHER_MODE_CBC,
    SCP_CIPHER_MODE_CTR,
} SCP_CIPHER_MODE;

//
// A collection of supported operating modes for CMAC computations.
//
typedef enum SCP_CMAC_MODE
{
    SCP_CMAC_MODE_COMPLETE,
    SCP_CMAC_MODE_PADDED,
    SCP_CMAC_MODE_PARTIAL,
} SCP_CMAC_MODE;

//
// A structure for communicating key information to applicable cryptographic
// interfaces. Should always be constructed using the provided SCP_KEY_* helper
// macros.
//
typedef struct SCP_KEY_DESC
{
    //
    // The type of key expressed by this key-descriptor. See the corresponding
    // SCP_KEY_* helper macros for more information regarding the different
    // supported key types.
    //
    enum SCP_KEY_TYPE
    {
        SCP_KEY_TYPE_RAW_BUFFER,
        SCP_KEY_TYPE_RAW_REGISTER,

        SCP_KEY_TYPE_DERIVED_ECB,
        SCP_KEY_TYPE_DERIVED_CBC,
    } keyType;

    // Type-specific key information.
    union
    {
        // Information for RAW_BUFFER-type keys.
        struct
        {
            // The physical address of the source buffer containing the key.
            uintptr_t rawBufferPa;
        };

        // Information for RAW_REGISTER-type keys.
        struct
        {
            // The index of the source register containing the key.
            SCP_REGISTER_INDEX rawRegisterIndex;
        };

        // Information for DERIVED_ECB-type keys.
        struct
        {
            // The hardware secret to be used in the key-derivation process.
            uint8_t ecbSecretIndex;

            // The physical address of the salt value to derive from.
            uintptr_t ecbSaltPa;
        };

        // Information for DERIVED_CBC-type keys.
        struct
        {
            // The hardware secret to be used in the key-derivation process.
            uint8_t cbcSecretIndex;

            // The physical address of the salt value to derive from.
            uintptr_t cbcSaltPa;

            // The physical address of the initialization vector for CBC.
            uintptr_t cbcIvPa;
        };
    };
} SCP_KEY_DESC;


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
LWRV_STATUS scpEncrypt(const SCP_KEY_DESC *pKeyDesc,
                       SCP_CIPHER_MODE cipherMode,
                       uintptr_t sourcePa,
                       uintptr_t destPa,
                       size_t size,
                       uintptr_t contextPa);

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
LWRV_STATUS scpDecrypt(const SCP_KEY_DESC *pKeyDesc,
                       SCP_CIPHER_MODE cipherMode,
                       uintptr_t sourcePa,
                       uintptr_t destPa,
                       size_t size,
                       uintptr_t contextPa);

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
LWRV_STATUS scpDmhash(uintptr_t sourcePa,
                      size_t size,
                      uintptr_t destPa,
                      uintptr_t initPa);

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
LWRV_STATUS scpCmac(const SCP_KEY_DESC *pKeyDesc,
                    SCP_CMAC_MODE cmacMode,
                    uintptr_t sourcePa,
                    size_t size,
                    uintptr_t destPa,
                    uintptr_t contextPa);

#endif // LIBLWRISCV_SCP_CRYPT_H
