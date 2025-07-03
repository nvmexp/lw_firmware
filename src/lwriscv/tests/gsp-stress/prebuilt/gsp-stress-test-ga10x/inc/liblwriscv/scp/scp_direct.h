/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_SCP_DIRECT_H
#define LIBLWRISCV_SCP_DIRECT_H

/*!
 * @file    scp_direct.h
 * @brief   Low-level SCP primitives.
 *
 * @note    Client applications should include scp.h instead of this file.
 */

// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// SDK includes.
#include <lwmisc.h>

// LWRISCV includes.
#include <lwriscv/status.h>

 // LIBLWRISCV includes.
#include <liblwriscv/io.h>

// SCP includes.
#include <liblwriscv/scp/scp_common.h>


/*!
 * @brief Issues a single CCI instruction to the SCP unit.
 *
 * @param[in] opcode    The CCI instruction to issue (e.g. XOR).
 * @param[in] rx_or_imm A register index or 6-bit immediate value.
 * @param[in] ry        A register index.
 *
 * @note The interpretation of the rx_or_imm and ry parameters is instruction-
 *       dependent. They can be set to zero when not needed.
 */
#define SCP_INSTRUCTION(opcode, rx_or_imm, ry) (                        \
    localWrite(LW_PRGNLCL_RISCV_SCPDMATRFCMD,                           \
        DRF_DEF(_PRGNLCL_RISCV, _SCPDMATRFCMD, _CCI_EX,    _CCI     ) | \
        DRF_NUM(_PRGNLCL_RISCV, _SCPDMATRFCMD, _RY,        ry       ) | \
        DRF_NUM(_PRGNLCL_RISCV, _SCPDMATRFCMD, _RX_OR_IMM, rx_or_imm) | \
        DRF_DEF(_PRGNLCL_RISCV, _SCPDMATRFCMD, _OPCODE,    _##opcode)   \
    )                                                                   \
)

// The size, in bytes, of each SCP GPR.
#define SCP_REGISTER_SIZE 16


//
// A collection of supported transfer sizes for queued (suppressed) read and
// write operations.
//
typedef enum SCP_TRANSFER_SIZE
{
    SCP_TRANSFER_SIZE_1R  = LW_PRGNLCL_RISCV_SCPDMATRFCMD_SIZE_16B,
    SCP_TRANSFER_SIZE_2R  = LW_PRGNLCL_RISCV_SCPDMATRFCMD_SIZE_32B,
    SCP_TRANSFER_SIZE_4R  = LW_PRGNLCL_RISCV_SCPDMATRFCMD_SIZE_64B,
    SCP_TRANSFER_SIZE_8R  = LW_PRGNLCL_RISCV_SCPDMATRFCMD_SIZE_128B,
    SCP_TRANSFER_SIZE_16R = LW_PRGNLCL_RISCV_SCPDMATRFCMD_SIZE_256B,
}SCP_TRANSFER_SIZE;


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
LWRV_STATUS scpStartDirect(void);

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
 *    ERR_GENERIC                   if an unexpected error oclwrred.
 *
 *    WARN_NOTHING_TO_DO            if the SCP driver is not operating in
 *                                  direct mode (no-op).
 *
 * Prepares the SCP driver for high-level operations by exiting direct mode.
 * Applications must call scpStopDirect() before ilwoking any high-level SCP
 * interfaces, unless otherwise noted.
 */
LWRV_STATUS scpStopDirect(void);

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
void scp_load(uintptr_t sourcePa, SCP_REGISTER_INDEX regIndex);

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
void scp_store(SCP_REGISTER_INDEX regIndex, uintptr_t destPa);

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
void scp_queue_read(uintptr_t sourcePa, SCP_TRANSFER_SIZE size);

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
void scp_queue_write(uintptr_t destPa, SCP_TRANSFER_SIZE size);

/*!
 * @brief Returns the active status of the SCPDMA unit.
 *
 * @return
 *    true  if the SCPDMA unit is active.
 *    false if the SCPDMA unit is idle.
 *
 * Applications can use scp_poll_dma() to determine whether a load, store,
 * or CCI transfer has finished.
 */
static inline bool
scp_poll_dma(void)
{
    return FLD_TEST_DRF(_PRGNLCL_RISCV, _SCPDMAPOLL, _DMA_ACTIVE, _ACTIVE,
        localRead(LW_PRGNLCL_RISCV_SCPDMAPOLL));
}

/*!
 * @brief Polls the active status of the SCPDMA unit and returns when idle.
 *
 * Applications can use scp_wait_dma() to ensure that a prior load, store,
 * or CCI transfer has finished.
 */
static inline void
scp_wait_dma(void)
{
    while (scp_poll_dma())
        ;
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
 * Checks for pending SCPDMA errors. Clears them after reading if bClear is
 * true.
 *
 * Applications can use scp_check_dma() to determine whether a load, store, or
 * CCI transfer has encountered problems.
 */
LWRV_STATUS scp_check_dma(bool bClear);

///////////////////////////////////////////////////////////////////////////////
// Begin instruction primitives. Refer to the IAS for full descriptions.

/*!
 * @brief Adds a 6-bit immediate value to register ry.
 *
 * @param[in] imm The 6-bit immediate value to add.
 * @param[in] ry  The destination register to add to.
 *
 * @note The default carry-chain is 64 bits in length.
 */
static inline void
scp_add(uint8_t imm, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(ADD, imm, ry);
}

/*!
 * @brief Swaps the byte-order of the value in register rx.
 *
 * @param[in] rx The source register containing the value to byte-swap.
 * @param[in] ry The destination register to receive the byte-swapped result.
 */
static inline void
scp_bswap(SCP_REGISTER_INDEX rx, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(BSWAP, rx, ry);
}

/*!
 * @brief Modifies the ACL of register ry.
 *
 * @param[in] imm The new ACL value, encoded as {Wi,Fi,Ki,Fs,Ks} = imm[4:0].
 * @param[in] ry  The register whose ACL will be updated.
 *
 * @note Only valid in secure contexts. Cannot grant secure permissions.
 */
static inline void
scp_chmod(uint8_t imm, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(CHMOD, imm, ry);
}

/*!
 * @brief Generates the first of the two subkeys required for CMAC computation.
 *
 * @param[in] rx The source register containing an encrypted zero value.
 * @param[in] ry The destination register to receive the generated subkey.
 *
 * @note The initial step of encrypting zero must be completed separately.
 */
static inline void
scp_cmac_sk(SCP_REGISTER_INDEX rx, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(CMAC_SK, rx, ry);
}

/*!
 * @brief Perform AES-128 decryption on the contents of register rx.
 *
 * @param[in] rx The source register containing the ciphertext to decrypt.
 * @param[in] ry The destination register to receive the decrypted plaintext.
 *
 * @note Obtains the key from the register specified in ku.
 */
static inline void
scp_decrypt(SCP_REGISTER_INDEX rx, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(DECRYPT, rx, ry);
}

/*!
 * @brief Perform AES-128 encryption on the contents of register rx.
 *
 * @param[in] rx The source register containing the plaintext to encrypt.
 * @param[in] ry The destination register to receive the encrypted ciphertext.
 *
 * @note Obtains the key from the register specified in ku.
 */
static inline void
scp_encrypt(SCP_REGISTER_INDEX rx, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(ENCRYPT, rx, ry);
}

/*!
 * @brief Performs AES-128 encryption on the engine's signature/hash.
 *
 * @param[in] rx The source register containing the encryption key to use.
 * @param[in] ry The destination register to receive the encrypted result.
 *
 * @note Only valid in secure contexts.
 */
static inline void
scp_encrypt_sig(SCP_REGISTER_INDEX rx, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(ENCRYPT_SIG, rx, ry);
}

/*!
 * @brief Offers the contents of register ry to the SCPDMA interface.
 *
 * @param[in] ry The source register whose data will be fetched.
 *
 * @note Fetches zeroes if register ry lacks sufficient access rights.
 */
static inline void
scp_fetch(SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(FETCH, 0, ry);
}

/*!
 * @brief Ilwalidates the engine's signature/hash.
 *
 * @note Only valid in secure contexts.
 */
static inline void
scp_forget_sig(void)
{
    SCP_INSTRUCTION(FORGET_SIG, 0, 0);
}

/*!
 * @brief Marks register ry as the key source for cryptographic operations.
 *
 * @param[in] ry The source register from which key material will be read.
 *
 * @note Updates ku with the index of ry only (contents are not copied).
 */
static inline void
scp_key(SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(KEY, 0, ry);
}

/*!
 * @brief Loads the next imm instructions into sequencer zero.
 *
 * @param[in] imm The number of instructions to load (0-16).
 *
 * @note Resets the sequencer if imm is zero.
 */
static inline void
scp_load_trace0(uint8_t imm)
{
    SCP_INSTRUCTION(LOAD_TRACE0, imm, 0);
}

/*!
 * @brief Loads the next imm instructions into sequencer one.
 *
 * @param[in] imm The number of instructions to load (0-16).
 *
 * @note Resets the sequencer if imm is zero.
 */
static inline void
scp_load_trace1(uint8_t imm)
{
    SCP_INSTRUCTION(LOAD_TRACE1, imm, 0);
}

/*!
 * @brief Ilwokes imm iterations of sequencer zero.
 *
 * @param[in] imm The number of iterations to execute (0-63).
 */
static inline void
scp_loop_trace0(uint8_t imm)
{
    SCP_INSTRUCTION(LOOP_TRACE0, imm, 0);
}

/*!
 * @brief Ilwokes imm iterations of sequencer one.
 *
 * @param[in] imm The number of iterations to execute (0-63).
 */
static inline void
scp_loop_trace1(uint8_t imm)
{
    SCP_INSTRUCTION(LOOP_TRACE1, imm, 0);
}

/*!
 * @brief Copies the contents of register rx into register ry.
 *
 * @param[in] rx The source register to copy from.
 * @param[in] ry The destination register to copy to.
 */
static inline void
scp_mov(SCP_REGISTER_INDEX rx, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(MOV, rx, ry);
}

/*!
 * @brief No operation.
 */
static inline void
scp_nop(void)
{
    SCP_INSTRUCTION(NOP, 0, 0);
}

/*!
 * @brief Accepts data from the SCPDMA interface and stores in register ry.
 *
 * @param[in] ry The destination register to receive the push data.
 */
static inline void
scp_push(SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(PUSH, 0, ry);
}

/*!
 * @brief Writes a true random number to register ry.
 *
 * @param[in] ry The destination register to receive the random number.
 *
 * @note Requires prior configuration of the RNG hardware. Not NIST-compliant.
 */
static inline void
scp_rand(SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(RAND, 0, ry);
}

/*!
 * @brief Colwerts a decryption key into an equivalent encryption key.
 *
 * @param[in] rx The source register containing the decryption key to colwert.
 * @param[in] ry The destination register to receive the encryption key.
 *
 * @note Ilwerse of the rkey10 instruction.
 */
static inline void
scp_rkey1(SCP_REGISTER_INDEX rx, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(RKEY1, rx, ry);
}

/*!
 * @brief Colwerts an encryption key into an equivalent decryption key.
 *
 * @param[in] rx The source register containing the encryption key to colwert.
 * @param[in] ry The destination register to receive the decryption key.
 *
 * @note Ilwerse of the rkey1 instruction.
 */
static inline void
scp_rkey10(SCP_REGISTER_INDEX rx, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(RKEY10, rx, ry);
}

/*!
 * @brief Loads one of 64 hardware secrets into register ry.
 *
 * @param[in] imm The index of the hardware secret to load (0-63).
 * @param[in] ry  The destination register to receive the hardware secret.
 *
 * @note Only valid in secure contexts. ACL is updated accordingly.
 */
static inline void
scp_secret(uint8_t imm, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(SECRET, imm, ry);
}

/*!
 * @brief Performs a bitwise XOR on the contents of registers rx and ry.
 *
 * @param[in] rx The first operand register.
 * @param[in] ry The second operand register and destination for the result.
 */
static inline void
scp_xor(SCP_REGISTER_INDEX rx, SCP_REGISTER_INDEX ry)
{
    SCP_INSTRUCTION(XOR, rx, ry);
}

// End instruction primitives.
///////////////////////////////////////////////////////////////////////////////

// Prevent external use of this macro.
#undef SCP_INSTRUCTION

#endif // LIBLWRISCV_SCP_DIRECT_H
