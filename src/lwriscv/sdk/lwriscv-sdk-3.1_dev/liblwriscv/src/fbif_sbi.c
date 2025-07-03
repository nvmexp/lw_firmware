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
 * @file    fbif.c
 * @brief   FBIF driver - SBI variant. Calls SK to configure TFBIF.
 */

#include <stddef.h>
#include <lwmisc.h>

#include <liblwriscv/fbif.h>
#include <lwriscv/sbi.h>
#include <liblwriscv/io.h>

LWRV_STATUS
fbif_configure_aperture
(
    const fbif_aperture_cfg_t *p_cfgs,
    uint8_t num_cfgs
)
{
    LWRV_STATUS err = LWRV_OK;
    SBI_RETURN_VALUE sbi_ret;
    uint8_t idx;
    uint32_t transcfg;

    if (p_cfgs == NULL)
    {
        err = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    for (idx = 0U; idx < num_cfgs; idx++)
    {
        if ((p_cfgs[idx].aperture_idx >= FBIF_NUM_APERTURES) ||
            (p_cfgs[idx].target >= FBIF_TRANSCFG_TARGET_COUNT) ||
            (p_cfgs[idx].l2c_wr >= FBIF_TRANSCFG_L2C_COUNT) ||
            (p_cfgs[idx].l2c_rd >= FBIF_TRANSCFG_L2C_COUNT))
        {
            err = LWRV_ERR_ILWALID_ARGUMENT;
            goto out;
        }

        transcfg = DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _TARGET, p_cfgs[idx].target) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE,
                ((p_cfgs[idx].b_target_va) ?
                LW_PRGNLCL_FBIF_TRANSCFG_MEM_TYPE_VIRTUAL : LW_PRGNLCL_FBIF_TRANSCFG_MEM_TYPE_PHYSICAL)) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _L2C_WR, p_cfgs[idx].l2c_wr) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _L2C_RD, p_cfgs[idx].l2c_rd) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _WACHK0, p_cfgs[idx].b_fbif_transcfg_wachk0_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _WACHK1, p_cfgs[idx].b_fbif_transcfg_wachk1_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _RACHK0, p_cfgs[idx].b_fbif_transcfg_rachk0_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _RACHK1, p_cfgs[idx].b_fbif_transcfg_rachk1_enable ? 1 : 0);
    
#ifdef LW_PRGNLCL_FBIF_TRANSCFG_ENGINE_ID_FLAG
            // Some instances of the FBIF do not have this field
        transcfg |= DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, p_cfgs[idx].b_engine_id_flag_own ? 1 : 0);
#endif //LW_PRGNLCL_FBIF_TRANSCFG_ENGINE_ID_FLAG

        sbi_ret = sbicall2(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FBIF_TRANSCFG_SET, (uint64_t)p_cfgs[idx].aperture_idx , (uint64_t)transcfg);
        if (sbi_ret.error != SBI_SUCCESS)
        {
            err = LWRV_ERR_ILWALID_ARGUMENT;
            goto out;
        }

        sbi_ret = sbicall2(SBI_EXTENSION_LWIDIA, SBI_LWFUNC_FBIF_REGIONCFG_SET, (uint64_t)p_cfgs[idx].aperture_idx , (uint64_t)p_cfgs[idx].region_id);
        if (sbi_ret.error != SBI_SUCCESS)
        {
            err = LWRV_ERR_ILWALID_ARGUMENT;
            goto out;
        }
    }

out:
    return err;
}
