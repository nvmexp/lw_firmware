/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#ifndef LIBLWRISCV_SLEEP_H
#define LIBLWRISCV_SLEEP_H

#include <stdint.h>

/**
 * @brief Stall exelwtion for at least \arg cycles cpu cycles.
 * @param cycles Number of cycles to wait
 * @return Number of cycles that were actually wait (may be greater)
 *
 * \note There is no guarantee that function will take exactly \arg cycles cycles.
 * \warning Core needs access to cycles performance counter (see @sa rdcycles)
 */
uint64_t delay(uint64_t cycles);

/**
 * @brief Put core to sleep for at least \arg uSec microseconds.
 * @param uSec Number of microseconds to wait
 *
 * \note There is no guarantee that function will take exactly \arg cycles cycles.
 * \warning Core needs access to time performance counter (see @sa rdtime)
 */
void     usleep(uint64_t uSec);

#endif // LIBLWRISCV_SLEEP_H
