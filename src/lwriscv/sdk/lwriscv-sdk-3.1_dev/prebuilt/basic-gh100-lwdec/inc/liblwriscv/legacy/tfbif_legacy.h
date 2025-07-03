/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_TFBIF_LEGACY_H
#define LIBLWRISCV_TFBIF_LEGACY_H


/*
 * This file is to maintain support for legacy clients which use the old deprecated
 * function names. Please do not use these functions in new code. Existing code should
 * migrate to using the new function names if possible.
 */

/* 
 * need to declare the old version of this struct. Can't use a typedef because the 
 * names of the fields have changed.
 */
typedef struct {
    uint8_t apertureIdx;
    uint8_t swid;
    bool vpr;
    uint8_t apertureId;
} TFBIF_APERTURE_CFG;


_Static_assert(sizeof(TFBIF_APERTURE_CFG) == sizeof(tfbif_aperture_cfg_t),
    "FBIF_APERTURE_CFG out of sync with fbif_aperture_cfg_t!");

#define tfbifConfigureAperture(p_cfgs, numCfgs) \
    tfbif_configure_aperture((p_cfgs), (numCfgs))

#endif // LIBLWRISCV_TFBIF_LEGACY_H
