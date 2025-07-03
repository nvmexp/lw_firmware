/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_MEMUTILS_H
#define LIBLWRISCV_MEMUTILS_H

#include <stddef.h>
#include <stdint.h>

#include <lwriscv/status.h>

#if LWRISCV_HAS_FBIF
#include <liblwriscv/fbif.h>
#endif // LWRISCV_HAS_FBIF


//
// Memory targets supported by riscvPaToTargetOffset().
// Expand as needed.
//
typedef enum RISCV_MEM_TARGET
{
    RISCV_MEM_TARGET_IMEM,
    RISCV_MEM_TARGET_DMEM,
    RISCV_MEM_TARGET_FBGPA,
    RISCV_MEM_TARGET_SYSGPA,
}RISCV_MEM_TARGET;


#if LWRISCV_HAS_FBIF
/*!
 * @brief Colwerts a RISCV PA to an FBIF target/offset pair.
 *
 * @param[in]  pa       The RISCV PA to colwert.
 *
 * @param[out] pTarget  The target aperture (e.g. COHERENT_SYSTEM) that the PA
 *                      resides within.
 *
 * @param[out] pOffset  The offset of the PA within the target aperture.
 *
 * @return LWRV_OK               if successful.
 *
 *         LWRV_ERR_OUT_OF_RANGE if PA is not within the range of a valid
 *                               memory aperture (SYSMEM or FB).
 */
LWRV_STATUS riscvPaToFbifAperture(uintptr_t pa, FBIF_TRANSCFG_TARGET *pTarget,
    uint64_t *pOffset);
#endif // LWRISCV_HAS_FBIF

/*!
 * @brief Colwerts a RISCV PA to a global target/offset pair.
 *
 * @param[in]  pa       The RISCV PA to colwert.
 *
 * @param[out] pTarget  The memory target (e.g. SYSMEM) that the PA resides
 *                      within (optional).
 *
 * @param[out] pOffset  The offset of the PA within the memory target
 *                      (optional).
 *
 * @return LWRV_OK               if successful.
 *
 *         LWRV_ERR_OUT_OF_RANGE if PA does not fall inside any of the
 *                               supported memory targets.
 */
LWRV_STATUS riscvPaToTargetOffset(uintptr_t pa, RISCV_MEM_TARGET *pTarget,
    uint64_t *pOffset);

#endif //LIBLWRISCV_MEMUTILS_H
