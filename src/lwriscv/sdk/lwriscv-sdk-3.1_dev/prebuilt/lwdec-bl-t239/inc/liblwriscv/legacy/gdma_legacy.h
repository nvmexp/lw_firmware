/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_GDMA_LEGACY_H
#define LIBLWRISCV_GDMA_LEGACY_H

/*
 * This file is to maintain support for legacy clients which use the old deprecated
 * function names. Please do not use these functions in new code. Existing code should
 * migrate to using the new function names if possible.
 */

typedef struct
{
    uint8_t rrWeight;
    uint8_t maxInFlight;
    bool bIrqEn;
    bool bMemq;
} GDMA_CHANNEL_CFG;

_Static_assert(sizeof(GDMA_CHANNEL_CFG) == sizeof(gdma_channel_cfg_t),
    "FBIF_APERTURE_CFG out of sync with fbif_aperture_cfg_t!");

typedef struct
{
    bool bVirtual;
    uint8_t asid;
} GDMA_CHANNEL_PRIV_CFG;

_Static_assert(sizeof(GDMA_CHANNEL_PRIV_CFG) == sizeof(gdma_channel_priv_cfg_t),
    "FBIF_APERTURE_CFG out of sync with fbif_aperture_cfg_t!");

typedef gdma_addr_mode_t GDMA_ADDR_MODE;

#ifdef LW_PRGNLCL_GDMA_CHAN_TRANS_CONFIG2_SRC_FB_COHERENT
typedef gdma_l2c_t GDMA_L2C;

typedef struct
{
    bool fbCoherent;
    GDMA_L2C fbL2cRd;
    uint8_t fbWpr;
} GDMA_EXT_ADDR_CFG;

_Static_assert(sizeof(GDMA_EXT_ADDR_CFG) == sizeof(gdma_ext_addr_cfg_t),
    "FBIF_APERTURE_CFG out of sync with fbif_aperture_cfg_t!");
#else

typedef struct
{
    uint8_t dbbGscid;
    bool dbbVpr;
} GDMA_EXT_ADDR_CFG;

_Static_assert(sizeof(GDMA_EXT_ADDR_CFG) == sizeof(gdma_ext_addr_cfg_t),
    "FBIF_APERTURE_CFG out of sync with fbif_aperture_cfg_t!");
#endif // LIBLWRISCV_GDMA_LEGACY_H

typedef struct
{
    GDMA_ADDR_MODE srcAddrMode;
    GDMA_ADDR_MODE destAddrMode;
    uint8_t wrapLen;
    uint8_t strideLogLen;
    bool bDestPosted;
    GDMA_EXT_ADDR_CFG srcExtAddrCfg;
    GDMA_EXT_ADDR_CFG destExtAddrCfg;
} GDMA_ADDR_CFG;

_Static_assert(sizeof(GDMA_ADDR_CFG) == sizeof(gdma_addr_cfg_t),
    "FBIF_APERTURE_CFG out of sync with fbif_aperture_cfg_t!");

#define gdmaCfgChannel(chan_idx, cfg) \
    gdma_cfg_channel((chan_idx), (const gdma_channel_cfg_t *)(cfg))

#define gdmaCfgChannelPriv(chan_idx, p_cfg) \
    gdma_cfg_channel_priv((chan_idx), (const gdma_channel_priv_cfg_t *)(p_cfg))

#define gdmaXferReg(src, dest, length, p_addr_cfg, chan_idx, sub_chan_idx, b_wait_complete, b_irq_en) \
    gdma_xfer_reg((src), (dest), (length), (gdma_addr_cfg_t *)(p_addr_cfg), (chan_idx), (sub_chan_idx), (b_wait_complete), (b_irq_en))

#define gdmaGetChannelStatus(chan_idx, p_req_produce, p_req_consume, p_req_complete) \
    gdma_get_channel_status((chan_idx), (p_req_produce), (p_req_consume), (p_req_complete))

#define gdmaGetErrorAndResetChannel(chan_idx, error_cause, error_id) \
    gdma_get_error_and_reset_channel(chan_idx, error_cause, error_id)

#endif // LIBLWRISCV_GDMA_LEGACY_H
