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
 * @file    fbif.c
 * @brief   FBIF driver
 *
 * Used to configure FBIF apertures
 */

#include <stddef.h>
#include <lwmisc.h>

#include <liblwriscv/fbif.h>
#include <liblwriscv/io.h>

//The register manual lwrrently doesn't define this as an indexed field.
//Because of this, we need to define our own. We have asked the hardware
//Folks to provide an indexed field definition in the manuals, so we don't
//need to do this here.
#define LW_PRGNLCL_FBIF_REGIONCFG_T(i) ((i)*4U+3U):((i)*4U)

LWRV_STATUS
fbif_configure_aperture
(
    const fbif_aperture_cfg_t *p_cfgs,
    uint8_t num_cfgs
)
{
    LWRV_STATUS err = LWRV_OK;
    uint8_t idx;
    uint32_t regioncfg;

    if (p_cfgs == NULL)
    {
        err = LWRV_ERR_ILWALID_ARGUMENT;
        goto out;
    }

    regioncfg = localRead(LW_PRGNLCL_FBIF_REGIONCFG);

    for (idx = 0U; idx < num_cfgs; idx++)
    {
        if ((p_cfgs[idx].aperture_idx >= FBIF_NUM_APERTURES) ||
            (p_cfgs[idx].target >= FBIF_TRANSCFG_TARGET_COUNT) ||
            (p_cfgs[idx].l2c_wr >= FBIF_TRANSCFG_L2C_COUNT) ||
            (p_cfgs[idx].l2c_rd >= FBIF_TRANSCFG_L2C_COUNT) ||
            (p_cfgs[idx].region_id >= (1U << DRF_SIZE(LW_PRGNLCL_FBIF_REGIONCFG_T0)))
        )
        {
            err = LWRV_ERR_ILWALID_ARGUMENT;
            goto out;
        }

        regioncfg = FLD_IDX_SET_DRF_NUM(_PRGNLCL, _FBIF_REGIONCFG, _T,
                                        p_cfgs[idx].aperture_idx, p_cfgs[idx].region_id, regioncfg);

        localWrite(LW_PRGNLCL_FBIF_TRANSCFG((uint32_t)p_cfgs[idx].aperture_idx),
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _TARGET, p_cfgs[idx].target) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _MEM_TYPE,
                ((p_cfgs[idx].b_target_va) ?
                LW_PRGNLCL_FBIF_TRANSCFG_MEM_TYPE_VIRTUAL : LW_PRGNLCL_FBIF_TRANSCFG_MEM_TYPE_PHYSICAL)) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _L2C_WR, p_cfgs[idx].l2c_wr) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _L2C_RD, p_cfgs[idx].l2c_rd) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _WACHK0, p_cfgs[idx].b_fbif_transcfg_wachk0_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _WACHK1, p_cfgs[idx].b_fbif_transcfg_wachk1_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _RACHK0, p_cfgs[idx].b_fbif_transcfg_rachk0_enable ? 1 : 0) |
            DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _RACHK1, p_cfgs[idx].b_fbif_transcfg_rachk1_enable ? 1 : 0)
#ifdef LW_PRGNLCL_FBIF_TRANSCFG_ENGINE_ID_FLAG
            //Some instances of the FBIF do not have this field
            | DRF_NUM(_PRGNLCL, _FBIF_TRANSCFG, _ENGINE_ID_FLAG, p_cfgs[idx].b_engine_id_flag_own ? 1 : 0)
#endif //LW_PRGNLCL_FBIF_TRANSCFG_ENGINE_ID_FLAG
            );
    }

    localWrite(LW_PRGNLCL_FBIF_REGIONCFG, regioncfg);

out:
    return err;
}
