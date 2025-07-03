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
 * @author  Eric Colter
 * @author  Antone Vogt-Varvak
 */

#ifndef CLK3_SIGNAL_H
#define CLK3_SIGNAL_H

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmusw.h"
#include "ctrl/ctrl2080/ctrl2080clkavfs.h"
#endif                      // CLK_SD_CHECK

#include "pmu/pmuifclk.h"
#include "clk3/clkprint.h"


/*******************************************************************************
 * ClkSignalPath/ClkSignalNode typedefs
*******************************************************************************/

/*!
 * @brief       Stack of nodes representing the signal path
 *
 * @detail      The special value CLK_SIGNAL_PATH_EMPTY represents an empty stack
 *
 *              The special value CLK_SIGNAL_NODE_INDETERMINATE terminates the stack.
 *              It is illegal to push this value onto the stack.
 *
 *              If the number of bits increases, it may be useful to change
 *              the order of members within ClkSignal.  See comments there.
 */
typedef RM_PMU_CLK_SIGNAL_PATH ClkSignalPath;

/*!
 * @brief       Node from a signal path representing a frequency source object
 *
 * @detail      Each node may be used as an index for an array of potential inputs
 *              for any frequency source object which may take multiple inputs.
 *
 *              The special value CLK_SIGNAL_NODE_INDETERMINATE represents an
 *              indeterminate or unspecified node which is to be completed by a
 *              call to clkConfig.
 */
typedef LwU8    ClkSignalNode;


/*******************************************************************************
 * ClkSignalPath/ClkSignalNode Constants
*******************************************************************************/

// Number of bits for each signal node.
#define CLK_SIGNAL_NODE_WIDTH           RM_PMU_CLK_SIGNAL_NODE_WIDTH

// Number of nodes that one signal path may contain
#define CLK_SIGNAL_PATH_LENGTH          ((sizeof(ClkSignalPath) * 8) / CLK_SIGNAL_NODE_WIDTH)

// Mask to isolate the lowest-ordered node from the signal path
#define CLK_SIGNAL_NODE_MASK            (BIT(CLK_SIGNAL_NODE_WIDTH) - 1)

//
// Number of potential nodes that can be represented with a single node value.
// We subtract one so because _INDETERMINATE is not included with the count.
//
#define CLK_SIGNAL_NODE_COUNT           (BIT(CLK_SIGNAL_NODE_WIDTH) - 1)

// Value to indicate that the node is not applicable nor specified
#define CLK_SIGNAL_NODE_INDETERMINATE   ((ClkSignalNode) CLK_SIGNAL_NODE_COUNT)

// Value to indicate an empty stack (Entire path is indeterminate.)
#define CLK_SIGNAL_PATH_EMPTY           ((ClkSignalPath) RM_PMU_CLK_SIGNAL_PATH_EMPTY)

//
// Bitmaps require one bit for each potential input.
// Nibblemaps require four bits for each potential input.
// Revision will be required if the number of potential inputs exceeds 64.
//
#if CLK_SIGNAL_NODE_COUNT <= 16
#define CLK_SIGNAL_NODE_BITMAP_TYPE    LwU16
#define CLK_SIGNAL_NODE_NIBBLEMAP_TYPE LwU64

#elif CLK_SIGNAL_NODE_COUNT <= 32
#define CLK_SIGNAL_NODE_BITMAP_TYPE    LwU32
#define CLK_SIGNAL_NODE_NIBBLEMAP_TYPE LwU128  /* Not lwrrently defined */

#elif CLK_SIGNAL_NODE_COUNT <= 64
#define CLK_SIGNAL_NODE_BITMAP_TYPE    LwU64
#define CLK_SIGNAL_NODE_NIBBLEMAP_TYPE LwU256  /* Not lwrrently defined */

#else
#error Unable to determine proper definitions for CLK_SIGNAL_NODE_BITMAP_TYPE and CLK_SIGNAL_NODE_NIBBLEMAP_TYPE.
#endif


/*******************************************************************************
 * ClkSignalPath Macros
*******************************************************************************/

/*!
 * @memberof    ClkSignalPath
 * @brief       Head node from the signal path stack
 *
 * @details     Since the path is a stack, the head node is the next one that
 *              will be popped and the last one that was pushed.  In general,
 *              it represents the active input for a frequency source object
 *              that has multiple inputs.
 *
 * @note        public:         Any function may call this macro.
 * @note        macro:          Defined as a macro to avoid stack frame.
 *
 * @param[in]   _path           Signal path from which we get the head node
 *
 * @return      Top (lowest ordered) node in the signal path.
 * @retval      CLK_SIGNAL_NODE_INDETERMINATE   The stack is empty.
 */
#define CLK_HEAD__SIGNAL_PATH(_path)        ((ClkSignalNode)((_path) & CLK_SIGNAL_NODE_MASK))


/*!
 * @memberof    ClkSignalPath
 * @brief       Signal path stack with one node
 *
 * @note        public:         Any function may call this macro.
 * @note        macro:          Defined as a macro to avoid stack frame.
 *
 * @param[in]   _node           Node that is the head of the resulting path
 *
 * @note        CLK_PUSH__SIGNAL_PATH is not used because it asserts that
 *              '_node' is not _INDETERMINATE, which is not a problem is this
 *              case.
 *
 * @return      Top (lowest ordered) node in the signal path.
 * @retval      CLK_SIGNAL_NODE_INDETERMINATE   The stack is empty.
 */
#define CLK_SINGLETON__SIGNAL_PATH(_node)   ((CLK_SIGNAL_PATH_EMPTY << CLK_SIGNAL_NODE_WIDTH) | (_node))


/*!
 * @memberof    ClkSignalPath
 * @brief       Indicate an empty path
 *
 * @details     Check if the path is empty (defined by CLK_SIGNAL_PATH_EMPTY)
 *
 * @note        public:         Any function may call this macro.
 * @note        macro:          Defined as a macro to avoid stack frame.
 *
 * @param[in]   _path           Signal path to test for emptiness
 *
 * @return      Top (lowest ordered) node in the signal path.
 * @retval      CLK_SIGNAL_NODE_INDETERMINATE   The stack is empty.
 */
#define CLK_IS_EMPTY__SIGNAL_PATH(_path)    ((_path) == CLK_SIGNAL_PATH_EMPTY)


/*!
 * @memberof    ClkSignalPath
 * @brief       Pop one node from the signal path stack
 *
 * @details     It is okay to pop from an empty path stack.
 *
 * @note        public:         Any function may call this function.
 * @note        macro:          Defined as a macro to avoid stack frame.
 *
 * @post        The top (lowest ordered) node is removed from the path.
 *              The highest ordered is CLK_SIGNAL_NODE_INDETERMINATE to end the stack.
 *              The path colwerges to CLK_SIGNAL_PATH_EMPTY in no more
 *              than CLK_SIGNAL_PATH_LENGTH calls to this function.
 *
 * @param[in]   _path           Signal path from which to pop (this)
 *
 * @return      Path with head node popped from '_path'
 */
#define CLK_POP__SIGNAL_PATH(_path)                                                                     \
    (((_path) >> CLK_SIGNAL_NODE_WIDTH)                             /* Remove the head node          */ \
    | (CLK_SIGNAL_NODE_INDETERMINATE <<                             /* Mark the highest ordered node */ \
        ((CLK_SIGNAL_PATH_LENGTH - 1) * CLK_SIGNAL_NODE_WIDTH)))    /* (i.e. last) as indeterminate. */


/*!
 * @memberof    ClkSignalPath
 * @brief       Push one node onto the signal path stack
 *
 * @details     The node is added to the path as the next (lowest ordered) node.
 *              The highest ordered node is discarded.
 *
 *              Since CLK_SIGNAL_NODE_INDETERMINATE is used to mark the end of
 *              the stack, it can not be pushed.  An assertion enforces this
 *              in debug builds.
 *
 *              No more than CLK_SIGNAL_PATH_LENGTH calls to this function is supported.
 *              In other words, it is illegal to push onto a full stack.  As such,
 *              the discarded node must not be CLK_SIGNAL_NODE_INDETERMINATE.
 *              An assertion enforces this in debug builds.
 *
 * @note        public:         Any function may call this function.
 * @note        macro:          Sometimes defined as a macro to avoid stack frame.
 *
 * @pre         '_path' must not be full.
 * @pre         '_node' must not be CLK_SIGNAL_NODE_INDETERMINATE.
 *
 * @param[in]   _path           Signal path to evaluate (this)
 * @param[in]   _node           Node to push onto the top
 *
 * @return      Path with '_node' pushed onto '_path'
 */
#define CLK_PUSH__SIGNAL_PATH(_path, _node)                                                \
    (((_path) << CLK_SIGNAL_NODE_WIDTH) | (_node))


/*!
 * @memberof    ClkSignalPath
 * @brief       Get specified node from the signal path stack
 *
 * @details     Node zero (the head node) is the next one that will be popped
 *              and the last one that was pushed.
 *
 * @ilwariant   CLK_PEEK__SIGNAL_PATH(0, path) == CLK_HEAD__SIGNAL_PATH(path)
 *
 * @ilwariant   The signal path is not changed.
 *
 * @note        public:         Any function may call this macro.
 * @note        macro:          Defined as a macro to avoid stack frame.
 *
 * @pre         '_index' must be less than CLK_SIGNAL_NODE_COUNT.
 *
 * @param[in]   _path           Signal path from which we get the head node
 * @param[in]   _index          Depth into the stack to get the return value
 *
 * @return      Specified node in the signal path.
 * @retval      CLK_SIGNAL_NODE_INDETERMINATE   The stack is empty or the node is indeterminate.
 */
#define CLK_PEEK__SIGNAL_PATH(_path, _index)                                \
        ((ClkSignalNode)(((_path) >> ((_index) * CLK_SIGNAL_NODE_WIDTH)) & CLK_SIGNAL_NODE_MASK))


/*!
 * @memberof    ClkSignalPath
 * @brief       Change specified node from the signal path stack
 *
 * @details     Node zero (the head node) is the next one that will be popped
 *              and the last one that was pushed.
 *
 * @note        public:         Any function may call this macro.
 * @note        macro:          Defined as a macro to avoid stack frame.
 *
 * @pre         '_index' must be less than CLK_SIGNAL_NODE_COUNT.
 * @pre         '_node' must fit within CLK_SIGNAL_NODE_MASK.
 *
 * @param[in    _path       Signal path from which we get the head node
 * @param[in]   _index      Depth into the stack of the node to modify
 * @param[in]   _node       Node value to replace at the index
 *
 * @return      Path with node at '_index' in '_path' changed to '_node'
 */
#define CLK_POKE__SIGNAL_PATH(_path, _index, _node)                         \
        (((_path) & (CLK_SIGNAL_NODE_MASK << (_index) * CLK_SIGNAL_NODE_WIDTH)) |   /* Zero out the old mode */ \
                                (((_node) << (_index) * CLK_SIGNAL_NODE_WIDTH)))    /* OR in the new mode value */


/*!
 * @memberof    ClkSignalPath
 * @brief       Compare paths
 *
 * @details     This function returns true iff 'this' conforms to 'target', which
 *              means that the two paths are equal from the top of the stack to
 *              the bottom of 'target' stack.  If 'target' is longer than 'this',
 *              false is returned.
 *
 *              This operation is not symmetric.
 *
 * @note        public:         Any function may call this function.
 *
 * @param[in]   self            Path to be checked
 * @param[in]   target          Target path
 */
LwBool clkConforms_SignalPath(ClkSignalPath path, ClkSignalPath target);


/*******************************************************************************
 * ClkSignal struct
*******************************************************************************/

typedef RM_PMU_CLK_SIGNAL ClkSignal;


/*!
 * @memberof    ClkSignal
 * @brief       Used to reset/ilwalidate the signal
 * @details     Everything is indeterminate.
 */
extern const ClkSignal clkReset_Signal;


/*!
 * @memberof    ClkSignal
 * @details     Everything is disabled or empty.
 */
extern const ClkSignal clkEmpty_Signal;


/*******************************************************************************
 * ClkSignal Macros
*******************************************************************************/

/*!
 * @memberof    ClkSignal
 * @brief       Indicates equal clock signals excluding the path and frequency.
 *
 * @details     Signals are considered equal within delta iff all data members
 *              are equal except the path and frequency.
 *
 *              This operation is symmetric.
 *
 * @note        macro:          Parameters may be evaluated more than once
 *
 * @param[in]   a               Signal to evaluate
 * @param[in]   b               Signal to evaluate
 *
 * @retval      LW_FALSE        a and b are not equal, ignoring path/frequency.
 * @retval      LW_TRUE         a and b are equal, ignoring path/frequency.
 */
#define CLK_ARE_EQUIVALENT_IGNORING_FREQ__SIGNAL(a, b)                                 \
   ((a).fracdiv   == (b).fracdiv   &&           /* Fractional divide params match   */ \
    (a).regimeId  == (b).regimeId)              /* Regime ID match                  */

/*!
 * @memberof    ClkSignal
 * @brief       Indicates effectively equal clock signals (ignoring path)
 *
 * @details     Signals are considered equivalent if all data members except the
 *              path are the same.
 *
 *              This operation is symmetric.
 *
 * @note        macro:          Parameters may be evaluated more than once
 *
 * @param[in]   a               Signal to evaluate
 * @param[in]   b               Signal to evaluate
 *
 * @retval      LW_FALSE        a and b are not equivalent.
 * @retval      LW_TRUE         a and b are equivalent.
 */
#define CLK_ARE_EQUIVALENT__SIGNAL(a, b)                                               \
   ((a).freqKHz == (b).freqKHz                  &&      /* Frequencies match        */ \
    CLK_ARE_EQUIVALENT_IGNORING_FREQ__SIGNAL(a, b))     /* Misc parameters match    */


/*!
 * @memberof    ClkSignal
 * @brief       Indicates equal clock signals including path
 *
 * @details     Signals are considered equal iff all data members are equal.
 *
 *              This operation is symmetric.
 *
 * @note        macro:          Parameters may be evaluated more than once
 *
 * @param[in]   a               Signal to evaluate
 * @param[in]   b               Signal to evaluate
 *
 * @retval      LW_FALSE        a and b are not equal.
 * @retval      LW_TRUE         a and b are equal.
 */
#define CLK_ARE_EQUAL__SIGNAL(a, b)                                        \
    ((a).path == (b).path           &&      /* Paths match              */ \
    CLK_ARE_EQUIVALENT__SIGNAL((a), (b)))   /* Signal qualities match   */


/*!
 * @memberof    ClkSignal
 * @brief       Indicates equal clock signals including path, but excluding the
 *              frequency.
 *
 * @details     Signals are considered equal within delta iff all data members
 *              are equal except the frequency.
 *
 *              This operation is symmetric.
 *
 * @note        macro:          Parameters may be evaluated more than once
 *
 * @param[in]   a               Signal to evaluate
 * @param[in]   b               Signal to evaluate
 *
 * @retval      LW_FALSE        a and b are not equal, ignoring frequency.
 * @retval      LW_TRUE         a and b are equal, ignoring frequency.
 */
#define CLK_ARE_EQUAL_IGNORING_FREQ__SIGNAL(a, b)                                  \
   ((a).path == (b).path                    &&          /* Paths match          */ \
    CLK_ARE_EQUIVALENT_IGNORING_FREQ__SIGNAL(a, b))     /* Parameters match     */


/*!
 * @memberof    ClkSignal
 * @brief       Indicates equal clock signals including path and the frequency
 *              is within tolerance.
 *
 * @details     Signals are considered equal within delta iff all data members
 *              are equal except frequency which should be within delta.
 *
 *              This operation is symmetric.
 *
 * @note        macro:          Parameters may be evaluated more than once
 *
 * @param[in]   a               Signal to evaluate
 * @param[in]   b               Signal to evaluate
 * @param[in]   deltaKHz        delta frequency
 *
 * @retval      LW_FALSE        a and b are not equal within delta.
 * @retval      LW_TRUE         a and b are equal within delta.
 */
#define CLK_ARE_EQUAL_WITHIN_DELTA__SIGNAL(a, b, deltaKHz)                             \
   (CLK_ARE_EQUAL_IGNORING_FREQ__SIGNAL(a, b)       &&  /* Same parameters          */ \
    LW_DELTA((a).freqKHz, (b).freqKHz) <= (deltaKHz))   /* Freq within tolerance    */


/*!
 * @memberof    ClkSignal
 * @brief       Indicates if 'signal' conforms to 'target' ignoring frequency and path.
 * @deprecated  For Clocks 3.0+, consider CLK_CONFORMS_IGNORING_PATH__TARGET_SIGNAL.
 *
 * @details     This operation is not symmetric.
 *
 * @note        inline:         No pointer to this function exists.
 *
 * @param[in]   signal          Signal to be checked
 * @param[in]   target          Target signal
 *
 * @retval      LW_FALSE        'signal' does not conform to 'target'.
 * @retval      LW_TRUE         'signal' conforms to 'target'.
 */
#define CLK_CONFORMS_IGNORING_PATH__SIGNAL(signal, target)                                     \
    ((signal).fracdiv   <= (target).fracdiv     &&       /* Fractional divide within limits */ \
    ((signal).regimeId  == (target).regimeId    ||       /* The regime IDs match            */ \
     (target).regimeId  == LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID)) /* or the target is "don't-care" */


/*!
 * @memberof    ClkSignal
 * @brief       Indicates if 'signal' conforms to 'target' ignoring frequency.
 * @deprecated  For Clocks 3.0+, consider CLK_CONFORMS__TARGET_SIGNAL.
 *
 * @details     This operation is not symmetric.
 *
 * @note        inline:         No pointer to this function exists.
 *
 * @param[in]   signal          Signal to be checked
 * @param[in]   target          Target signal (including path)
 *
 * @retval      LW_FALSE        'signal' does not conform to 'target'.
 * @retval      LW_TRUE         'signal' conforms to 'target'.
 */
#define CLK_CONFORMS__SIGNAL(signal, target)                                                           \
    (clkConforms_SignalPath((signal).path, (target).path)   &&  /* Path must conform                */ \
    CLK_CONFORMS_IGNORING_PATH__SIGNAL((signal), (target)))     /* Everything else except frequency */


/*!
 * @memberof    ClkSignal
 * @brief       Indicates if 'signal' conforms to 'target' and the frequency is within tolerance.
 * @deprecated  For Clocks 3.0+, consider CLK_CONFORMS_WITHIN_DELTA__TARGET_SIGNAL.
 *
 * @details     This operation is not symmetric.
 *
 * @note        inline:         No pointer to this function exists.
 *
 * @param[in]   signal          Signal to be checked
 * @param[in]   target          Target signal
 *
 * @retval      LW_FALSE        'signal' does not conform to 'target'.
 * @retval      LW_TRUE         'signal' conforms to 'target'.
 */
#define CLK_CONFORMS_WITHIN_DELTA__SIGNAL(signal, target, deltaKHz)                            \
    (CLK_CONFORMS__SIGNAL((signal), (target))  &&               /* Signal must conform      */ \
    LW_DELTA((signal).freqKHz, (target).freqKHz) <= (deltaKHz)) /* Freq within tolerance    */


/*******************************************************************************
 * ClkSignal Function
*******************************************************************************/

#if CLK_PRINT_ENABLED
void clkPrint_Signal(ClkSignal *this, const char *prefix)
    GCC_ATTRIB_SECTION("imem_libClkPrint", "clkPrint_Signal");
#endif


/*******************************************************************************
 * ClkTargetSignal struct
*******************************************************************************/

typedef RM_PMU_CLK_FREQ_TARGET_SIGNAL ClkTargetSignal;

/*!
 * @memberof    ClkTargetSignal
 * @brief       Used to reset/ilwalidate the signal
 */
extern const ClkTargetSignal clkReset_TargetSignal;


/*******************************************************************************
 * ClkTargetSignal Constants
*******************************************************************************/


/*!
 * @memberof    ClkTargetSignal
 * @brief       Unbounded Maximum Sentinel Value
 * @see         RM_PMU_CLK_FREQ_TARGET_SIGNAL
 */
#define CLK_RANGE_MAX_UNBOUNDED__TARGET_SIGNAL LW_U32_MAX


/*******************************************************************************
 * ClkTargetSignal Assignment Macros
*******************************************************************************/

/*!
 * @memberof    ClkTargetSignal
 * @brief       Assign target from output signal
 * @version     Clocks 3.0 and after
 *
 * @details     This macro assigns the members in 'target' based on 'signal'
 *              such that passing 'target' to 'clkConfig' results in 'signal'.
 *
 *              Most members are copied verbatim from 'signal'.  Additionally,
 *              'rangeKHz' is set to a single frequency, 'freqKHz'. 'bColdOverride'
 *              and 'finalFreqKHz' are assigned from 'context'.  Nothing remains
 *              unassigned.
 *
 *              Mainly, this is an optimization so that the frequency source
 *              object doesn't have to iterate over as many spelwlative paths,
 *              divider values, etc., when it is reconfigured.
 *
 * @todo        Consider applying a 1/2% tolerance delta to 'rangeKHz'.
 *
 * @note        macro:          Parameters may be evaluated more than once
 *
 * @param[out]  _target         Target signal to assign
 * @param[in]   _signal         Output signal to achieve
 * @param[in]   _context        Additional target parameters not included in 'signal'
 */
#define CLK_ASSIGN_FROM_SIGNAL__TARGET_SIGNAL(_target, _signal)             \
do                                                                          \
{                                                                           \
    (_target).super          = (_signal);                                   \
    (_target).rangeKHz.min   =                                              \
    (_target).rangeKHz.max   = (_target).super.freqKHz;                     \
} while (LW_FALSE)


/*!
 * @memberof    ClkTargetSignal
 * @brief       Assign target from output frequency
 * @version     Clocks 3.0 and after
 *
 * @details     This macro assigns the members in 'target' based on 'freqKHz'
 *              such that passing 'target' to 'clkConfig' results in 'freqKHz'.
 *
 *              Most members are copied verbatim from 'context'.  'freqKHz' is
 *              assigned to '_FREQKHZ'.  'rangeKHz' is assigned to 1/2% of
 *              'freqKHz'.  Nothing is left unassigned.
 *
 *              Mainly, this is an optimization so that the frequency source
 *              object doesn't have to iterate over as many spelwlative paths,
 *              divider values, etc., when it is reconfigured.
 *
 * @note        macro:          Parameters may be evaluated more than once
 *
 * @param[out]  _TARGET         Target signal to assign
 * @param[in]   _FREQKHZ        Output frequency to achieve
 * @param[in]   _CONTEXT        Additional target parameters not included in 'signal'
 */
#define CLK_ASSIGN_FROM_FREQ__TARGET_SIGNAL(_TARGET, _FREQKHZ, _CONTEXT)        \
do                                                                              \
{                                                                               \
    (_TARGET)                = (_CONTEXT);                                      \
    (_TARGET).super.freqKHz  = (_FREQKHZ);                                      \
    LW_ASSIGN_DELTA_RANGE((_TARGET).rangeKHz, (_FREQKHZ), (_FREQKHZ) / 200);    \
} while (LW_FALSE)


/*!
 * @memberof    ClkTargetSignal
 * @brief       Multiply the target frequencies
 * @version     Clocks 3.0 and after
 *
 * @details     This macro multiplies 'freqKHz', 'finalFreqKHz', and 'rangeKHz'
 *              by 'x'.  If additional frequencies are added to ClkTargetSignal
 *              (or ClkSignal), they must be added to this macro as well.
 *
 *              If the upper bound is CLK_RANGE_MAX_UNBOUNDED__TARGET_SIGNAL,
 *              which is 0xffffffff to indicate "unbounded", or if the
 *              multiplication would cause overflow, the upper bound is set to
 *              CLK_RANGE_MAX_UNBOUNDED__TARGET_SIGNAL.
 *
 *              No check is made for the overdlow of the other members.
 *
 * @note        macro:          Parameters may be evaluated more than once
 *
 * @param[in/out]   target      Target signal to change
 * @param[in]       x           Value to multiply
 */
#define CLK_MULTIPLY__TARGET_SIGNAL(target, x)      \
do                                                  \
{                                                   \
    (target).super.freqKHz    *= (x);                                           /* Multiply the target frequency */ \
    (target).rangeKHz.min     *= (x);                                           /* Multiply the lower bound      */ \
    if ((target).rangeKHz.max > CLK_RANGE_MAX_UNBOUNDED__TARGET_SIGNAL / (x))   /* Check for overflow            */ \
    {                                                                                                               \
        (target).rangeKHz.max = CLK_RANGE_MAX_UNBOUNDED__TARGET_SIGNAL;         /* Upwardly unbounded            */ \
    }                                                                                                               \
    else                                            \
    {                                               \
        (target).rangeKHz.max *= (x);                                           /* Multiply the upper bound      */ \
    }                                               \
} while (LW_FALSE)


/*!
 * @memberof    ClkTargetSignal
 * @brief       Divide the target frequencies
 * @version     Clocks 3.0 and after
 *
 * @details     This macro multiplies 'freqKHz', 'finalFreqKHz', and 'rangeKHz'
 *              by 'x'.  If additional frequencies are added to ClkTargetSignal
 *              (or ClkSignal), they must be added to this macro as well.
 *
 *              If the upper bound is CLK_RANGE_MAX_UNBOUNDED__TARGET_SIGNAL,
 *              which is 0xffffffff to indicate "unbounded", it is left unchanged.
 *
 * @note        macro:          Parameters may be evaluated more than once
 *
 * @param[in/out]   target      Target signal to change
 * @param[in]       x           Value to divide
 */
#define CLK_DIVIDE__TARGET_SIGNAL(target, x)        \
do                                                  \
{                                                   \
    (target).super.freqKHz    = ((target).super.freqKHz + (x) / 2) / (x);       /* Rounding divide the target frequency */ \
    (target).rangeKHz.min     = ((target).rangeKHz.min  + (x) / 2) / (x);       /* Rounding divide the lower bound      */ \
    if ((target).rangeKHz.max < CLK_RANGE_MAX_UNBOUNDED__TARGET_SIGNAL)         /* Unless there is no upper bound       */ \
    {                                                                                                                      \
        (target).rangeKHz.max = ((target).rangeKHz.max  + (x) / 2) / (x);       /* rounding divide it as well           */ \
    }                                                                                                                      \
} while (LW_FALSE)


/*******************************************************************************
 * ClkTargetSignal Test Macros
*******************************************************************************/

/*!
 * @memberof    ClkTargetSignal
 * @brief       Indicates equal target clock signals
 * @version     Clocks 3.0 and after
 *
 * @details     Signals are considered equal iff all data members are equal.
 *
 *              This operation is symmetric.
 *
 * @note        macro:          Parameters may be evaluated more than once
 *
 * @param[in]   a               Target signal to evaluate
 * @param[in]   b               Target signal to evaluate
 *
 * @retval      LW_FALSE        a and b are not equal.
 * @retval      LW_TRUE         a and b are equal.
 */
#define CLK_ARE_EQUAL__TARGET_SIGNAL(a, b)                                     \
    (CLK_ARE_EQUAL__SIGNAL((a).super, (b).super)    &&  /* Base class       */ \
    LW_EQUAL_RANGE((a).rangeKHz, (b).rangeKHz)      &&  /* Min/max          */

/*!
 * @memberof    ClkTargetSignal
 * @brief       Indicates if 'signal' conforms to 'target' ignoring path.
 * @version     Clocks 3.0 and after
 *
 * @details     This operation is not symmetric.
 *
 * @note        inline:         No pointer to this function exists.
 *
 * @param[in]   signal          ClkSignal to be checked
 * @param[in]   target          ClkTargetSignal for comparison
 *
 * @retval      LW_FALSE        'signal' does not conform to 'target'.
 * @retval      LW_TRUE         'signal' conforms to 'target'.
 */
#define CLK_CONFORMS_IGNORING_PATH__TARGET_SIGNAL(signal, target)                                      \
    (CLK_CONFORMS_IGNORING_PATH__SIGNAL((signal), (target).super)   &&  /* Super class conformity   */ \
    LW_WITHIN_INCLUSIVE_RANGE((target).rangeKHz, (signal).freqKHz))     /* Range conformity         */


/*!
 * @memberof    ClkTargetSignal
 * @brief       Indicates if 'signal' conforms to 'target'.
 * @version     Clocks 3.0 and after
 *
 * @details     This operation is not symmetric.
 *
 * @note        inline:         No pointer to this function exists.
 *
 * @param[in]   signal          ClkSignal to be checked
 * @param[in]   target          ClkTargetSignal for comparison
 *
 * @retval      LW_FALSE        'signal' does not conform to 'target'.
 * @retval      LW_TRUE         'signal' conforms to 'target'.
 */
#define CLK_CONFORMS__TARGET_SIGNAL(signal, target)                                                            \
    (clkConforms_SignalPath((signal).path, (target).super.path)     &&  /* Path must conform                */ \
    CLK_CONFORMS_IGNORING_PATH__TARGET_SIGNAL((signal), (target)))      /* Everything else except frequency */


/*!
 * @memberof    ClkTargetSignal
 * @brief       Indicates if 'signal' conforms to 'target' and the frequency is within tolerance.
 * @version     Clocks 3.0 and after
 *
 * @details     This operation is not symmetric.
 *
 * @note        inline:         No pointer to this function exists.
 *
 * @param[in]   signal          ClkSignal to be checked
 * @param[in]   target          ClkTargetSignal for comparison
 *
 * @retval      LW_FALSE        'signal' does not conform to 'target'.
 * @retval      LW_TRUE         'signal' conforms to 'target'.
 */
#define CLK_CONFORMS_WITHIN_DELTA__TARGET_SIGNAL(signal, target, deltaKHz)                            \
    (CLK_CONFORMS__TARGET_SIGNAL((signal), (target))                && /* Signal must conform      */ \
    LW_DELTA((signal).freqKHz, (target).super.freqKHz) <= (deltaKHz))  /* Freq within tolerance    */

#endif         // CLK3_SIGNAL_H

