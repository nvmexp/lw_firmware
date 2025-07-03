/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   mpu.c
 * @brief  LW-MPU interfaces.
 */

#include <lwmisc.h>
#include <lwriscv-mpu.h>
#include <lwtypes.h>
#include <riscv_csr.h>

#include "debug.h"
#include "dmem_addrs.h"
#include "mpu.h"
#include "util.h"


// Total number of MPU regions supported by LW-MPU.
#define MPUIDX_COUNT LW_RISCV_CSR_MPU_ENTRY_COUNT

// Index of first MPU region reserved by the bootstub.
#define MPUIDX_BOOTSTUB_START (MPUIDX_COUNT-2)

// Indices of reserved regions.
#define MPUIDX_IDENTITY_BOOTSTUB (MPUIDX_BOOTSTUB_START + 0)
#define MPUIDX_IDENTITY_ALL      (MPUIDX_BOOTSTUB_START + 1)

// Alignment utilities.
#define MPU_GRANULARITY (1 << DRF_BASE(LW_RISCV_CSR_SMPUPA_BASE))
#define MPU_CALC_BASE(BASE, SIZE) LW_ALIGN_DOWN(BASE, MPU_GRANULARITY)
#define MPU_CALC_SIZE(BASE, SIZE) LW_ALIGN_UP((SIZE) + ((BASE) & (MPU_GRANULARITY - 1)), \
                                              MPU_GRANULARITY)


// Supported modes for configuring the MPU.
typedef enum LW_RISCV_MPU_SETTING_TYPE
{
    // Mbare mode (LW-MPU disabled).
    LW_RISCV_MPU_SETTING_TYPE_MBARE = 0,
    // Manual configuration.
    LW_RISCV_MPU_SETTING_TYPE_MANUAL = 1,
    // Automatic configuration (with possible additional regions).
    LW_RISCV_MPU_SETTING_TYPE_AUTOMATIC = 2,
} LW_RISCV_MPU_SETTING_TYPE;

// Parameters for a single MPU mapping.
typedef struct LW_RISCV_MPU_REGION
{
    LwU64 vaBase;
    LwU64 paBase;
    LwU64 range;
    LwU64 attribute;
} LW_RISCV_MPU_REGION;

// Complete MPU settings (directly inserted into .LWPU.mpu_info).
typedef struct LW_RISCV_MPU_INFO
{
    //
    // Use values from LW_RISCV_MPU_SETTING_TYPE enumeration.
    //
    // This defaults to _MBARE.
    //
    LwU32 mpuSettingType;

    //
    // If this field is non-zero, it is the number of mpuRegion entries that follow. This contains
    // the total number of regions for _MANUAL, or the number of additional regions for _AUTOMATIC.
    // For _MBARE, this field must be zero.
    // Examples:
    //   0x00000000 = No manual regions.
    //   0x00000001 = One manual region defined in this section.
    // The total number of regions must be less than or equal to MPUIDX_BOOTSTUB_START.
    //
    // This defaults to 0.
    //
    LwU32 mpuRegionCount;

    // MPU regions follow.
    LW_RISCV_MPU_REGION mpuRegion[];
} LW_RISCV_MPU_INFO;


/*!
 * @brief Clears all MPU entries.
 */
static void
_mpuClear(void)
{
    for (LwU64 i = 0; i < MPUIDX_COUNT; i++)
    {
        csr_write(LW_RISCV_CSR_SMPUIDX,  i);
        csr_write(LW_RISCV_CSR_SMPUVA,   0);
        csr_write(LW_RISCV_CSR_SMPUPA,   0);
        csr_write(LW_RISCV_CSR_SMPURNG,  0);
        csr_write(LW_RISCV_CSR_SMPUATTR, 0);
    }
}

/*!
 * @brief Enables LW-MPU mode.
 */
static inline void
_mpuEnable(void)
{
    csr_write(LW_RISCV_CSR_SATP,
        DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _LWMPU) |
        DRF_DEF64(_RISCV_CSR, _SATP, _ASID, _BARE)  |
        DRF_DEF64(_RISCV_CSR, _SATP, _PPN,  _BARE));
}

/*!
 * @brief Dumps an MPU entry along with an error message.
 *
 * @param[in] pMessage  The error message to print (without trailing newline).
 * @param[in] pMpuInfo  A pointer to the MPU settings.
 * @param[in] index     The index of the problem entry.
 */
static void
_mpuError
(
    const char *pMessage,
    const LW_RISCV_MPU_INFO *pMpuInfo,
    LwU32 index
)
{
    dbgPuts(LEVEL_CRIT, pMessage);
    dbgPuts(LEVEL_CRIT, "\nIndex: 0x");
    dbgPutHex(LEVEL_CRIT, 8, index);
    dbgPuts(LEVEL_CRIT, "\lwA Base: 0x");
    dbgPutHex(LEVEL_CRIT, 16, pMpuInfo->mpuRegion[index].vaBase & ~1ULL);
    dbgPuts(LEVEL_CRIT, "\nPA Base: 0x");
    dbgPutHex(LEVEL_CRIT, 16, pMpuInfo->mpuRegion[index].paBase);
    dbgPuts(LEVEL_CRIT, "\nSize: 0x");
    dbgPutHex(LEVEL_CRIT, 16, pMpuInfo->mpuRegion[index].range);
    dbgPuts(LEVEL_CRIT, "\nAttributes: 0x");
    dbgPutHex(LEVEL_CRIT, 16, pMpuInfo->mpuRegion[index].attribute);
    dbgPuts(LEVEL_CRIT, "\n");
}

/*!
 * @brief Applies the application's MPU mappings.
 *
 * @param[in] pMpuInfo  A pointer to the MPU settings to apply.
 *
 * @note Assumes that the provided MPU settings have already been validated.
 */
static void
_mpuMapApplication
(
    const LW_RISCV_MPU_INFO *pMpuInfo
)
{
    for (LwU32 i = 0U; i < pMpuInfo->mpuRegionCount; i++)
    {
        //
        // Apply each region's settings blindly as we assume they've already
        // been separately validated.
        //
        csr_write(LW_RISCV_CSR_SMPUIDX,  i);
        csr_write(LW_RISCV_CSR_SMPUVA,   pMpuInfo->mpuRegion[i].vaBase);
        csr_write(LW_RISCV_CSR_SMPUPA,   pMpuInfo->mpuRegion[i].paBase);
        csr_write(LW_RISCV_CSR_SMPURNG,  pMpuInfo->mpuRegion[i].range);
        csr_write(LW_RISCV_CSR_SMPUATTR, pMpuInfo->mpuRegion[i].attribute);
    }
}

/*!
 * @brief Applies the bootstub's MPU mappings.
 *
 * @param[in] pBootstubBase The base address of the bootstub.
 * @param[in] bootstubSize  The total size of the bootstub.
 */
static void
_mpuMapBootstub
(
    const void *pBootstubBase,
    LwU64 bootstubSize
)
{
    // Compute MPU-aligned base and extent values for the bootstub region.
    const LwU64 carveoutBase = MPU_CALC_BASE((LwU64)pBootstubBase, bootstubSize);
    const LwU64 carveoutSize = MPU_CALC_SIZE((LwU64)pBootstubBase, bootstubSize);

    //
    // Write the identity entry for the bootstub carveout. This entry will
    // have execute permissions enabled.
    //
    csr_write(LW_RISCV_CSR_SMPUIDX,  MPUIDX_IDENTITY_BOOTSTUB);
    csr_write(LW_RISCV_CSR_SMPUVA,   carveoutBase | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));
    csr_write(LW_RISCV_CSR_SMPUPA,   carveoutBase);
    csr_write(LW_RISCV_CSR_SMPURNG,  carveoutSize);
    csr_write(LW_RISCV_CSR_SMPUATTR, DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SX, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1));

    //
    // Write another identity entry for the entire memory space, this time with
    // execute permissions disabled.
    //
    csr_write(LW_RISCV_CSR_SMPUIDX,  MPUIDX_IDENTITY_ALL);
    csr_write(LW_RISCV_CSR_SMPUVA,   0 | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));
    csr_write(LW_RISCV_CSR_SMPUPA,   0);
    csr_write(LW_RISCV_CSR_SMPURNG,  DRF_NUM64(_RISCV_CSR, _SMPURNG, _RANGE, ~0ULL));
    csr_write(LW_RISCV_CSR_SMPUATTR, DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW,        1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR,        1));
}

/*!
 * @brief Checks whether the given MPU settings are valid.
 *
 * @param[in] pMpuInfo      A pointer to the MPU settings to validate.
 * @param[in] pBootstubBase The base address of the bootstub.
 * @param[in] bootstubSize  The total size of the bootstub.
 *
 * @return
 *      LW_TRUE     if the provided settings are valid.
 *      LW_FALSE    otherwise.
 */
static LwBool
_mpuValidate
(
    const LW_RISCV_MPU_INFO *pMpuInfo,
    const void *pBootstubBase,
    LwU64 bootstubSize
)
{   
    // Validate the configuration mode.
    if (pMpuInfo->mpuSettingType == LW_RISCV_MPU_SETTING_TYPE_MBARE)
    {
        // Bare mode implies no MPU settings.
        if (pMpuInfo->mpuRegionCount != 0U)
        {
            dbgPuts(LEVEL_CRIT, "Error: Application is requesting bare "
                "mode operation but has MPU regions defined.\n");
            return LW_FALSE;
        }

        // No other validation needed.
        return LW_TRUE;
    }

    else if (pMpuInfo->mpuSettingType != LW_RISCV_MPU_SETTING_TYPE_MANUAL)
    {
        //
        // Application is requesting automatic mode (not supported) or
        // the mpuSettingType field is corrupt.
        //
        dbgPuts(LEVEL_CRIT, "Error: Unsupported memory mode: 0x");
        dbgPutHex(LEVEL_CRIT, 8, pMpuInfo->mpuSettingType);
        dbgPuts(LEVEL_CRIT, "\n");

        return LW_FALSE;
    }

    // LW-MPU mode implies at least one MPU mapping should be present.
    if (pMpuInfo->mpuRegionCount == 0U)
    {
        dbgPuts(LEVEL_CRIT, "Error: Application is requesting LW-MPU mode "
            "operation but has no MPU regions defined.\n");
        return LW_FALSE;
    }

    // Make sure we can fit all of the application's mappings.
    if (pMpuInfo->mpuRegionCount > MPUIDX_BOOTSTUB_START)
    {
        dbgPuts(LEVEL_CRIT, "Error: Application requires more MPU "
            "regions than can be supported.\nRequested: 0x");
        dbgPutHex(LEVEL_CRIT, 8, pMpuInfo->mpuRegionCount);
        dbgPuts(LEVEL_CRIT, "\nSupported: 0x");
        dbgPutHex(LEVEL_CRIT, 8, MPUIDX_BOOTSTUB_START);
        dbgPuts(LEVEL_CRIT, "\n");

        return LW_FALSE;
    }

    // Compute the total size of the MPU descriptor.
    const LwU64 mpuDescriptorSize = sizeof(*pMpuInfo) +
        (pMpuInfo->mpuRegionCount * sizeof(pMpuInfo->mpuRegion[0]));

    // Validate the mappings themselves.
    for (LwU32 i = 0U; i < pMpuInfo->mpuRegionCount; i++)
    {
        // Check VA alignment (ignore bottom "valid" bit).
        if (!LW_IS_ALIGNED64(pMpuInfo->mpuRegion[i].vaBase & ~1ULL, MPU_GRANULARITY))
        {
            _mpuError("Error: MPU region has unaligned VA base.", pMpuInfo, i);
            return LW_FALSE;
        }

        // Check PA alignment.
        if (!LW_IS_ALIGNED64(pMpuInfo->mpuRegion[i].paBase, MPU_GRANULARITY))
        {
            _mpuError("Error: MPU region has unaligned PA base.", pMpuInfo, i);
            return LW_FALSE;
        }

        // Check size alignment.
        if (!LW_IS_ALIGNED64(pMpuInfo->mpuRegion[i].range, MPU_GRANULARITY))
        {
            _mpuError("Error: MPU region has unaligned size.", pMpuInfo, i);
            return LW_FALSE;
        }

        // Check for VA overflow.
        if (utilPtrDoesOverflow(pMpuInfo->mpuRegion[i].vaBase & ~1ULL,
                                pMpuInfo->mpuRegion[i].range))
        {
            _mpuError("Error: MPU region overflows VA space.", pMpuInfo, i);
            return LW_FALSE;
        }

        // Check for PA overflow.
        if (utilPtrDoesOverflow(pMpuInfo->mpuRegion[i].paBase,
                                pMpuInfo->mpuRegion[i].range))
        {
            _mpuError("Error: MPU region overflows PA space.", pMpuInfo, i);
            return LW_FALSE;
        }

        // Check for VA collision with bootstub code.
        if (utilCheckOverlap(pMpuInfo->mpuRegion[i].vaBase & ~1ULL,
                             pMpuInfo->mpuRegion[i].range,
                             (LwUPtr)pBootstubBase,
                             bootstubSize))
        {
            // Check whether colliding entry is compatible with identity entry.
            if (((pMpuInfo->mpuRegion[i].vaBase & ~1ULL) != pMpuInfo->mpuRegion[i].paBase)          ||
                FLD_TEST_DRF64(_RISCV_CSR, _SMPUATTR, _SX,  _RST, pMpuInfo->mpuRegion[i].attribute) ||
                FLD_TEST_DRF64(_RISCV_CSR, _SMPUATTR, _SR,  _RST, pMpuInfo->mpuRegion[i].attribute))
            {
                _mpuError("Error: MPU region collides with bootstub code.", pMpuInfo, i);
                return LW_FALSE;
            }
        }

        // Check for VA collision with bootstub data.
        if (utilCheckOverlap(pMpuInfo->mpuRegion[i].vaBase & ~1ULL,
                             pMpuInfo->mpuRegion[i].range,
                             DMEM_CONTENTS_BASE,
                             DMEM_CONTENTS_SIZE) ||
            utilCheckOverlap(pMpuInfo->mpuRegion[i].vaBase & ~1ULL,
                             pMpuInfo->mpuRegion[i].range,
                             (LwUPtr)pMpuInfo,
                             mpuDescriptorSize))
        {
            // Check whether colliding entry is compatible with identity entry.
            if (((pMpuInfo->mpuRegion[i].vaBase & ~1ULL) != pMpuInfo->mpuRegion[i].paBase)          ||
                FLD_TEST_DRF64(_RISCV_CSR, _SMPUATTR, _SW,  _RST, pMpuInfo->mpuRegion[i].attribute) ||
                FLD_TEST_DRF64(_RISCV_CSR, _SMPUATTR, _SR,  _RST, pMpuInfo->mpuRegion[i].attribute))
            {
                _mpuError("Error: MPU region overlaps bootstub data.", pMpuInfo, i);
                return LW_FALSE;
            }
        }
    }

    // Finished.
    return LW_TRUE;
}

///////////////////////////////////////////////////////////////////////////////

/*!
 * @brief Validates and applies the provided MPU settings.
 *
 * @param[in] pMpuInfo      A pointer to the desired MPU settings.
 * @param[in] pBootstubBase The base address of the bootstub.
 * @param[in] bootstubSize  The total size of the bootstub.
 *
 * @return
 *      LW_TRUE     if successful.
 *      LW_FALSE    if the provided MPU settings are invalid.
 *
 * Validates the contents of pMpuInfo and, if LW-MPU mode is requested,
 * initializes and enables the MPU accordingly. Also creates identity mappings
 * for use by the bootstub as applicable.
 */
LwBool
mpuInit
(
    const LW_RISCV_MPU_INFO *pMpuInfo,
    const void *pBootstubBase,
    LwU64 bootstubSize
)
{   
    // Ensure the requested MPU settings are valid before applying them.
    if (!_mpuValidate(pMpuInfo, pBootstubBase, bootstubSize))
        return LW_FALSE;

    // Skip MPU setup in bare mode.
    if (pMpuInfo->mpuSettingType == LW_RISCV_MPU_SETTING_TYPE_MBARE)
        return LW_TRUE;

    // Clear any prior MPU mappings.
    _mpuClear();

    // Apply the bootstub's mappings.
    _mpuMapBootstub(pBootstubBase, bootstubSize);

    // Apply the application's mappings.
    _mpuMapApplication(pMpuInfo);

    // Enable the MPU.
    _mpuEnable();

    // Report status.
    dbgPuts(LEVEL_INFO, "MPU enabled with 0x");
    dbgPutHex(LEVEL_INFO, 2, pMpuInfo->mpuRegionCount);
    dbgPuts(LEVEL_INFO, " regions.\n");

    // Done.
    return LW_TRUE;
}
