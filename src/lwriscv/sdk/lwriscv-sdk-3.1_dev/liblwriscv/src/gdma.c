/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    gdma.c
 * @brief   GDMA driver
 *
 * Used to transfer data between TCM, external memory, and IO
 */

#include <stddef.h>
#include <lwmisc.h>
#include <liblwriscv/gdma.h>
#include <liblwriscv/io.h>
#include <liblwriscv/print.h>

#define LW_PRGNLCL_GDMA_IRQ_CTRL_CHANx_DIS(i)     (i):(i)
#define LW_PRGNLCL_GDMA_IRQ_CTRL_CHANx_DIS_TRUE LW_PRGNLCL_GDMA_IRQ_CTRL_CHAN0_DIS_TRUE
#define LW_PRGNLCL_GDMA_IRQ_CTRL_CHANx_DIS_FALSE LW_PRGNLCL_GDMA_IRQ_CTRL_CHAN0_DIS_FALSE

// assert that our gdma_addr_mode_t enum matches the HW values
_Static_assert(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1_SRC_ADDR_MODE_INC == GDMA_ADDR_MODE_INC,
    "GDMA_ADDR_MODE_INC mismatch with register manual");
_Static_assert(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1_SRC_ADDR_MODE_FIX == GDMA_ADDR_MODE_FIX,
    "GDMA_ADDR_MODE_FIX mismatch with register manual");
_Static_assert(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1_SRC_ADDR_MODE_WRAP == GDMA_ADDR_MODE_WRAP,
    "GDMA_ADDR_MODE_WRAP mismatch with register manual");
_Static_assert(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1_SRC_ADDR_MODE_STRIDE == GDMA_ADDR_MODE_STRIDE,
    "GDMA_ADDR_MODE_STRIDE mismatch with register manual");


LWRV_STATUS
gdma_cfg_channel (uint8_t chan_idx, const gdma_channel_cfg_t *p_cfg)
{
    uint32_t reg;
    if ((chan_idx >= GDMA_MAX_NUM_CHANNELS) || (p_cfg == NULL))
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    reg = DRF_NUM(_PRGNLCL_GDMA, _CHAN_COMMON_CONFIG, _RR_WEIGHT, p_cfg->rr_weight);
    reg |= DRF_NUM(_PRGNLCL_GDMA, _CHAN_COMMON_CONFIG, _MAX_INFLIGHT, p_cfg->max_in_flight);

    if(p_cfg->b_memq)
    {
        //
        // AS TODO: uncomment this when we add memQ support
        // reg |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_COMMON_CONFIG, _MEMQ, _TRUE);
        //
        return LWRV_ERR_NOT_SUPPORTED;
    }
    else
    {
        reg |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_COMMON_CONFIG, _MEMQ, _FALSE);
    }

    localWrite(LW_PRGNLCL_GDMA_CHAN_COMMON_CONFIG(chan_idx), reg);

    reg = localRead(LW_PRGNLCL_GDMA_IRQ_CTRL);

    if (p_cfg->b_irq_en)
    {
        reg = FLD_IDX_SET_DRF(_PRGNLCL_GDMA, _IRQ_CTRL, _CHANx_DIS, chan_idx, _FALSE, reg);
    }
    else
    {
        reg = FLD_IDX_SET_DRF(_PRGNLCL_GDMA, _IRQ_CTRL, _CHANx_DIS, chan_idx, _TRUE, reg);
    }

    localWrite(LW_PRGNLCL_GDMA_IRQ_CTRL, reg);

    return LWRV_OK;
}

LWRV_STATUS
gdma_cfg_channel_priv (uint8_t chan_idx, const gdma_channel_priv_cfg_t *p_cfg)
{
    if ((chan_idx >= GDMA_MAX_NUM_CHANNELS) || (p_cfg == NULL))
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

#ifdef LW_PRGNLCL_GDMA_VIRTUAL_CONTROL
    uint32_t reg = DRF_NUM(_PRGNLCL_GDMA, _VIRTUAL_CONTROL, _ASID, p_cfg->asid);

    if (p_cfg->b_virtual)
    {
        reg |= DRF_DEF(_PRGNLCL_GDMA, _VIRTUAL_CONTROL, _VA, _TRUE);
    }
    else
    {
        reg |= DRF_DEF(_PRGNLCL_GDMA, _VIRTUAL_CONTROL, _VA, _FALSE);
    }

    localWrite(LW_PRGNLCL_GDMA_VIRTUAL_CONTROL(chan_idx), reg);
#else
    if (p_cfg->b_virtual)
    {
        return LWRV_ERR_NOT_SUPPORTED;
    }
#endif

    return LWRV_OK;
}

LWRV_STATUS
gdma_xfer_reg (
    uint64_t src,
    uint64_t dest,
    uint32_t length,
    gdma_addr_cfg_t *p_addr_cfg,
    uint8_t chan_idx,
    uint8_t sub_chan_idx,
    bool b_wait_complete,
    bool b_irq_en
)
{
    uint32_t reg, timeout;
    if ((chan_idx >= GDMA_MAX_NUM_CHANNELS) ||
        (sub_chan_idx >= GDMA_NUM_SUBCHANNELS) ||
        (!LW_IS_ALIGNED(src, GDMA_ADDR_ALIGN)) ||
        (!LW_IS_ALIGNED(dest, GDMA_ADDR_ALIGN)) ||
        (!LW_IS_ALIGNED(length, GDMA_ADDR_ALIGN)) ||
        (length > GDMA_MAX_XFER_LENGTH))
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    // If HW Request Queue is full, wait for one of the transfers to complete
    timeout = 10000000;
    do {
        reg = localRead(LW_PRGNLCL_GDMA_CHAN_STATUS(chan_idx));
        if (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_VLD, _TRUE, reg) ||
            FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _FAULTED, _TRUE, reg))
        {
            // If the channel is in an error state, new requests cannot be queued
            return LWRV_ERR_ILWALID_STATE;
        }

        if((timeout--) == 0)
        {
            return LWRV_ERR_TIMEOUT;
        }
    }
    while (FLD_TEST_DRF_NUM(_PRGNLCL_GDMA, _CHAN_STATUS, _REQ_QUEUE_SPACE, 0, reg));


    // Write the src and dest addresses
    localWrite(LW_PRGNLCL_GDMA_CHAN_SRC_ADDR(chan_idx), LwU64_LO32(src));
    localWrite(LW_PRGNLCL_GDMA_CHAN_SRC_ADDR_HI(chan_idx), LwU64_HI32(src));

    localWrite(LW_PRGNLCL_GDMA_CHAN_DEST_ADDR(chan_idx), LwU64_LO32(dest));
    localWrite(LW_PRGNLCL_GDMA_CHAN_DEST_ADDR_HI(chan_idx), LwU64_HI32(dest));

    if (p_addr_cfg)
    {
#ifdef LW_RISCV_AMAP_PRIV_START
        // Complex address modes are only allowed in PRIV range
        if ((((src < LW_RISCV_AMAP_PRIV_START) || (src >= LW_RISCV_AMAP_PRIV_END)) &&
             (p_addr_cfg->src_addr_mode != GDMA_ADDR_MODE_INC)) ||
            (((dest < LW_RISCV_AMAP_PRIV_START) || (dest >= LW_RISCV_AMAP_PRIV_END)) &&
             (p_addr_cfg->dest_addr_mode != GDMA_ADDR_MODE_INC)))
        {
            return LWRV_ERR_ILWALID_ARGUMENT;
        }

#else // LW_RISCV_AMAP_PRIV_START
        if ((p_addr_cfg->src_addr_mode != GDMA_ADDR_MODE_INC) || (p_addr_cfg->dest_addr_mode != GDMA_ADDR_MODE_INC))
        {
            return LWRV_ERR_ILWALID_ARGUMENT;
        }
#endif // LW_RISCV_AMAP_PRIV_START

        reg = DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _SRC_ADDR_MODE, p_addr_cfg->src_addr_mode) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _DEST_ADDR_MODE, p_addr_cfg->dest_addr_mode) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _WRAP_LEN, p_addr_cfg->wrap_len) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _STRIDE_LENLOG, p_addr_cfg->stride_log_len);

        if (p_addr_cfg->b_dest_posted)
        {
            reg |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _DEST_POSTED, _TRUE);
        }
        else
        {
            reg |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _DEST_POSTED, _FALSE);
        }
    }
    else
    {
        // default address mode: incremental addressing
        reg = DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _SRC_ADDR_MODE, _INC) |
            DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _DEST_ADDR_MODE, _INC) |
            DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG1, _DEST_POSTED, _FALSE);
    }

    localWrite(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG1(chan_idx), reg);


#ifdef LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_COHERENT
    if (p_addr_cfg)
    {
        reg = DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _SRC_FB_COHERENT, p_addr_cfg->src_ext_addr_cfg.fb_coherent ? 1 : 0) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _SRC_FB_L2C_RD, p_addr_cfg->src_ext_addr_cfg.fb_l2c_rd) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _SRC_FB_WPR, p_addr_cfg->src_ext_addr_cfg.fb_wpr) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _DEST_FB_COHERENT, p_addr_cfg->dest_ext_addr_cfg.fb_coherent ? 1 : 0) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _DEST_FB_L2C_WR, p_addr_cfg->dest_ext_addr_cfg.fb_l2c_rd) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _DEST_FB_WPR, p_addr_cfg->dest_ext_addr_cfg.fb_wpr);
    }
    else
    {
        // defaults: coherent access, L2C evict normal, WPR ID 0
        reg = DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _SRC_FB_COHERENT, 1) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _SRC_FB_L2C_RD, GDMA_L2C_L2_EVICT_NORMAL) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _DEST_FB_COHERENT, 1) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _DEST_FB_L2C_WR, GDMA_L2C_L2_EVICT_NORMAL);
    }
#else // LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_COHERENT
    if (p_addr_cfg)
    {
        reg = DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _SRC_DBB_GSCID, p_addr_cfg->src_ext_addr_cfg.dbb_gscid) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _SRC_DBB_VPR, p_addr_cfg->src_ext_addr_cfg.dbb_vpr ? 1 : 0) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _DEST_DBB_GSCID, p_addr_cfg->dest_ext_addr_cfg.dbb_gscid) |
            DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG2, _DEST_DBB_VPR, p_addr_cfg->dest_ext_addr_cfg.dbb_vpr ? 1 : 0);
    }
    else
    {
        reg = 0;
    }
#endif // LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_COHERENT

    localWrite(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2(chan_idx), reg);

    reg = DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _LENGTH, (length/GDMA_ADDR_ALIGN));
    reg |= DRF_NUM(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _SUBCHAN, sub_chan_idx);
    if (b_irq_en)
    {
        reg |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _COMPLETE_IRQEN, _TRUE);
    }
    else
    {
        reg |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _COMPLETE_IRQEN, _FALSE);
    }

    reg |= DRF_DEF(_PRGNLCL_GDMA, _CHAN_TRANS_CONFIG0, _LAUNCH, _TRUE);

    // This kicks off the DMA
    localWrite(LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG0(chan_idx), reg);

    if (b_wait_complete)
    {
        uint32_t req_produce = localRead(LW_PRGNLCL_GDMA_CHAN_REQ_PRODUCE(chan_idx));
        uint32_t req_complete;

        // If HW Request Queue is full, wait for one of the transfers to complete
        timeout = 10000000;
        do
        {
            //busy bit may not assert right away, so check req_complete == req_produce too
            req_complete = localRead(LW_PRGNLCL_GDMA_CHAN_REQ_COMPLETE(chan_idx));

            reg = localRead(LW_PRGNLCL_GDMA_CHAN_STATUS(chan_idx));
            if (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_VLD, _TRUE, reg) ||
                FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _FAULTED, _TRUE, reg))
            {
                return LWRV_ERR_GENERIC;
            }

            if((timeout--) == 0)
            {
                return LWRV_ERR_TIMEOUT;
            }
        }
        while (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _BUSY, _TRUE, reg) || (req_complete < req_produce));
    }

    return LWRV_OK;
}

LWRV_STATUS
gdma_get_channel_status(uint8_t chan_idx, uint32_t *p_req_produce, uint32_t *p_req_consume, uint32_t *p_req_complete)
{
    uint32_t status;
    if (chan_idx >= GDMA_MAX_NUM_CHANNELS)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    if (p_req_produce)
    {
        *p_req_produce = localRead(LW_PRGNLCL_GDMA_CHAN_REQ_PRODUCE(chan_idx));
    }

    if (p_req_consume)
    {
        *p_req_consume = localRead(LW_PRGNLCL_GDMA_CHAN_REQ_CONSUME(chan_idx));
    }

    if (p_req_complete)
    {
        *p_req_complete = localRead(LW_PRGNLCL_GDMA_CHAN_REQ_COMPLETE(chan_idx));
    }

    status = localRead(LW_PRGNLCL_GDMA_CHAN_STATUS(chan_idx));
    if (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_VLD, _TRUE, status) ||
        FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _FAULTED, _TRUE, status))
    {
        return LWRV_ERR_GENERIC;
    }

    return LWRV_OK;
}

LWRV_STATUS
gdma_get_error_and_reset_channel(uint8_t chan_idx, uint8_t *error_cause, uint32_t *error_id)
{
    uint32_t status, reg;
    if (chan_idx >= GDMA_MAX_NUM_CHANNELS)
    {
        return LWRV_ERR_ILWALID_ARGUMENT;
    }

    status = localRead(LW_PRGNLCL_GDMA_CHAN_STATUS(chan_idx));
    if (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_VLD, _FALSE, status))
    {
        // channel is not in an error state
        return LWRV_ERR_ILWALID_STATE;
    }

    if (error_cause)
    {
        *error_cause = DRF_VAL(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_CAUSE, status);
    }

    if (error_id)
    {
        *error_id = localRead(LW_PRGNLCL_GDMA_CHAN_REQ_ERROR(chan_idx));
    }

    // reset the channel
    localWrite(LW_PRGNLCL_GDMA_CHAN_CONTROL(chan_idx),
        DRF_DEF(_PRGNLCL_GDMA, _CHAN_CONTROL, _RESET, _TRUE));

    // wait for reset to complete
    uint32_t timeout = 10000;
    do {
        reg = localRead(LW_PRGNLCL_GDMA_CHAN_STATUS(chan_idx));

        if((timeout--) == 0)
        {
            return LWRV_ERR_TIMEOUT;
        }
    }
    while (FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _ERROR_VLD, _TRUE, reg) ||
           FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _FAULTED, _TRUE, reg) ||
           FLD_TEST_DRF(_PRGNLCL_GDMA, _CHAN_STATUS, _BUSY, _TRUE, reg));

    return LWRV_OK;
}
