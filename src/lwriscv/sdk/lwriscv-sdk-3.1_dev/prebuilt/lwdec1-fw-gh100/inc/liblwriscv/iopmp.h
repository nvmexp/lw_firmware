/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef LIBLWRISCV_IOPMP_H
#define LIBLWRISCV_IOPMP_H

/*!
 * @file        iopmp.h
 * @brief       Interfaces for the Peregrine IO-PMP controls.
 */

// Standard includes.
#include <stdbool.h>

// SDK includes.
#include <lwmisc.h>

// LIBLWRISCV includes.
#include <liblwriscv/io.h>


// Temporary work-around for typo in PRGNLCL manual (bug #3394780).
#ifndef LW_PRGNLCL_RISCV_IOPMP_ERR_STAT_VALID_TRUE
#define LW_PRGNLCL_RISCV_IOPMP_ERR_STAT_VALID_TRUE LW_PRGNLCL_RISCV_IOPMP_ERR_STAT_VALID_TURE
#endif


/*!
 * @brief Checks whether error-capture is enabled for a given IO-PMP master.
 *
 * @param[in] master  The IO-PMP master to check (e.g. CPDMA).
 *
 * @return
 *    true  if error-capture is enabled.
 *    false if error-capture is disabled.
 *
 * @note Does not supported vectored masters (use the _IDX variant instead).
 */
#define IOPMP_IS_CAPTURE_ENABLED(master) (                                    \
    FLD_TEST_DRF(_PRGNLCL_RISCV, _IOPMP_ERR_CAPEN, _MASTER_##master, _ENABLE, \
        localRead(LW_PRGNLCL_RISCV_IOPMP_ERR_CAPEN))                          \
)

/*!
 * @brief Checks whether error-capture is enabled for a vectored IO-PMP master.
 *
 * @param[in] master  The vectored IO-PMP master to check (e.g. PMB).
 * @param[in] index   The index of the desired vector entry.
 *
 * @return
 *    true  if error-capture is enabled.
 *    false if error-capture is disabled.
 */
#define IOPMP_IS_CAPTURE_ENABLED_IDX(master, index) (                       \
    FLD_IDX_TEST_DRF(_PRGNLCL_RISCV, _IOPMP_ERR_CAPEN, _MASTER_##master,    \
        (index), _ENABLE, localRead(LW_PRGNLCL_RISCV_IOPMP_ERR_CAPEN))      \
)


/*!
 * @brief Checks whether an IO-PMP error is pending.
 *
 * @return
 *    true  if an error is pending.
 *    false otherwise.
 */
static inline bool
iopmpHasError(void)
{
    return FLD_TEST_DRF(_PRGNLCL_RISCV, _IOPMP_ERR_STAT, _VALID, _TRUE,
        localRead(LW_PRGNLCL_RISCV_IOPMP_ERR_STAT));
}

#endif // LIBLWRISCV_IOPMP_H
