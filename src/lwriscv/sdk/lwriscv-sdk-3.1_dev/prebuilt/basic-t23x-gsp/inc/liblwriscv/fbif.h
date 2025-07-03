/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
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

/**
 * @brief The number of configurable FBIF apertures.
 */
#define FBIF_NUM_APERTURES 8U

/**
 * @brief Possible targets for external memory access. These enum values
 * correspond to the TARGET field of the TRANSCFG register.
 *
 * @typedef-title fbif_transcfg_target_t
 */
typedef enum
{
    FBIF_TRANSCFG_TARGET_LOCAL_FB,
    FBIF_TRANSCFG_TARGET_COHERENT_SYSTEM,
    FBIF_TRANSCFG_TARGET_NONCOHERENT_SYSTEM,
    FBIF_TRANSCFG_TARGET_COUNT
} fbif_transcfg_target_t;

/**
 * @brief L2 Cache eviction policies for external memory access. These enum
 * values correspond to the L2C_WR and L2C_RD fields of the TRANSCFG register.
 *
 * @typedef-title fbif_transcfg_l2c_t
 */
typedef enum
{
    FBIF_TRANSCFG_L2C_L2_EVICT_FIRST,
    FBIF_TRANSCFG_L2C_L2_EVICT_NORMAL,
    FBIF_TRANSCFG_L2C_L2_EVICT_LAST,
    FBIF_TRANSCFG_L2C_COUNT
} fbif_transcfg_l2c_t;

/**
 * @brief Configuration struct for an FBIF Aperture. The fields in this structure
 * are used to configure the FBIF_TRANSCFG and FBIF_REGIONCFG registers of the
 * FBIF, which determine how the FBDMA engine will access external memory. More
 * details on these fields can be found in the FBIF Hardware documentation.
 *
 * ``aperture_idx``: The aperture to configure (0-7)
 *
 * ``target``: External memory target. Programmed into the TARGET field of the
 * FBIF_TRANSCFG register corresponding to this aperture.
 *
 * ``b_target_va``: Programmed into the MEM_TYPE field of the FBIF_TRANSCFG register
 * corresponding to this aperture.
 *  True : use virtual access opcode
 *  False : use physical access opcode, based on 'target'
 *
 * ``l2c_wr``: L2 Cache eviction class for write requests. Programmed into the L2C_WR
 * field of the FBIF_TRANSCFG register corresponding to this aperture.
 *
 * ``l2c_rd``: L2 Cache eviction class for read requests. Programmed into the L2C_RD
 * field of the FBIF_TRANSCFG register corresponding to this aperture.
 *
 * ``b_fbif_transcfg_wachk0_enable``: Perform Address Check 0 on writes. Programmed into
 * the WACHK0 field of the FBIF_TRANSCFG register corresponding to this aperture.
 *
 * ``fbif_transcfg_wachk1_enable``: Perform Address Check 1 on writes. Programmed into
 * the WACHK1 field of the FBIF_TRANSCFG register corresponding to this aperture.
 *
 * ``fbif_transcfg_rachk0_enable``: Perform Address Check 0 on reads. Programmed into
 * the RACHK0 field of the FBIF_TRANSCFG register corresponding to this aperture.
 *
 * ``fbif_transcfg_rachk1_enable``: Perform Address Check 1 on reads. Programmed into
 * the RACHK1 field of the FBIF_TRANSCFG register corresponding to this aperture.
 *
 * ``b_engine_id_flag_own``: Selects the value to program into the ENGINE_ID_FLAG
 *  field of the the FBIF_TRANSCFG register corresponding to this aperture.
 *                   true : FBIF_TRANSCFG_ENGINE_ID_FLAG_OWN
 *                   false : FBIF_TRANSCFG_ENGINE_ID_FLAG_BAR2_FN0
 *
 * ``region_id``: Value to program into the FBIF_REGIONCFG register corresponding
 * to this aperture. This is the WPR_ID (0-3) which will be used for accesses
 * through this aperture. 0 = Inselwre accesses. 1-3 = Used when accessing a WPR.
 */
typedef struct {
    uint8_t aperture_idx;
    fbif_transcfg_target_t target;
    bool b_target_va;
    fbif_transcfg_l2c_t l2c_wr;
    fbif_transcfg_l2c_t l2c_rd;
    bool b_fbif_transcfg_wachk0_enable;
    bool b_fbif_transcfg_wachk1_enable;
    bool b_fbif_transcfg_rachk0_enable;
    bool b_fbif_transcfg_rachk1_enable;
    bool b_engine_id_flag_own;
    uint8_t region_id;
} fbif_aperture_cfg_t;

/**
 * @brief Configure some of the 8 DMA apertures in FBIF.
 * A similar function (tfbif_configure_aperture) exists for TFBIF
 * which is used on CheetAh SOC engines. This function is not thread
 * safe. The client must ensure that there is no chance of it being
 * preempted by another task or ISR which may call a DMA function
 * or touch the FBIF registers.
 *
 * @param[in] p_cfgs an array of fbif_aperture_cfg_t - one per aperture
 * @param[in] num_cfgs is the size of cfgs (1-8)
 * @return LWRV_OK on success
 *         LWRV_ERR_ILWALID_ARGUMENT if cfgs is not sane.
 *              Some of the cfgs may still be applied, even if this error is returned.
 */
LWRV_STATUS fbif_configure_aperture(const fbif_aperture_cfg_t *p_cfgs, uint8_t num_cfgs);

#if LWRISCV_FEATURE_LEGACY_API 
/*
 * Maintain legacy support for clients which use the old function names.
 * Include at the end so that it may access the types in this fiile and typedef them.
 */
#include <liblwriscv/legacy/fbif_legacy.h>
#endif //LWRISCV_FEATURE_LEGACY_API

#endif // LIBLWRISCV_FBIF_H
