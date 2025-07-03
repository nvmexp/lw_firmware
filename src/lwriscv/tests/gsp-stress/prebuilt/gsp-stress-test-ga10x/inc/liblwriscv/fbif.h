/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_FBIF_H
#define LIBLWRISCV_FBIF_H

#include <stdint.h>
#include <stdbool.h>
#include <lwriscv/status.h>

#ifndef LWRISCV_HAS_FBIF
#error "This header should only be used on engines which have a FBIF block."
#endif // LWRISCV_HAS_FBIF

#define FBIF_NUM_APERTURES 8

/*!
 * Enum values corresponding to the Fields of the TRANSCFG register
 */
typedef enum {
    FBIF_TRANSCFG_TARGET_LOCAL_FB,
    FBIF_TRANSCFG_TARGET_COHERENT_SYSTEM,
    FBIF_TRANSCFG_TARGET_NONCOHERENT_SYSTEM,
    FBIF_TRANSCFG_TARGET_COUNT
} FBIF_TRANSCFG_TARGET;

typedef enum {
    FBIF_TRANSCFG_L2C_L2_EVICT_FIRST,
    FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
    FBIF_TRANSCFG_L2C_L2_EVICT_LAST,
    FBIF_TRANSCFG_L2C_COUNT
} FBIF_TRANSCFG_L2C;

/*!
 * Configuration struct for a FBIF Aperture
 */
typedef struct {
    /*
     * The aperture to configure (0-7)
     */
    uint8_t apertureIdx;

    /*
     * Values to program into the FBIF_TRANSCFG register
     */
    FBIF_TRANSCFG_TARGET target;

    /*
     * true : use Virtual Addressing
     * false : use Physical Addressing
     */
    bool bTargetVa;
    FBIF_TRANSCFG_L2C l2cWr;
    FBIF_TRANSCFG_L2C l2cRd;
    bool fbifTranscfWachk0Enable;
    bool fbifTranscfWachk1Enable;
    bool fbifTranscfRachk0Enable;
    bool fbifTranscfRachk1Enable;

    /*
     * false : FBIF_TRANSCFG_ENGINE_ID_FLAG_BAR2_FN0
     * true : FBIF_TRANSCFG_ENGINE_ID_FLAG_OWN
     */
    bool bEngineIdFlagOwn;

    /*
     * Value to program into the FBIF_REGIONCFG_Tx field corresponding to this aperture
     */
    uint8_t regionid;
} FBIF_APERTURE_CFG;

/*!
 * \brief Configure some of the 8 DMA apertures in FBIF.
 * A similar function (tfbifConfigureAperture) exists for TFBIF
 * Which is used on CheetAh SOC engines.
 *
 * @param[in] cfgs an array of FBIF_APERTURE_CFG - one per aperture
 * @param[in] numCfgs is the size of cfgs (1-8)
 * @return LWRV_OK on success
 *         LWRV_ERR_ILWALID_ARGUMENT if cfgs is not sane.
 *              Some of the cfgs may still be applied, even if this error is returned.
 *
 * - Not thread safe. Client must ensure that there is no chance of this function being
 *   preempted by another Task or ISR which may call a DMA function or touch the FBIF registers
 */
LWRV_STATUS fbifConfigureAperture(const FBIF_APERTURE_CFG *cfgs, uint8_t numCfgs);
#endif // LIBLWRISCV_FBIF_H
