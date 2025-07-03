/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file: booter_bootvec_tu10x.c
 */

//
// Includes
//
#include "booter.h"

#include "dev_gsp.h"
#include "dev_riscv_csr_64.h"
#include "dev_riscv_csr_64_addendum.h"
#include "lw_gsp_riscv_address_map.h"

/*!
 * @brief Programs the bootvector for RISC-Vs
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 *     This function writes a raw machine-code stub to the target core's ITCM. This is
 *     an ugly hack to work around a Turing/GA100 bug wherein RISC-V cores must boot from
 *     ITCM, rather than FB as we would like. This function is only intended to be used
 *     for prototyping and testing of RISC-V LS on Turing/GA100. The code here should not be
 *     copied anywhere and should not be prod-signed unless prior approval is given
 *     by HS code signers. Any modifications to this code should also go through
 *     HS signers.
 *
 *     This hack was used to minimize the overhead in getting a working RISC-V LS
 *     implementation ready for Turing/GA100. It will be removed for GA10X+.
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * @param[in] regionId   WPR region ID
 * @param[in] pWprMeta   GSP-RM Metadata struct
 */
BOOTER_STATUS
booterSetupBootvec_TU10X
(
    LwU32         regionId,
    GspFwWprMeta *pWprMeta
)
{
    LwU32 cmd;

    LwU64 bootvec = LW_RISCV_AMAP_FBGPA_START + pWprMeta->bootBinOffset;

    //
    // RISCV on Turing and GA100 has a bug (2031674) preventing booting from WPR and GVA.
    // Since RISC-V absolutely wants to boot from FB under a WPR, we will write a tiny stub
    // program to beginning of IMEM, and boot that. This will not be needed in GA10X+ (COREUCODES-531).
    //
    cmd = DRF_NUM(_PFALCON_FALCON, _IMEMC, _OFFS, 0) |
          DRF_NUM(_PFALCON_FALCON, _IMEMC, _BLK, 0)  |
          DRF_DEF(_PFALCON_FALCON, _IMEMC, _AINCW, _TRUE);

    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_IMEMC(0), cmd);

    // Need to write an IMEM tag even though we shouldn't be using it.
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_IMEMT(0), 0);

    {
        LwU32 i;

        //
        // Set WPR ID for memory accesses in MBARE mode.
        // Bootloader will also retrieve this value for use elsewhere.
        //
        LwU64 uacc = DRF_NUM64(_RISCV_CSR, _UACCESSATTR, _WPR, regionId);

        //
        // Set mask of privilege levels usable by machine and user code when
        // accessing external recources to zero (NS level)
        //
        LwU64 mspm = DRF_DEF64(_RISCV_CSR, _MSPM, _MPLM_LEVEL0, _ENABLE) |
                     DRF_DEF64(_RISCV_CSR, _MSPM, _UPLM_LEVEL0, _ENABLE);

        //
        // Use privilege level two (FALCON_SCTL.LSMODE_LEVEL) when accessing
        // external resources from either machine or user mode. Set ICD to use
        // same level.
        //
        LwU64 mrsp = DRF_DEF64(_RISCV_CSR, _MRSP, _MRPL, _LEVEL0) | //LEVEL2
                     DRF_DEF64(_RISCV_CSR, _MRSP, _URPL, _LEVEL0) | //LEVEL2
                     DRF_DEF64(_RISCV_CSR, _MRSP, _ICD_PL, _USE_PL0); //USE_LWR

        // Write the stub program:
        //      lw      t0, data_pool+8(zero) [uaccessattr]
        //      ld      t1, data_pool+0(zero) [bootvec]
        //      csrw    uaccessattr, t0
        //      ld      t2, data_pool+16(zero)[mspm]
        //      ld      t3, data_pool+24(zero)[mrsp]
        //      csrw    mspm, t2
        //      csrw    mrsp, t3
        //      fence
        //      fence.i
        //      jalr    zero, t1, 0
        //  data_pool:
        //      .long bootvec
        //      .long uaccessattr
        //      .long mspm
        //      .long mrsp
        const LwU32 imemBootstub[] = {
            0x00002003 | (5 << 7)   | (0x30 << 20),                     //      lw      t0, data_pool+8(zero) [uaccessattr]
            // NOTE: If target core is RV32, use 2003 (as above) for the `lw` instruction instead.
            0x00003003 | (6 << 7)   | (0x28 << 20),                     //      ld      t1, data_pool+0(zero) [bootvec]
            0x00001073 | (5 << 15)  | (LW_RISCV_CSR_UACCESSATTR << 20), //      csrw    uaccessattr, t0
            0x00003003 | (7 << 7)   | (0x38 << 20),                     //      ld      t2, data_pool+16(zero)[mspm]
            0x00003003 | (28 << 7)  | (0x40 << 20),                     //      ld      t3, data_pool+24(zero)[mrsp]
            0x00001073 | (7 << 15)  | (LW_RISCV_CSR_MSPM << 20),        //      csrw    mspm, t2
            0x00001073 | (28 << 15) | (LW_RISCV_CSR_MRSP << 20),        //      csrw    mrsp, t3
            // Fences are required to make uaccessattr take effect.
            0x0FF0000F,                                                 //      fence
            0x0000100F,                                                 //      fence.i
            0x00000067 | (6 << 15),                                     //      jalr    zero, t1, 0
                                                                        //  data_pool: [@0x28]
            bootvec, bootvec >> 32,                                     //      .dword bootvec
            uacc, uacc >> 32,                                           //      .dword uaccessattr
            mspm, mspm >> 32,                                           //      .dword mspm
            mrsp, mrsp >> 32,                                           //      .dword mrsp
        };

        for (i = 0; i < sizeof(imemBootstub) / sizeof(*imemBootstub); i++)
        {
            BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_IMEMD(0), imemBootstub[i]);
        }
    }

    //
    // When IMEM blocks are _not_ secure, they just require the last dword to be written,
    // in order to flush the contents to real IMEM. Therefore, we write a zero to
    // OFFS = Size of IMEM block in 32-bit words - 1.
    //
    // If the IMEM block was marked secure, the entire block would need to be written to.
    //
    cmd = FLD_SET_DRF_NUM(_PFALCON_FALCON, _IMEMC, _OFFS, FLCN_IMEM_BLK_SIZE_IN_BYTES / sizeof(LwU32) - 1, cmd);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_IMEMC(0), cmd);
    BOOTER_REG_WR32(BAR0, LW_PGSP_FALCON_IMEMD(0), 0);

    // Bootstrap was written to the beginning of IMEM, so set the boot vector to 0.
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BOOT_VECTOR_HI, 0);
    BOOTER_REG_WR32(BAR0, LW_PGSP_RISCV_BOOT_VECTOR_LO, 0);

    return BOOTER_OK;
}

