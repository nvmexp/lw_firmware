/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PLM_WPR_HAL_H
#define PLM_WPR_HAL_H

/*!
 * @file        hal.h
 * @brief       Hardware abstraction layer.
 */

// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// Manual includes.
#include LWRISCV64_MANUAL_ADDRESS_MAP

// SDK includes.
#include <lwmisc.h>

// LWRISCV includes.
#include <lwriscv/csr.h>

// LIBLWRISCV includes.
#include <liblwriscv/io.h>

// Local includes.
#include "engine.h"


/*!
 * @brief Returns the ending address of the specified memory region.
 *
 * @param[in] region  The memory region whose ending address will be returned
 *                    (e.g. DMEM).
 */
#define HAL_GET_END_ADDRESS(region) (                               \
    LW_RISCV_AMAP_##region##_START +                                \
        (DRF_VAL(_PRGNLCL_FALCON, _HWCFG3, _##region##_TOTAL_SIZE,  \
            localRead(LW_PRGNLCL_FALCON_HWCFG3)) << 8ULL)           \
)

/*!
 * @brief Checks access to a given device-map group.
 *
 * @param[in] mode  The operating mode to check (e.g. MMODE).
 * @param[in] group The target device-map group (e.g. SCP).
 * @param[in] op    The desired operation (e.g. READ).
 *
 * @return
 *    true  if access is permitted.
 *    false if access is denied.
 *
 * Checks whether the given operating mode (MMODE / SUBMMODE) has the
 * requested access rights (READ / WRITE) to the specified device-map group.
 */
#define HAL_HAS_DEVICEMAP_ACCESS(mode, group, op) (                         \
    FLD_IDX_TEST_DRF(_PRGNLCL_RISCV, _DEVICEMAP_RISCV##mode, _##op,         \
        LW_PRGNLCL_DEVICE_MAP_GROUP_##group, _ENABLE,                       \
        localRead(LW_PRGNLCL_RISCV_DEVICEMAP_RISCV##mode(                   \
            LW_PRGNLCL_DEVICE_MAP_GROUP_##group /                           \
            LW_PRGNLCL_RISCV_DEVICEMAP_RISCV##mode##_##op##__SIZE_1)))      \
)

/*!
 * @brief Sets the operating mode to return to after handling an exception.
 *
 * @param[in] mode  The operating mode to return to. Valid values are USER,
 *                  SUPERVISOR, and MACHINE.
 *
 * @note Only valid if called from M-mode (S-mode callers not supported).
 */
#define HAL_SET_RETURN_MODE(mode) (                             \
    csr_write(LW_RISCV_CSR_MSTATUS,                             \
        FLD_SET_DRF64(_RISCV_CSR, _MSTATUS, _MPP, _##mode,      \
            csr_read(LW_RISCV_CSR_MSTATUS)))                    \
)

///////////////////////////////////////////////////////////////////////////////

// Abstract exception types recognized by halGetExceptionType().
typedef enum HAL_EXCEPTION_TYPE
{
    HAL_EXCEPTION_TYPE_IACC,    // Instruction-access-fault.
    HAL_EXCEPTION_TYPE_LACC,    // Load-access-fault.
    HAL_EXCEPTION_TYPE_CALL,    // System call.

    HAL_EXCEPTION_TYPE_UNEX,    // Unexpected exception.
}HAL_EXCEPTION_TYPE;

//
// Supported external privilege-levels. Note that short names are used here to
// improve the readbility of calls to testSingleAccess().
//
typedef enum HAL_PRIVILEGE_LEVEL
{
    LEVEL0 = LW_RISCV_CSR_MRSP_MRPL_LEVEL0, // PL0.
    LEVEL1 = LW_RISCV_CSR_MRSP_MRPL_LEVEL1, // PL1.
    LEVEL2 = LW_RISCV_CSR_MRSP_MRPL_LEVEL2, // PL2.
    LEVEL3 = LW_RISCV_CSR_MRSP_MRPL_LEVEL3, // PL3.
}HAL_PRIVILEGE_LEVEL;

//
// Flags for specifying privilege-level masks. Any combination of the below
// values is a valid PLM setting. Note that short names are used here to
// improve the readability of calls to testSingleAccess().
//
typedef enum HAL_PRIVILEGE_MASK
{
    PLM0 = (1 << 0),    // Allow access to L0 clients.
    PLM1 = (1 << 1),    // Allow access to L1 clients.
    PLM2 = (1 << 2),    // Allow access to L2 clients.
    PLM3 = (1 << 3),    // Allow access to L3 clients.
}HAL_PRIVILEGE_MASK;

//
// Recognized write-protected-region (WPR) IDs. Note that short names are used
// here to improve the readbility of calls to testSingleAccess().
//
typedef enum HAL_REGION_ID
{
    NONWPR = 0, // Non-WPR region.
    WPR1   = 1, // WPR1.
}HAL_REGION_ID;

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Prepares the FBIF interface for general use (physical mode).
 */
void halConfigureFBIF(void);

/*!
 * @brief Configures WPR1 and fills it with a subWPR for the host engine.
 *
 * @param[in] base  The starting FB offset at which to place WPR1.
 * @param[in] size  The size to allocate to WPR1, in bytes.
 *
 * @note Due to alignment constraints, the resulting WPR may extend beyond the
 *       originally-requested region. Initial configuration has read/write
 *       disabled for all privilege-levels.
 */
void halConfigureWPR(uint32_t base, uint32_t size);

/*!
 * @brief Constructs a concrete PLM setting from an abstract PLM description.
 *
 * @param[in] plm  The desired permissions for the constructed PLM.
 *
 * @return
 *    The constructed PLM value.
 *
 * @note The requested permissions are applied to both the read and write
 *       controls and the constructed PLM has all sources enabled.
 */
uint32_t halConstructPLM(HAL_PRIVILEGE_MASK plm);

/*!
 * @brief Creates an one-to-one MPU mapping from VA space to PA space.
 *
 * @param[in] index  The MPU index at which to install the new mapping.
 *
 * @note Enables all permissions by default. Only valid if called from M- or
 *       S-mode.
 */
void halCreateIdentityMapping(uint8_t index);

/*!
 * @brief Disables WPR1 and the host engine's subWPR.
 */
void halDisableWPR(void);

/*!
 * @brief Returns the type of exception a given exception cause represents.
 *
 * @param[in] mcause  Code indicating the event that caused the exception.
 *
 * @return
 *    The type of exception represented by mcause.
 */
HAL_EXCEPTION_TYPE halGetExceptionType(uint64_t mcause);

/*!
 * @brief Returns a string describing the current host platform.
 *
 * @return
 *    A user-friendly null-terminated string describing the host platform.
 */
const char *halGetPlatformName(void);

/*!
 * @brief Retrieves the read/write permissions of the host engine's subWPR.
 *
 * @param[in] readPLM  A pointer to receive the current read permissions for
 *                     the subWPR.
 *
 * @param[in] writePLM A pointer to receive the current write permissions for
 *                     the subWPR.
 *
 * @note Assumes the subWPR has already been configured.
 */
void halGetWPRPLM(HAL_PRIVILEGE_MASK *readPLM, HAL_PRIVILEGE_MASK *writePLM);

/*!
 * @brief Drops from M-mode to S-mode.
 *
 * @note Only valid if called from M-mode.
 */
void halMToS(void);

/*!
 * @brief Drops from M-mode to U-mode.
 *
 * @note Only valid if called from M-mode.
 */
void halMToU(void);

#if ENGINE_TEST_GDMA
/*!
 * @brief Resets the indicated GDMA channel to recover from a fault or error.
 *
 * @param[in] channel  The GDMA channel to reset.
 *
 * @note Blocks until the reset is complete.
 */
void halResetGDMAChannel(uint8_t channel);
#endif // ENGINE_TEST_GDMA

/*!
 * @brief Configures the WPR ID to use for DMA transfers.
 *
 * @param[in] index  The index of the FBIF aperture to configure.
 * @param[in] wprId  The WPR ID to assign to the aperture.
 */
void halSetDMAWPRID(uint8_t index, HAL_REGION_ID wprId);

/*!
 * @brief Configures the read/write permissions of the host engine's subWPR.
 *
 * @param[in] readPLM  The desired read permissions for the subWPR.
 * @param[in] writePLM The desired write permissions for the subWPR.
 *
 * @note Assumes the subWPR has already been configured and that the caller has
 *       sufficient external privileges to make the necessary modifications.
 */
void halSetWPRPLM(HAL_PRIVILEGE_MASK readPLM, HAL_PRIVILEGE_MASK writePLM);

/*!
 * @brief Checks that IO-PMP is fully disabled (all accesses permitted).
 *
 * @return
 *    true  if IO-PMP is fully disabled.
 *    false if IO-PMP is not fully disabled.
 */
bool halVerifyIOPMP(void);

/*!
 * @brief Checks whether WPR1 and the host engine's subWPR have been configured.
 *
 * @return
 *    true  if WPR1 and the host engine's subWPR are active.
 *    false if WPR1 and/or the host engine's subWPR are inactive.
 */
bool halVerifyWPR(void);

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Configures ICD for easier debugging.
 *
 * @note Only valid if called from M-mode.
 */
static inline void
halConfigureICD(void)
{
    //
    // Use the current operating mode's privilege-level for external accesses.
    // Always use M-mode for internal accesses.
    //
    csr_write(LW_RISCV_CSR_MDBGCTL,
        DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDPL,                _USE_LWR) |
        DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDMEMPRV_OVERRIDE,   _TRUE)    |
        DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDMEMPRV,            _M)       |
        DRF_DEF64(_RISCV_CSR, _MDBGCTL, _ICDCSRPRV_S_OVERRIDE, _FALSE)   |
        DRF_DEF64(_RISCV_CSR, _MDBGCTL, _MICD_DIS,             _FALSE));
}

/*!
 * @brief Disables delegation of interrupts/exceptions to S-mode.
 *
 * @note Only valid if called from M-mode.
 */
static inline void
halDisableDelegation(void)
{
    // Disable delegation for all interrupts.
    csr_write(LW_RISCV_CSR_MIDELEG, 0ULL);

    // Disable delegation for all exceptions.
    csr_write(LW_RISCV_CSR_MEDELEG, 0ULL);
}

/*!
 * @brief Disables all RISC-V interrupt sources.
 *
 * @note Only valid if called from M-mode.
 */
static inline void
halDisableInterrupts(void)
{
    localWrite(LW_PRGNLCL_RISCV_IRQMCLR, 0xFFFFFFFFU);
}

/*!
 * @brief Switches to bare mode for memory operations.
 *
 * @note Only valid if called from M- or S-mode.
 */
static inline void
halDisableMPU(void)
{
    csr_write(LW_RISCV_CSR_SATP,
        FLD_SET_DRF64(_RISCV_CSR, _SATP, _MODE, _BARE,
            csr_read(LW_RISCV_CSR_SATP)));
}

/*!
 * @brief Enables all privilege levels for all operating modes.
 *
 * @note Requires that the M-mode already has all levels enabled and/or that
 *       M-mode is not locked (MMLOCK=0). Only valid if called from M-mode.
 */
static inline void
halEnableAllPrivLevels(void)
{
    uint64_t mspm = csr_read(LW_RISCV_CSR_MSPM);
    mspm = FLD_SET_DRF64(_RISCV_CSR, _MSPM, _MPLM, _LEVEL3, mspm);
    mspm = FLD_SET_DRF64(_RISCV_CSR, _MSPM, _SPLM, _LEVEL3, mspm);
    mspm = FLD_SET_DRF64(_RISCV_CSR, _MSPM, _UPLM, _LEVEL3, mspm);
    csr_write(LW_RISCV_CSR_MSPM, mspm);
}

/*!
 * @brief Switches to virtual mode for memory operations.
 *
 * @note Requires prior configuration of the MPU. Only valid if called from M-
 *       or S-mode.
 */
static inline void
halEnableMPU(void)
{
    csr_write(LW_RISCV_CSR_SATP,
        FLD_SET_DRF64(_RISCV_CSR, _SATP, _MODE, _LWMPU,
            csr_read(LW_RISCV_CSR_SATP)));
}

/*!
 * @brief Enables the tracebuffer for all operating modes.
 */
static inline void
halEnableTracebuffer(void)
{
    uint32_t reg = localRead(LW_PRGNLCL_RISCV_TRACECTL);
    reg = FLD_SET_DRF(_PRGNLCL_RISCV, _TRACECTL, _MMODE_ENABLE, _TRUE,  reg);
    reg = FLD_SET_DRF(_PRGNLCL_RISCV, _TRACECTL, _SMODE_ENABLE, _TRUE,  reg);
    reg = FLD_SET_DRF(_PRGNLCL_RISCV, _TRACECTL, _UMODE_ENABLE, _TRUE,  reg);
    reg = FLD_SET_DRF(_PRGNLCL_RISCV, _TRACECTL, _MODE,         _STACK, reg);
    reg = FLD_SET_DRF(_PRGNLCL_RISCV, _TRACECTL, _INTR_ENABLE,  _FALSE, reg);
    localWrite(LW_PRGNLCL_RISCV_TRACECTL, reg);
}

/*!
 * @brief Initializes the MPU for future use (does not enable).
 *
 * @param[in] numEntries  The maximum number of MPU entries to enable.
 *
 * @note Only valid if called from M-mode.
 */
static inline void
halInitMPU
(
    uint8_t numEntries
)
{
    csr_write(LW_RISCV_CSR_MMPUCTL,
        DRF_DEF64(_RISCV_CSR, _MMPUCTL, _HASH_LWMPU_EN, _FALSE)          |
        DRF_NUM64(_RISCV_CSR, _MMPUCTL, _ENTRY_COUNT,   numEntries)      |
        DRF_NUM64(_RISCV_CSR, _MMPUCTL, _START_INDEX,   0));
}

/*!
 * @brief Releases the BootROM-enabled priv-lockdown for the host engine.
 */
static inline void
halReleaseLockdown(void)
{
    localWrite(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN,
        FLD_SET_DRF(_PRGNLCL_RISCV_BR, _PRIV_LOCKDOWN, _LOCK, _UNLOCKED,
            localRead(LW_PRGNLCL_RISCV_BR_PRIV_LOCKDOWN)));
}

/*!
 * @brief Sets default address-translation and -protection rules.
 *
 * @note Only valid if called from M-mode.
 */
static inline void
halResetAddressingMode(void)
{
    uint64_t mstatus = csr_read(LW_RISCV_CSR_MSTATUS);
    mstatus = FLD_SET_DRF64(_RISCV_CSR, _MSTATUS, _MMI,  _DISABLE, mstatus);
    mstatus = FLD_SET_DRF64(_RISCV_CSR, _MSTATUS, _MPRV, _DISABLE, mstatus);
    csr_write(LW_RISCV_CSR_MSTATUS, mstatus);
}

/*!
 * @brief Configures the WPR ID(s) to use for bare-mode memory accesses.
 *
 * @param[in] fetch The WPR ID to assign to bare-mode instruction fetches.
 * @param[in] load  The WPR ID to assign to bare-mode loads/stores.
 *
 * @note Only valid if called from M-mode.
 */
static inline void
halSetBareWPRID
(
    HAL_REGION_ID fetch,
    HAL_REGION_ID load
)
{
    // Set WPR ID for instruction fetch.
    csr_write(LW_RISCV_CSR_MFETCHATTR,
        FLD_SET_DRF_NUM64(_RISCV_CSR, _MFETCHATTR, _WPR, fetch,
            csr_read(LW_RISCV_CSR_MFETCHATTR)));

    // Set WPR ID for load/store.
    csr_write(LW_RISCV_CSR_MLDSTATTR,
        FLD_SET_DRF_NUM64(_RISCV_CSR, _MLDSTATTR, _WPR, load,
            csr_read(LW_RISCV_CSR_MLDSTATTR)));
}

/*!
 * @brief Configures the external privilege-level for each operating mode.
 *
 * @param[in] machine    The external privilege-level to configure for M-mode.
 * @param[in] supervisor The external privilege-level to configure for S-mode.
 * @param[in] user       The external privilege-level to configure for U-mode.
 *
 * @note Assumes that the requested configuration is valid per MSPM settings.
 *       Only valid if called from M-mode.
 */
static inline void
halSetPrivLevels
(
    HAL_PRIVILEGE_LEVEL machine,
    HAL_PRIVILEGE_LEVEL supervisor,
    HAL_PRIVILEGE_LEVEL user
)
{
    uint64_t mrsp = csr_read(LW_RISCV_CSR_MRSP);
    mrsp = FLD_SET_DRF_NUM64(_RISCV_CSR, _MRSP, _MRPL, machine,    mrsp);
    mrsp = FLD_SET_DRF_NUM64(_RISCV_CSR, _MRSP, _SRPL, supervisor, mrsp);
    mrsp = FLD_SET_DRF_NUM64(_RISCV_CSR, _MRSP, _URPL, user,       mrsp);
    csr_write(LW_RISCV_CSR_MRSP, mrsp);
}

/*!
 * @brief Configures the WPR ID to use for virtual-mode memory accesses.
 *
 * @param[in] index  The index of the MPU entry to configure.
 * @param[in] wprId  The WPR ID to assign to the entry.
 *
 * @note Only valid if called from M- or S-mode.
 */
static inline void
halSetVirtualWPRID
(
    uint8_t index,
    HAL_REGION_ID wprId
)
{
    csr_write(LW_RISCV_CSR_SMPUIDX,
        DRF_NUM64(_RISCV_CSR, _SMPUIDX, _INDEX, index));

    csr_write(LW_RISCV_CSR_SMPUATTR,
        FLD_SET_DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, wprId,
            csr_read(LW_RISCV_CSR_SMPUATTR)));
}

/*!
 * @brief Moves the exception-return address forward by one instruction.
 *
 * @note Assumes that compressed-ISA is disabled. Only valid if called from
 *       M-mode.
 */
static inline void
halSkipInstruction(void)
{
    uint64_t mepc = csr_read(LW_RISCV_CSR_MEPC);

    // Add size of one instruction to MEPC (4 bytes for non-compressed ISA).
    csr_write(LW_RISCV_CSR_MEPC,
        FLD_SET_DRF_NUM64(_RISCV_CSR, _MEPC, _EPC,
            DRF_VAL64(_RISCV_CSR, _MEPC, _EPC, mepc) + 4ULL, mepc));
}

/*!
 * @brief Unlocks external access to IMEM and DMEM.
 */
static inline void
halUnlockTcm(void)
{
    uint32_t reg = localRead(LW_PRGNLCL_FALCON_LOCKPMB);
    reg = FLD_SET_DRF(_PRGNLCL_FALCON, _LOCKPMB, _IMEM, _UNLOCK, reg);
    reg = FLD_SET_DRF(_PRGNLCL_FALCON, _LOCKPMB, _DMEM, _UNLOCK, reg);
    localWrite(LW_PRGNLCL_FALCON_LOCKPMB, reg);
}

/*!
 * @brief Writes the passed value to the host engine's zeroth mailbox.
 *
 * @param[in] value  The value to write.
 */
static inline void
halWriteMailbox0
(
    uint32_t value
)
{
    localWrite(LW_PRGNLCL_FALCON_MAILBOX0,
        FLD_SET_DRF_NUM(_PRGNLCL_FALCON, _MAILBOX0, _DATA, value,
            localRead(LW_PRGNLCL_FALCON_MAILBOX0)));
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Verifies default address-translation and -protection rules are set.
 *
 * @return
 *    true  if default address-translation and -protection rules are set.
 *    false if default address-translation and -protection rules are not set.
 *
 * @note Only valid if called from M-mode.
 */
static inline bool
halVerifyAddressingMode(void)
{
    uint64_t mstatus = csr_read(LW_RISCV_CSR_MSTATUS);

    return FLD_TEST_DRF64(_RISCV_CSR, _MSTATUS, _MMI,  _DISABLE, mstatus) &&
           FLD_TEST_DRF64(_RISCV_CSR, _MSTATUS, _MPRV, _DISABLE, mstatus);
}

/*!
 * @brief Checks that FBIF is configured to allow physical accesses.
 *
 * @return
 *    true  if FBIF is correclty configured.
 *    false if FBIF is not configured.
 */
static inline bool
halVerifyFBIF(void)
{
    uint32_t fbifCtl = localRead(LW_PRGNLCL_FBIF_CTL);

    return FLD_TEST_DRF(_PRGNLCL_FBIF, _CTL, _ENABLE, _TRUE, fbifCtl) &&
        FLD_TEST_DRF(_PRGNLCL_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW, fbifCtl);
}

/*!
 * @brief Checks whether the indicated MPU entry is valid.
 *
 * @param[in] index  The MPU index to check.
 *
 * @return
 *    true  if the indicated MPU entry is valid.
 *    false if the indicated MPU entry is invalid.
 *
 * @note Only valid if called from M- or S-mode.
 */
static inline bool
halVerifyMPUEntry(uint8_t index)
{
    csr_write(LW_RISCV_CSR_SMPUIDX,
        DRF_NUM64(_RISCV_CSR, _SMPUIDX, _INDEX, index));

    return !FLD_TEST_DRF64(_RISCV_CSR, _SMPUVA, _VLD, _RST,
        csr_read(LW_RISCV_CSR_SMPUVA));
}

/*!
 * @brief Checks whether the NACK_AS_ACK option is enabled for FBIF accesses.
 *
 * @return
 *    true  if NACK_AS_ACK is enabled.
 *    false if NACK_AS_ACK is disabled or unsupported.
 */
static inline bool
halVerifyNackMode(void)
{
#if ENGINE_HAS_NACK_AS_ACK
    return FLD_TEST_DRF(_PRGNLCL_FBIF, _CTL2, _NACK_MODE, _NACK_AS_ACK,
        localRead(LW_PRGNLCL_FBIF_CTL2));
#else // ENGINE_HAS_NACK_AS_ACK
    return false;
#endif // ENGINE_HAS_NACK_AS_ACK
}

/*!
 * @brief Checks that all privilege-levels are enabled for all operating modes.
 *
 * @return
 *    true  if all privilege-levels are enabled.
 *    false if one or more privilege-levels are disabled.
 *
 * @note Only valid if called from M-mode.
 */
static inline bool
halVerifyPrivLevels(void)
{
    uint64_t mspm = csr_read(LW_RISCV_CSR_MSPM);

    return FLD_TEST_DRF64(_RISCV_CSR, _MSPM, _MPLM, _LEVEL3, mspm) &&
           FLD_TEST_DRF64(_RISCV_CSR, _MSPM, _SPLM, _LEVEL3, mspm) &&
           FLD_TEST_DRF64(_RISCV_CSR, _MSPM, _UPLM, _LEVEL3, mspm);
}

#endif // PLM_WPR_HAL_H
