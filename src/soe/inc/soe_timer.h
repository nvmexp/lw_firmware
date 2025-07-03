/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_TIMER_H
#define SOE_TIMER_H

/*!
 * @file   soe_timer.h
 * @brief  Common defines
*/

/*!
 * @brief   Macro to set TIMER TIMER_0_INTERVAL value with interval in usec.
 *
 * @note    This TIMER implementation is specific only to SOE and its clones
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
 *          extra cycle to take care.
 *
 *          3. Since PTIMER_1US is 1.024us, rounds up when fraction part exceeds 0.5,
 *          and rounds down else.
 *
 * @param[in]   intervalUs  Time interval in usec to program.
 */
#define INTERVALUS_TO_TIMER0_ONESHOT_VAL(intervalUs)    (((intervalUs)*1000+512)/1024)
#define INTERVALUS_TO_TIMER0_CONTINUOUS_VAL(intervalUs) ((((intervalUs)*1000+512)/1024)-1)

typedef enum
{
    TIMER_MODE_SETUP = 0,
    TIMER_MODE_START,
    TIMER_MODE_STOP,
} TIMER_MODE;

typedef enum
{
    TIMER_EVENT_NONE = 0,
    TIMER_EVENT_THERM,
} TIMER_EVENT;

#endif // SOE_TIMER_H
