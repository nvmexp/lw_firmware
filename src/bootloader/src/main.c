 /* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 1993-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <falcon-intrinsics.h>
#include "rmflcnbl.h"
#include "falcon.h"
#include "lwmisc.h"
#include "dev_pwr_falcon_csb.h"

#define SVEC_REG(base, size, s, e)  (((base) >> 8) | (((size) | (s)) << 16) | ((e) << 17))

// DMA CTX update macros
#define  DMA_SET_IMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~((LwU32)0x7U << 0)) | ((dmaIdx) << 0))
#define  DMA_SET_DMREAD_CTX(ctxSpr, dmaIdx)  \
    (((ctxSpr) & ~((LwU32)0x7U << 8)) | ((dmaIdx) << 8))
#define  DMA_SET_DMWRITE_CTX(ctxSpr, dmaIdx) \
    (((ctxSpr) & ~((LwU32)0x7U << 12)) | ((dmaIdx) << 12))


// Global variable to hold Loader config
#define ATTR_OVLY(ov)               __attribute__ ((section(ov)))
#define ATTR_ALIGNED(align)         __attribute__ ((aligned(align)))

RM_FLCN_BL_DMEM_DESC g_loaderCfg ATTR_ALIGNED(16) ATTR_OVLY(".data");

// Function prototypes
typedef int (*ENTRY_FN)(int argc, char **argv);
typedef int (*ENTRY_FN_NOARGS)(void);

int main(int argc, char** argv)
{
    int            rc             = 0;
    LwU32          offs           = 0;
    LwU32          blk            = 0;
    LwU32          dataSize       = g_loaderCfg.dataSize;
    LwU32          entryFn        = g_loaderCfg.codeEntryPoint;
    LwU32          loaderArgc     = g_loaderCfg.argc;
    LwU32          loaderArgv     = g_loaderCfg.argv;
#if defined(HS_FALCON) && (HS_FALCON == 1)
    LwU32          selwreCodeSize = g_loaderCfg.selwreCodeSize;
#endif

    // Set the base
    falc_wspr(IMB, ((g_loaderCfg.codeDmaBase.lo >> 8) |
                    (g_loaderCfg.codeDmaBase.hi << 24)));
#if defined(PA_47_BIT_SUPPORTED) && (PA_47_BIT_SUPPORTED == 1)
    falc_wspr(IMB1, (g_loaderCfg.codeDmaBase.hi >> 8));
#endif

    // Set the CTX (needs to be set for IMREAD,DMREAD, and DMWRITE
    falc_wspr(CTX, DMA_SET_IMREAD_CTX(0U,  g_loaderCfg.ctxDma) |
                   DMA_SET_DMREAD_CTX(0U,  g_loaderCfg.ctxDma) |
                   DMA_SET_DMWRITE_CTX(0U, g_loaderCfg.ctxDma));

#if defined(CLEAN_IMEM) && (CLEAN_IMEM == 1)
    //
    // Mark Valid or Secure IMEM blocks to Invalid and Inselwre.
    // When ACR binary exelwtes on the falcon(PMU/SEC2), it leaves IMEM blocks
    // as Secure Invalid or Valid Inselwre.
    // Any of the above combination can cause Ucode load to fail due to
    // IMEM double hit. Hence bootloader needs to clean those IMEM blocks.
    //

    LwU32  blkInfo        = 0;
    LwU32  clearImemLimit = 0;
    LwBool bBlkValid      = LW_FALSE;
    LwBool bBlkSelwre     = LW_FALSE;

    // Ilwalidate IMEM only till BootLoader Offset.
    falc_imtag(&clearImemLimit, CLEAR_IMEM_LIMIT);
    clearImemLimit = IMTAG_IDX_GET(clearImemLimit);

    // Using the non_selwre code for dummy DMA
    offs = g_loaderCfg.nonSelwreCodeOff;

    for (blk = 0; blk < clearImemLimit; blk++)
    {
        falc_imblk (&blkInfo , blk);
        bBlkValid  = !IMBLK_IS_ILWALID(blkInfo);
        bBlkSelwre =  IMBLK_IS_SELWRE(blkInfo);

        if (bBlkValid)
        {
            if (!bBlkSelwre)
            {
                // Block is Valid Secure, ilwalidate it
                falc_imilw(blk);
            }
            else
            {
                // do nothing
            }
        }
        else // !bBlkValid
        {
            if (bBlkSelwre)
            {
                // Block is Invalid Secure, inselwre it
                falc_imread(offs, (blk * 256U));
                falc_imwait();
                falc_imilw(blk);
            }
            else
            {
                break;
            }
        }
    }
#endif

    blk = 0;
    // Load the non_selwre code first, as CTX and DMA Base is already set
    offs = g_loaderCfg.nonSelwreCodeOff;
    while (offs < (g_loaderCfg.nonSelwreCodeOff + g_loaderCfg.nonSelwreCodeSize))
    {
        falc_imread(offs, blk);
        offs += 256U;
        blk  += 256U;
    }
    falc_imwait();

#if defined(HS_FALCON) && (HS_FALCON == 1)
    //
    // Load the secure code
    // Load signature to SCP R6 register
    // Only do it if it's needed
    //
    if (selwreCodeSize > 0U)
    {
        asm ("ccr <mt:DMEM,sc:0,sup:0,tc:2> ;");
        falc_dmwrite(0, ((6 << 16) | (LwU32)(g_loaderCfg.signature)));
        falc_dmwait();

        // Setup SVEC register to assert secure bit
        falc_wspr(SEC, SVEC_REG(0, 0, 1, 0));

        offs = g_loaderCfg.selwreCodeOff;
        while (offs < (g_loaderCfg.selwreCodeOff + selwreCodeSize))
        {
            falc_imread(offs, blk);
            offs += 256U;
            blk  += 256U;
        }

        falc_imwait();

        //
        // Set up SVEC register to de-assert secure bit and indicate WL PC range
        // Note that selwre_blk_base is 16 bits wide, so only codeDmaBase.lo is needed
        //
        falc_wspr(SEC, SVEC_REG((LwU32)((g_loaderCfg.codeDmaBase.lo >> 8) + g_loaderCfg.selwreCodeOff), g_loaderCfg.selwreCodeSize, 0, 0));
    }
#endif

    //
    // Load Data now..
    // Set DMB
    //
    // Set the CTX (needs to be set for IMREAD,DMREAD, and DMWRITE
    falc_wspr(CTX, DMA_SET_IMREAD_CTX(0U,  g_loaderCfg.ctxDma) |
                   DMA_SET_DMREAD_CTX(0U,  g_loaderCfg.ctxDma) |
                   DMA_SET_DMWRITE_CTX(0U, g_loaderCfg.ctxDma));
    falc_wspr(DMB, ((g_loaderCfg.dataDmaBase.lo >> 8) |
                    (g_loaderCfg.dataDmaBase.hi << 24)));
#if defined(PA_47_BIT_SUPPORTED) && (PA_47_BIT_SUPPORTED == 1)
    falc_wspr(DMB1, (g_loaderCfg.dataDmaBase.hi >> 8));
#endif

    //
    // Load the data into DMEM offset 0
    // g_LoaderCfg will be overwritten..
    //
    offs = 0;
    blk = 0;
    while (offs < dataSize)
    {
        falc_dmread(offs, blk + 0x60000U); // Size 256B
        offs += 256U;
        blk  += 256U;
    }
    falc_dmwait();

#if defined(ENGINE_FECS) && (ENGINE_FECS == 1)
    //
    // Reset REGIONCFG and DMA CMD override for FECS
    // TODO: Remove once FMODEL is able to handle this properly
    //
    falc_stx((unsigned int *)0x28800, 0);
    falc_wspr(DMB, ((g_loaderCfg.dataDmaBase.lo >> 8) |
                    (g_loaderCfg.dataDmaBase.hi << 24)));
#if defined(PA_47_BIT_SUPPORTED) && (PA_47_BIT_SUPPORTED == 1)
    falc_wspr(DMB1, (g_loaderCfg.dataDmaBase.hi >> 8));
#endif
    falc_stx((unsigned int *)0x28d00, 0);
#endif

#if defined(ENGINE_GPCCS) && (ENGINE_GPCCS == 1)
    //
    // Reset REGIONCFG and DMA CMD override for GPCCS
    // TODO: Remove once FMODEL is able to handle this properly
    //
    falc_stx((unsigned int *)0x28800, 0);
    falc_wspr(DMB, ((g_loaderCfg.dataDmaBase.lo >> 8) |
                    (g_loaderCfg.dataDmaBase.hi << 24)));
#if defined(PA_47_BIT_SUPPORTED) && (PA_47_BIT_SUPPORTED == 1)
    falc_wspr(DMB1, (g_loaderCfg.dataDmaBase.hi >> 8));
#endif
    falc_stx((unsigned int *)0x28d00, 0);
#endif

#if defined(VIRTUAL_DMA) && (VIRTUAL_DMA == 1)
    // On PMU, we just ack the CTXSW interrupt
    if (falc_ldx_i((LwU32*)LW_CMSDEC_FALCON_IRQSTAT,  0) & 0x08)
    {
        falc_stx((LwU32*)LW_CMSDEC_FALCON_DEBUG1,  1<<16);
        falc_stx((LwU32*)LW_CMSDEC_FALCON_CTXACK,  0x2);
        falc_stx((LwU32*)LW_CMSDEC_FALCON_IRQSCLR, 0x08);
    }
#endif

    // Call the entry point
    if (loaderArgc > 0U)
    {
        rc = ((ENTRY_FN)entryFn)(loaderArgc, (char **)loaderArgv);
    }
    else
    {
        rc = ((ENTRY_FN_NOARGS)entryFn)();
    }

#if defined(HS_FALCON) && (HS_FALCON == 1)
    if (selwreCodeSize > 0U)
    {
        // Enable interrupts
        asm ("ssetb 16;");
        asm ("ssetb 17;");
        // Falcon core v > 4
        asm ("ssetb 18;");

        // Clear SVEC register
        falc_wspr(SEC, 0);
    }
#endif

    return rc;
}
