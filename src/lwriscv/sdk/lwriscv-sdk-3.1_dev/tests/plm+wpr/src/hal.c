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
 * @file        hal.c
 * @brief       Hardware abstraction layer.
 */

// Standard includes.
#include <stdbool.h>
#include <stdint.h>

// Manual includes.
#include <dev_fb.h>
#include <dev_top.h>

// SDK includes.
#include <lwmisc.h>

// LWRISCV includes.
#include <lwriscv/csr.h>

// LIBLWRISCV includes.
#include <liblwriscv/io.h>

// Local includes.
#include "engine.h"
#include "hal.h"
#include "util.h"


// Engine-specific subWPR configuration registers.
#define HAL_ENGINE_SUBWPR_CFGA UTIL_JOIN(               \
    LW_PFB_PRI_MMU_FALCON_, ENGINE_PREFIX, _CFGA)(0)
#define HAL_ENGINE_SUBWPR_CFGB UTIL_JOIN(               \
    LW_PFB_PRI_MMU_FALCON_, ENGINE_PREFIX, _CFGB)(0)


// Prototypes for local helper functions. See definitions for details.
static inline void _halExceptionReturn(void);


/*!
 * @brief Prepares the FBIF interface for general use (physical mode).
 */
void
halConfigureFBIF(void)
{
    // Enable physical FBIF requests.
    localWrite(LW_PRGNLCL_FBIF_CTL,
        DRF_DEF(_PRGNLCL_FBIF, _CTL, _ENABLE,            _TRUE)     |
        DRF_DEF(_PRGNLCL_FBIF, _CTL, _ALLOW_PHYS_NO_CTX, _ALLOW));

#if ENGINE_HAS_NACK_AS_ACK
    // Prevent hanging on failed accesses.
    localWrite(LW_PRGNLCL_FBIF_CTL2,
        DRF_DEF(_PRGNLCL_FBIF, _CTL2, _NACK_MODE, _NACK_AS_ACK));
#endif // ENGINE_HAS_NACK_AS_ACK
}

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
void
halConfigureWPR
(
    uint32_t base,
    uint32_t size
)
{
    //
    // Compute aligned extents for the region. Note that the starting and
    // ending addresses are inclusive so there's no need to round up here.
    //
    const uint32_t start = base        >>
        LW_PFB_PRI_MMU_WPR1_ADDR_LO_ALIGNMENT;
    const uint32_t end = (base + size) >>
        LW_PFB_PRI_MMU_WPR1_ADDR_HI_ALIGNMENT;

    //
    // Set start and end addresses for WPR1. Note that the resulting region
    // will actually end up being 128K-aligned, even though the addresses
    // provided here need only be 4K-aligned.
    //
    priWrite(LW_PFB_PRI_MMU_WPR1_ADDR_LO,
        DRF_NUM(_PFB_PRI_MMU, _WPR1_ADDR_LO, _VAL, start));
    priWrite(LW_PFB_PRI_MMU_WPR1_ADDR_HI,
        DRF_NUM(_PFB_PRI_MMU, _WPR1_ADDR_HI, _VAL, end));

    // Force WPR ID to match for secure accesses.
    priWrite(LW_PFB_PRI_MMU_WPR_ALLOW_READ,
        FLD_SET_DRF_NUM(_PFB_PRI_MMU, _WPR_ALLOW_READ,  _MIS_MATCH, 0,
            priRead(LW_PFB_PRI_MMU_WPR_ALLOW_READ)));
    priWrite(LW_PFB_PRI_MMU_WPR_ALLOW_WRITE,
        FLD_SET_DRF_NUM(_PFB_PRI_MMU, _WPR_ALLOW_WRITE, _MIS_MATCH, 0,
            priRead(LW_PFB_PRI_MMU_WPR_ALLOW_WRITE)));

    // Configure engine-specific subWPR.
    priWrite(HAL_ENGINE_SUBWPR_CFGA,
        DRF_NUM(_PFB_PRI_MMU, _FALCON_SEC_CFGA, _ADDR_LO, start));
    priWrite(HAL_ENGINE_SUBWPR_CFGB,
        DRF_NUM(_PFB_PRI_MMU, _FALCON_SEC_CFGB, _ADDR_HI, end));
}

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
uint32_t
halConstructPLM
(
    HAL_PRIVILEGE_MASK plm
)
{
    //
    // Default settings: report violations and enable all sources. Start with
    // all privilege-levels disabled for now so we can selectively re-enable
    // afterwards as needed.
    //
    // Note that every PLM should have the same register layout, so we just use
    // a random one here from a header that we already needed.
    //
    uint32_t result =
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0,  _DISABLE)      |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL1,  _DISABLE)      |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL2,  _DISABLE)      |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL3,  _DISABLE)      |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _DISABLE)      |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _DISABLE)      |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _DISABLE)      |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL3, _DISABLE)      |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _READ_VIOLATION,          _REPORT_ERROR) |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _WRITE_VIOLATION,         _REPORT_ERROR) |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _SOURCE_READ_CONTROL,     _BLOCKED)      |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _SOURCE_WRITE_CONTROL,    _BLOCKED)      |
        (uint32_t)DRF_DEF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _SOURCE_ENABLE,           _ALL_SOURCES_ENABLED);

    // Enable PL0 if requested.
    if (UTIL_IS_SET(plm, PLM0))
    {
        result = FLD_SET_DRF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL0,  _ENABLE, result);
        result = FLD_SET_DRF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL0, _ENABLE, result);
    }

    // Enable PL1 if requested.
    if (UTIL_IS_SET(plm, PLM1))
    {
        result = FLD_SET_DRF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL1,  _ENABLE, result);
        result = FLD_SET_DRF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL1, _ENABLE, result);
    }

    // Enable PL2 if requested.
    if (UTIL_IS_SET(plm, PLM2))
    {
        result = FLD_SET_DRF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL2,  _ENABLE, result);
        result = FLD_SET_DRF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL2, _ENABLE, result);
    }

    // Enable PL3 if requested.
    if (UTIL_IS_SET(plm, PLM3))
    {
        result = FLD_SET_DRF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _READ_PROTECTION_LEVEL3,  _ENABLE, result);
        result = FLD_SET_DRF(_PFB_NISO, _OPEN_PRIV_LEVEL_MASK, _WRITE_PROTECTION_LEVEL3, _ENABLE, result);
    }

    // Return final PLM value.
    return result;
}

/*!
 * @brief Creates an one-to-one MPU mapping from VA space to PA space.
 *
 * @param[in] index  The MPU index at which to install the new mapping.
 *
 * @note Enables all permissions by default. Only valid if called from M- or
 *       S-mode.
 */
void
halCreateIdentityMapping
(
    uint8_t index
)
{
    // Select requested entry.
    csr_write(LW_RISCV_CSR_SMPUIDX,
        DRF_NUM64(_RISCV_CSR, _SMPUIDX, _INDEX, index));

    // Map from start of VA space (mark valid).
    csr_write(LW_RISCV_CSR_SMPUVA, 0 |
        DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));

    // Map to start of PA space.
    csr_write(LW_RISCV_CSR_SMPUPA, 0);

    // Map entire address range.
    csr_write(LW_RISCV_CSR_SMPURNG,
        DRF_NUM64(_RISCV_CSR, _SMPURNG, _RANGE, ~0ULL));

    //
    // Allow read/write/execute for all operating modes.
    // Enable caching and coherency for external locations (FB/SYSMEM).
    // No VPR (WPR configurable separately).
    //
    csr_write(LW_RISCV_CSR_SMPUATTR,
        DRF_DEF64(_RISCV_CSR, _SMPUATTR, _VPR,       _RST)  |
        DRF_NUM64(_RISCV_CSR, _SMPUATTR, _COHERENT,  1)     |
        DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, 1)     |
        DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SX,        1)     |
        DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW,        1)     |
        DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR,        1)     |
        DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UX,        1)     |
        DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UW,        1)     |
        DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UR,        1));
}

/*!
 * @brief Disables WPR1 and the host engine's subWPR.
 */
void
halDisableWPR(void)
{
    // Disable the engine-specific subWPR.
    priWrite(HAL_ENGINE_SUBWPR_CFGA,
        (uint32_t)DRF_DEF(_PFB_PRI_MMU, _FALCON_SEC_CFGA, _ADDR_LO, _INIT));
    priWrite(HAL_ENGINE_SUBWPR_CFGB,
        (uint32_t)DRF_DEF(_PFB_PRI_MMU, _FALCON_SEC_CFGB, _ADDR_HI, _INIT));

    // Disable WPR1.
    priWrite(LW_PFB_PRI_MMU_WPR1_ADDR_LO,
        (uint32_t)DRF_DEF(_PFB_PRI_MMU, _WPR1_ADDR_LO, _VAL, _INIT));
    priWrite(LW_PFB_PRI_MMU_WPR1_ADDR_HI,
        (uint32_t)DRF_DEF(_PFB_PRI_MMU, _WPR1_ADDR_HI, _VAL, _INIT));
}

/*!
 * @brief Returns the type of exception a given exception cause represents.
 *
 * @param[in] mcause  Code indicating the event that caused the exception.
 *
 * @return
 *    The type of exception represented by mcause.
 */
HAL_EXCEPTION_TYPE
halGetExceptionType
(
    uint64_t mcause
)
{
    // Check whether this is an interrupt rather than an exception.
    if (!FLD_TEST_DRF64(_RISCV_CSR, _MCAUSE, _INT, _RST, mcause))
    {
        // None supported lwrrently.
        return HAL_EXCEPTION_TYPE_UNEX;
    }
    
    // Extract exception code and colwert to abstract type.
    switch (DRF_VAL64(_RISCV_CSR, _MCAUSE, _EXCODE, mcause))
    {
        // Instruction-access-fault.
        case LW_RISCV_CSR_MCAUSE_EXCODE_IACC_FAULT:
            return HAL_EXCEPTION_TYPE_IACC;
        
        // Load-access-fault.
        case LW_RISCV_CSR_MCAUSE_EXCODE_LACC_FAULT:
            return HAL_EXCEPTION_TYPE_LACC;

        // Environment call from U-/S-/M-mode.
        case LW_RISCV_CSR_MCAUSE_EXCODE_UCALL:
        case LW_RISCV_CSR_MCAUSE_EXCODE_SCALL:
        case LW_RISCV_CSR_MCAUSE_EXCODE_MCALL:
            return HAL_EXCEPTION_TYPE_CALL;

        // Unexpected / other.
        default:
            return HAL_EXCEPTION_TYPE_UNEX;
    }
}

/*!
 * @brief Returns a string describing the current host platform.
 *
 * @return
 *    A user-friendly null-terminated string describing the host platform.
 */
const char *
halGetPlatformName(void)
{
    // Map the current platform to a user-friendly name.
    switch (DRF_VAL(_PTOP, _PLATFORM, _TYPE, priRead(LW_PTOP_PLATFORM)))
    {
        case LW_PTOP_PLATFORM_TYPE_SILICON:
            return "silicon";

        case LW_PTOP_PLATFORM_TYPE_FMODEL:
            return "fmodel";

        case LW_PTOP_PLATFORM_TYPE_RTL:
            return "RTL";

        case LW_PTOP_PLATFORM_TYPE_EMU:
            return "emulation";

        case LW_PTOP_PLATFORM_TYPE_FPGA:
            return "FPGA";

        default:
            return "unknown";
    }
}

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
void
halGetWPRPLM
(
    HAL_PRIVILEGE_MASK *readPLM,
    HAL_PRIVILEGE_MASK *writePLM
)
{
    // Retrieve read permissions.
    *readPLM = DRF_VAL(_PFB_PRI_MMU, _FALCON_SEC_CFGA, _ALLOW_READ,
        priRead(HAL_ENGINE_SUBWPR_CFGA));

    // Retrieve write permissions.
    *writePLM = DRF_VAL(_PFB_PRI_MMU, _FALCON_SEC_CFGB, _ALLOW_WRITE,
        priRead(HAL_ENGINE_SUBWPR_CFGB));
}

/*!
 * @brief Drops from M-mode to S-mode.
 *
 * @note Only valid if called from M-mode.
 */
void
halMToS(void)
{
    // Set return mode to S-mode.
    HAL_SET_RETURN_MODE(SUPERVISOR);

    // Return as if from an exception to change modes.
    _halExceptionReturn();
}

/*!
 * @brief Drops from M-mode to U-mode.
 *
 * @note Only valid if called from M-mode.
 */
void
halMToU(void)
{
    // Set return mode to U-mode.
    HAL_SET_RETURN_MODE(USER);

    // Return as if from an exception to change modes.
    _halExceptionReturn();
}

#if ENGINE_TEST_GDMA
/*!
 * @brief Resets the indicated GDMA channel to recover from a fault or error.
 *
 * @param[in] channel  The GDMA channel to reset.
 *
 * @note Blocks until the reset is complete.
 */
void
halResetGDMAChannel
(
    uint8_t channel
)
{
    // Issue the reset request.
    localWrite((uint32_t)LW_PRGNLCL_GDMA_CHAN_CONTROL(channel),
        DRF_DEF(_PRGNLCL_GDMA, _CHAN_CONTROL, _RESET, _TRUE));

    // Wait for the reset to complete.
    uint32_t chanStatus;
    do
    {
        chanStatus = localRead((uint32_t)LW_PRGNLCL_GDMA_CHAN_STATUS(channel));
    } while (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_VLD, _TRUE, chanStatus) ||
             FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _FAULTED,   _TRUE, chanStatus) ||
             FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _BUSY,      _TRUE, chanStatus));
}
#endif // ENGINE_TEST_GDMA

/*!
 * @brief Configures the WPR ID to use for DMA transfers.
 *
 * @param[in] index  The index of the FBIF aperture to configure.
 * @param[in] wprId  The WPR ID to assign to the aperture.
 */
void
halSetDMAWPRID
(
    uint8_t index,
    HAL_REGION_ID wprId
)
{
    uint32_t cfg = localRead(LW_PRGNLCL_FBIF_REGIONCFG);

    // Unfortunately, REGIONCFG is not indexed, hence this ugly alternative.
    switch (index)
    {
        case 0:
            cfg = FLD_SET_DRF_NUM(_PRGNLCL_FBIF, _REGIONCFG, _T0, wprId, cfg);
            break;
        case 1:
            cfg = FLD_SET_DRF_NUM(_PRGNLCL_FBIF, _REGIONCFG, _T1, wprId, cfg);
            break;
        case 2:
            cfg = FLD_SET_DRF_NUM(_PRGNLCL_FBIF, _REGIONCFG, _T2, wprId, cfg);
            break;
        case 3:
            cfg = FLD_SET_DRF_NUM(_PRGNLCL_FBIF, _REGIONCFG, _T3, wprId, cfg);
            break;
        case 4:
            cfg = FLD_SET_DRF_NUM(_PRGNLCL_FBIF, _REGIONCFG, _T4, wprId, cfg);
            break;
        case 5:
            cfg = FLD_SET_DRF_NUM(_PRGNLCL_FBIF, _REGIONCFG, _T5, wprId, cfg);
            break;
        case 6:
            cfg = FLD_SET_DRF_NUM(_PRGNLCL_FBIF, _REGIONCFG, _T6, wprId, cfg);
            break;
        case 7:
            cfg = FLD_SET_DRF_NUM(_PRGNLCL_FBIF, _REGIONCFG, _T7, wprId, cfg);
            break;
    }

    localWrite(LW_PRGNLCL_FBIF_REGIONCFG, cfg);
}

/*!
 * @brief Configures the read/write permissions of the host engine's subWPR.
 *
 * @param[in] readPLM  The desired read permissions for the subWPR.
 * @param[in] writePLM The desired write permissions for the subWPR.
 *
 * @note Assumes the subWPR has already been configured and that the caller has
 *       sufficient external privileges to make the necessary modifications.
 */
void
halSetWPRPLM
(
    HAL_PRIVILEGE_MASK readPLM,
    HAL_PRIVILEGE_MASK writePLM
)
{
    // Apply read setting.
    priWrite(HAL_ENGINE_SUBWPR_CFGA,
        FLD_SET_DRF_NUM(_PFB_PRI_MMU, _FALCON_SEC_CFGA, _ALLOW_READ, readPLM,
            priRead(HAL_ENGINE_SUBWPR_CFGA)));

    // Apply write setting.
    priWrite(HAL_ENGINE_SUBWPR_CFGB,
        FLD_SET_DRF_NUM(_PFB_PRI_MMU, _FALCON_SEC_CFGB, _ALLOW_WRITE, writePLM,
            priRead(HAL_ENGINE_SUBWPR_CFGB)));
}

/*!
 * @brief Checks that IO-PMP is fully disabled (all accesses permitted).
 *
 * @return
 *    true  if IO-PMP is fully disabled.
 *    false if IO-PMP is not fully disabled.
 */
bool
halVerifyIOPMP(void)
{
    uint8_t registerIndex, entryIndex;

    // Loop through each IO-PMP addressing-mode register.
    for (registerIndex = 0;
         registerIndex < LW_PRGNLCL_RISCV_IOPMP_MODE__SIZE_1;
         registerIndex++)
    {
        // Read the current register value.
        uint32_t lwrrentRegister = localRead((uint32_t)
            LW_PRGNLCL_RISCV_IOPMP_MODE(registerIndex));

        // Check each entry in the current register.
        for (entryIndex = 0;
             entryIndex < LW_PRGNLCL_RISCV_IOPMP_MODE_VAL_ENTRY__SIZE_1;
             entryIndex++)
        {
            // Ensure that the entry is disabled.
            if (!FLD_IDX_TEST_DRF(_PRGNLCL_RISCV, _IOPMP_MODE, _VAL_ENTRY,
                entryIndex, _OFF, lwrrentRegister))
            {
                return false;
            }
        }
    }

    // All IO-PMP entries are disabled.
    return true;
}

/*!
 * @brief Checks whether WPR1 and the host engine's subWPR have been configured.
 *
 * @return
 *    true  if WPR1 and the host engine's subWPR are active.
 *    false if WPR1 and/or the host engine's subWPR are inactive.
 */
bool
halVerifyWPR(void)
{
    // Verify that WPR1 is configured.
    if (DRF_VAL(_PFB_PRI_MMU, _WPR1_ADDR_LO, _VAL,
                priRead(LW_PFB_PRI_MMU_WPR1_ADDR_LO)) >
        DRF_VAL(_PFB_PRI_MMU, _WPR1_ADDR_HI, _VAL,
                priRead(LW_PFB_PRI_MMU_WPR1_ADDR_HI)))
    {
        return false;
    }

    // Verify WPR-ID enforcement.
    if(!FLD_TEST_DRF_NUM(_PFB_PRI_MMU, _WPR_ALLOW_READ,  _MIS_MATCH, 0,
                         priRead(LW_PFB_PRI_MMU_WPR_ALLOW_READ)) ||
       !FLD_TEST_DRF_NUM(_PFB_PRI_MMU, _WPR_ALLOW_WRITE, _MIS_MATCH, 0,
                         priRead(LW_PFB_PRI_MMU_WPR_ALLOW_WRITE)))
    {
        return false;
    }

    // Verify that the host engine's subWPR is configured.
    if(FLD_TEST_DRF(_PFB_PRI_MMU, _FALCON_SEC_CFGA, _ADDR_LO, _INIT,
        priRead(HAL_ENGINE_SUBWPR_CFGA)) ||
       FLD_TEST_DRF(_PFB_PRI_MMU, _FALCON_SEC_CFGB, _ADDR_HI, _INIT,
        priRead(HAL_ENGINE_SUBWPR_CFGB)))
    {
        return false;
    }

    // Verification successful.
    return true;
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Returns from this function as if returning from an exception.
 *
 * @note Only valid if called from M-mode.
 */
static inline void
_halExceptionReturn(void)
{
    //
    // Set exception-return address to the end of this function and then
    // execute an M-mode exception-return.
    //
    __asm__ volatile
    (
        "auipc a5, 0x0;"
        "addi  a5, a5, 16;"
        "csrw  mepc, a5;"
        "mret;"
        "nop" // Fix GCC bug.
    );

    return;
}
