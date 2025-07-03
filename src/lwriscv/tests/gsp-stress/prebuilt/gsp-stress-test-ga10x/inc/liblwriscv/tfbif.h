/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_TFBIF_H
#define LIBLWRISCV_TFBIF_H

#include <stdint.h>
#include <stdbool.h>
#include <lwriscv/status.h>

#ifndef LWRISCV_HAS_TFBIF
#error "This header should only be used on engines which have a TFBIF block."
#endif // LWRISCV_HAS_TFBIF

#define NUM_TFBIF_APERTURES 8

/*!
 * Configuration struct for a TFBIF Aperture. This is the CheetAh equivilant of FBIF_APERTURE_CFG
 */
typedef struct {
    /*
     * The aperture to configure (0-7)
     */
    uint8_t apertureIdx;

    /*
     * Value to program into the LW_PRGNLCL_TFBIF_TRANSCFG_ATTx_SWID field
     */
    uint8_t swid;

    /*
     * Value to program into the REGIONCFG_Tx_VPR field corresponding to this aperture
     */
    bool vpr;

    /*
     * Aperture ID AKA GSC_ID
     * Value to program into the REGIONCFGx_Tx_APERT_ID field corresponding to this aperture
     */
    uint8_t apertureId;
} TFBIF_APERTURE_CFG;

/*!
 * \brief Configure some of the 8 DMA apertures in TFBIF.
 * A similar function (fbifConfigureAperture) exists for FBIF
 * Which is used on dGPU and CheetAh iGPU engines.
 *
 * @param[in] cfgs an array of TFBIF_APERTURE_CFG - one per aperture
 * @param[in] numCfgs is the size of cfgs (1-8)
 * @return LWRV_OK on success
 *         LWRV_ERR_ILWALID_ARGUMENT if cfgs is not sane
 *
 * - Not thread safe. Client must ensure that there is no chance of this function being
 *   preempted by another Task or ISR which may call a DMA function or touch the FBIF registers
 */
LWRV_STATUS tfbifConfigureAperture(const TFBIF_APERTURE_CFG *cfgs, uint8_t numCfgs);
#endif // LIBLWRISCV_TFBIF_H
