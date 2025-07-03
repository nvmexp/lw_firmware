/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   main.c
 * @brief  Bootloader main.
 *
 * This bootloader implements the design specified at:
 * https://confluence.lwpu.com/display/Peregrine/RISC-V+Bootloader
 *
 * That document should be kept up-to-date with all design changes made here.
 */

#include <riscvifriscv.h>
#include <engine.h>

#include <dev_fuse.h>

#include "bootloader.h"

#include "debug.h"
#include "elf.h"
#include "mpu.h"
#include "state.h"
#include "util.h"

// MK TODO: Remove this code after HOST1x sets boot args proper
#if RUN_ON_SEC
#include "flcnifcmn.h"
#include "gsp/gspifgsp.h"
#endif // RUN_ON_SEC

#ifdef IS_SSP_ENABLED
#include <ssp.h>
#endif // IS_SSP_ENABLED

/*!
 * Ucode specification for RISCV bootloader (signed)
 * TODO: Unify with /chips_a/drivers/resman/kernel/inc/objflcn.h (COREUCODES-530).
 */
typedef struct
{
    // Total size of FB carveout (image and reserved space).
    LwU32 fbReservedSize;
    // Size of "golden" image ([manifest, FMC,] bootloader, params, ELF, padding).
    LwU32 imageSize;
    // Size of ELF file.
    LwU32 elfSize;
    // Size of padding.
    LwU32 padSize;
    // ELF file.
    LwU8 elf[];
} FLCN_RISCV_BOOTLDR_UCODE, *PFLCN_RISCV_BOOTLDR_UCODE;

#ifdef IS_FUSE_CHECK_ENABLED
/*!
 * @brief Checks that privsec is enabled in production elwironments.
 *
 * @return Void on success. Does not return on failure.
 */
static void
_checkFusing(void)
{
    // Only check fuse in production elwironments.
    if (FLD_TEST_DRF_ENG(_SCP, _CTL_STAT, _DEBUG_MODE, _DISABLED,
                         csbReadPA(ENGINE_REG(_SCP_CTL_STAT))))
    {
        // Ensure that privsec is enabled.
        if (!FLD_TEST_DRF(_FUSE_OPT, _PRIV_SEC_EN, _DATA, _YES,
                          bar0ReadPA(LW_FUSE_OPT_PRIV_SEC_EN)))
        {
            // Bad fusing!
            dbgExit(BOOTLOADER_ERROR_BAD_FUSING);
        }
    }
}
#endif // IS_FUSE_CHECK_ENABLED

/*!
 * @brief Obtain boot arguments and validate them.
 *
 * @param[in]  pLoaderBase   Starting address of the bootloader (for bounds-checks).
 * @param[in]  carveoutSize  Size of the bootloader carveout (for bounds-checks).
 * @param[out] pValidated    Structure to receive validated copy of untrusted paramters.
 *
 * @return Validated pointer to raw (ulwalidated) boot arguments. Does not return on failure.
 */
static LW_RISCV_BOOTLDR_PARAMS *
_getValidatedBootargs
(
    const void *pLoaderBase,
    LwUPtr carveoutSize,
    LW_RISCV_BOOTLDR_PARAMS *pValidated
)
{
    //
    // Initialize params pointer
    // MK TODO We can hal'ify it if we need to use different registers for
    // different engines.
    //
    LW_RISCV_BOOTLDR_PARAMS *pUntrusted = (LW_RISCV_BOOTLDR_PARAMS *)(
                (((LwU64)intioRead(LW_PRGNLCL_FALCON_MAILBOX1)) << 32) |
                intioRead(LW_PRGNLCL_FALCON_MAILBOX0));
    if (pUntrusted == NULL)
    {
        dbgPuts(LEVEL_ALWAYS, "Bootargs are null...\n");
        dbgExit(BOOTLOADER_ERROR_BAD_BOOTARGS);
    }

// MK TODO: Remove this code after HOST1x sets boot args proper
#if RUN_ON_SEC
    pUntrusted->bootType = RM_RISCV_BOOTLDR_BOOT_TYPE_RM;
    pUntrusted->size = sizeof(RM_GSP_BOOT_PARAMS);
    pUntrusted->version = RM_RISCV_BOOTLDR_VERSION;
#endif // RUN_ON_SEC

    // Check alignment before dereferencing.
    if (((LwUPtr)pUntrusted & MPU_GRANULARITY_MASK) != 0)
    {
        dbgPuts(LEVEL_ALWAYS, "Invalid bootargs alignment.\n");
        dbgExit(BOOTLOADER_ERROR_BAD_BOOTARGS);
    }

    //
    // Make a local copy to prevent changes during or after validation.
    // If the pointer is invalid or points somewhere we can't read,
    // we'll simply hang here.
    //
    *pValidated = *pUntrusted;

    //
    // Check that the structure contains at least as many parameters
    // as we're expecting to use here in the bootloader.
    //
    if (pValidated->size < sizeof(LW_RISCV_BOOTLDR_PARAMS))
    {
        dbgPuts(LEVEL_ALWAYS, "Invalid bootargs size.\n");
        dbgExit(BOOTLOADER_ERROR_BAD_BOOTARGS);
    }

    //
    // Ensure parameters don't overlap our own careveout as a safety measure
    // (i.e. in case we ever write any data back to them).
    //
    if (utilPtrDoesOverflow((LwUPtr)pUntrusted, pValidated->size) ||
           utilCheckOverlap((LwUPtr)pUntrusted, pValidated->size,
                            (LwUPtr)pLoaderBase, carveoutSize))
    {
        dbgPuts(LEVEL_ALWAYS, "Bootargs overlap with loader carveout.\n");
        dbgExit(BOOTLOADER_ERROR_BAD_BOOTARGS);
    }

    // Check that we support the supplied version.
    if (pValidated->version != RM_RISCV_BOOTLDR_VERSION)
    {
        dbgPuts(LEVEL_ALWAYS, "Invalid bootargs version.\n");
        dbgExit(BOOTLOADER_ERROR_BAD_BOOTARGS);
    }

    // Return raw pointer (only address has been validated).
    return pUntrusted;
}

/*!
 * @brief Bootloader main function.
 *
 * @param[in] pLoaderBase   Base address of the loader image
 * @param[in] pUcode        Pointer to the ucode and its configuration.
 * @param[in] accessId      WPR ID or GSC ID of region we booted from.
 *
 * @return Does not return.
 */
__attribute__((noreturn))
void
bootloaderMain
(
    const void *pLoaderBase,
    const PFLCN_RISCV_BOOTLDR_UCODE pUcode
)
{
    LW_BOOTLOADER_STATE state;
    LW_RISCV_BOOTLDR_PARAMS *pRawParams, safeParams;

    SET_STATE(&state);

#ifdef IS_SSP_ENABLED
    // Prepare stack canary before calling any other functions.
    sspCanarySetup();
#endif // IS_SSP_ENABLED

#ifdef IS_FUSE_CHECK_ENABLED
    // Check for fusing errors (security bar).
    _checkFusing();
#endif // IS_FUSE_CHECK_ENABLED

    //
    // Callwlate some useful values that we'll need later. We're forced to make
    // a few assumptions about the image layout here, but only here at least.
    //
    //    pReservedBase - Start of the pre-reserved RW region for loading.
    //    reservedSize  - Size of the pre-reserved RW region for loading.
    //    loaderSize    - Size of the loader image (loader + params + ELF + padding).
    //    carveoutSize  - Total memory footprint (loader image + reserved space).
    //
    const void *pReservedBase = pUcode->elf + pUcode->elfSize + pUcode->padSize;
    const LwUPtr reservedSize = pUcode->fbReservedSize - pUcode->imageSize;
    const LwUPtr loaderSize   = (LwUPtr)pReservedBase - (LwUPtr)pLoaderBase;
    const LwUPtr carveoutSize = loaderSize + reservedSize;

    // Enable interrupts at core (we have MEMERR configured to catch ASM errors)
    csr_set(LW_RISCV_CSR_SIE, DRF_NUM64(_RISCV, _CSR_SIE, _SEIE, 1));
    csr_set(LW_RISCV_CSR_SSTATUS, DRF_DEF64(_RISCV, _CSR_SSTATUS, _SIE, _ENABLE));

    // Get WPR or GSC ID of region we booted from.
#ifdef USE_GSCID
    LwU32 accessId  = DRF_VAL(_PRGNLCL_RISCV, _BCR_DMACFG_SEC, _GSCID,
                              intioRead(LW_PRGNLCL_RISCV_BCR_DMACFG_SEC));
#else // USE_GSCID
    LwU32 accessId  = DRF_VAL(_PRGNLCL_RISCV, _BCR_DMACFG_SEC, _WPRID,
                              intioRead(LW_PRGNLCL_RISCV_BCR_DMACFG_SEC));
#endif // USE_GSCID

#   ifdef LW_PRGNLCL_TFBIF_REGIONCFG
    LwU32 regionCfgData = (DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T0_VPR, accessId) |
                           DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T1_VPR, accessId) |
                           DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T2_VPR, accessId) |
                           DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T3_VPR, accessId) |
                           DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T4_VPR, accessId) |
                           DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T5_VPR, accessId) |
                           DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T6_VPR, accessId) |
                           DRF_NUM(_PRGNLCL, _TFBIF_REGIONCFG, _T7_VPR, accessId));

    intioWrite(LW_PRGNLCL_TFBIF_REGIONCFG, regionCfgData);
#   endif // LW_PRGNLCL_TFBIF_REGIONCFG

    // Work-around for accessing non-WPR (e.g. SYSMEM) under LS.
    elfSetIdentityRegions(pLoaderBase, carveoutSize, accessId);

    dbgInit();
    dbgPuts(LEVEL_ALWAYS, "!Bootloader starting!\n");
    dbgPuts(LEVEL_ALWAYS, "Bootloader base: 0x");
    dbgPutHex(LEVEL_ALWAYS, 16, (LwU64)pLoaderBase);
    dbgPuts(LEVEL_ALWAYS, "\n");

    //
    // pRawParams points to the original boot-arguments that we need to
    // forward to the application, which must then do its own sanitization
    // (we only guarantee here that it doesn't overlap with our carveout).
    // safeParams contains a validated copy of the arguments, which can be
    // used safely here as needed.
    //
    // Later we may want to find a way to pass the validated copy to the
    // application instead.
    //
    pRawParams = _getValidatedBootargs(pLoaderBase, carveoutSize, &safeParams);

    if (!elfBegin(GET_ELF_STATE(), pUcode->elf, pUcode->elfSize, pRawParams,
                  safeParams.size, pLoaderBase, loaderSize, pReservedBase,
                  reservedSize, accessId))
    {
        dbgPuts(LEVEL_ALWAYS, "ELF load failed\n");
        dbgExit(BOOTLOADER_ERROR_BAD_ELF);
    }

    elfLoad(GET_ELF_STATE());

    // We will only get here if the ELF failed to load.
    dbgPuts(LEVEL_ALWAYS, "!Load failed!\n");
    dbgExit(BOOTLOADER_ERROR_LOAD_FAILURE);
}
