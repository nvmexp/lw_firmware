/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_RTTIMER_H
#define SOE_RTTIMER_H

/*!
 * @file   soe_rttimer.h
 * @brief  Common defines
*/

/*!
 * @brief   Macro to set RTTIMER TIMER_0_INTERVAL value with interval in usec.
 *
 * @note    This RTTIMER implementation is specific only to SOE and its clones
 *          (GSP-lite and GSP):
 *          1. TIMER0 is working in engine clock domain, and TIMER0 counter will
 *          derease by '1' once the tick event happens(rising edge of engine
 *          clock, or ptimer bit[5] toggle - 1024 clocks thus actual PTIMER_1US
 *          physical time is 1.024us).
 *
 *          2. Once Timer Counter became zero, IRQ will generated @(next engine
 *          clock rising edge). For Continuous counting mode, Timer Counter
 *          will load initial interval value @ (next tick event) and continue
 *          counting down @ (every tick event). The TIMER0 counter will stay at
 *          0 for one full tick period before reloading the interval, hence have
 *          extra cycyle to take care.
 *
 *          3. Since PTIMER_1US is 1.024us, rounds up when fraction part exceeds 0.5,
 *          and rounds down else.
 *
 * @param[in]   intervalUs  Time interval in usec to program.
 */
#define INTERVALUS_TO_TIMER0_CONTINUOUS_VAL(intervalUs) ((((intervalUs)*1000+512)/1024)-1)

#endif // SOE_RTTIMER_H

