/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file
 * @see     https://confluence.lwpu.com/display/RMCLOC/Clocks+3.0
 * @author  Antone Vogt-Varvak
 */

#include "clk3/clksignal.h"

/*******************************************************************************
 * ClkSignalPath
*******************************************************************************/

/*!
 * @memberof    ClkSignalPath
 * @brief       Compare paths
 *
 * @details     This function returns true iff 'path' conforms to 'target',
 *              which means that the two paths are equal from the top of the
 *              stack to the bottom of 'target' stack.  If 'target' is longer
 *              than 'path', false is returned.
 *
 *              This operation is not symmetric.
 *
 * @note        public:         Any function may call this function.
 *
 * @todo        On gcc, consider using exclusive-or and __builtin_ffs here so
 *              that the loop is not needed and this can be colwerted to a
 *              macro or inlined function.
 *
 * @see         http://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html
 *
 * @param[in]   path            Path to be checked
 * @param[in]   target          Target path
 */
LwBool
clkConforms_SignalPath
(
    ClkSignalPath path,
    ClkSignalPath target
)
{
    while (LW_TRUE)
    {
        // All is good if we get to the end of the target stack.
        if (CLK_IS_EMPTY__SIGNAL_PATH(target))
        {
            return LW_TRUE;
        }

        // False if we disagree or reach the end of this stack.
        if (CLK_HEAD__SIGNAL_PATH(path) != CLK_HEAD__SIGNAL_PATH(target) &&
            CLK_HEAD__SIGNAL_PATH(target) != CLK_SIGNAL_NODE_INDETERMINATE)
        {
            return LW_FALSE;
        }

        // Pop the head node and continue.
        path   = CLK_POP__SIGNAL_PATH(path);
        target = CLK_POP__SIGNAL_PATH(target);
    }
}


/*******************************************************************************
 * ClkSignal / ClkTargetSignal
*******************************************************************************/

/*!
 * @memberof    ClkSignal
 * @brief       Used to reset/ilwalidate the signal
 * @details     Everything is indeterminate.
 */
const ClkSignal clkReset_Signal
        GCC_ATTRIB_SECTION("dmem_clk3x", "clkReset_Signal") =
{
    .freqKHz        = 0,
    .path           = CLK_SIGNAL_PATH_EMPTY,
    .source         = LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID,
    .fracdiv        = LW_TRISTATE_INDETERMINATE,
    .regimeId       = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID,
    .dvcoMinFreqMHz = 0,
};


/*!
 * @memberof    ClkPmuRpc
 * @brief       Signal for objects of this class
 * @details     Everything is disabled or empty.
 */
const ClkSignal clkEmpty_Signal
        GCC_ATTRIB_SECTION("dmem_clk3x", "clkEmpty_Signal") =
{
    .freqKHz        = 0,
    .path           = CLK_SIGNAL_PATH_EMPTY,
    .source         = LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID,
    .fracdiv        = LW_TRISTATE_FALSE,
    .regimeId       = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID,
    .dvcoMinFreqMHz = 0,
};


/*!
 * @memberof    ClkTargetSignal
 * @brief       Used to reset/ilwalidate the signal
 * @note        If this changes, you'll want to change
 *              clkReset_CallNode_DbgInfo.
 */
const ClkTargetSignal clkReset_TargetSignal
        GCC_ATTRIB_SECTION("dmem_clk3x", "clkReset_TargetSignal") =
{
    .super = {
        .freqKHz        = 0,
        .path           = CLK_SIGNAL_PATH_EMPTY,
        .source         = LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID,
        .fracdiv        = LW_TRISTATE_INDETERMINATE,
        .regimeId       = LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID,
        .dvcoMinFreqMHz = 0,
    },
    .rangeKHz = {
        .min =          0,
        .max =          0xffffffff,
    },
};


/*!
 * @brief       Print debugging info
 *
 * @memberof    ClkSignal
 *
 * @param[in]   this        Required signal to print
 * @param[in]   prefix      Required text to place at the start of the message
 */
#if CLK_PRINT_ENABLED
void clkPrint_Signal(ClkSignal *this, const char *prefix)
{
    CLK_PRINTF("%s: freq=%uKHz path=0x%08x fracdiv=%u regine=0x%02x source=0x%02x dvcoMinFreq=%uMHz\n",
            prefix,                             // To identify the object
            (unsigned) this->freqKHz,           // Zero means unknown
            (unsigned) this->path,              // Stack, 'f' means don't-care
            (unsigned) this->fracdiv,           // 0=no  1=yes  2=don't care
            (unsigned) this->regimeId,          // LW2080_CTRL_CLK_NAFLL_REGIME_ID_<XYZ>
            (unsigned) this->source,            // LW2080_CTRL_CLK_PROG_1X_SOURCE_<XYZ>
            (unsigned) this->dvcoMinFreqMHz);   // Zero means n/a
}
#endif
