/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_FBIF_LEGACY_H
#define LIBLWRISCV_FBIF_LEGACY_H

/*
 * This file is to maintain support for legacy clients which use the old deprecated
 * function names. Please do not use these functions in new code. Existing code should
 * migrate to using the new function names if possible.
 */

typedef fbif_transcfg_target_t FBIF_TRANSCFG_TARGET;

typedef fbif_transcfg_l2c_t FBIF_TRANSCFG_L2C;

/* 
 * need to declare the old version of this struct. Can't use a typedef because the 
 * names of the fields have changed.
 */
typedef struct {
    uint8_t apertureIdx;
    FBIF_TRANSCFG_TARGET target;
    bool bTargetVa;
    FBIF_TRANSCFG_L2C l2cWr;
    FBIF_TRANSCFG_L2C l2cRd;
    bool fbifTranscfWachk0Enable;
    bool fbifTranscfWachk1Enable;
    bool fbifTranscfRachk0Enable;
    bool fbifTranscfRachk1Enable;
    bool bEngineIdFlagOwn;
    uint8_t regionid;
} FBIF_APERTURE_CFG;

_Static_assert(sizeof(FBIF_APERTURE_CFG) == sizeof(fbif_aperture_cfg_t),
    "FBIF_APERTURE_CFG out of sync with fbif_aperture_cfg_t!");

#define fbifConfigureAperture(p_cfgs, numCfgs) \
    fbif_configure_aperture((const fbif_aperture_cfg_t *)(p_cfgs), (numCfgs))

#endif // LIBLWRISCV_FBIF_LEGACY_H
