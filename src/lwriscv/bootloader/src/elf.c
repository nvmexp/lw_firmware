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
 * @file   elf.c
 * @brief  ELF loader.
 */

#include <lwmisc.h>
#include <riscvifriscv.h>

#include "bootloader.h"
#include "debug.h"
#include "state.h"
#include "elf.h"
#include "elf_defs.h"
#include "mpu.h"
#include "dev_top.h"
#include "util.h"
#include "dmem_addrs.h"
#include "engine.h"
#include "flcnretval.h"

#ifdef UTF_TEST
# include "lwriscv-csr-utf.h"
# define TSTATIC
#else
# include <riscv_csr.h>
# define TSTATIC static
#endif

#if __riscv_xlen != 64 && __riscv_xlen != 32
# error "ELF loader only supports 32bit and 64bit RISC-V"
#endif

#if __riscv_xlen == 32
# error "ELF loader has not been tested for 32bit, and may not work properly"
#endif

#define RISCV_ALIGNMENT_BYTES (__riscv_xlen/8)
#define RISCV_ALIGNMENT_MASK (RISCV_ALIGNMENT_BYTES-1)

#define RISCV_ALIGNMENT_ASSUME(ptr) __builtin_assume_aligned(ptr, RISCV_ALIGNMENT_BYTES)

// RISC-V instructions are at least 16 bits.
#define RISCV_MIN_INSTRUCTION_LENGTH 2
#define RISCV_MIN_INSTRUCTION_ALIGNMENT_MASK (RISCV_MIN_INSTRUCTION_LENGTH-1)

//
// Remapped physical addresses take the format 0xFF#*************, where # is the loadbase to
// remap the address to, and * is the address added to the loadbase.
//
#define REMAPPED_PHYS_SHIFT            52
#define REMAPPED_PHYS_MASK             0xF

#define REMAPPED_PHYS_BASE_RW          0xFF0
#define REMAPPED_PHYS_BASE_ELF         0XFF1
#define REMAPPED_PHYS_BASE_ELF_ODP_COW 0xFF2

// The default MPU configuration if .LWPU.mpu_info section does not exist in target program.
static const LW_RISCV_MPU_INFO defaultMpuInfo = {
    .mpuSettingType = LW_RISCV_MPU_SETTING_TYPE_MBARE,
    .mpuRegionCount = 0,
};

/*!
 * @brief Begins exelwtion of new program.
 *
 * Clears the last bits of state required, then jumps to the new program.
 *
 * @param[in] entryPoint   Entry point of the new program.
 * @param[in] paramsPa     Phyiscal address of bootargs struct.
 * @param[in] paramsVa     Virtual address of bootargs struct.
 * @param[in] stackStart   Start address of stack to clobber.
 * @param[in] stackEnd     End address of stack to clobber.
 * @param[in] elfAddrPhys  Physical address of the ELF file.
 * @param[in] elfSize      Size in bytes of the ELF file.
 * @param[in] loadBase     Physical address of the pre-reserved RW load region.
 * @param[in] wprId        WPR ID of region we booted from.
 *
 * @return Does not return.
 */
void bootLoad(LwUPtr entryPoint, LwUPtr paramsPa, LwUPtr paramsVa,
              LwUPtr stackStart, LwUPtr stackEnd, LwUPtr elfAddrPhys,
              LwUPtr elfSize, LwUPtr loadBase, LwU64 wprId)
    __attribute__((noreturn));

// Hand-rolled strcmp since we don't have libc here.
TSTATIC int strcmp_(const char *src1, const char *src2)
{
    while (*src1 && *src1 == *src2)
    {
        src1++;
        src2++;
    }
    return *(const unsigned char*)src1 - *(const unsigned char*)src2;
}

// memcpy because we don't have libc here.
// This is its own function because we could mock it for unit tests later.
TSTATIC void memcpy_(void *dest, const void *src, LwLength size)
{
    const LwU8 *srcByte = (const LwU8 *)src;
    LwU8       *destByte = (LwU8 *)dest;
    LwU8       *destEnd = destByte + size;

    // Copy LwU64 chunks if possible.
    if (((LwUPtr)dest & 7) == 0 && ((LwUPtr)src & 7) == 0)
    {
        while (destByte < (destEnd - 7))
        {
            *(LwU64*)destByte = *(LwU64*)srcByte;
            destByte += 8;
            srcByte += 8;
        }
    }

    // Copy remaining as bytes.
    while (destByte < destEnd)
    {
        *(destByte++) = *(srcByte++);
    }
}

// memset because we don't have libc here.
TSTATIC void memset_(void *dest, char value, LwLength size)
{
    LwU8 *destByte = (LwU8 *)dest;
    LwU8 *destEnd = destByte + size;

    // Fill LwU64 chunks if possible.
    if ((((LwUPtr)dest) & 7) == 0)
    {
        LwU64 fill = value & 0xFF;
        fill |= fill << 8;
        fill |= fill << 16;
        fill |= fill << 32;

        while (destByte < (destEnd - 7))
        {
            *(LwU64*)destByte = fill;
            destByte += 8;
        }
    }

    // Fill remaining as bytes.
    while (destByte < destEnd)
    {
        *(destByte++) = value;
    }
}


#if ELF_LOAD_USE_DMA

/* ============================= START DMA CODE - REMOVE ONCE LIBLWRISCV IS INTEGRATED! =========== */
# define DMA_BLOCK_SIZE_MIN   0x4
# define DMA_BLOCK_SIZE_MAX   0x100

//
// This is copied pretty much verbatim from liblwriscv
// (LWRV_STATUS replaced with FLCN_STATUS).
// MMINTS-TODO: remove this and link to the real liblwriscv once it's integrated into chips_a.
//
TSTATIC FLCN_STATUS
dmaPa
(
    LwU64   tcmPa,
    LwU64   apertureOffset,
    LwU64   sizeBytes,
    LwU8    dmaIdx,
    LwBool  bReadExt
)
{
    LwBool bTcmImem = LW_FALSE;
    LwU64  tcmOffset;
    LwU32  dmaCmd = 0;
    LwU64  dmaBlockSize;
    LwU64  dmaEncBlockSize;
    LwU32  dmatrfcmd;
    LwU32  fbOffset;

    // tcmPa is in ITCM or DTCM?
    if ((tcmPa >= LW_RISCV_AMAP_IMEM_START) &&
        (tcmPa < (LW_RISCV_AMAP_IMEM_START + IMEM_SIZE)))
    {
        bTcmImem = LW_TRUE;
        tcmOffset = tcmPa - LW_RISCV_AMAP_IMEM_START;
    }
    else if ((tcmPa >= LW_RISCV_AMAP_DMEM_START) &&
        (tcmPa < (LW_RISCV_AMAP_DMEM_START + DMEM_SIZE)))
    {
        tcmOffset = tcmPa - LW_RISCV_AMAP_DMEM_START;
    }
    else
    {
        return FLCN_ERR_OUT_OF_RANGE;
    }

    if (((sizeBytes | tcmOffset | apertureOffset) & (DMA_BLOCK_SIZE_MIN - 1)) != 0)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Nothing to copy, bail after checks
    if (sizeBytes == 0)
    {
        return FLCN_OK;
    }

    if (bTcmImem)
    {
        dmaCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _TRUE, dmaCmd);
    }
    else
    {
        dmaCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IMEM, _FALSE, dmaCmd);
        dmaCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _SET_DMTAG, _FALSE, dmaCmd);
    }

    if (bReadExt) // Ext -> TCM
    {
        dmaCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _FALSE, dmaCmd);
    }
    else // TCM -> Ext
    {
        dmaCmd = FLD_SET_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _WRITE, _TRUE, dmaCmd);
    }
    dmaCmd = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _CTXDMA, dmaIdx, dmaCmd);

    //
    // Break up apertureOffset into a base/offset to be used in DMATRFBASE, TRFFBOFFS
    // Note: We cannot use tcmOffset for both TRFMOFFS and TRFFBOFFS because this
    // would require us to callwlate a DMATRFBASE value by subtracting
    // (apertureOffset - tcmOffset) and this may result in a misaligned value
    // which we cannot program into DMATRFBASE.
    //
    fbOffset = (LwU32) (apertureOffset & 0xFF);

    intioWrite(LW_PRGNLCL_FALCON_DMATRFBASE, (LwU32)((apertureOffset >> 8) & 0xffffffffU));
# ifdef LW_PRGNLCL_FALCON_DMATRFBASE1
    intioWrite(LW_PRGNLCL_FALCON_DMATRFBASE1, (LwU32)(((apertureOffset >> 8) >> 32) & 0xffffffffU));
# else //LW_PRGNLCL_FALCON_DMATRFBASE1
    if ((((apertureOffset >> 8) >> 32) & 0xffffffffU) != 0)
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }
# endif //LW_PRGNLCL_FALCON_DMATRFBASE1

    while (sizeBytes != 0)
    {
        // Wait if we're full
        while (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _FULL, _TRUE, intioRead(LW_PRGNLCL_FALCON_DMATRFCMD)))
        {
        }

        intioWrite(LW_PRGNLCL_FALCON_DMATRFMOFFS, (LwU32)tcmOffset); // This is fine as long as caller does error checking.
        intioWrite(LW_PRGNLCL_FALCON_DMATRFFBOFFS, fbOffset);

        // Determine the largest pow2 block transfer size of the remaining data
        dmaBlockSize = tcmOffset | fbOffset | DMA_BLOCK_SIZE_MAX;
        dmaBlockSize = LOWESTBIT(dmaBlockSize);

        // Reduce the dmaBlockSize to not exceed remaining data.
        while (dmaBlockSize > sizeBytes)
        {
            dmaBlockSize >>= 1;
        }

        // Colwert the pow2 block size to block size encoding
        dmaEncBlockSize = dmaBlockSize / DMA_BLOCK_SIZE_MIN;
        dmaEncBlockSize = BIT_IDX_64(dmaEncBlockSize);

        dmaCmd = FLD_SET_DRF_NUM(_PRGNLCL, _FALCON_DMATRFCMD, _SIZE, (LwU32)dmaEncBlockSize, dmaCmd); // MK: Cast to mute compilation error, dmaBlockSize is guaranteed to fit

        intioWrite(LW_PRGNLCL_FALCON_DMATRFCMD, dmaCmd);

        sizeBytes -= dmaBlockSize;
        tcmOffset += dmaBlockSize;
        fbOffset += (LwU32)dmaBlockSize; // MK: Cast to mute compiler warning, dmaBlockSize will fit
    }

    // Wait for completion of any remaining transfers
    do
    {
        dmatrfcmd = intioRead(LW_PRGNLCL_FALCON_DMATRFCMD);
    }
    while (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _IDLE, _FALSE, dmatrfcmd));

    if (FLD_TEST_DRF(_PRGNLCL, _FALCON_DMATRFCMD, _ERROR, _TRUE, dmatrfcmd))
    {
        return FLCN_ERROR;
    }

    return FLCN_OK;
}
/* ============================= END DMA CODE - REMOVE ONCE LIBLWRISCV IS INTEGRATED! ============= */

# if defined(PMU_RTOS)
#  include "pmu/pmuifcmn.h"
#  define DMAIDX_FBGPA   (RM_PMU_DMAIDX_UCODE)
#  define DMAIDX_SYSGPA  (RM_PMU_DMAIDX_PHYS_SYS_COH)
# elif defined(GSP_RTOS)
#  include "gsp/gspifcmn.h"
#  define DMAIDX_FBGPA   (RM_GSP_DMAIDX_UCODE)
#  define DMAIDX_SYSGPA  (RM_GSP_DMAIDX_PHYS_SYS_COH_FN0)
# elif defined(SEC2_RTOS)
#  include "gsp/sec2ifcmn.h"
#  define DMAIDX_FBGPA   (RM_SEC2_DMAIDX_UCODE)
#  define DMAIDX_SYSGPA  (RM_SEC2_DMAIDX_PHYS_SYS_COH_FN0)
# else
#  error "DMA index not defined!"
# endif // GSP_RTOS


# if DMEM_DMA_BUFFER_BASE % DMA_BLOCK_SIZE_MAX != 0
#  error "DMEM_DMA_BUFFER_BASE must be aligned to DMA_BLOCK_SIZE_MAX!"
# endif // MEM_DMA_BUFFER_BASE % DMA_BLOCK_SIZE_MAX != 0

# if DMEM_DMA_BUFFER_SIZE % DMA_BLOCK_SIZE_MAX != 0
#  error "DMEM_DMA_BUFFER_SIZE must be aligned to DMA_BLOCK_SIZE_MAX!"
# endif // MEM_DMA_BUFFER_SIZE % DMA_BLOCK_SIZE_MAX != 0

# if DMEM_DMA_BUFFER_SIZE <= 0
#  error "DMEM_DMA_BUFFER_SIZE must be greater than 0 if ELF_LOAD_USE_DMA is enabled!"
# endif // DMEM_DMA_BUFFER_SIZE <= 0


# if DMEM_DMA_ZERO_BUFFER_BASE % DMA_BLOCK_SIZE_MAX != 0
#  error "DMEM_DMA_ZERO_BUFFER_BASE must be aligned to DMA_BLOCK_SIZE_MAX!"
# endif // DMEM_DMA_ZERO_BUFFER_BASE % DMA_BLOCK_SIZE_MAX != 0

# if DMEM_DMA_ZERO_BUFFER_SIZE % DMA_BLOCK_SIZE_MAX != 0
#  error "DMEM_DMA_ZERO_BUFFER_SIZE must be aligned to DMA_BLOCK_SIZE_MAX!"
# endif // DMEM_DMA_ZERO_BUFFER_SIZE % DMA_BLOCK_SIZE_MAX != 0

# if DMEM_DMA_ZERO_BUFFER_SIZE <= 0
#  error "DMEM_DMA_ZERO_BUFFER_SIZE must be greater than 0 if ELF_LOAD_USE_DMA is enabled!"
# endif // DMEM_DMA_ZERO_BUFFER_SIZE <= 0


/*!
 * @brief Initializes DMA for loading ELF section data.
 * Identifies an aperture (Sysmem or FB) for the ELF and the load target,
 * and configures the state and the DMA HW accordingly.
 *
 * @param[in] pState  State structure.
 */
TSTATIC void _elfInitDma(LW_ELF_STATE *pState)
{
    LwBool bApertureFound = LW_FALSE;

    //
    // Make sure the zero buffer is zeroed out! (this is just a sanity,
    // this shouldn't really be needed since TCM is zeroed out on reset).
    //
    memset_((LwU8*)DMEM_DMA_ZERO_BUFFER_BASE, 0, DMEM_DMA_ZERO_BUFFER_SIZE);

    // TODO: implement this properly for TFBIF!
    LwU32 transcfgVal = 0; // val to write to TRANSCFG for this dmaIdx
    LwU32 regioncfgVals = intioRead(LW_PRGNLCL_FBIF_REGIONCFG);

# ifdef LW_RISCV_AMAP_SYSGPA_START
    if (utilCheckWithin(LW_RISCV_AMAP_SYSGPA_START, LW_RISCV_AMAP_SYSGPA_SIZE, (LwUPtr)pState->pElf, pState->elfSize) &&
        utilCheckAddrWithin(LW_RISCV_AMAP_SYSGPA_START, LW_RISCV_AMAP_SYSGPA_SIZE, pState->loadBases[LW_ELF_LOAD_BASE_INDEX_RESERVED_RW]))
    {
        bApertureFound = LW_TRUE;

        pState->dmaIdx       = DMAIDX_SYSGPA;
        pState->apertureBase = LW_RISCV_AMAP_SYSGPA_START;
        pState->apertureSize = LW_RISCV_AMAP_SYSGPA_SIZE;

        transcfgVal = DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
                      DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET,   _COHERENT_SYSMEM);
    }
# endif // LW_RISCV_AMAP_SYSGPA_START

# ifdef LW_RISCV_AMAP_FBGPA_START
    if (utilCheckWithin(LW_RISCV_AMAP_FBGPA_START, LW_RISCV_AMAP_FBGPA_SIZE, (LwUPtr)pState->pElf, pState->elfSize) &&
        utilCheckAddrWithin(LW_RISCV_AMAP_FBGPA_START, LW_RISCV_AMAP_FBGPA_SIZE, pState->loadBases[LW_ELF_LOAD_BASE_INDEX_RESERVED_RW]))
    {
        if (bApertureFound)
        {
            dbgPuts(LEVEL_ALWAYS, "Bootloader: elf (and loadbase) located in overlapping apertures!\n");
            dbgExit(BOOTLOADER_ERROR_LOAD_FAILURE);
            // unreached
        }

        bApertureFound = LW_TRUE;

        pState->dmaIdx       = DMAIDX_FBGPA;
        pState->apertureBase = LW_RISCV_AMAP_FBGPA_START;
        pState->apertureSize = LW_RISCV_AMAP_FBGPA_SIZE;

        transcfgVal = DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE, _PHYSICAL) |
                      DRF_DEF(_PRGNLCL, _FBIF_TRANSCFG, _TARGET,   _LOCAL_FB);
    }
# endif // LW_RISCV_AMAP_FBGPA_START

    if (!bApertureFound)
    {
        dbgPuts(LEVEL_ALWAYS, "Bootloader: couldn't match elf and loadbase to an aperture!\n");
        dbgExit(BOOTLOADER_ERROR_LOAD_FAILURE);
        // unreached
    }

    // Configure aperture
    if ((intioRead(LW_PRGNLCL_FBIF_TRANSCFG((LwU32)pState->dmaIdx)) & transcfgVal) != transcfgVal)
    {
        // Don't needlessly set TRANSCFG in cases when it could be readonly. See if it fits our current needs first.
        intioWrite(LW_PRGNLCL_FBIF_TRANSCFG((LwU32)pState->dmaIdx), transcfgVal);
    }

    if (DRF_IDX_VAL(_PRGNLCL, _FBIF_REGIONCFG, _T, ((LwU32)pState->dmaIdx), regioncfgVals) != ((LwU32)pState->wprId))
    {
        // Don't needlessly set REGIONCFG in cases when it could be readonly. See if it fits our current needs first.
        intioWrite(LW_PRGNLCL_FBIF_REGIONCFG,
                   FLD_IDX_SET_DRF_NUM(_PRGNLCL, _FBIF_REGIONCFG, _T, ((LwU32)pState->dmaIdx), ((LwU32)pState->wprId), regioncfgVals));
    }
}

/*!
 * @brief memcpy-like DMA mem transfer. Supports FB->TCM, Sysmem->TCM, Sysmem<->Sysmem and FB<->FB.
 *        Falls back to normal memcpy for transfers to EMEM or non-256-byte-aligned IMEM addresses.
 *
 * @param[in] pState  State structure.
 * @param[in] dest    Destination PA (must be in IMEM, DMEM or the aperture identified by _elfInitDma).
 * @param[in] src     Source PA, expected to be in the ELF (must be in the aperture).
 * @param[in] size    Size, in bytes, of the data to be transferred.
 */
TSTATIC void _elfLoadDmaTransfer(const LW_ELF_STATE *pState, void *dest, const void *src, LwLength size)
{
    LwUPtr srcPa  = (LwUPtr)src;
    LwUPtr destPa = (LwUPtr)dest;

    LwUPtr srcPaOffset  = srcPa - pState->apertureBase;
    LwUPtr destPaOffset = destPa - pState->apertureBase;

    if (!utilCheckWithin(pState->apertureBase, pState->apertureSize, srcPa, size))
    {
        dbgPuts(LEVEL_ALWAYS, "Bootloader: cannot DMA from non-aperture source, addr = ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, srcPa);
        dbgPuts(LEVEL_ALWAYS, ", size = ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, size);
        dbgPuts(LEVEL_ALWAYS, "\n");
        dbgExit(BOOTLOADER_ERROR_LOAD_FAILURE);
        // unreached
    }

    if (utilCheckWithin(pState->apertureBase, pState->apertureSize, destPa, size))
    {
        while (size != 0U)
        {
            LwU64 lwrrSize = LW_MIN(size, DMEM_DMA_BUFFER_SIZE);

            if (dmaPa(DMEM_DMA_BUFFER_BASE, srcPaOffset, lwrrSize, pState->dmaIdx, LW_TRUE) != FLCN_OK)
            {
                dbgPuts(LEVEL_ALWAYS, "Bootloader - DMA to DMEM tmp buffer failed!\n");
                dbgExit(BOOTLOADER_ERROR_LOAD_FAILURE);
            }

            if (dmaPa(DMEM_DMA_BUFFER_BASE, destPaOffset, lwrrSize, pState->dmaIdx, LW_FALSE) != FLCN_OK)
            {
                dbgPuts(LEVEL_ALWAYS, "Bootloader - DMA from DMEM tmp buffer failed!\n");
                dbgExit(BOOTLOADER_ERROR_LOAD_FAILURE);
            }

            size         -= lwrrSize;
            srcPaOffset  += lwrrSize;
            destPaOffset += lwrrSize;
        }
    }
    else if ((utilCheckWithin(LW_RISCV_AMAP_IMEM_START, IMEM_SIZE, destPa, size) &&
              ((srcPaOffset % DMA_BLOCK_SIZE_MAX) == 0U) && ((size % DMA_BLOCK_SIZE_MAX) == 0U)
             ) ||
             utilCheckWithin(LW_RISCV_AMAP_DMEM_START, DMEM_SIZE, destPa, size))
    {
        //
        // IMEM DMAs have stricter alignment requirements because we can't control block size for them.
        // DMEM DMAs do not have this restriction.
        //
        if (dmaPa(destPa, srcPaOffset, size, pState->dmaIdx, LW_TRUE) != FLCN_OK)
        {
            dbgPuts(LEVEL_ALWAYS, "Bootloader - attempted DMA to TCM, failed, addr = ");
            dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, destPa);
            dbgPuts(LEVEL_ALWAYS, ", size = ");
            dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, size);
            dbgPuts(LEVEL_ALWAYS, "\n");
            dbgExit(BOOTLOADER_ERROR_LOAD_FAILURE);
            // unreached
        }
    }
    else
    {
        // Use normal memcpy thing for fallback (in case of EMEM, or in case of unaligned IMEM transfers)
        memcpy_(dest, src, size);
    }
}

/*!
 * @brief memset-like DMA utility function for zeroing out memory in the aperture.
 *        Falls back to normal memset for non-aperture addresses.
 *
 * @param[in] pState  State structure.
 * @param[in] dest    Destination PA (must be in the aperture identified by _elfInitDma).
 * @param[in] size    Size, in bytes, of the data to be zeroed.
 */
TSTATIC void _elfLoadDmaZeroPage(const LW_ELF_STATE *pState, void *dest, LwLength size)
{
    // With DMA, we can use a zero-page to do memset(0)
    LwUPtr destPa = (LwUPtr)dest;
    LwUPtr destPaOffset = destPa - pState->apertureBase;

    if (utilCheckWithin(pState->apertureBase, pState->apertureSize, destPa, size))
    {
        while (size != 0U)
        {
            LwU64 lwrrSize = LW_MIN(size, DMEM_DMA_ZERO_BUFFER_SIZE);

            if (dmaPa(DMEM_DMA_ZERO_BUFFER_BASE, destPaOffset, lwrrSize, pState->dmaIdx, LW_FALSE) != FLCN_OK)
            {
                dbgPuts(LEVEL_ALWAYS, "Bootloader - DMA from DMEM zero-buffer failed, addr = ");
                dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, destPa);
                dbgPuts(LEVEL_ALWAYS, ", size = ");
                dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, size);
                dbgPuts(LEVEL_ALWAYS, "\n");
                dbgExit(BOOTLOADER_ERROR_LOAD_FAILURE);
                // unreached
            }

            size         -= lwrrSize;
            destPaOffset += lwrrSize;
        }
    }
    else
    {
        // Use normal memset for fallback for zeroing out non-aperture addrs
        memset_(dest, 0, size);
    }
}

#endif // ELF_LOAD_USE_DMA

/*!
 * @brief Validates that the memory is within the loader image.
 *
 * @param[in]   BASE    Base address of the memory to check.
 * @param[in]   SIZE    Number of bytes of the memory to check.
 *
 * @return 1 if it is within the loader, 0 otherwise.
 */
#define PTR_WITHIN_LOADER(BASE, SIZE) \
    utilCheckWithin(pState->loaderBase, pState->loaderSize, (LwUPtr)(BASE), SIZE)

/*!
 * @brief Checks whether the memory overlaps with the loader image.
 *
 * @param[in]   BASE    Base address of the memory to check.
 * @param[in]   SIZE    Number of bytes of the memory to check.
 *
 * @return 1 if it overlaps with the loader, 0 otherwise.
 */
#define PTR_OVERLAPS_LOADER(BASE, SIZE) \
    utilCheckOverlap(pState->loaderBase, pState->loaderSize, (LwUPtr)(BASE), SIZE)

/*!
 * @brief Validates that the memory is within the loader image and does not cause integer overflow.
 *
 * @param[in]   BASE    Base address of the memory to check.
 * @param[in]   SIZE    Number of bytes of the memory to check.
 *
 * @return TRUE if it is valid and within the loader, FALSE otherwise.
 */
#define PTR_WITHIN_LOADER_NO_OVERFLOW(BASE, SIZE) \
    (PTR_WITHIN_LOADER(BASE, SIZE) && !utilPtrDoesOverflow((LwUPtr)(BASE), SIZE))

/*!
 * @brief Checks whether the memory overlaps with the loader image or causes integer overflow.
 *
 * @param[in]   BASE    Base address of the memory to check.
 * @param[in]   SIZE    Number of bytes of the memory to check.
 *
 * @return TRUE if it is invalid or overlaps the bootloader, FALSE otherwise.
 */
#define PTR_OVERLAPS_LOADER_OR_OVERFLOWS(BASE, SIZE) \
    (PTR_OVERLAPS_LOADER(BASE, SIZE) || utilPtrDoesOverflow((LwUPtr)(BASE), SIZE))

/*!
 * @brief Updates the lowestVa and highestVa variables if necessary.
 *
 * @param[in]   BASE    Base address of the virtual memory region.
 * @param[in]   SIZE    Number of bytes in the virtual memory region.
 */
#define PTR_VA_BOUNDS_UPDATE(BASE, SIZE) do { \
    if ((BASE) < pState->lowestVa) \
    { \
        pState->lowestVa = BASE; \
    } \
    if (((BASE) + (SIZE)) > pState->highestVa) \
    { \
        pState->highestVa = (BASE) + (SIZE); \
    } \
} while (LW_FALSE)

/*!
 * @brief Returns whether section is configured by the linker script as run-in-place.
 *
 * @param[in] physAddr Physical address of section BEFORE _elfPhysAddrMap
 *
 * @return LW_TRUE if is mapped as run-in-place, LW_FALSE otherwise
 */
TSTATIC LwBool
_elfIsAddrMappedRunInPlace
(
    LwUPtr physAddr
)
{
    return ELF_IN_PLACE && ((physAddr >> REMAPPED_PHYS_SHIFT) == REMAPPED_PHYS_BASE_ELF);
}

/*!
 * @brief Returns whether section is configured by the linker script as ODP copy-on-write run-in-place.
 *        Essentially, it's just an identifier for DMEM ODP sections when ELF_IN_PLACE_FULL_ODP_COW is on.
 *
 * @param[in] physAddr Physical address of section BEFORE _elfPhysAddrMap
 *
 * @return LW_TRUE if is mapped as ODP-COW-run-in-place, LW_FALSE otherwise
 */
TSTATIC LwBool
_elfIsAddrMappedODPCOW
(
    LwUPtr physAddr
)
{
    return ELF_IN_PLACE_FULL_ODP_COW && ((physAddr >> REMAPPED_PHYS_SHIFT) == REMAPPED_PHYS_BASE_ELF_ODP_COW);
}

/*!
 * @brief If required, remaps a physical address to an in-place ELF physical address.
 *
 * @param[in] pState State structure.
 * @param[in] pPhdr  The definition of the Phdr to check. If NULL, all pHdrs are checked.
 * @param[in] addr   Address to possibly remap.
 *
 * @return Real in-place ELF physical address.
 */
TSTATIC LwUPtr
_elfInPlacePhysAddrMapToRemapped
(
    const LW_ELF_STATE *pState,
    const Elf_Phdr     *pPhdr,
    LwUPtr              addr
)
{
    if (ELF_IN_PLACE &&
        // only do all of this if run-in-place is enabled in the first place
        (
            (
                // Check for the full elf-in-place conditions (compile-time flag enabled and RO WPR disabled)
                ELF_IN_PLACE_FULL_IF_NOWPR && (pState->wprId == 0) &&

                // Check if this is a non-ELF read/write FB addr, since this case is for them specifically
                (((addr) >> REMAPPED_PHYS_SHIFT) == REMAPPED_PHYS_BASE_RW)
            ) ||
            //
            // If the above conditions for r/w sections run-in-place are not met, make sure this addr has the
            // right phys base for read-only run-in-place sections (designated by linker script).
            //
            _elfIsAddrMappedRunInPlace(addr) || _elfIsAddrMappedODPCOW(addr)
        ))
    {
        if (pPhdr == NULL)
        {
            const Elf_Phdr *pPhdrs = pState->pPhdrs;

            for (LwU32 i = 0; i < pState->pEhdr->e_phnum; i++)
            {
                if (utilCheckAddrWithin(pPhdrs[i].p_paddr, pPhdrs[i].p_filesz, addr))
                {
                    pPhdr = &pPhdrs[i];
                    break;
                }
            }
        }

        if (pPhdr == NULL)
        {
            dbgPuts(LEVEL_INFO, "Warning: failed to find addr in phdrs, possibly an optimized-out section: ");
            dbgPutHex(LEVEL_INFO, __riscv_xlen / 4, addr);
            dbgPuts(LEVEL_INFO, "\n");
            return (LwUPtr)NULL;
        }

        if ((pPhdr->p_type == PT_LOAD) && (pPhdr->p_filesz > 0) && // valid section
            (pPhdr->p_memsz == pPhdr->p_filesz) && // section must be entirely contained in file to be run-in-place
            utilCheckAddrWithin(pPhdr->p_paddr, pPhdr->p_filesz, addr))
        {
            return (((LwUPtr)REMAPPED_PHYS_BASE_ELF) << REMAPPED_PHYS_SHIFT) +
                   pPhdr->p_offset + (addr - pPhdr->p_paddr);
        }
        else if (ELF_IN_PLACE_FULL_ODP_COW && _elfIsAddrMappedODPCOW(addr))
        {
            //
            // Override phys base to RW, b.c. with elf-in-place ODP-COW we might have
            // some sections that are marked with the ELF_ODP_COW_PA phys base but are not entirely
            // contained in the file, so they might need to be copied over.
            //
            addr &= ((1UL << REMAPPED_PHYS_SHIFT) - 1U);
            addr += (((LwUPtr)REMAPPED_PHYS_BASE_RW) << REMAPPED_PHYS_SHIFT);
            return addr;
        }
    }

    return (LwUPtr)NULL;
}

/*!
 * @brief Returns whether section is run-in-place.
 *
 * @param[in] pState State structure.
 * @param[in] pPhdr  The definition of the Phdr of the section to check.
 *
 * @return LW_TRUE if is run in place, LW_FALSE otherwise
 */
TSTATIC LwBool
_elfIsRunInPlaceSection
(
    const LW_ELF_STATE *pState,
    const Elf_Phdr *pPhdr
)
{
    LwUPtr remappedAddr = _elfInPlacePhysAddrMapToRemapped(pState, pPhdr, pPhdr->p_paddr);

    return ((remappedAddr >> REMAPPED_PHYS_SHIFT) == REMAPPED_PHYS_BASE_ELF);
}

/*!
 * @brief If required, remaps a physical address.
 *
 * @param[in] pState State structure.
 * @param[in] pPhdr  The definition of the Phdr to check. If NULL, all pHdrs are checked.
 * @param[in] addr   Address to possibly remap.
 *
 * @return Real physical address.
 */
TSTATIC LwUPtr
_elfPhysAddrMap
(
    const LW_ELF_STATE *pState,
    const Elf_Phdr     *pPhdr,
    LwUPtr              addr
)
{
    if (ELF_IN_PLACE)
    {
        // If in-place ELF is enabled, try it first
        LwUPtr elfInPlaceAddr = _elfInPlacePhysAddrMapToRemapped(pState, pPhdr, addr);

        if (elfInPlaceAddr != (LwUPtr)NULL)
        {
            addr = elfInPlaceAddr;
        }

        // Even if elfInPlaceAddr == NULL, it could still be a heap-only section (which are NOBITS), so try with non-run-in-place
    }

    if ((addr >> REMAPPED_PHYS_SHIFT) >= REMAPPED_PHYS_BASE_RW)
    {
        LwU32 loadBase = (addr >> REMAPPED_PHYS_SHIFT) & REMAPPED_PHYS_MASK;

        if (loadBase < pState->loadBaseCount)
        {
            return (addr & ((1UL << REMAPPED_PHYS_SHIFT) - 1U)) +
                    pState->loadBases[loadBase];
        }
    }

    // If the above options didn't work, this is probably a TCM address, which doesn't need to be remapped.
    return addr;
}

/*!
 * @brief Validates a physical address.
 *
 * @param[in] pState State structure.
 * @param[in] addr   Address to validate.
 *
 * @return 1 on success, 0 on failure.
 */
TSTATIC int
_elfPhysAddrValidate
(
    const LW_ELF_STATE *pState,
    LwUPtr addr
)
{
    if ((addr >> REMAPPED_PHYS_SHIFT) >= REMAPPED_PHYS_BASE_RW)
    {
        LwU32 index = (addr >> REMAPPED_PHYS_SHIFT) & REMAPPED_PHYS_MASK;
        // Verify that the specified load base is present.
        if (index >= pState->loadBaseCount)
        {
            dbgPuts(LEVEL_CRIT, "Relocated PA in undefined loadbase: ");
            dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, addr);
            dbgPuts(LEVEL_CRIT, "\n");
            return 0;
        }
    }
    else
    {
        // TODO: Can do more validation here (such as ensuring the address is real).
    }
    return 1;
}

/*!
 * @brief Validates an ELF Ehdr.
 *
 * @param[in]   pEhdr  ELF's Ehdr.
 *
 * @return 1 on success, 0 on failure.
 */
TSTATIC int
_elfValidateEhdr
(
    const Elf_Ehdr *pEhdr
)
{
    if (!IS_ELF(*pEhdr))
    {
        dbgPuts(LEVEL_CRIT, "Not a valid ELF file\n");
        return 0;
    }

#if __riscv_xlen == 32
    if (pEhdr->e_ident[4] != ELFCLASS32)
#elif __riscv_xlen == 64
    if (pEhdr->e_ident[4] != ELFCLASS64)
#endif
    {
        dbgPuts(LEVEL_CRIT, "Wrong class\n");
        return 0;
    }

    if (pEhdr->e_ident[5] != ELFDATA2LSB)
    {
        dbgPuts(LEVEL_CRIT, "Wrong endian\n");
        return 0;
    }

    if (pEhdr->e_type != ET_EXEC)
    {
        dbgPuts(LEVEL_CRIT, "Not an exelwtable\n");
        return 0;
    }

    if (pEhdr->e_machine != EM_RISCV)
    {
        dbgPuts(LEVEL_CRIT, "Not a RISC-V binary\n");
        return 0;
    }

    if (pEhdr->e_phoff & RISCV_ALIGNMENT_MASK)
    {
        dbgPuts(LEVEL_CRIT, "Phdr not aligned\n");
        return 0;
    }

    if (pEhdr->e_shoff & RISCV_ALIGNMENT_MASK)
    {
        dbgPuts(LEVEL_CRIT, "Shdr not aligned\n");
        return 0;
    }

    return 1;
}

/*!
 * @brief Computes the size of an ELF file from its Ehdr.
 *
 * @param[in]  pEhdr  Pointer to the Ehdr of the ELF to measure.
 *
 * @return The size in bytes of the ELF file.
 */
TSTATIC LwU64
_elfComputeSize
(
    const Elf_Ehdr *pEhdr
)
{
    //
    // The section headers are at the end of the ELF file,
    // so we can use them to callwlate the size of the ELF.
    //
    return pEhdr->e_shoff + pEhdr->e_shnum * sizeof(Elf_Shdr);
}

/*!
 * @brief Validates program headers.
 *
 * Specific checks in this function:
 * - Only PT_LOAD phdrs are allowed.
 * - File size is <= memory size for each phdr.
 * - Physical address is valid.
 * - paddr and vaddr are aligned if MPU is enabled.
 * - paddr == vaddr if MPU is disabled.
 * - Does any section overlap the bootloader?
 * - Does any section overlap another section?
 * - Does any section attempt to write outside the allowed areas?
 *
 * @param[inout] pState     State structure.
 * @param[in]    pPhdrs     Array of ELF Phdrs.
 * @param[in]    phdrCount  Number of Phdrs.
 *
 * @return 1 on success, 0 on failure.
 */
TSTATIC int
_elfValidatePhdrs
(
    LW_ELF_STATE *pState,
    const Elf_Phdr *pPhdrs,
    LwU16 phdrCount
)
{
    LwUPtr physAddr, mpuAddr, mpuSize;

    for (LwU16 i = 0; i < phdrCount; i++)
    {
        if (pPhdrs[i].p_type == PT_LOAD)
        {
            dbgPuts(LEVEL_TRACE, "Check section ");
            dbgPutHex(LEVEL_TRACE, 4, i);
            dbgPuts(LEVEL_TRACE, "\n");
            if (pPhdrs[i].p_filesz > pPhdrs[i].p_memsz)
            {
                dbgPuts(LEVEL_CRIT, "Section 0x");
                dbgPutHex(LEVEL_CRIT, 4, i);
                dbgPuts(LEVEL_CRIT, ": Section filesz > memsz, invalid!\n");
                return 0;
            }

            if (!PTR_WITHIN_LOADER_NO_OVERFLOW(pState->pElf + pPhdrs[i].p_offset,
                                               pPhdrs[i].p_filesz))
            {
                dbgPuts(LEVEL_CRIT, "Section 0x");
                dbgPutHex(LEVEL_CRIT, 4, i);
                dbgPuts(LEVEL_CRIT, ": Phdr's data pointer is invalid.\n");
                return 0;
            }

            if (!_elfPhysAddrValidate(pState, pPhdrs[i].p_paddr))
            {
                dbgPuts(LEVEL_CRIT, "Section 0x");
                dbgPutHex(LEVEL_CRIT, 4, i);
                dbgPuts(LEVEL_CRIT, ": Phdr.paddr is invalid\n");
                return 0;
            }

            if (utilPtrDoesOverflow(pPhdrs[i].p_paddr, pPhdrs[i].p_memsz))
            {
                dbgPuts(LEVEL_CRIT, "Section 0x");
                dbgPutHex(LEVEL_CRIT, 4, i);
                dbgPuts(LEVEL_CRIT, ": Section size overflows memory space before addr remapping\n");
                return 0;
            }

            physAddr = _elfPhysAddrMap(pState, &pPhdrs[i], pPhdrs[i].p_paddr);

            if (pState->pMpuInfo->mpuSettingType != LW_RISCV_MPU_SETTING_TYPE_MBARE)
            {
                if ((pPhdrs[i].p_vaddr & MPU_GRANULARITY_MASK) != 0)
                {
                    dbgPuts(LEVEL_CRIT, "Section 0x");
                    dbgPutHex(LEVEL_CRIT, 4, i);
                    dbgPuts(LEVEL_CRIT, ": Phdr.vaddr must be aligned to MPU granularity\n");
                    dbgPuts(LEVEL_CRIT, "Phdr.vaddr = ");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pPhdrs[i].p_vaddr);
                    dbgPuts(LEVEL_CRIT, "\n");
                    return 0;
                }

                if ((physAddr & MPU_GRANULARITY_MASK) != 0)
                {
                    dbgPuts(LEVEL_CRIT, "Section 0x");
                    dbgPutHex(LEVEL_CRIT, 4, i);
                    dbgPuts(LEVEL_CRIT, ": Phdr.physAddr must be aligned to MPU granularity\n");
                    dbgPuts(LEVEL_CRIT, "Phdr.physAddr = ");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pPhdrs[i].p_paddr);
                    dbgPuts(LEVEL_CRIT, "\n");
                    dbgPuts(LEVEL_CRIT, "Remapped physAddr = ");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, physAddr);
                    dbgPuts(LEVEL_CRIT, "\n");
                    return 0;
                }
                mpuAddr = MPU_CALC_BASE(pPhdrs[i].p_vaddr, pPhdrs[i].p_memsz);
                mpuSize = MPU_CALC_SIZE(pPhdrs[i].p_vaddr, pPhdrs[i].p_memsz);
            }
            else
            {
                mpuAddr = pPhdrs[i].p_paddr;
                mpuSize = pPhdrs[i].p_memsz;

                if (pPhdrs[i].p_vaddr != pPhdrs[i].p_paddr)
                {
                    dbgPuts(LEVEL_CRIT, "Section 0x");
                    dbgPutHex(LEVEL_CRIT, 4, i);
                    dbgPuts(LEVEL_CRIT, ": Phdr.vaddr must equal Phdr.paddr in Mbare mode\n");
                    return 0;
                }
            }

            if (utilPtrDoesOverflow(physAddr, pPhdrs[i].p_memsz))
            {
                dbgPuts(LEVEL_CRIT, "Section 0x");
                dbgPutHex(LEVEL_CRIT, 4, i);
                dbgPuts(LEVEL_CRIT, ": Section size overflows memory space\n");
                return 0;
            }

            if (utilPtrDoesOverflow(mpuAddr, mpuSize))
            {
                dbgPuts(LEVEL_CRIT, "Section 0x");
                dbgPutHex(LEVEL_CRIT, 4, i);
                dbgPuts(LEVEL_CRIT, ": Section's MPU-aligned size overflows memory space\n");
                return 0;
            }

            if (_elfIsRunInPlaceSection(pState, &pPhdrs[i]) || _elfIsAddrMappedRunInPlace(pPhdrs[i].p_paddr))
            {
                //
                // MMINTS-TODO: elf-in-place ODP COW sections actually might need the else check as well.
                // Need to consider this and figure out a good way to do that.
                //

                // Make sure run-in-place sections are fully contained in ELF.
                if (!utilCheckWithin((LwUPtr)pState->pElf, pState->elfSize, physAddr, pPhdrs[i].p_memsz))
                {
                    dbgPuts(LEVEL_CRIT, "Section 0x");
                    dbgPutHex(LEVEL_CRIT, 4, i);
                    dbgPuts(LEVEL_CRIT, ": Section is run-in-place but not fully contained in elf\nSection start: '0x");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, physAddr);
                    dbgPuts(LEVEL_CRIT, "'\nSection mem size: '0x");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pPhdrs[i].p_memsz);
                    dbgPuts(LEVEL_CRIT, "'\nElf start: '0x");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, (LwUPtr) pState->pElf);
                    dbgPuts(LEVEL_CRIT, "'\nElf size: '0x");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pState->elfSize);
                    dbgPuts(LEVEL_CRIT, "'\n");
                    return 0;
                }

                // sanity check: section must be read-only, but only if we're in read-only WPR.
                if (_elfIsAddrMappedRunInPlace(pPhdrs[i].p_paddr) && (pPhdrs[i].p_flags & PF_W))
                {
                    dbgPuts(LEVEL_CRIT, "Section mapped by linker as run-in-place should have been read-only but is writable: Section 0x");
                    dbgPutHex(LEVEL_CRIT, 4, i);
                    dbgPuts(LEVEL_CRIT, "\n");
                    return 0;
                }

                // sanity check: mem size must equal file size.
                if (pPhdrs[i].p_memsz != pPhdrs[i].p_filesz)
                {
                    dbgPuts(LEVEL_CRIT, "Run-in-place section should have mem size equal to file size but does not: Section 0x");
                    dbgPutHex(LEVEL_CRIT, 4, i);
                    dbgPuts(LEVEL_CRIT, "\nMem size: '0x");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pPhdrs[i].p_memsz);
                    dbgPuts(LEVEL_CRIT, "'\nFile size: '0x");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pPhdrs[i].p_filesz);
                    dbgPuts(LEVEL_CRIT, "'\n");
                    return 0;
                }
            }
            else
            {
                //
                // Fail if section overlaps bootloader and is not run-in-place.
                // (in this case the term "bootloader" includes the bootloader
                // image itself, as well as the elf image).
                //
                if (PTR_OVERLAPS_LOADER(physAddr, pPhdrs[i].p_memsz))
                {
                    dbgPuts(LEVEL_CRIT, "Phdr overlap failure!\n1ST_LO: '0x");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pState->loaderBase);
                    dbgPuts(LEVEL_CRIT, "'\n1ST_SZ: '0x");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pState->loaderSize);
                    dbgPuts(LEVEL_CRIT, "'\n2ND_LO: '0x");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, physAddr);
                    dbgPuts(LEVEL_CRIT, "'\n2ND_SZ: '0x");
                    dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pPhdrs[i].p_memsz);
                    dbgPuts(LEVEL_CRIT, "'\n");
                    dbgPuts(LEVEL_CRIT, "Section 0x");
                    dbgPutHex(LEVEL_CRIT, 4, i);
                    dbgPuts(LEVEL_CRIT, " overlaps bootloader\n");
                    return 0;
                }
            }

            // Fail if section overlaps another section.
            // For performance, only iterate sections before this one.
            for (LwU16 other = 0; other < i; other++)
            {
                if (pPhdrs[other].p_type == PT_LOAD)
                {
                    // Verify PA doesn't overlap.
                    LwUPtr mpuAddrOther;
                    LwUPtr mpuSizeOther;
                    LwUPtr physAddrOther;
                    physAddrOther = _elfPhysAddrMap(pState, &pPhdrs[other], pPhdrs[other].p_paddr);
                    if (utilCheckOverlap(physAddrOther, pPhdrs[other].p_memsz,
                                         physAddr, pPhdrs[i].p_memsz))
                    {
                        dbgPuts(LEVEL_CRIT, "Phdr overlap failure!\n1ST_LO: '0x");
                        dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, physAddrOther);
                        dbgPuts(LEVEL_CRIT, "'\n1ST_SZ: '0x");
                        dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pPhdrs[other].p_memsz);
                        dbgPuts(LEVEL_CRIT, "'\n2ND_LO: '0x");
                        dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, physAddr);
                        dbgPuts(LEVEL_CRIT, "'\n2ND_SZ: '0x");
                        dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pPhdrs[i].p_memsz);
                        dbgPuts(LEVEL_CRIT, "'\n");
                        dbgPuts(LEVEL_CRIT, "Section 0x");
                        dbgPutHex(LEVEL_CRIT, 4, i);
                        dbgPuts(LEVEL_CRIT, ": Section's physical placement overlaps other section ");
                        dbgPutHex(LEVEL_CRIT, 4, other);
                        dbgPuts(LEVEL_CRIT, "\n");
                        return 0;
                    }

                    // Verify VA doesn't overlap.
                    if (pState->pMpuInfo->mpuSettingType != LW_RISCV_MPU_SETTING_TYPE_MBARE)
                    {
                        mpuAddrOther = MPU_CALC_BASE(pPhdrs[other].p_vaddr, pPhdrs[other].p_memsz);
                        mpuSizeOther = MPU_CALC_SIZE(pPhdrs[other].p_vaddr, pPhdrs[other].p_memsz);
                        if (utilCheckOverlap(mpuAddrOther, mpuSizeOther, mpuAddr, mpuSize))
                        {
                            dbgPuts(LEVEL_CRIT, "Phdr overlap failure!\n1ST_LO: '0x");
                            dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, mpuAddrOther);
                            dbgPuts(LEVEL_CRIT, "'\n1ST_SZ: '0x");
                            dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, mpuSizeOther);
                            dbgPuts(LEVEL_CRIT, "'\n2ND_LO: '0x");
                            dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, mpuAddr);
                            dbgPuts(LEVEL_CRIT, "'\n2ND_SZ: '0x");
                            dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, mpuSize);
                            dbgPuts(LEVEL_CRIT, "'\n");
                            dbgPuts(LEVEL_CRIT, "Section 0x");
                            dbgPutHex(LEVEL_CRIT, 4, i);
                            dbgPuts(LEVEL_CRIT, ": Section's virtual placement overlaps other section ");
                            dbgPutHex(LEVEL_CRIT, 4, other);
                            dbgPuts(LEVEL_CRIT, "\n");
                            return 0;
                        }
                    }
                }
            }

            // Placement in DMEM is ok, except for the canary+stack+perfctr+dbg area.
            // Placement in IMEM is ok.
            // Placement in EMEM is ok.
            // Placement in FB is ok, as long as it's within the load region.
            // Run in place is ok, if sections are non-TCM r/o
            if (!utilCheckWithin(LW_RISCV_AMAP_DMEM_START, DMEM_CONTENTS_BASE - LW_RISCV_AMAP_DMEM_START,
                                 physAddr, pPhdrs[i].p_memsz) &&
                !utilCheckWithin(LW_RISCV_AMAP_IMEM_START, IMEM_SIZE,
                                 physAddr, pPhdrs[i].p_memsz) &&
                !utilCheckWithin(LW_RISCV_AMAP_EMEM_START, EMEM_SIZE,
                                 physAddr, pPhdrs[i].p_memsz) &&
                !utilCheckWithin(pState->reservedBase, pState->reservedSize,
                                 physAddr, pPhdrs[i].p_memsz) &&
                !utilCheckWithin((LwUPtr) pState->pElf, pState->elfSize,
                                 physAddr, pPhdrs[i].p_memsz))
            {
                dbgPuts(LEVEL_CRIT, "Section 0x");
                dbgPutHex(LEVEL_CRIT, 4, i);
                dbgPuts(LEVEL_CRIT, " phys 0x");
                dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, physAddr);
                dbgPuts(LEVEL_CRIT, " - 0x");
                dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, physAddr + pPhdrs[i].p_memsz);
                dbgPuts(LEVEL_CRIT, " not within valid region. \lwalid regions:\n");
                dbgPuts(LEVEL_CRIT, "DMEM: '0x");
                dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, LW_RISCV_AMAP_DMEM_START);
                dbgPuts(LEVEL_CRIT, "' for '0x");
                dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, DMEM_STACK_BASE - LW_RISCV_AMAP_DMEM_START);
                dbgPuts(LEVEL_CRIT, "'\nIMEM: '0x");
                dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, LW_RISCV_AMAP_IMEM_START);
                dbgPuts(LEVEL_CRIT, "' for '0x");
                dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, IMEM_SIZE);
                dbgPuts(LEVEL_CRIT, "'\nEMEM: '0x");
                dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, LW_RISCV_AMAP_EMEM_START);
                dbgPuts(LEVEL_CRIT, "' for '0x");
                dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, EMEM_SIZE);
                dbgPuts(LEVEL_CRIT, "'\nFB: '0x");
                dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pState->reservedBase);
                dbgPuts(LEVEL_CRIT, "' for '0x");
                dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pState->reservedSize);
                dbgPuts(LEVEL_CRIT, "'\n");
                // Fail if we weren't in any of the above sections.
                return 0;
            }

            PTR_VA_BOUNDS_UPDATE(mpuAddr, mpuSize);
        }
        else
        {
            dbgPuts(LEVEL_CRIT, "Section 0x");
            dbgPutHex(LEVEL_CRIT, 4, i);
            dbgPuts(LEVEL_CRIT, ": Unhandled phdr type '0x");
            dbgPutHex(LEVEL_CRIT, 8, pPhdrs[i].p_type);
            dbgPuts(LEVEL_CRIT, "'\n");
            return 0;
        }
    }
    return 1;
}

/*!
 * @brief Validates and updates state variables for section header string table.
 *
 * The specific checks are:
 * - Section index is valid.
 * - Section used for shstrtab is SHT_STRTAB typed.
 * - shstrtab isn't empty and ends in NUL.
 *
 * @param[inout] pState State structure.
 * @param[in]    pEhdr  ELF Ehdr.
 * @param[in]    pShdrs Array of ELF Shdrs.
 *
 * @return 1 on success, 0 on failure.
 */
TSTATIC int
_elfHandleShstrtab
(
    LW_ELF_STATE *pState,
    const Elf_Ehdr *pEhdr,
    const Elf_Shdr *pShdrs
)
{
    // Make sure shstrndx is valid.
    if (pEhdr->e_shstrndx >= pEhdr->e_shnum)
    {
        dbgPuts(LEVEL_ERROR, "e_shstrndx is not a valid section\n");
        return 0;
    }

    pState->pShstrtabShdr = &pShdrs[pEhdr->e_shstrndx];
    if (pState->pShstrtabShdr->sh_type != SHT_STRTAB)
    {
        dbgPuts(LEVEL_ERROR, "e_shstrndx doesn't point to a string table\n");
        return 0;
    }

    pState->pShstrtab = (const char*)(pState->pElf + pState->pShstrtabShdr->sh_offset);
    pState->shstrtabSize = pState->pShstrtabShdr->sh_size;
    if (pState->shstrtabSize == 0)
    {
        dbgPuts(LEVEL_ERROR, "shstrtab is empty\n");
        return 0;
    }
    if (!PTR_WITHIN_LOADER_NO_OVERFLOW(pState->pShstrtab, pState->shstrtabSize))
    {
        dbgPuts(LEVEL_ERROR, "shstrtab is not in loader area\n");
        return 0;
    }
    if (pState->pShstrtab[pState->shstrtabSize - 1] != '\0')
    {
        dbgPuts(LEVEL_ERROR, "shstrtab doesn't end in NUL\n");
        return 0;
    }

    return 1;
}

/*!
 * @brief Processes a single Section header.
 *
 * In this case, we use this to check for and save out .LWPU.mpu_info section data.
 *
 * @param[inout] pState State structure.
 *
 * @return 1 on success, 0 on failure.
 */
TSTATIC int
_elfProcessShdr
(
    LW_ELF_STATE *pState,
    const Elf_Shdr *pShdr
)
{
    const char *pShdrName = pState->pShstrtab + pShdr->sh_name;

    // Treat zero-size shdr like it doesn't exist.
    if (pShdr->sh_size == 0)
    {
        return 1;
    }

    // Read in and verify MPU info section.
    if (strcmp_(pShdrName, ".LWPU.mpu_info") == 0)
    {
        if (pShdr->sh_size < sizeof(LW_RISCV_MPU_INFO))
        {
            dbgPuts(LEVEL_CRIT, "MPU info section is too small\n");
            return 0;
        }

        if (pShdr->sh_offset & RISCV_ALIGNMENT_MASK)
        {
            dbgPuts(LEVEL_CRIT, "mpu_info data not aligned\n");
            return 0;
        }

        pState->pMpuInfo = (const LW_RISCV_MPU_INFO*)(pState->pElf + pShdr->sh_offset);

        LwUPtr expectedSize = (pState->pMpuInfo->mpuRegionCount * sizeof(LW_RISCV_MPU_REGION)) +
                              sizeof(LW_RISCV_MPU_INFO);

        // Make sure expectedSize is actually what we wanted it to be.
        if (((expectedSize - sizeof(LW_RISCV_MPU_INFO)) / sizeof(LW_RISCV_MPU_REGION)) !=
            pState->pMpuInfo->mpuRegionCount)
        {
            dbgPuts(LEVEL_CRIT, "Too many manual regions, size overflowed\n");
            return 0;
        }

        if (pShdr->sh_size != expectedSize)
        {
            dbgPuts(LEVEL_CRIT, "MPU info section is wrong size for region count\nGot: 0x");
            dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, pShdr->sh_size);
            dbgPuts(LEVEL_CRIT, " Expected: 0x");
            dbgPutHex(LEVEL_CRIT, __riscv_xlen / 4, expectedSize);
            dbgPuts(LEVEL_CRIT, "\n");
            return 0;
        }
    }
    // Any additional section handling would go here.
    return 1;
}


/*!
 * @brief Validates and processes section headers.
 *
 * The checks are:
 * - Section name shstrtab index is valid.
 *
 * @param[inout] pState State structure.
 * @param[in]    pEhdr  ELF Ehdr.
 * @param[in]    pShdrs Array of ELF Shdrs.
 *
 * @return 1 on success, 0 on failure.
 */
TSTATIC int
_elfHandleShdrs
(
    LW_ELF_STATE *pState,
    const Elf_Ehdr *pEhdr,
    const Elf_Shdr *pShdrs
)
{
    // Validate shdrs and save out information if we care.
    for (LwU16 i = 0; i < pEhdr->e_shnum; i++)
    {
        if (pShdrs[i].sh_name >= pState->shstrtabSize)
        {
            dbgPuts(LEVEL_CRIT, "Shdr's name is invalid\n");
            return 0;
        }

        //
        // Sections should generally reside fully within the ELF file. However,
        // SHT_NOBITS sections don't actually consume any space within the
        // file, hence we treat it as empty for the purposes of this check.
        //
        LwU64 sectionSize = (pShdrs[i].sh_type == SHT_NOBITS) ? 0U : pShdrs[i].sh_size;
        if (!PTR_WITHIN_LOADER_NO_OVERFLOW(pState->pElf + pShdrs[i].sh_offset, sectionSize))
        {
            dbgPuts(LEVEL_CRIT, "Shdr is outside loader area\n");
            return 0;
        }

        if (!_elfProcessShdr(pState, &pShdrs[i]))
        {
            dbgPuts(LEVEL_CRIT, "Failed to process shdr\n");
            return 0;
        }
    }

    return 1;
}

/*!
 * @brief Validates and does initial setup for MPU configuration.
 *
 * The checks are:
 * - vaddr is aligned.
 * - paddr is aligned.
 * - range is aligned.
 * - paddr is valid.
 *
 * @param[inout] pState State structure.
 *
 * @return 1 on success, 0 on failure.
 */
TSTATIC int
_mpuValidate
(
    LW_ELF_STATE *pState
)
{
    LwUPtr sizeNeeded;
    LwUPtr startAddr;

    switch (pState->pMpuInfo->mpuSettingType)
    {
        case LW_RISCV_MPU_SETTING_TYPE_MBARE:
            if (pState->pMpuInfo->mpuRegionCount != 0)
            {
                dbgPuts(LEVEL_CRIT, "Mbare mode must have 0 regions defined.\n");
                return 0;
            }
            // Mbare configuration doesn't need to check anything else.
            return 1;

        case LW_RISCV_MPU_SETTING_TYPE_MANUAL:
            if (pState->pMpuInfo->mpuRegionCount >= MPUIDX_BOOTLOADER_RESERVED)
            {
                dbgPuts(LEVEL_CRIT, "Too many MPU regions\n");
                return 0;
            }
            break;
        case LW_RISCV_MPU_SETTING_TYPE_AUTOMATIC:
            if ((pState->pMpuInfo->mpuRegionCount + pState->pEhdr->e_phnum) >= MPUIDX_BOOTLOADER_RESERVED)
            {
                dbgPuts(LEVEL_CRIT, "Too many MPU regions\n");
                return 0;
            }
            break;
    }

    // Find highest and lowest VAs for manual MPU regions
    for (LwU32 i = 0; i < pState->pMpuInfo->mpuRegionCount; i++)
    {
        // Don't want to check the bottom bit, as it could be 0 or 1, for the enable bit.
        if ((pState->pMpuInfo->mpuRegion[i].vaBase & (MPU_GRANULARITY_MASK & ~1)) != 0)
        {
            dbgPuts(LEVEL_CRIT, "Mpu region VA is unaligned\n");
            return 0;
        }
        if ((pState->pMpuInfo->mpuRegion[i].paBase & MPU_GRANULARITY_MASK) != 0)
        {
            dbgPuts(LEVEL_CRIT, "Mpu region PA is unaligned\n");
            return 0;
        }
        if ((pState->pMpuInfo->mpuRegion[i].range & MPU_GRANULARITY_MASK) != 0)
        {
            dbgPuts(LEVEL_CRIT, "Mpu region range is unaligned\n");
            return 0;
        }
        if (!_elfPhysAddrValidate(pState, pState->pMpuInfo->mpuRegion[i].paBase))
        {
            dbgPuts(LEVEL_CRIT, "Mpu region PA is invalid\n");
            return 0;
        }

        LwUPtr memBase = pState->pMpuInfo->mpuRegion[i].vaBase & ~1ULL;
        LwUPtr memSize = pState->pMpuInfo->mpuRegion[i].range;

        if (utilPtrDoesOverflow(memBase, memSize))
        {
            dbgPuts(LEVEL_CRIT, "Mpu region overflows\n");
            return 0;
        }

        if (memSize > 0)
        {
            PTR_VA_BOUNDS_UPDATE(memBase, memSize);
        }
    }

    // Get MPU aligned base and size for the loader's program.
    pState->loaderMpuPaBase = MPU_CALC_BASE(pState->loaderBase, pState->loaderSize);
    pState->loaderMpuSize = MPU_CALC_SIZE(pState->loaderBase, pState->loaderSize);

    pState->stackMpuPaBase = MPU_CALC_BASE(DMEM_CONTENTS_BASE, DMEM_CONTENTS_SIZE);
    pState->stackMpuSize = MPU_CALC_SIZE(DMEM_CONTENTS_BASE, DMEM_CONTENTS_SIZE);

    // We want gap between every sector + start/end gap
    sizeNeeded = MPU_GRANULARITY +
                 pState->loaderMpuSize + MPU_GRANULARITY +
                 pState->stackMpuSize + MPU_GRANULARITY +
                 pState->bootargsMpuSize + MPU_GRANULARITY;

    if (pState->lowestVa >= sizeNeeded)
    {
        startAddr = 0;
    }
    else if (pState->highestVa < (LwU64)-sizeNeeded)
    {
        startAddr = pState->highestVa;
    }
    else
    {
        // Somehow we managed to not have any holes at beginning or end of VA.
        // That's a problem.
        dbgPuts(LEVEL_ALWAYS, "Can't manage to keep bootloader program in RISCV-VA\n");
        return 0;
    }

    startAddr += MPU_GRANULARITY;

    startAddr += pState->loaderMpuSize + MPU_GRANULARITY;

    startAddr += pState->stackMpuSize + MPU_GRANULARITY;

    pState->bootargsMpuVaBase = startAddr;

    // Determine the actual base and size (align to MPU).
    dbgPuts(LEVEL_INFO, "Low VA '0x");
    dbgPutHex(LEVEL_INFO, __riscv_xlen / 4, pState->lowestVa);
    dbgPuts(LEVEL_INFO, "'\nHigh VA '0x");
    dbgPutHex(LEVEL_INFO, __riscv_xlen / 4, pState->highestVa);
    dbgPuts(LEVEL_INFO, "'\nloaderSize '0x");
    dbgPutHex(LEVEL_INFO, __riscv_xlen / 4, pState->loaderMpuSize);
    dbgPuts(LEVEL_INFO, "'\nstackSize '0x");
    dbgPutHex(LEVEL_INFO, __riscv_xlen / 4, pState->stackMpuSize);
    dbgPuts(LEVEL_INFO, "'\n");

    return 1;
}

/*!
 * @brief Obtains and validates the entry point for the target application.
 *
 * @param[inout]  pState  State structure.
 *
 * @return 1 on success, 0 on failure.
 */
TSTATIC int
_elfGetEntryPoint
(
    LW_ELF_STATE *pState
)
{
    LwUPtr entryPoint = pState->pEhdr->e_entry;

    // In bare mode, the entry point is a physical address.
    if (pState->pMpuInfo->mpuSettingType == LW_RISCV_MPU_SETTING_TYPE_MBARE)
    {
        // Basic valididty check.
        if (!_elfPhysAddrValidate(pState, entryPoint))
        {
            dbgPuts(LEVEL_ALWAYS, "Entrypoint physical address not valid\n");
            return 0;
        }

        // Re-map if neeeded.
        entryPoint = _elfPhysAddrMap(pState, NULL, entryPoint);

        // Check for overflow (very unlikely).
        if (utilPtrDoesOverflow(entryPoint, RISCV_MIN_INSTRUCTION_LENGTH))
        {
            dbgPuts(LEVEL_ALWAYS, "Entrypoint does not point to valid instruction\n");
            return 0;
        }

        // Entry point must belong to a permitted load region (IMEM or reserved space).
        if (!utilCheckWithin(LW_RISCV_AMAP_IMEM_START, IMEM_SIZE,
                             entryPoint, RISCV_MIN_INSTRUCTION_LENGTH) &&
            !utilCheckWithin(pState->reservedBase, pState->reservedSize,
                             entryPoint, RISCV_MIN_INSTRUCTION_LENGTH))
        {
            dbgPuts(LEVEL_ALWAYS, "Entrypoint not within load region\n");
            return 0;
        }
    }

    // All other modes imply a virtual address.
    else
    {
        // Check overflow before checking ranges.
        if (utilPtrDoesOverflow(entryPoint, RISCV_MIN_INSTRUCTION_LENGTH))
        {
            dbgPuts(LEVEL_ALWAYS, "Entrypoint does not point to valid instruction\n");
            return 0;
        }

        //
        // Just do a courtesy check that the entry point lies somewhere
        // within the virtual address space of the application. That prevents
        // us from accidentally utilizing one of the bootloader's MPU mappings
        // to jump to some undefined region. We may still hit a VA hole or
        // a region lacking execute permissions, but then we'll just fault
        // immediately.
        //
        if (!utilCheckWithin(pState->lowestVa, pState->highestVa - pState->lowestVa,
                             entryPoint, RISCV_MIN_INSTRUCTION_LENGTH))
        {
            dbgPuts(LEVEL_ALWAYS, "Entrypoint falls outside of VA range\n");
            return 0;
        }
    }

    // In any case, address should be aligned.
    if ((entryPoint & RISCV_MIN_INSTRUCTION_ALIGNMENT_MASK) != 0)
    {
        dbgPuts(LEVEL_ALWAYS, "Entrypoint is not aligned\n");
        return 0;
    }

    pState->entryPoint = entryPoint;
    return 1;
}

/*!
 * @brief Validates an ELF and sets up the state necessary for further use.
 *
 * @param[out]  pState          State structure.
 * @param[in]   pElf            Pointer to the ELF file in memory.
 * @param[in]   elfSize         Size in bytes of the ELF file.
 * @param[in]   pParams         Pointer to the boot arguments.
 * @param[in]   paramSize       Size of the boot arguments.
 * @param[in]   pLoaderBase     Base address of the loader.
 * @param[in]   loaderSize      Size in bytes of the loader image (this program + params + ELF + padding).
 * @param[in]   pReservedBase   Base address of the reserved RW space for loading.
 * @param[in]   reservedSize    Size in bytes of the reserved RW space for loading.
 * @param[in]   wprId           WPR ID of region we booted from.
 *
 * @return Zero on failure, one on success.
 */
int
elfBegin
(
    LW_ELF_STATE *pState,
    const void *pElf,
    LwUPtr elfSize,
    const LW_RISCV_BOOTLDR_PARAMS *pParams,
    LwUPtr paramSize,
    const void *pLoaderBase,
    LwUPtr loaderSize,
    const void *pReservedBase,
    LwUPtr reservedSize,
    LwU32 wprId
)
{
    pState->pElf              = pElf;
    pState->elfSize           = elfSize;
    pState->bootargsMpuPaBase = MPU_CALC_BASE((LwUPtr)pParams, paramSize);
    pState->bootargsMpuSize   = MPU_CALC_SIZE((LwUPtr)pParams, paramSize);
    pState->loaderBase        = (LwUPtr)pLoaderBase;
    pState->loaderSize        = loaderSize;
    pState->reservedBase      = (LwUPtr)pReservedBase;
    pState->reservedSize      = reservedSize;
    pState->lowestVa          = (LwUPtr)-1;
    pState->highestVa         = 0;
    pState->wprId             = wprId;

    //
    // Lwrrently, we support re-mapping for the pre-reserved RW space
    // that follows the loader image (i.e. right after the ELF) and
    // space within the elf for run-in-place sections, which includes
    // non-TCM-resident RO sections by default (when the elf is RO
    // itself due to WPR), and all non-TCM-resident sections when WPR
    // is disabled and ELF_IN_PLACE_FULL_IF_NOWPR is enabled.
    //

    pState->loadBases[LW_ELF_LOAD_BASE_INDEX_RESERVED_RW]  = pState->reservedBase;
    pState->loadBaseCount = 1;

    // ELF_IN_PLACE
    pState->loadBases[LW_ELF_LOAD_BASE_INDEX_IN_PLACE_ELF] = (LwUPtr)pElf;
    pState->loadBaseCount++;

    // ELF_IN_PLACE_FULL_ODP_COW
    pState->loadBases[LW_ELF_LOAD_BASE_INDEX_ODP_COW] = (LwUPtr)pElf;
    pState->loadBaseCount++;

    if (ELF_IN_PLACE)
    {
        LwBool bIsFullElfInPlace = LW_FALSE;

        if (ELF_IN_PLACE_FULL_ODP_COW)
        {
            dbgPuts(LEVEL_ALWAYS, "Bootloader: full elf-in-place enabled for paged sections due to ELF ODP copy-on-write.\n");
            bIsFullElfInPlace = LW_TRUE;
        }

        if (ELF_IN_PLACE_FULL_IF_NOWPR && (pState->wprId == 0))
        {
            dbgPuts(LEVEL_ALWAYS, "Bootloader: full elf-in-place enabled for all sections b.c. read-only WPR is disabled.\n");
            bIsFullElfInPlace = LW_TRUE;
        }

        if (bIsFullElfInPlace)
        {
            dbgPuts(LEVEL_ALWAYS, "Bootloader: Due to full-elf-in-place all eligible memsize == filesize FB-backed sections are kept in-place.\n");
        }
    }

# if ELF_LOAD_USE_DMA
    _elfInitDma(pState);
# endif // ELF_LOAD_USE_DMA

    if (((LwUPtr)pState->pElf & RISCV_ALIGNMENT_MASK) != 0)
    {
        dbgPuts(LEVEL_ALWAYS, "ELF base is unaligned: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, (LwUPtr)pState->pElf);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    if ((pState->elfSize & RISCV_ALIGNMENT_MASK) != 0)
    {
        dbgPuts(LEVEL_ALWAYS, "ELF size is unaligned: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->elfSize);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    if ((pState->loaderBase & RISCV_ALIGNMENT_MASK) != 0)
    {
        dbgPuts(LEVEL_ALWAYS, "Loader base is unaligned: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->loaderBase);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    if ((pState->loaderSize & RISCV_ALIGNMENT_MASK) != 0)
    {
        dbgPuts(LEVEL_ALWAYS, "Loader size is unaligned: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->loaderSize);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    if ((pState->reservedBase & RISCV_ALIGNMENT_MASK) != 0)
    {
        dbgPuts(LEVEL_ALWAYS, "Reserved base is unaligned: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->reservedBase);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    if ((pState->reservedSize & RISCV_ALIGNMENT_MASK) != 0)
    {
        dbgPuts(LEVEL_ALWAYS, "Reserved size is unaligned: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->reservedSize);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    if (PTR_OVERLAPS_LOADER_OR_OVERFLOWS(pState->reservedBase, pState->reservedSize))
    {
        dbgPuts(LEVEL_ALWAYS, "Reserved space collides with loader: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->reservedBase);
        dbgPuts(LEVEL_ALWAYS, " for ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->reservedSize);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    if (PTR_OVERLAPS_LOADER_OR_OVERFLOWS(pState->bootargsMpuPaBase, pState->bootargsMpuSize))
    {
        dbgPuts(LEVEL_ALWAYS, "Bootargs collide with loader: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->bootargsMpuPaBase);
        dbgPuts(LEVEL_ALWAYS, " for ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->bootargsMpuSize);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    if (utilCheckOverlap(pState->reservedBase, pState->reservedSize,
                         pState->bootargsMpuPaBase, pState->bootargsMpuSize))
    {
        dbgPuts(LEVEL_ALWAYS, "Bootargs collide with reserved space: \n");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->bootargsMpuPaBase);
        dbgPuts(LEVEL_ALWAYS, " for ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->bootargsMpuSize);
        dbgPuts(LEVEL_ALWAYS, "\nwith ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->reservedBase);
        dbgPuts(LEVEL_ALWAYS, " for ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->reservedSize);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    pState->pMpuInfo = &defaultMpuInfo;

    pState->pEhdr = (const Elf_Ehdr*)RISCV_ALIGNMENT_ASSUME(pElf);
    if (!PTR_WITHIN_LOADER_NO_OVERFLOW(pState->pEhdr, sizeof(Elf_Ehdr)))
    {
        dbgPuts(LEVEL_ALWAYS, "Ehdr reaches outside loader area\n");
        return 0;
    }

    if (!_elfValidateEhdr(pState->pEhdr))
    {
        dbgPuts(LEVEL_ALWAYS, "Ehdr validate failed\n");
        return 0;
    }

    //
    // Verify that the actual size of the ELF file we've been given matches
    // the expected size from the boot parameters. This check guards against
    // compilation errors as well as malicious swapping of the ELF payload.
    //
    LwU64 computedSize = _elfComputeSize(pState->pEhdr);
    if (pState->elfSize != computedSize)
    {
        dbgPuts(LEVEL_ALWAYS, "ELF size does not match expected size\nExpected: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->elfSize);
        dbgPuts(LEVEL_ALWAYS, "\nActual: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, computedSize);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    pState->pPhdrs = (const Elf_Phdr*)RISCV_ALIGNMENT_ASSUME(pElf + pState->pEhdr->e_phoff);
    if (!PTR_WITHIN_LOADER_NO_OVERFLOW(pState->pPhdrs, pState->pEhdr->e_phnum * sizeof(Elf_Phdr)))
    {
        dbgPuts(LEVEL_ALWAYS, "Phdrs reach outside loader area\n");
        return 0;
    }

    pState->pShdrs = (const Elf_Shdr*)RISCV_ALIGNMENT_ASSUME(pElf + pState->pEhdr->e_shoff);
    if (!PTR_WITHIN_LOADER_NO_OVERFLOW(pState->pShdrs, pState->pEhdr->e_shnum * sizeof(Elf_Shdr)))
    {
        dbgPuts(LEVEL_ALWAYS, "Shdrs reach outside loader area\n");
        return 0;
    }

    if (!_elfHandleShstrtab(pState, pState->pEhdr, pState->pShdrs))
    {
        dbgPuts(LEVEL_CRIT, "Shstrtab handling failed\n");
        return 0;
    }

    if (!_elfHandleShdrs(pState, pState->pEhdr, pState->pShdrs))
    {
        dbgPuts(LEVEL_ALWAYS, "Shdr validate failed\n");
        return 0;
    }

    if (!_elfValidatePhdrs(pState, pState->pPhdrs, pState->pEhdr->e_phnum))
    {
        dbgPuts(LEVEL_ALWAYS, "Phdr validate failed\n");
        return 0;
    }

    if (!_mpuValidate(pState))
    {
        dbgPuts(LEVEL_ALWAYS, "MPU validation failed\n");
        return 0;
    }

    if (!_elfGetEntryPoint(pState))
    {
        dbgPuts(LEVEL_ALWAYS, "Entrypoint not valid: ");
        dbgPutHex(LEVEL_ALWAYS, __riscv_xlen / 4, pState->pEhdr->e_entry);
        dbgPuts(LEVEL_ALWAYS, "\n");
        return 0;
    }

    return 1;
}

/*!
 * @brief Loads MPU regions into the MPU.
 *
 * @param[inout] pState State structure.
 */
TSTATIC void
_mpuRegionsApply
(
    const LW_ELF_STATE *pState
)
{
    // Current operating MPU index.
    LwU64 mpuIndex = 0;

    dbgPuts(LEVEL_INFO, "Building manual MPU regions\n");
    for (LwU32 i = 0; i < pState->pMpuInfo->mpuRegionCount; i++, mpuIndex++)
    {
        LwUPtr physAddr = _elfPhysAddrMap(pState, NULL, pState->pMpuInfo->mpuRegion[i].paBase);
        LwU64 mpuAttr = pState->pMpuInfo->mpuRegion[i].attribute;

        //
        // Set the WPR ID for regions that fall within the loader carveout.
        // These should only reasonably point to either a location within the
        // ELF or to the pre-reserved RW load region, both of which fall within
        // WPR when in LS.
        //
        if (utilCheckWithin((LwUPtr)pState->pElf, pState->elfSize,
                            physAddr, pState->pMpuInfo->mpuRegion[i].range) ||
            utilCheckWithin(pState->reservedBase, pState->reservedSize,
                            physAddr, pState->pMpuInfo->mpuRegion[i].range))
        {
            mpuAttr = FLD_SET_DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, pState->wprId, mpuAttr);
        }

        csr_write(LW_RISCV_CSR_SMPUIDX, mpuIndex);
        csr_write(LW_RISCV_CSR_SMPUPA, physAddr);
        csr_write(LW_RISCV_CSR_SMPURNG, pState->pMpuInfo->mpuRegion[i].range);
        csr_write(LW_RISCV_CSR_SMPUATTR, mpuAttr);
        csr_write(LW_RISCV_CSR_SMPUVA, pState->pMpuInfo->mpuRegion[i].vaBase);

        dbgPuts(LEVEL_INFO, "!!REGION!!\lwA: '0x");
        dbgPutHex(LEVEL_INFO, __riscv_xlen / 4, pState->pMpuInfo->mpuRegion[i].vaBase);
        dbgPuts(LEVEL_INFO, "'\nPA: '0x");
        dbgPutHex(LEVEL_INFO, __riscv_xlen / 4, physAddr);
        dbgPuts(LEVEL_INFO, "'\nRNG: '0x");
        dbgPutHex(LEVEL_INFO, __riscv_xlen / 4, pState->pMpuInfo->mpuRegion[i].range);
        dbgPuts(LEVEL_INFO, "'\nATTR: '0x");
        dbgPutHex(LEVEL_INFO, __riscv_xlen / 4, mpuAttr);
        dbgPuts(LEVEL_INFO, "'\n");
    }

    // Build regions based on section information.
    if (pState->pMpuInfo->mpuSettingType == LW_RISCV_MPU_SETTING_TYPE_AUTOMATIC)
    {
        dbgPuts(LEVEL_INFO, "Building section MPU regions\n");
        for (LwU16 i = 0; i < pState->pEhdr->e_phnum; i++, mpuIndex++)
        {
            const Elf_Phdr * const pPhdr = &pState->pPhdrs[i];
            LwUPtr physAddr = _elfPhysAddrMap(pState, pPhdr, pPhdr->p_paddr);

            LwUPtr memBase = pPhdr->p_vaddr;
            LwUPtr memSize = LW_ALIGN_UP(pPhdr->p_memsz, MPU_GRANULARITY);
            LwU64  memAttr = 0;

            //
            // WPR is inherited from boot region. Note that this will not work if
            // we're in LS and the region we're setting up here is not in WPR or TCM!
            //
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, pState->wprId);
            // Cacheable bit is bit20 in flags (inside MASKOS); 0(default) is on
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, !((pPhdr->p_flags >> 20) & 1));
            // Coherent bit is bit21 in flags (inside MASKOS); 0(default) is on
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _COHERENT, !((pPhdr->p_flags >> 21) & 1));
            // L2 cache write evict setting is bit23:22 in flags (inside MASKOS); 0(default) is "evict first"
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _L2C_WR, (pPhdr->p_flags >> 22) & 3);
            // L2 cache read evict setting is bit25:24 in flags (inside MASKOS); 0(default) is "evict first"
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _L2C_RD, (pPhdr->p_flags >> 24) & 3);
            // Read privilege
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UR, !!(pPhdr->p_flags & PF_R));
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, !!(pPhdr->p_flags & PF_R));
            // Write privilege
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UW, !!(pPhdr->p_flags & PF_W));
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, !!(pPhdr->p_flags & PF_W));
            // Execute privilege
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _UX, !!(pPhdr->p_flags & PF_X));
            memAttr |= DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SX, !!(pPhdr->p_flags & PF_X));

            csr_write(LW_RISCV_CSR_SMPUIDX, mpuIndex);
            csr_write(LW_RISCV_CSR_SMPUPA, physAddr);
            csr_write(LW_RISCV_CSR_SMPURNG, memSize);
            csr_write(LW_RISCV_CSR_SMPUATTR, memAttr);
            csr_write(LW_RISCV_CSR_SMPUVA, memBase |
                                           DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));

            dbgPuts(LEVEL_INFO, "!!AUTO-REGION!!\lwA: '0x");
            dbgPutHex(LEVEL_INFO, 64 / 4, memBase | 1);
            dbgPuts(LEVEL_INFO, "'\nPA: '0x");
            dbgPutHex(LEVEL_INFO, 64 / 4, physAddr);
            dbgPuts(LEVEL_INFO, "'\nRNG: '0x");
            dbgPutHex(LEVEL_INFO, 64 / 4, memSize);
            dbgPuts(LEVEL_INFO, "'\nATTR: '0x");
            dbgPutHex(LEVEL_INFO, 64 / 4, memAttr);
            dbgPuts(LEVEL_INFO, "'\n");
        }
    }
}

/*!
 * @brief Loads a single Phdr into memory.
 *
 * @param[inout] pState State structure.
 * @param[in]    pPhdr  The definition of the Phdr to load.
 */
TSTATIC void
_elfLoadPhdr
(
    const LW_ELF_STATE *pState,
    const Elf_Phdr     *pPhdr,
    LwU32               platform
)
{
    LwUPtr physAddr = _elfPhysAddrMap(pState, pPhdr, pPhdr->p_paddr);
    LwU8 *destAddr;

    dbgPuts(LEVEL_TRACE, "PHDR attempting load: from '0x");
    dbgPutHex(LEVEL_TRACE, __riscv_xlen / 4, (LwU64)(pState->pElf + pPhdr->p_offset));
    dbgPuts(LEVEL_TRACE, "' for '0x");
    dbgPutHex(LEVEL_TRACE, __riscv_xlen / 4, pPhdr->p_filesz);
    dbgPuts(LEVEL_TRACE, "',\n             to '0x");
    dbgPutHex(LEVEL_TRACE, __riscv_xlen / 4, physAddr);
    dbgPuts(LEVEL_TRACE, "' for '0x");
    dbgPutHex(LEVEL_TRACE, __riscv_xlen / 4, pPhdr->p_memsz);
    dbgPuts(LEVEL_TRACE, "'.\n");

    destAddr = (LwU8*)(physAddr);
    if ((pPhdr->p_filesz > 0) && !_elfIsRunInPlaceSection(pState, pPhdr))
    {
        // memcpy is only needed if this section is not run-in-place.
#if ELF_LOAD_USE_DMA
        _elfLoadDmaTransfer(pState, destAddr, pState->pElf + pPhdr->p_offset, pPhdr->p_filesz);
#else // ELF_LOAD_USE_DMA
        memcpy_(destAddr, pState->pElf + pPhdr->p_offset, pPhdr->p_filesz);
#endif // ELF_LOAD_USE_DMA

        dbgPuts(LEVEL_TRACE, "Done with memcpy to '0x");
        dbgPutHex(LEVEL_TRACE, __riscv_xlen / 4, physAddr);
        dbgPuts(LEVEL_TRACE, "' for '0x");
        dbgPutHex(LEVEL_TRACE, __riscv_xlen / 4, pPhdr->p_memsz);
        dbgPuts(LEVEL_TRACE, "' (unpatched addr '0x");
        dbgPutHex(LEVEL_TRACE, __riscv_xlen / 4, pPhdr->p_paddr);
        dbgPuts(LEVEL_TRACE, "').\n");
    }
    else
    {
        dbgPuts(LEVEL_TRACE, "Skipped loading section at addr '0x");
        dbgPutHex(LEVEL_TRACE, __riscv_xlen / 4, pPhdr->p_paddr);
        dbgPuts(LEVEL_TRACE, "', since it's a run-in-place section.\n");

        return;
    }

    if ((pPhdr->p_memsz - pPhdr->p_filesz) != 0)
    {
        //
        // The ELF format removes trailing zeroes from sections. We add them back here.
        // On Fmodel and emu FB is zero-initialized, so we don't need this.
        //
        // However, in prod, doing so at run-time creates a significant security risk,
        // so we compile out this code when we hit prod.
        //
#ifndef IS_PRESILICON
        (void)platform;
#else // IS_PRESILICON
        if (FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FMODEL, platform) ||
            FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _RTL, platform))
        {
            dbgPuts(LEVEL_INFO, "PRESILICON/Bootloader: sim platform detected, skipping memset for PHDR!\n");
        }
        else
#endif // IS_PRESILICON
        {
#if ELF_LOAD_USE_DMA
            _elfLoadDmaZeroPage(pState, destAddr + pPhdr->p_filesz, pPhdr->p_memsz - pPhdr->p_filesz);
#else  // ELF_LOAD_USE_DMA
            memset_(destAddr + pPhdr->p_filesz, 0, pPhdr->p_memsz - pPhdr->p_filesz);
#endif // ELF_LOAD_USE_DMA
        }
    }
}

/*!
 * @brief Loads the ELF program headers into memory.
 *
 * @param[in] pState State structure.
 */
TSTATIC void
_elfLoadPhdrs
(
    const LW_ELF_STATE *pState,
    LwU32               platform
)
{
    // Load the binary into memory.
    dbgPuts(LEVEL_INFO, "Loading Phdrs into memory\n");
    for (LwU16 i = 0; i < pState->pEhdr->e_phnum; i++)
    {
        if ((pState->pPhdrs[i].p_type == PT_LOAD) && (pState->pPhdrs[i].p_memsz > 0))
        {
            _elfLoadPhdr(pState, &pState->pPhdrs[i], platform);
        }
    }
    dbgPuts(LEVEL_INFO, "Loaded!\n");
}

/*!
 * @brief Configures the MPU for simulated "bare" operation (see COREUCODES-531).
 *
 * @param[in] pLoaderBase   Base address of the bootloader in memory.
 * @param[in] loaderExtent  Total footprint of loader image and pre-reserved load region.
 * @param[in] wprId         WPR ID of region we booted from.
 */
void
_mpuSetIdentityRegions
(
    const void *pLoaderBase,
    LwUPtr loaderExtent,
    LwU32 wprId
)
{
    //
    // Write identity entry for the image carveout (bootloader, params, ELF,
    // padding, and reserved space). No user access. Machine read/write/execute.
    // Cacheable and coherent. WPR ID matches load region.
    //
    csr_write(LW_RISCV_CSR_SMPUIDX, MPUIDX_IDENTITY_CARVEOUT);
    csr_write(LW_RISCV_CSR_SMPUATTR, DRF_NUM64(_RISCV_CSR, _SMPUATTR, _WPR, wprId) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SX, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _COHERENT, 1));

    //
    // Cover everything from the start of the bootloader to the end of the load region.
    // This should include the bootloader itself, boot parameters, application ELF
    // (with padding), and the reserved RW space for loading the ELF sections. It
    // does not include the manifest or FMC, if present, but we shouldn't be touching
    // those anyway.
    //
    const LwU64 carveoutBase = (LwU64)MPU_CALC_BASE((LwUPtr)pLoaderBase, loaderExtent);
    const LwU64 carveoutSize = (LwU64)MPU_CALC_SIZE((LwUPtr)pLoaderBase, loaderExtent);

    csr_write(LW_RISCV_CSR_SMPUVA, carveoutBase | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));
    csr_write(LW_RISCV_CSR_SMPUPA, carveoutBase);
    csr_write(LW_RISCV_CSR_SMPURNG, carveoutSize);

    //
    // Write identity entry for all other memory (WPR ID == 0 always).
    // No user access. Machine read/write. No execute. Cacheable and non-coherent (ignored for TCM).
    //
    csr_write(LW_RISCV_CSR_SMPUIDX, MPUIDX_IDENTITY_ALL);
    csr_write(LW_RISCV_CSR_SMPUVA, 0 | DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));
    csr_write(LW_RISCV_CSR_SMPUPA, 0);
    csr_write(LW_RISCV_CSR_SMPURNG, DRF_NUM64(_RISCV_CSR, _SMPURNG, _RANGE, ~0ULL));
    csr_write(LW_RISCV_CSR_SMPUATTR, DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SW, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _COHERENT, 0));
}

/*!
 * @brief Clears MPU configuration.
 */
void mpuClear(void)
{
    for (int i = 0; i < MPUIDX_COUNT; i++)
    {
        csr_write(LW_RISCV_CSR_SMPUIDX, i);
        csr_write(LW_RISCV_CSR_SMPUVA, 0);
        csr_write(LW_RISCV_CSR_SMPUPA, 0);
        csr_write(LW_RISCV_CSR_SMPURNG, 0);
        csr_write(LW_RISCV_CSR_SMPUATTR, 0);
    }
}

/*!
 * @brief Sets up identity entries in the MPU to work around WPR-ID requirements (see COREUCODES-531).
 *
 * @param[in] pLoaderBase   Base address of the bootloader in memory.
 * @param[in] loaderExtent  Total footprint of loader image and pre-reserved load region.
 * @param[in] wprId         WPR ID of region we booted from.
 */
void
elfSetIdentityRegions
(
    const void *pLoaderBase,
    LwUPtr loaderExtent,
    LwU32 wprId
)
{
    // Clear all MPU settings.
    mpuClear();

    // Create identity regions.
    _mpuSetIdentityRegions(pLoaderBase, loaderExtent, wprId);

    // Enable MPU
    csr_write(LW_RISCV_CSR_SATP,
    DRF_DEF64(_RISCV_CSR, _SATP, _MODE, _LWMPU) |
    DRF_DEF64(_RISCV_CSR, _SATP, _ASID, _BARE)  |
    DRF_DEF64(_RISCV_CSR, _SATP, _PPN, _BARE));
}

/*!
 * @brief Loads a prepared ELF file into memory and exelwtes it.
 *
 * @param[inout] pState         State structure.
 */
void
elfLoad
(
    LW_ELF_STATE *pState
)
{
    //
    // TODO: This function uses a ridilwlous amount of stack, and wastes code space,
    // because it thinks for some reason that it needs to save all the non-volatile registers,
    // despite the function being marked as noreturn...
    //
    // This should be debugged and "fixed".
    //

#ifdef IS_PRESILICON
# if !USE_CBB && defined(LW_RISCV_AMAP_PRIV_START)
    LwU32 platform = bar0ReadPA(LW_PTOP_PLATFORM);
# else // USE_CBB
    // @todo  LW_RISCV_AMAP_PRIV_START is not available in some instances
    LwU32 platform = DRF_DEF(_PTOP, _PLATFORM, _TYPE, _FMODEL);
# endif // USE_CBB
#else // IS_PRESILICON
    LwU32 platform = DRF_DEF(_PTOP, _PLATFORM, _TYPE, _SILICON);
#endif // IS_PRESILICON

    if (FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _FMODEL, platform) ||
        FLD_TEST_DRF(_PTOP, _PLATFORM, _TYPE, _RTL, platform))
    {
        dbgPuts(LEVEL_ALWAYS, "PRESILICON/Bootloader: sim platform detected, will skip memsets for every PHDR! Be wary of unscrubbed FB/Sysmem!\n");
    }

    // Load the target program into memory.
    _elfLoadPhdrs(pState, platform);

    //
    // Unfortunately this can't be brought out to a function (read the big warning below)
    // This turns on the MPU, adjusts pointers, then loads the target program's MPU regions.
    //
    if (pState->pMpuInfo->mpuSettingType != LW_RISCV_MPU_SETTING_TYPE_MBARE)
    {
        // Set the MPU regions.
        _mpuRegionsApply(pState);
    }

    dbgPuts(LEVEL_ALWAYS, "Booting...\n");

    dbgDisable();

    // Write the bootargs MPU mapping. We want that entry always,
    // no matter whether bootloader runs as S or M.
    csr_write(LW_RISCV_CSR_SMPUIDX, MPUIDX_BOOTLOADER_BOOTARGS);
    csr_write(LW_RISCV_CSR_SMPUVA, pState->bootargsMpuVaBase |
                                   DRF_NUM64(_RISCV_CSR, _SMPUVA, _VLD, 1));
    csr_write(LW_RISCV_CSR_SMPUPA, pState->bootargsMpuPaBase);
    csr_write(LW_RISCV_CSR_SMPURNG, pState->bootargsMpuSize);
    // User no access, Machine mode ro, cacheable, non-coherent
    csr_write(LW_RISCV_CSR_SMPUATTR, DRF_NUM64(_RISCV_CSR, _SMPUATTR, _SR, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _CACHEABLE, 1) |
                                     DRF_NUM64(_RISCV_CSR, _SMPUATTR, _COHERENT, 0));

    // Disable interrupts in SSTATUS and SIE, but leave MEMERR enabled
    csr_clear(LW_RISCV_CSR_SIE, DRF_NUM64(_RISCV, _CSR_SIE, _SEIE, 1));
    csr_clear(LW_RISCV_CSR_SSTATUS, DRF_DEF64(_RISCV, _CSR_SSTATUS, _SIE, _ENABLE));

    //
    // Scrub stack/registers and jump to PMU entry-point. Note that we
    // cannot skip stack scrubbing on fmodel like we used to due to
    // security-baseline requirements.
    //
    bootLoad(pState->entryPoint, pState->bootargsMpuPaBase,
             pState->bootargsMpuVaBase, DMEM_STACK_BASE,
             DMEM_STACK_BASE + STACK_SIZE, (LwUPtr)pState->pElf,
             pState->elfSize, pState->loadBases[LW_ELF_LOAD_BASE_INDEX_RESERVED_RW],
             pState->wprId);

    //
    // we shouldn't get here anyways, so mark unreachable.
    // we can't return either, because $ra isn't valid anymore.
    //
    __builtin_unreachable();
}
