/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_limit_35.c
 * @copydoc perf_limit_35.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"

//CRPTODO - Temporary alias until it can be merged into PMU/uproc infrastructure.
#define LWMISC_MEMSET(_s, _c, _n) memset((_s), (_c), (_n))

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"

#include "pmu_objclk.h"
#include "volt/objvolt.h"

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Helper macro to iterate over the ARBITRATION_OUTPUT_TUPLE_35 in an ARBITRATION_OUTPUT_35 structure.
 * Iterates the iterator variable @ref _iter from 0 up to
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS.
 *
 * @note Must be used in conjunction with @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR_END.
 *
 * @param[in]  _pArbOutput35
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_35 structure to iterate over.
 * @param[out] _iter
 *     Iterator variable.  Will iterate from 0 to
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS.
 * @param[out] _pTuple35
 *     PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 pointer variable to point to the
 *     tuple at the current @ref _iter index.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR(_pArbOutput35, _iter, _pTuple35) \
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(_iter)                                  \
    {                                                                                     \
        (_pTuple35) = &((_pArbOutput35)->tuples[(_iter)]);

/*!
 * Iteration end macro.  To be used to end @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR().
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR_END                              \
    }                                                                                     \
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END

/*!
 * Helper macro to init a @ref PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35
 * struct.
 *
 * @param[in] _pArbOutputTuple35
 *      Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 struct to init.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35_INIT(_pArbOutputTuple35)    \
    do {                                                                    \
        memset((_pArbOutputTuple35), 0x00,                                  \
            sizeof(PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35));               \
    } while (LW_FALSE)

/*!
 * Helper macro to export a @ref PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35
 * struct to a @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35.
 *
 * @param[in]  _pArbOutputTuple35
 *      Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 struct to export.
 * @param[out] _pArbOutputTuple35Rmctrl
 *      Pointer to LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35
 *      struct to populate.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_EXPORT(_pArbOutputTuple35, _pArbOutputTuple35Rmctrl) \
    do {                                                                                             \
        (_pArbOutputTuple35Rmctrl)->pstateIdx = (_pArbOutputTuple35)->pstateIdx;                     \
        memcpy(&((_pArbOutputTuple35Rmctrl)->clkDomains), &((_pArbOutputTuple35)->clkDomains),       \
            sizeof((_pArbOutputTuple35)->clkDomains));                                               \
        memcpy(&((_pArbOutputTuple35Rmctrl)->voltRails), &((_pArbOutputTuple35)->voltRails),         \
            sizeof((_pArbOutputTuple35)->voltRails));                                                \
    } while (LW_FALSE)

/*!
 * Helper macro to init at @ref PERF_LIMITS_ARBITRATION_OUTPUT_35 struct.
 *
 * @param[in] _pArbOutput35
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_35 struct to init.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_35_INIT(_pArbOutput35)                         \
    do {                                                                              \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 *_pTuple35;                           \
        LwU8 _i;                                                                      \
        PERF_LIMITS_ARBITRATION_OUTPUT_INIT(&((_pArbOutput35)->super));               \
        PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR((_pArbOutput35), _i,         \
            _pTuple35)                                                                \
        {                                                                             \
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35_INIT(_pTuple35);                  \
        }                                                                             \
        PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR_END;                         \
    } while (LW_FALSE)

/*!
 * Helper macro to export a @ref PERF_LIMITS_ARBITRATION_OUTPUT_35 struct to a
 * @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_35.
 *
 * @param[in]  _pArbOutput35
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_35 struct to export.
 * @param[out] _pArbOutput35Rmctrl
 *     Pointer to LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_35 populate.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_35_EXPORT(_pArbOutput35, _pArbOutput35Rmctrl)  \
    do {                                                                              \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 *_pTuple35;                           \
        LwU8 _i;                                                                      \
        PERF_LIMITS_ARBITRATION_OUTPUT_EXPORT(&((_pArbOutput35)->super),              \
            &((_pArbOutput35Rmctrl)->super));                                         \
        PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR((_pArbOutput35), _i,         \
            _pTuple35)                                                                \
        {                                                                             \
            PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_EXPORT(                           \
                _pTuple35, &((_pArbOutput35Rmctrl)->tuples[_i]));                     \
        }                                                                             \
        PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR_END;                         \
    } while (LW_FALSE)

/*!
 * Helper macro to export a @ref PERF_LIMIT_ARBITRATION_INPUT_35
 * struct to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 counterpart.
 *
 * @param[in]    _pArbInput35
 *     Pointer to @ref PERF_LIMIT_ARBITRATION_INPUT_35 to export.
 * @param[out]   _pArbInput35Rmctrl
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 to
 *     populate.
 */
#define PERF_LIMIT_ARBITRATION_INPUT_35_EXPORT(_pArbInput35, _pArbInput35Rmctrl) \
    do {                                                                         \
        LwU8 _i;                                                                 \
        (void)boardObjGrpMaskExport_E32(&((_pArbInput35)->clkDomainsMask),       \
            &((_pArbInput35Rmctrl)->clkDomainsMask));                            \
        (void)boardObjGrpMaskExport_E32(&((_pArbInput35)->voltRailsMask),        \
            &((_pArbInput35Rmctrl)->voltRailsMask));                             \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(_i)                        \
        {                                                                        \
            LwU8 _j;                                                             \
            (_pArbInput35Rmctrl)->tuples[_i].pstateIdx =                         \
                (_pArbInput35)->tuples[_i].pstateIdx;                            \
            for (_j = 0; _j < PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS; _j++) \
            {                                                                    \
                (_pArbInput35Rmctrl)->tuples[_i].clkDomains[_j] =                \
                    (_pArbInput35)->tuples[_i].clkDomains[_j];                   \
            }                                                                    \
            for (_j = 0; _j < PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS; _j++)   \
            {                                                                    \
                (_pArbInput35Rmctrl)->tuples[_i].voltRails[_j] =                 \
                    (_pArbInput35)->tuples[_i].voltRails[_j];                    \
            }                                                                    \
        }                                                                        \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;                       \
        for (_i = 0; _i < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; _i++)     \
        {                                                                        \
            (_pArbInput35Rmctrl)->voltageMaxLooseuV[_i] =                        \
                (_pArbInput35)->voltageMaxLooseuV[_i];                           \
        }                                                                        \
    } while (LW_FALSE)

/*!
 * Helper macro to retreive the corresponding field within a @ref
 * PERF_LIMIT_ARBITRATION_INPUT_35 struct's tuples[] array as a @ref
 * PERF_LIMIT_<XYZ>_RANGE structure.  This is a general macro which is used by
 * type-specific macros below, which will specify the @ref _field parameter.
 *
 * @param[in]     _pArbInput35
 *     Pointer to @ref PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[out]    _pRange
 *     Pointer to @ref PERF_LIMITS_<XYZ>_RANGE in which to min and max @ref
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::_field.
 * @param[in]     _field
 *     Field name within @ref
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE to retrieve into
 *     @ref _pRange.  This will be appended to the references to the @ref
 *     PERF_LIMIT_ARBITRATION_INPUT_35::values[].  An example value would be
 *     something like "pstateIdx" which would lead to something like
 *     "(_pArbInput35)->tuples[_i].pstateIdx".
 */
#define PERF_LIMIT_35_ARB_INPUT_GET_RANGE(_pArbInput35, _pRange, _field)       \
    do {                                                                       \
        LwU8 _i;                                                               \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(_i)                      \
        {                                                                      \
            (_pRange)->values[_i] = (_pArbInput35)->tuples[_i]._field;         \
        }                                                                      \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;                     \
    } while (LW_FALSE)

/*!
 * Helper macro to set the corresponding field within a @ref
 * PERF_LIMIT_ARBITRATION_INPUT_35 struct's tuples[] array from a @ref
 * PERF_LIMIT_<XYZ>_RANGE structure.  This is a general macro which is used by
 * type-specific macros below, which will specify the @ref _field parameter.
 *
 * @param[out]    _pArbInput35
 *     Pointer to @ref PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[in]     _pRange
 *     Pointer to @ref PERF_LIMITS_<XYZ>_RANGE from which which to populate the
 *     min and max @ref
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::_field.
 * @param[in]     _field
 *     Field name within @ref
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE to from @ref
 *     _pRange.  This will be appended to the references to the @ref
 *     PERF_LIMIT_ARBITRATION_INPUT_35::values[].  An example value would be
 *     something like "pstateIdx" which would lead to something like
 *     "(_pArbInput35)->tuples[_i].pstateIdx".
 */
#define PERF_LIMIT_35_ARB_INPUT_SET_RANGE(_pArbInput35, _pRange, _field)       \
    do {                                                                       \
        LwU8 _i;                                                               \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(_i)                      \
        {                                                                      \
            (_pArbInput35)->tuples[_i]._field = (_pRange)->values[_i];         \
        }                                                                      \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;                     \
    } while (LW_FALSE)

/*!
 * Helper macro to set the corresponding field within a @ref
 * PERF_LIMIT_ARBITRATION_INPUT_35 struct's tuples[] array from a single value
 * (i.e. min == max).  This is a general macro which is used by type-specific
 * macros below, which will specify the @ref _field parameter.
 *
 * @param[pit]    _pArbInput35
 *     Pointer to @ref PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[in]     _value
 *     Value with which which to populate the min and max @ref
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::_field.
 * @param[in]     _field
 *     Field name within @ref
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE to from @ref
 *     _pRange.  This will be appended to the references to the @ref
 *     PERF_LIMIT_ARBITRATION_INPUT_35::values[].  An example value would be
 *     something like "pstateIdx" which would lead to something like
 *     "(_pArbInput35)->tuples[_i].pstateIdx".
 */
#define PERF_LIMIT_35_ARB_INPUT_SET_VALUE(_pArbInput35, _value, _field)        \
    do {                                                                       \
        LwU8 _i;                                                               \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(_i)                      \
        {                                                                      \
            (_pArbInput35)->tuples[_i]._field = _value;                        \
        }                                                                      \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;                     \
    } while (LW_FALSE)

/*!
 * Helper macro to retrieve the PSTATE indexes from a @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 structures min and max
 * _TUPLEs as a @ref PERF_LIMITS_PSTATE_RANGE.
 *
 * Wrapper to @ref PERF_LIMIT_35_ARB_INPUT_GET_RANGE().
 *
 * @param[in]     _pArbInput35
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[out]    _pPstateRange
 *     Pointer to @ref PERF_LIMITS_PSTATE_RANGE in which to return the min and
 *     max _TUPLE pstateIdxs.
 */
#define PERF_LIMIT_35_ARB_INPUT_PSTATE_GET_RANGE(_pArbInput35, _pPstateRange)   \
    PERF_LIMIT_35_ARB_INPUT_GET_RANGE(_pArbInput35, _pPstateRange, pstateIdx)

/*!
 * Helper macro to set the PSTATE indexes in a @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 structures min and max
 * _TUPLEs from a @ref PERF_LIMITS_PSTATE_RANGE.
 *
 * Wrapper to @ref PERF_LIMIT_35_ARB_INPUT_SET_RANGE().
 *
 * @param[out]     _pArbInput35
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[in]     _pPstateRange
 *     Pointer to @ref PERF_LIMITS_PSTATE_RANGE from which to populate the min
 *     and max _TUPLE pstateIdxs.
 */
#define PERF_LIMIT_35_ARB_INPUT_PSTATE_SET_RANGE(_pArbInput35, _pPstateRange)   \
    PERF_LIMIT_35_ARB_INPUT_SET_RANGE(_pArbInput35, _pPstateRange, pstateIdx)

/*!
 * Helper macro to set the PSTATE indexes in a @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 structures min and max
 * _TUPLEs from a single value.
 *
 * Wrapper to @ref PERF_LIMIT_35_ARB_INPUT_SET_VALUE().
 *
 * @param[out]     _pArbInput35
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[in]     _value
 *     Value with which to populate the min and max _TUPLE pstateIdxs.
 */
#define PERF_LIMIT_35_ARB_INPUT_PSTATE_SET_VALUE(_pArbInput35, _value)          \
    PERF_LIMIT_35_ARB_INPUT_SET_VALUE(_pArbInput35, _value, pstateIdx)

/*!
 * Helper macro to retrieve an PERF_LIMITS_ARBITRATION_OUTPUT_35 MIN and MAX
 * _TUPLE values of a given @ref _field into a provided @ref
 * PERF_LIMITS_<XYZ>_RANGE structure.
 *
 * Providing the values in a PERF_LIMITS_<XYZ>_RANGE structure allows using the
 * various @ref PERF_LIMITS_RANGE_<XYZ>() macros to sanity checks on the values
 * or to use the values with values[] array-based accessors.
 *
 * @param[in]      _pArbOutput35
 *     Pointer to @ref PERF_LIMITS_ARBITRATION_OUTPUT_35 structure.
 * @param[in]      _field
 *     Name of the of @ref
 *     PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 structure field to retrieve.
 *     Will be concatenated to the @ref _pOutputTuple35 local variable within
 *     the macro.
 */
#define PERF_LIMITS_35_ARB_OUTPUT_TUPLES_GET_RANGE(_pArbOutput35, _field, _pRange)    \
    do {                                                                              \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 *_pOutputTuple35;                     \
        LwU8 _i;                                                                      \
        PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR((_pArbOutput35), _i,         \
            _pOutputTuple35)                                                          \
        {                                                                             \
            (_pRange)->values[_i] = _pOutputTuple35->_field.value;                    \
        }                                                                             \
        PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR_END;                         \
    } while (LW_FALSE)

/*!
 * Helper macro to apply a given input @ref _value to a @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_35 structure's MIN and MAX TUPLEs' @ref _field
 * as a MIN comparison only (i.e. only set if @ref _value is < the @ref _field).
 *
 * This macro is a wrapper to @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MIN(), automating
 * the process of calling it for both TUPLEs in the ARBITRATION_OUTPUT.
 *
 * @param[in/out]  _pArbOutput35
 *     Pointer to @ref PERF_LIMITS_ARBITRATION_OUTPUT_35 structure to modify.
 * @param[in]      _field
 *     Name of the of @ref
 *     PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 structure field to retrieve.
 *     Will be concatenated to the @ref _pOutputTuple35 local variable within
 *     the macro.
 * @param[in]      _value
 *     Value to apply to the PERF_LIMITS_ARBITRATION_OUTPUT_35.  Will be passed
 *     directly to @ref
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MIN().
 * @param[in]      _limitId
 *     @ref LW2080_CTRL_PERF_PERF_LIMIT_ID of the PERF_LMIT which is setting
 *     this _value. Will be passed directly to @ref
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MIN().
 */
#define PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MIN(_pArbOutput35, _field, _value, _limitId) \
    do {                                                                             \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 *_pOutputTuple35;                    \
        LwU8 _i;                                                                     \
        PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR((_pArbOutput35), _i,        \
            _pOutputTuple35)                                                         \
        {                                                                            \
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MIN(         \
                &(_pOutputTuple35->_field), (_value), (_limitId));                   \
        }                                                                            \
        PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR_END;                        \
    } while (LW_FALSE)

/*!
 * Helper macro to apply a given input @ref _value to a @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_35 structure's MIN and MAX TUPLEs' @ref _field
 * as a MAX comparison only (i.e. only set if @ref _value is > the @ref _field).
 *
 * This macro is a wrapper to @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MAX(), automating
 * the process of calling it for both TUPLEs in the ARBITRATION_OUTPUT.
 *
 * @param[in/out]  _pArbOutput35
 *     Pointer to @ref PERF_LIMITS_ARBITRATION_OUTPUT_35 structure to modify.
 * @param[in]      _field
 *     Name of the of @ref
 *     PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 structure field to retrieve.
 *     Will be concatenated to the @ref _pOutputTuple35 local variable within
 *     the macro.
 * @param[in]      _value
 *     Value to apply to the PERF_LIMITS_ARBITRATION_OUTPUT_35.  Will be passed
 *     directly to @ref
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MAX().
 * @param[in]      _limitId
 *     @ref LW2080_CTRL_PERF_PERF_LIMIT_ID of the PERF_LMIT which is setting
 *     this _value. Will be passed directly to @ref
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MAX().
 */
#define PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MAX(_pArbOutput35, _field, _value, _limitId) \
    do {                                                                             \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 *_pOutputTuple35;                    \
        LwU8 _i;                                                                     \
        PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR((_pArbOutput35), _i,        \
            _pOutputTuple35)                                                         \
        {                                                                            \
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MAX(         \
                &(_pOutputTuple35->_field), (_value), (_limitId));                   \
        }                                                                            \
        PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR_END;                        \
    } while (LW_FALSE)

/*!
 * @defgroup PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM
 *
 * Set of enums representing the field within @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
 * to access or mutate via various helper functions.
 *
 * @ref PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_KHZ
 *     @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35::super.freqkHz
 * @ref PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_NOISE_UNAWARE_KHZ
 *     @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35::freqNoiseUnawareMinkHz
 *
 * @{
 */
typedef LwU8 PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM;
#define PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_KHZ                0x0
#define PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_NOISE_UNAWARE_KHZ  0x1
/*!@}*/

/*!
 * @defgroup PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM
 *
 * Set of enums representing the field within @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35
 * to access or mutate via various helper functions.
 *
 * @ref PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM_VOLTAGE_UV
 *     @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35::super.voltageuV
 * @ref PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM_VOLTAGE_MIN_LOOSE_UV
 *     @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35::voltageMinLooseuV
 *
 * @{
 */
typedef LwU8 PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM;
#define PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM_VOLTAGE_UV            0x0
#define PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM_VOLTAGE_MIN_LOOSE_UV  0x1
/*!@}*/

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @interface PERF_LIMIT_35
 *
 * Virtual interface for @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE-specific @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT -> @ref
 * PERF_LIMIT_ARBITRATION_INPUT_35 colwersion functions.
 * Individual implementations of these are expected to be called from @ref
 * s_perfLimit35ClientInputToArbInput() in a switch based on the _TYPE.
 *
 * @param[in]       pLimits35
 *     @ref PERF_LIMITS_35 pointer.
 * @param[in]       pLimit35
 *     @ref PERF_LIMIT_35 pointer.
 * @param[in]       pArbOutput35
 *     @ref PERF_LIMITS_ARBITRATION_OUTPUT_35 structure.  This is input only,
 *     providing various CLK_DOMAIN and VOLT_RAIL masks for the arbitration
 *     algorithm.  It will not be modified by this function.
 * @param[out]     pArbInput35
 *     @ref PERF_LIMIT_ARBITRATION_INPUT_35 structure which will be populated
 *     based on TYPE-specific data.
 * @param[in]      pClientInput
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT structure which
 *     contains type-specific information.
 *
 * @return FLCN_OK
 *     TYPE-specific colwersion completed successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Invalid TYPE-specific data in @ref pClientInput.
 * @return Other errors
 *     Unexpected errors.
 */
#define PerfLimit35ClientInputTypeToArbInput(fname) FLCN_STATUS (fname)(PERF_LIMITS_35 *pLimits35, PERF_LIMIT_35 *pLimit35, PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35, PERF_LIMIT_ARBITRATION_INPUT_35 *pArbInput35, LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput)

/* ------------------------ Static Inline Functinos --------------------------*/
/*!
 * Helper function to initialize a @ref PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE.
 *
 * @param[out]   pArbInput35Tuple
 *     Pointer to @ref PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE to initialize.
 * @param[in]    value
 *     Value to which all elements of the tuple should be initialized.  Expected
 *     to be either @ref LW_U32_MIN or @ref LW_U32_MAX.
 */
static inline void
GCC_ATTRIB_ALWAYSINLINE()
perfLimitArbitrationInput35TupleInit
(
    PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pArbInput35Tuple,
    LwU32                                  value
)
{
    LwU8 i;

    pArbInput35Tuple->pstateIdx = value;
    for (i = 0; i < PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS; i++)
    {
        pArbInput35Tuple->clkDomains[i] = value;
    }
    for (i = 0; i < PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS; i++)
    {
        pArbInput35Tuple->voltRails[i] = value;
    }
}

/*!
 * Helper function to initialize a @ref PERF_LIMIT_ARBITRATION_INPUT_35
 * struct.
 *
 * @param[out]   pArbInput35
 *     Pointer to PERF_LIMIT_ARBITRATION_INPUT_35 to init.
 */
static inline void
GCC_ATTRIB_ALWAYSINLINE()
perfLimitArbitrationInput35Init
(
    PERF_LIMIT_ARBITRATION_INPUT_35 *pArbInput35
)
{
    LwU8 i;

    boardObjGrpMaskInit_E32(&(pArbInput35->clkDomainsMask));
    boardObjGrpMaskInit_E32(&(pArbInput35->voltRailsMask));

    perfLimitArbitrationInput35TupleInit(
        &(pArbInput35->tuples[
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN]),
        LW_U32_MIN);
    perfLimitArbitrationInput35TupleInit(
        &(pArbInput35->tuples[
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX]),
        LW_U32_MAX);

    for (i = 0; i < LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS; i++)
    {
        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_SET(
            &(pArbInput35->voltageMaxLooseuV[i]),
            LW_U32_MAX, LW2080_CTRL_PERF_PERF_LIMIT_ID_NOT_SET);
    }
}

/*!
 * Helper function to retrieve
 * PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::clkDomains[] value, indexed
 * by CLK_DOMAIN.  Provides sanity checking for CLK_DOMAIN index,
 * assuring no buffer overrun.
 *
 * @param[in]      pcArbInput35Tuple
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE pointer.
 * @param[in/out]  pVfDomain
 *     Pointer to @ref PERF_LIMITS_VF.  @ref PERF_LIMITS_VF::idx
 *     specifies the CLK_DOMAIN index as input.  @ref
 *     PERF_LIMITS_VF::value returns the value as output.
 *
 * @return FLCN_OK
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::clkDomains[] return successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_GET_VALUE
(
    const PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pcArbInput35Tuple,
    PERF_LIMITS_VF                              *pVfDomain
)
{
    if ((pcArbInput35Tuple == NULL) ||
        (pVfDomain == NULL) ||
        (pVfDomain->idx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pVfDomain->value = pcArbInput35Tuple->clkDomains[pVfDomain->idx];
    return FLCN_OK;
}

/*!
 * Helper function to set
 * PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::clkDomains[] value, indexed
 * by CLK_DOMAIN.  Provides sanity checking for CLK_DOMAIN index,
 * assuring no buffer overrun.
 *
 * @param[in]      pArbInput35Tuple
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE pointer.
 * @param[in/out]  pVfDomain
 *     Pointer to @ref PERF_LIMITS_VF.  @ref PERF_LIMITS_VF::idx
 *     specifies the CLK_DOMAIN index as input.  @ref
 *     PERF_LIMITS_VF::value returns the value as output.
 *
 * @return FLCN_OK
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::clkDomains[] return successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_SET_VALUE
(
    PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pArbInput35Tuple,
    PERF_LIMITS_VF                        *pVfDomain
)
{
    if ((pArbInput35Tuple == NULL) ||
        (pVfDomain == NULL) ||
        (pVfDomain->idx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pArbInput35Tuple->clkDomains[pVfDomain->idx] = pVfDomain->value;
    return FLCN_OK;
}

/*!
 * Helper function to apply a range bounding to a
 * PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::clkDomains[] value, indexed
 * by CLK_DOMAIN.  Provides sanity checking for CLK_DOMAIN index,
 * assuring no buffer overrun.
 *
 * @param[in]      pArbInput35Tuple
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE pointer.
 * @param[in]      pVfDomainRange
 *     Pointer to @ref PERF_LIMITS_VF_RANGE.  @ref PERF_LIMITS_VF_RANGE::idx
 *     specifies the CLK_DOMAIN index as input.  @ref
 *     PERF_LIMITS_VF_RANGE::values specifies the range of values to bound.
 *
 * @return FLCN_OK
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::clkDomains[] bounded successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_BOUND_VALUE
(
    PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pArbInput35Tuple,
    PERF_LIMITS_VF_RANGE                  *pVfDomainRange
)
{
    PERF_LIMITS_VF vfDomain;
    FLCN_STATUS    status;

    if ((pArbInput35Tuple == NULL) ||
        (pVfDomainRange == NULL) ||
        (pVfDomainRange->idx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto  PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_BOUND_VALUE_exit;
    }

    vfDomain.idx = pVfDomainRange->idx;

    status = PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_GET_VALUE(pArbInput35Tuple, &vfDomain);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto  PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_BOUND_VALUE_exit;
    }

    PERF_LIMITS_RANGE_VALUE_BOUND(pVfDomainRange, &vfDomain.value);

    status = PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_SET_VALUE(pArbInput35Tuple, &vfDomain);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto  PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_BOUND_VALUE_exit;
    }

 PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_BOUND_VALUE_exit:
    return status;
}

/*!
 * Helper function to retrieve the CLK_DOMAIN frequency (kHz) values from a @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 structures min and max
 * _TUPLEs as a @ref PERF_LIMITS_VF_RANGE.
 *
 * Wrapper to @ref PERF_LIMIT_35_ARB_INPUT_GET_RANGE().
 *
 * @param[in]      pArbInput35
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[in/out]  pVfDomainRange
 *     Pointer to @ref PERF_LIMITS_VF_RANGE.  @ref PERF_LIMITS_VF_RANGE::idx
 *     specifies the CLK_DOMAIN index as input.  This macro will return
 *     frequency values in @ref PERF_LIMITS_VF_RANGE::values.
 *
 * @return FLCN_OK
 *     CLK_DOMAIN frequency (kHz) values retrieved successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_CLK_DOMAIN_GET_RANGE
(
    const PERF_LIMIT_ARBITRATION_INPUT_35 *pcArbInput35,
    PERF_LIMITS_VF_RANGE                  *pVfDomainRange
)
{
    if ((pcArbInput35 == NULL) ||
        (pVfDomainRange == NULL) ||
        (pVfDomainRange->idx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    PERF_LIMIT_35_ARB_INPUT_GET_RANGE(pcArbInput35, pVfDomainRange,
        clkDomains[pVfDomainRange->idx]);

    return FLCN_OK;
}

/*!
 * Helper function to set the CLK_DOMAIN frequency (kHz) values in a @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 structures min and max
 * _TUPLEs from a @ref PERF_LIMITS_VF_RANGE.
 *
 * Wrapper to @ref PERF_LIMIT_35_ARB_INPUT_SET_RANGE().
 *
 * @param[out]     pArbInput35
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[in]      pVfDomainRange
 *     Pointer to @ref PERF_LIMITS_VF_RANGE.  @ref PERF_LIMITS_VF_RANGE::idx
 *     specifies the CLK_DOMAIN index as input.  @ref
 *     PERF_LIMITS_VF_RANGE::values specifies the range of values to set.
 *
 * @return FLCN_OK
 *     CLK_DOMAIN frequency (kHz) values set successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_CLK_DOMAIN_SET_RANGE
(
    PERF_LIMIT_ARBITRATION_INPUT_35 *pArbInput35,
    PERF_LIMITS_VF_RANGE            *pVfDomainRange
)
{
    if ((pArbInput35 == NULL) ||
        (pVfDomainRange == NULL) ||
        (pVfDomainRange->idx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    PERF_LIMIT_35_ARB_INPUT_SET_RANGE(pArbInput35, pVfDomainRange,
        clkDomains[pVfDomainRange->idx]);
    boardObjGrpMaskBitSet(&(pArbInput35->clkDomainsMask),
        pVfDomainRange->idx);

    return FLCN_OK;
}

/*!
 * Helper function to set the CLK_DOMAIN frequency (kHz) values in a @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 structures min and max
 * _TUPLEs from a single value.
 *
 * Wrapper to @ref PERF_LIMIT_35_ARB_INPUT_SET_VALUE().
 *
 * @param[out]     pArbInput35
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[in]      pVfDomain
 *     Pointer to @ref PERF_LIMITS_VF.  @ref PERF_LIMITS_VF::idx
 *     specifies the CLK_DOMAIN index as input.  @ref
 *     PERF_LIMITS_VF::value specifies the value to set.
 *
 * @return FLCN_OK
 *     CLK_DOMAIN frequency (kHz) values set successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_CLK_DOMAIN_SET_VALUE
(
    PERF_LIMIT_ARBITRATION_INPUT_35 *pArbInput35,
    PERF_LIMITS_VF                  *pVfDomain
)
{
    if ((pArbInput35 == NULL) ||
        (pVfDomain == NULL) ||
        (pVfDomain->idx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    PERF_LIMIT_35_ARB_INPUT_SET_VALUE(pArbInput35, pVfDomain->value,
        clkDomains[pVfDomain->idx]);
    boardObjGrpMaskBitSet(&(pArbInput35->clkDomainsMask),
        pVfDomain->idx);

    return FLCN_OK;
}

/*!
 * Helper function to retrieve
 * PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::voltRails[] value, indexed
 * by VOLT_RAIL.  Provides sanity checking for VOLT_RAIL index,
 * assuring no buffer overrun.
 *
 * @param[in]      pcArbInput35Tuple
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE pointer.
 * @param[in/out]  pVfDomain
 *     Pointer to @ref PERF_LIMITS_VF.  @ref PERF_LIMITS_VF::idx
 *     specifies the VOLT_RAIL index as input.  @ref
 *     PERF_LIMITS_VF::value returns the value as output.
 *
 * @return FLCN_OK
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::voltRails[] return successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_TUPLE_VOLT_RAIL_GET_VALUE
(
    const PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pcArbInput35Tuple,
    PERF_LIMITS_VF                              *pVfRail
)
{
    if ((pcArbInput35Tuple == NULL) ||
        (pVfRail == NULL) ||
        (pVfRail->idx >= PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pVfRail->value = pcArbInput35Tuple->voltRails[pVfRail->idx];
    return FLCN_OK;
}

/*!
 * Helper function to set
 * PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::voltRails[] value, indexed
 * by VOLT_RAIL.  Provides sanity checking for VOLT_RAIL index,
 * assuring no buffer overrun.
 *
 * @param[in]      pArbInput35Tuple
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE pointer.
 * @param[in/out]  pVfDomain
 *     Pointer to @ref PERF_LIMITS_VF.  @ref PERF_LIMITS_VF::idx
 *     specifies the VOLT_RAIL index as input.  @ref
 *     PERF_LIMITS_VF::value returns the value as output.
 *
 * @return FLCN_OK
 *     PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE::voltRails[] set successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_TUPLE_VOLT_RAIL_SET_VALUE
(
    PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pArbInput35Tuple,
    PERF_LIMITS_VF                        *pVfRail
)
{
    if ((pArbInput35Tuple == NULL) ||
        (pVfRail == NULL) ||
        (pVfRail->idx >= PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pArbInput35Tuple->voltRails[pVfRail->idx] = pVfRail->value;
    return FLCN_OK;
}

/*!
 * Helper function to retrieve the VOLT_RAIL voltage (uV) values from a @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 structures min and max
 * _TUPLEs as a @ref PERF_LIMITS_VF_RANGE.
 *
 * Wrapper to @ref PERF_LIMIT_35_ARB_INPUT_GET_RANGE().
 *
 * @param[in]      pArbInput35
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[in/out]  pVfRailRange
 *     Pointer to @ref PERF_LIMITS_VF_RANGE.  @ref PERF_LIMITS_VF_RANGE::idx
 *     specifies the VOLT_RAIL index as input.  This macro will return
 *     voltage values in @ref PERF_LIMITS_VF_RANGE::values.
 *
 * @return FLCN_OK
 *     VOLT_RAIL voltage (uV) values retrieved successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_VOLT_RAIL_GET_RANGE
(
    const PERF_LIMIT_ARBITRATION_INPUT_35 *pcArbInput35,
    PERF_LIMITS_VF_RANGE                  *pVfRailRange
)
{
    if ((pcArbInput35 == NULL) ||
        (pVfRailRange == NULL) ||
        (pVfRailRange->idx >= PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    PERF_LIMIT_35_ARB_INPUT_GET_RANGE(pcArbInput35, pVfRailRange,
        voltRails[pVfRailRange->idx]);

    return FLCN_OK;
}

/*!
 * Helper function to set the VOLT_RAIL voltage (uV) values in a @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 structures min and max
 * _TUPLEs from a @ref PERF_LIMITS_VF_RANGE.
 *
 * Wrapper to @ref PERF_LIMIT_35_ARB_INPUT_SET_RANGE().
 *
 * @param[out]     pArbInput35
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[in]      pVfRailRange
 *     Pointer to @ref PERF_LIMITS_VF_RANGE.  @ref PERF_LIMITS_VF_RANGE::idx
 *     specifies the VOLT_RAIL index as input.  @ref
 *     PERF_LIMITS_VF_RANGE::values specifies the range of values to set.
 *
 * @return FLCN_OK
 *     VOLT_RAIL voltage (uV) values set successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_VOLT_RAIL_SET_RANGE
(
    PERF_LIMIT_ARBITRATION_INPUT_35 *pArbInput35,
    PERF_LIMITS_VF_RANGE            *pVfRailRange
)
{
    if ((pArbInput35 == NULL) ||
        (pVfRailRange == NULL) ||
        (pVfRailRange->idx >= PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    PERF_LIMIT_35_ARB_INPUT_SET_RANGE(pArbInput35, pVfRailRange,
        voltRails[pVfRailRange->idx]);
    boardObjGrpMaskBitSet(&(pArbInput35->voltRailsMask),
        pVfRailRange->idx);

    return FLCN_OK;
}

/*!
 * Helper function to set the VOLT_RAIL voltage (uV) values in a @ref
 * LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35 structures min and max
 * _TUPLEs from a single value.
 *
 * Wrapper to @ref PERF_LIMIT_35_ARB_INPUT_SET_VALUE().
 *
 * @param[out]     pArbInput35
 *     Pointer to @ref LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35.
 * @param[in]      pVfRail
 *     Pointer to @ref PERF_LIMITS_VF.  @ref PERF_LIMITS_VF::idx
 *     specifies the CLK_DOMAIN index as input.  @ref
 *     PERF_LIMITS_VF::value specifies the value to set.
 *
 * @return FLCN_OK
 *     VOLT_RAIL voltage (uV) values set successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMIT_35_ARB_INPUT_VOLT_RAIL_SET_VALUE
(
    PERF_LIMIT_ARBITRATION_INPUT_35 *pArbInput35,
    PERF_LIMITS_VF                  *pVfRail
)
{
    if ((pArbInput35 == NULL) ||
        (pVfRail == NULL) ||
        (pVfRail->idx >= PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    PERF_LIMIT_35_ARB_INPUT_SET_VALUE(pArbInput35, pVfRail->value,
        voltRails[pVfRail->idx]);
    boardObjGrpMaskBitSet(&(pArbInput35->voltRailsMask),
        pVfRail->idx);

    return FLCN_OK;
}

/*!
 * Helper function to bound a variable within a set of @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
 * structures within a @ref PERF_LIMITS_ARBITRATION_OUTPUT_35::tuples[]
 * array.  The variable to bound is specified the @ref
 * PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM enum.
 *
 * Wrapper to @ref PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MIN(), @ref
 * PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MAX().  Provides sanity checking
 * of array indexes.
 *
 * @param[out]     pArbOutput35
 *     Pointer to @PERF_LIMITS_ARBITRATION_OUTPUT_35.
 * @param[in]      pVfRail
 *     Pointer to @ref PERF_LIMITS_VF.  @ref PERF_LIMITS_VF::idx
 *     specifies the CLK_DOMAIN index as input.  @ref
 *     PERF_LIMITS_VF::value specifies the value to user for bounding.
 * @param[in]      clkDomainEnum
 *     @ref PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM specifying
 *     the variable within @ref
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
 *     to bound.
 * @param[in] bMin
 *     Boolean specifying MIN (@ref LW_TRUE) vs. MAX (@ref LW_FALSE) direction
 *     of bounding.
 * @param[in]      pLimit35
 *     PERF_LIMIT_35 pointer to the limit corresponding to this request.
 *
 * @return FLCN_OK
 *     CLK_DOMAIN tuples' variable successfully bound.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not bound the value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_BOUND
(
   PERF_LIMITS_ARBITRATION_OUTPUT_35              *pArbOutput35,
   PERF_LIMITS_VF                                 *pVfDomain,
   PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM clkDomEnum,
   LwBool                                          bMin,
   PERF_LIMIT_35                                  *pLimit35
)
{
    FLCN_STATUS status;

    if ((pArbOutput35 == NULL) ||
        (pVfDomain == NULL) ||
        (pVfDomain->idx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_BOUND_exit;
    }

    status = FLCN_ERR_ILWALID_ARGUMENT;
    switch (clkDomEnum)
    {
        case PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_KHZ:
        {
            if (bMin)
            {
               PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MIN(pArbOutput35,
                   clkDomains[pVfDomain->idx].super.freqkHz, pVfDomain->value,
                   BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));
            }
            else
            {
               PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MAX(pArbOutput35,
                   clkDomains[pVfDomain->idx].super.freqkHz, pVfDomain->value,
                   BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));
            }
            status = FLCN_OK;
            break;
        }
        case PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_NOISE_UNAWARE_KHZ:
        {
            if (bMin)
            {
               PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MIN(pArbOutput35,
                   clkDomains[pVfDomain->idx].freqNoiseUnawareMinkHz, pVfDomain->value,
                   BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));
            }
            else
            {
               PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MAX(pArbOutput35,
                   clkDomains[pVfDomain->idx].freqNoiseUnawareMinkHz, pVfDomain->value,
                   BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));
            }
            status = FLCN_OK;
            break;
        }
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_BOUND_exit;
    }

PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_BOUND_exit:
    return status;
}

/*!
 * Helper function to return variable values within a set of @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
 * structures within a @ref PERF_LIMITS_ARBITRATION_OUTPUT_35::tuples[]
 * array.  The variable to bound is specified the @ref
 * PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM enum.
 *
 * Wrapper to @ref PERF_LIMITS_35_ARB_OUTPUT_TUPLES_GET_RANGE().
 * Provides sanity checking of array indexes.
 *
 * @param[out]     pArbOutput35
 *     Pointer to @PERF_LIMITS_ARBITRATION_OUTPUT_35.
 * @param[in/out]  pVfRail
 *     Pointer to @ref PERF_LIMITS_VF_RANGE.  @ref
 *     PERF_LIMITS_VF_RANGE::idx specifies the CLK_DOMAIN index as
 *     input.  The variable values will be returned in @ref
 *     PERF_LIMITS_VF::values[].
 * @param[in]      clkDomainEnum
 *     @ref PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM specifying
 *     the variable within @ref
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
 *     to bound.
 *
 * @return FLCN_OK
 *     CLK_DOMAIN tuples' variable successfully returned..
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not bound the value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_GET_RANGE
(
   PERF_LIMITS_ARBITRATION_OUTPUT_35              *pArbOutput35,
   PERF_LIMITS_VF_RANGE                           *pVfDomainRange,
   PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM clkDomEnum
)
{
    FLCN_STATUS status;

    if ((pArbOutput35 == NULL) ||
        (pVfDomainRange == NULL) ||
        (pVfDomainRange->idx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_GET_RANGE_exit;
    }

    status = FLCN_ERR_ILWALID_ARGUMENT;
    switch (clkDomEnum)
    {
        case PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_KHZ:
        {
            PERF_LIMITS_35_ARB_OUTPUT_TUPLES_GET_RANGE(pArbOutput35,
               clkDomains[pVfDomainRange->idx].super.freqkHz, pVfDomainRange);
            status = FLCN_OK;
            break;
        }
        case PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_NOISE_UNAWARE_KHZ:
        {
            PERF_LIMITS_35_ARB_OUTPUT_TUPLES_GET_RANGE(pArbOutput35,
               clkDomains[pVfDomainRange->idx].freqNoiseUnawareMinkHz, pVfDomainRange);
            status = FLCN_OK;
            break;
        }
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_GET_RANGE_exit;
    }

PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_GET_RANGE_exit:
    return status;
}

/*!
 * Helper function to bound a variable within a set of @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35
 * structures within a @ref PERF_LIMITS_ARBITRATION_OUTPUT_35::tuples[]
 * array.  The variable to bound is specified the @ref
 * PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM enum.
 *
 * Wrapper to @ref PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MIN(), @ref
 * PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MAX().  Provides sanity checking
 * of array indexes.
 *
 * @param[out]     pArbOutput35
 *     Pointer to @PERF_LIMITS_ARBITRATION_OUTPUT_35.
 * @param[in]      pVfRail
 *     Pointer to @ref PERF_LIMITS_VF.  @ref PERF_LIMITS_VF::idx
 *     specifies the VOLT_RAIL index as input.  @ref
 *     PERF_LIMITS_VF::value specifies the value to user for bounding.
 * @param[in]      voltRailEnum
 *     @ref PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM specifying
 *     the variable within @ref
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35
 *     to bound.
 * @param[in] bMin
 *     Boolean specifying MIN (@ref LW_TRUE) vs. MAX (@ref LW_FALSE) direction
 *     of bounding.
 * @param[in]      pLimit35
 *     PERF_LIMIT_35 pointer to the limit corresponding to this request.
 *
 * @return FLCN_OK
 *     VOLT_RAIL tuples' variable successfully bound.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not bound the value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMITS_ARB_OUTPUT_35_TUPLES_VOLT_RAIL_BOUND
(
   PERF_LIMITS_ARBITRATION_OUTPUT_35              *pArbOutput35,
   PERF_LIMITS_VF                                 *pVfRail,
   PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM  voltRailEnum,
   LwBool                                          bMin,
   PERF_LIMIT_35                                  *pLimit35
)
{
    FLCN_STATUS status;

    if ((pArbOutput35 == NULL) ||
        (pVfRail == NULL) ||
        (pVfRail->idx >= PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto PERF_LIMITS_ARB_OUTPUT_35_TUPLES_VOLT_RAIL_BOUND_exit;
    }

    status = FLCN_ERR_ILWALID_ARGUMENT;
    switch (voltRailEnum)
    {
        case PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM_VOLTAGE_UV:
        {
            if (bMin)
            {
               PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MIN(pArbOutput35,
                   voltRails[pVfRail->idx].super.voltageuV, pVfRail->value,
                   BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));
            }
            else
            {
               PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MAX(pArbOutput35,
                   voltRails[pVfRail->idx].super.voltageuV, pVfRail->value,
                   BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));
            }
            status = FLCN_OK;
            break;
        }
        case PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM_VOLTAGE_MIN_LOOSE_UV:
        {
            if (bMin)
            {
               PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MIN(pArbOutput35,
                   voltRails[pVfRail->idx].voltageMinLooseuV, pVfRail->value,
                   BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));
            }
            else
            {
               PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MAX(pArbOutput35,
                   voltRails[pVfRail->idx].voltageMinLooseuV, pVfRail->value,
                   BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));
            }
            status = FLCN_OK;
            break;
        }
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto PERF_LIMITS_ARB_OUTPUT_35_TUPLES_VOLT_RAIL_BOUND_exit;
    }

PERF_LIMITS_ARB_OUTPUT_35_TUPLES_VOLT_RAIL_BOUND_exit:
    return status;
}

/*!
 * Helper function to return variable values within a set of @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35
 * structures within a @ref PERF_LIMITS_ARBITRATION_OUTPUT_35::tuples[]
 * array.  The variable to bound is specified the @ref
 * PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM enum.
 *
 * Wrapper to @ref PERF_LIMITS_35_ARB_OUTPUT_TUPLES_GET_RANGE().
 * Provides sanity checking of array indexes.
 *
 * @param[out]     pArbOutput35
 *     Pointer to @PERF_LIMITS_ARBITRATION_OUTPUT_35.
 * @param[in/out]  pVfRail
 *     Pointer to @ref PERF_LIMITS_VF_RANGE.  @ref
 *     PERF_LIMITS_VF_RANGE::idx specifies the VOLT_RAIL index as
 *     input.  The variable values will be returned in @ref
 *     PERF_LIMITS_VF::values[].
 * @param[in]      voltRailEnum
 *     @ref PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM specifying
 *     the variable within @ref
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35
 *     to bound.
 *
 * @return FLCN_OK
 *     VOLT_RAIL tuples' variable successfully returned..
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not bound the value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMITS_ARB_OUTPUT_35_TUPLES_VOLT_RAIL_GET_RANGE
(
   PERF_LIMITS_ARBITRATION_OUTPUT_35              *pArbOutput35,
   PERF_LIMITS_VF_RANGE                           *pVfRailRange,
   PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM  voltRailEnum
)
{
    FLCN_STATUS status;

    if ((pArbOutput35 == NULL) ||
        (pVfRailRange == NULL) ||
        (pVfRailRange->idx >= PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto PERF_LIMITS_ARB_OUTPUT_35_TUPLES_VOLT_RAIL_GET_RANGE_exit;
    }

    status = FLCN_ERR_ILWALID_ARGUMENT;
    switch (voltRailEnum)
    {
        case PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM_VOLTAGE_UV:
        {
            PERF_LIMITS_35_ARB_OUTPUT_TUPLES_GET_RANGE(pArbOutput35,
               voltRails[pVfRailRange->idx].super.voltageuV, pVfRailRange);
            status = FLCN_OK;
            break;
        }
        case PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM_VOLTAGE_MIN_LOOSE_UV:
        {
            PERF_LIMITS_35_ARB_OUTPUT_TUPLES_GET_RANGE(pArbOutput35,
               voltRails[pVfRailRange->idx].voltageMinLooseuV, pVfRailRange);
            status = FLCN_OK;
            break;
        }
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto PERF_LIMITS_ARB_OUTPUT_35_TUPLES_VOLT_RAIL_GET_RANGE_exit;
    }

PERF_LIMITS_ARB_OUTPUT_35_TUPLES_VOLT_RAIL_GET_RANGE_exit:
    return status;
}

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Static Function Prototypes ---------------------- */
static FLCN_STATUS s_perfLimits35ArbOutputInitFromDefault(PERF_LIMITS_35 *pLimits35, PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "s_perfLimits35ArbOutputInitFromDefault");
static FLCN_STATUS s_perfLimit35ClientInputToArbInput(PERF_LIMITS_35 *pLimits35, PERF_LIMIT_35 *pLimit35, PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "s_perfLimit35ClientInputToArbInput");
static PerfLimit35ClientInputTypeToArbInput  (s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX");
static PerfLimit35ClientInputTypeToArbInput  (s_perfLimit35ClientInputTypeToArbInput_FREQUENCY)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "s_perfLimit35ClientInputTypeToArbInput_FREQUENCY");
static PerfLimit35ClientInputTypeToArbInput  (s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX");
static PerfLimit35ClientInputTypeToArbInput  (s_perfLimit35ClientInputTypeToArbInput_VOLTAGE)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "s_perfLimit35ClientInputTypeToArbInput_VOLTAGE");
static FLCN_STATUS s_perfLimit35ArbInputClkDomainsPropagate(PERF_LIMITS_35 *pLimits35, PERF_LIMIT_35 *pLimit35, PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35, PERF_LIMIT_ARBITRATION_INPUT_35 *pArbInput35)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "s_perfLimit35ArbInputClkDomainsPropagate");
static FLCN_STATUS s_perfLimit35ArbInputToArbOutput(PERF_LIMITS_35 *pLimits35, PERF_LIMIT_35 *pLimit35, PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "s_perfLimit35ArbInputToArbOutput");
static FLCN_STATUS s_perfLimits35ArbOutputVoltages(PERF_LIMITS_35 *pLimits35, PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "s_perfLimits35ArbOutputVoltages");
static FLCN_STATUS s_perfLimits35ArbInputLooseVoltageInit(PERF_LIMITS_35 *pLimits35)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "s_perfLimits35ArbInputLooseVoltageInit");

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * PERF_LIMIT_35 implementation
 *
 * @copydoc PerfLimitsConstruct
 */
FLCN_STATUS
perfLimitsConstruct_35
(
    PERF_LIMITS **ppLimits
)
{
    PERF_LIMITS_35 *pLimits35;
    FLCN_STATUS     status = FLCN_OK;

    //
    // Do NOT update PERF_LIMITS version as doing so will override the
    // version set by child class.
    //
    // pLimits35->super.version = LW2080_CTRL_PERF_PERF_LIMITS_VERSION_35;
    //

    // Construct the PERF_LIMITS SUPER object
    status = perfLimitsConstruct_SUPER(ppLimits);
    if (status != FLCN_OK)
    {
        PMU_HALT();
        goto perfLimitsConstruct_35_exit;
    }
    pLimits35 = (PERF_LIMITS_35 *)*ppLimits;

    //
    // Init various PERF_LIMITS_ARBITRATION_OUTPUT_35 structs and hook them up
    // to pointers in PERF_LIMITS object.
    //
    PERF_LIMITS_ARBITRATION_OUTPUT_35_INIT(&pLimits35->arbOutput35Apply);
    pLimits35->super.pArbOutputApply = &pLimits35->arbOutput35Apply.super;
    PERF_LIMITS_ARBITRATION_OUTPUT_35_INIT(&pLimits35->arbOutput35Scratch);
    pLimits35->super.pArbOutputScratch = &pLimits35->arbOutput35Scratch.super;

perfLimitsConstruct_35_exit:
    return status;
}

/*!
 * PERF_LIMITS_35 implementation
 *
 * @copydoc PerfLimitsActiveDataGet
 */
PERF_LIMIT_ACTIVE_DATA *
perfLimitsActiveDataGet_35
(
    PERF_LIMITS *pLimits,
    LwU8         idx
)
{
    PERF_LIMITS_35 *pLimits35 = (PERF_LIMITS_35 *)pLimits;

    if (idx >= PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE)
    {
        PMU_BREAKPOINT();
        return NULL;
    }

    return &pLimits35->activeData[idx].super;
}

/*!
 * PERF_LIMITS_35 implementation
 *
 * @copydoc PerfLimitsActiveDataAcquire
 */
FLCN_STATUS
perfLimitsActiveDataAcquire_35
(
    PERF_LIMITS                              *pLimits,
    PERF_LIMIT                               *pLimit,
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput
)
{
    PERF_LIMIT_ACTIVE_DATA_35 *pActiveData35;

    if ((pLimit == NULL) ||
        (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID == pLimit->activeDataIdx))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }

    pActiveData35 = (PERF_LIMIT_ACTIVE_DATA_35 *)perfLimitsActiveDataGet(pLimits, pLimit->activeDataIdx);
    if (pActiveData35 == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }
    perfLimitArbitrationInput35Init(&pActiveData35->arbInput);

    return FLCN_OK;
}

/*!
 * PERF_LIMITS_35 implementation.
 *
 * @copydoc PerfLimitsArbitrate
 */
FLCN_STATUS
perfLimitsArbitrate_35
(
    PERF_LIMITS                    *pLimits,
    PERF_LIMITS_ARBITRATION_OUTPUT *pArbOutput,
    BOARDOBJGRPMASK_E255           *pLimitMaskExclude,
    LwBool                          bMin
)
{
    PERF_LIMITS_35                    *pLimits35    = (PERF_LIMITS_35 *)pLimits;
    PERF_LIMIT                        *pLimit       = NULL;
    PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35 =
        (PERF_LIMITS_ARBITRATION_OUTPUT_35 *)pArbOutput;
    PERF_POLICIES_35                  *pPolicies35  =
        (PERF_POLICIES_35 *)PERF_POLICIES_GET();
    BOARDOBJGRPMASK_E255 limitMaskArb;
    LwBoardObjIdx        l;
    FLCN_STATUS          status = FLCN_OK;
    LwBool               bUpdatePolicies = (pLimitMaskExclude == NULL) ||
                                (boardObjGrpMaskIsZero(pLimitMaskExclude));

    // Reset the PERF_POLICIES only on an ARBITRATE_AND_APPLY
    if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY)) &&
        (bUpdatePolicies))
    {
        status = perfPolicies35ResetLimitInput(pPolicies35);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrate_35_exit;
        }
    }

    // Init the min and max tuples from the default arbitration output.
    status = s_perfLimits35ArbOutputInitFromDefault(pLimits35, pArbOutput35);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsArbitrate_35_exit;
    }

    // Init the loose voltage limit information
    status = s_perfLimits35ArbInputLooseVoltageInit(pLimits35);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsArbitrate_35_exit;
    }

    // Build mask of limits amongst which to arbitrate - i.e. active & included.
    boardObjGrpMaskInit_E255(&limitMaskArb);
    status = boardObjGrpMaskCopy(&limitMaskArb,
                                 &(pLimits35->super.limitMaskActive));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsArbitrate_35_exit;
    }
    //
    // If exclude mask specified, ilwert (~) it and then and (&) into
    // limitMaskArb.
    //
    if (pLimitMaskExclude != NULL)
    {
        BOARDOBJGRPMASK_E255 limitMaskInclude;

        boardObjGrpMaskInit_E255(&limitMaskInclude);

        status = boardObjGrpMaskCopy(&limitMaskInclude, pLimitMaskExclude);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrate_35_exit;
        }

        status = boardObjGrpMaskIlw(&limitMaskInclude);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrate_35_exit;
        }

        status = boardObjGrpMaskAnd(&limitMaskArb, &limitMaskArb, &limitMaskInclude);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrate_35_exit;
        }
    }

    // Loop over active, included PERF_LIMITs:
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_LIMIT, pLimit, l, &limitMaskArb.super)
    {
        PERF_LIMIT_35 *pLimit35 = (PERF_LIMIT_35 *)pLimit;

        //
        // a. If necessary, colwert CLIENT_INPUT -> ARBITRATION_INPUT.
        // Necessary when caching is disabled or PERF_LIMIT is "dirty" (i.e. new
        // CLIENT_INPUT or depended state has changed)
        //
        if (!pLimits35->super.bCachingEnabled ||
            boardObjGrpMaskBitGet(&(pLimits35->super.limitMaskDirty), l))
        {
            status = s_perfLimit35ClientInputToArbInput(pLimits35, pLimit35,
                        pArbOutput35);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfLimitsArbitrate_35_exit;
            }

            if (pLimits35->super.bCachingEnabled)
            {
                boardObjGrpMaskBitClr(&(pLimits35->super.limitMaskDirty), l);
            }
        }

        if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY)) &&
            (bUpdatePolicies) &&
            (pLimit35->super.perfPolicyId != LW2080_CTRL_PERF_POLICY_SW_ID_ILWALID))
        {
            LwBool bMin = (FLD_TEST_REF(LW2080_CTRL_PERF_LIMIT_INFO_FLAGS_MIN,
                    _TRUE, pLimit35->super.flags));
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX tupleIdx =
                bMin ?
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN :
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX;
            PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pArbInput35Tuple =
                &pLimits35->activeData[pLimit35->super.activeDataIdx].arbInput.tuples[tupleIdx];
            PERF_POLICY_35 *pPolicy35 = (PERF_POLICY_35 *)PERF_POLICY_GET(pLimit35->super.perfPolicyId);
            if (pPolicy35 == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto perfLimitsArbitrate_35_exit;
            }

            status = perfPolicy35UpdateLimitInput(pPolicy35, pArbInput35Tuple, bMin);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfLimitsArbitrate_35_exit;
            }
        }

        // b. Apply ARBITRATION_INPUT to ARBITRATION_OUTPUT.
        status = s_perfLimit35ArbInputToArbOutput(pLimits35, pLimit35,
                    pArbOutput35);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrate_35_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Process voltages for final target frequencies.
    status = s_perfLimits35ArbOutputVoltages(pLimits35, pArbOutput35);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsArbitrate_35_exit;
    }

    //
    // Pick the result tuple from either the min or the max, depending on the
    // requested result.  Copying from _35-specific tuple struct to PERF_LIMITS
    // common/version-independent struct.
    //
    {
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE *pTuple =
            &pArbOutput35->super.tuple;
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 *pTuple35 =
            (bMin) ?
                &pArbOutput35->tuples[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN] :
                &pArbOutput35->tuples[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX];
        LwBoardObjIdx d, r;

        // Copy the PSTATE TUPLE_VALUE
        pTuple->pstateIdx = pTuple35->pstateIdx;

        // Copy the TUPLE_CLK_DOMAIN structs.
        for (d = 0; d < PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS; d++)
        {
            pTuple->clkDomains[d] = pTuple35->clkDomains[d].super;
        }

        // Copy the TUPLE_VOLT_RAIL structs.
        for (r = 0; r < PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS; r++)
        {
            pTuple->voltRails[r] = pTuple35->voltRails[r].super;
        }
    }

    if ((PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY)) &&
        (bUpdatePolicies))
    {
        status = perfPolicies35UpdateViolationTime(pPolicies35, &pArbOutput35->super.tuple);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrate_35_exit;
        }
    }

perfLimitsArbitrate_35_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfLimitGrpIfaceModel10ObjSetImpl_35
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pDesc
)
{
    RM_PMU_PERF_PERF_LIMIT_35 *pLimit35Desc;
    PERF_LIMIT_35             *pLimit35;
    FLCN_STATUS                status;

    // Call super-class constructor.
    status = perfLimitGrpIfaceModel10ObjSetImpl_SUPER(pModel10, ppBoardObj, size, pDesc);
    if (status != FLCN_OK)
    {
        goto perfLimitGrpIfaceModel10ObjSetImpl_35_exit;
    }
    pLimit35     = (PERF_LIMIT_35 *)*ppBoardObj;
    pLimit35Desc = (RM_PMU_PERF_PERF_LIMIT_35 *)pDesc;

    // Set PERF_LIMIT_35 parameters.
    pLimit35->bStrictPropagationPstateConstrained =
        pLimit35Desc->bStrictPropagationPstateConstrained;
    pLimit35->bForceNoiseUnawareStrictPropgation  =
        pLimit35Desc->bForceNoiseUnawareStrictPropgation;

perfLimitGrpIfaceModel10ObjSetImpl_35_exit:
    return status;
}

/*!
 * PERF_LIMIT_35 implementation
 *
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
FLCN_STATUS
perfLimitsIfaceModel10GetStatusHeader_35
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_GET_STATUS_HEADER *pLimitsStatus    =
        (RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_GET_STATUS_35     *pLimits35Status =
        &pLimitsStatus->data.v35;
    PERF_LIMITS_35 *pLimits35 = (PERF_LIMITS_35 *)pBoardObjGrp;

    // PERF_LIMITS_35 status variables.
    PERF_LIMITS_ARBITRATION_OUTPUT_35_EXPORT(&pLimits35->arbOutput35Apply,
        &pLimits35Status->arbOutput35Apply);

    return FLCN_OK;
}

/*!
 * PERF_LIMIT SUPER implementation
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
perfLimitIfaceModel10GetStatus_35
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_PERF_LIMIT_35_GET_STATUS *pLimit35Status =
        (RM_PMU_PERF_PERF_LIMIT_35_GET_STATUS *)pBuf;
    PERF_LIMITS   *pLimits  = PERF_PERF_LIMITS_GET();
    PERF_LIMIT_35 *pLimit35 = (PERF_LIMIT_35 *)pBoardObj;
    FLCN_STATUS    status   = FLCN_OK;

    // Call super-class impementation;
    status = perfLimitIfaceModel10GetStatus_SUPER(pModel10, pBuf);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitIfaceModel10GetStatus_35_exit;
    }

    // CRPTODO/JBH-TODO - Class-specific data will be added in a follow-up CL.
    (void)pLimit35Status->super.super.type;

    // If ACTIVE_DATA acquired/non-NULL copy out STATUS data from that ACTIVE_DATA.
    if (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID != pLimit35->super.activeDataIdx)
    {
        // CRPTODO/JBH-TODO - Class-specific data will be added in a follow-up CL.
        PERF_LIMIT_ACTIVE_DATA_35 *pActiveData35 =
            (PERF_LIMIT_ACTIVE_DATA_35 *)perfLimitsActiveDataGet(
                pLimits, pLimit35->super.activeDataIdx);

        // Export the ARBITATION_INPUT -> RMCTRL.
        PERF_LIMIT_ARBITRATION_INPUT_35_EXPORT(&pActiveData35->arbInput,
            &pLimit35Status->arbInput);
    }
    // If ACTIVE_DATA not-acquired/NULL, instead indicate all STATUS data as DISABLED.
    else
    {
        // Init the RMCTRL data to the unset state.
        LW2080_CTRL_PERF_PERF_LIMIT_ARBITRATION_INPUT_35_INIT(
            &pLimit35Status->arbInput);
    }

perfLimitIfaceModel10GetStatus_35_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * Helper function to init PERF_LIMITS_ARBITRATION_OUTPUT_35 structure from
 * PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT.
 *
 * @param[in]  pLimits35    PERF_LIMITS_35 pointer
 * @param[out] pArbOutput35
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_35 structure to init.
 *
 * @return FLCN_OK
 *     PERF_LIMITS_ARBITRATION_OUTPUT_35 structure successfully inited.
 * @return Other errors
 *     Unexpected error encountered.
 */
static FLCN_STATUS
s_perfLimits35ArbOutputInitFromDefault
(
    PERF_LIMITS_35                    *pLimits35,
    PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35
)
{
    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT        *pArbOutputDefault =
        &pLimits35->super.arbOutputDefault;
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35       *pTuple35      = NULL;
    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLE  *pTupleDefault = NULL;
    CLK_DOMAIN   *pDomain = NULL;
    LwBoardObjIdx d;
    FLCN_STATUS   status  = FLCN_OK;
    LwU8          i;

    //
    // If caching not enabled, re-compute the ARBITRATION_OUTPUT_DEFAULT result
    // for use in this arbitration run.
    //
    if (!pLimits35->super.bCachingEnabled)
    {
        status = perfLimitsArbOutputDefaultCache(&pLimits35->super,
                    pArbOutputDefault);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimits35ArbOutputInitFromDefault_exit;
        }
    }

    // Sanity check that the CLK_DOMAINS masks are equal.
    if (!boardObjGrpMaskIsEqual(&(pArbOutput35->super.clkDomainsMask),
                                &(pArbOutputDefault->clkDomainsMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto s_perfLimits35ArbOutputInitFromDefault_exit;
    }

    // Loop over min and max tuple.
    PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR(pArbOutput35, i, pTuple35)
    {
        LwU8 loPriLimitIdx = boardObjGrpMaskBitIdxLowest(&pLimits35->super.limitMaskActive);
        pTupleDefault = &pArbOutputDefault->tuples[i];

        // 1. Init the whole PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 struct.
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35_INIT(pTuple35);

        // 2. Copy out the pstate index.
        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_SET(
            &pTuple35->pstateIdx, pTupleDefault->pstateIdx,
            loPriLimitIdx);

        // 3. Copy out the CLK_DOMAIN frequency ranges.
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
            &pArbOutput35->super.clkDomainsMask.super)
        {
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
               *pTupleClkDomain35;
            PERF_LIMITS_VF vfDomain;
            LwBool         bNoiseUnaware = LW_TRUE;

            //
            // Retrieve the pointer to the
            // LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35.
            //
            PMU_ASSERT_OK_OR_GOTO(status,
                PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35_GET_CLK_DOMAIN(
                    pTuple35, d, &pTupleClkDomain35),
                s_perfLimits35ArbOutputInitFromDefault_exit);

            //
            // Retrieve the CLK_DOMAIN frequency from the
            // ARBITRATION_OUTPUT_DEFAULT_TUPLE.
            //
            vfDomain.idx   = BOARDOBJ_GRP_IDX_TO_8BIT(d);
            PMU_ASSERT_OK_OR_GOTO(status,
                PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLE_CLK_DOMAIN_GET(
                    pTupleDefault, &vfDomain),
                s_perfLimits35ArbOutputInitFromDefault_exit);

            // Target frequency is copied directly from ARBITRATION_OUTPUT_DEFAULT.
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_SET(
                &pTupleClkDomain35->super.freqkHz, vfDomain.value,
                loPriLimitIdx);

            //
            // Noise-unaware minimum frequency is copied only if that frequency
            // value is noise-unaware per CLK_PROG.
            //
            status = perfLimitsFreqkHzIsNoiseUnaware(&vfDomain, &bNoiseUnaware);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_perfLimits35ArbOutputInitFromDefault_exit;
            }
            if (bNoiseUnaware)
            {
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_SET(
                    &pTupleClkDomain35->freqNoiseUnawareMinkHz, vfDomain.value,
                    loPriLimitIdx);
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR_END;

s_perfLimits35ArbOutputInitFromDefault_exit:
    return status;
}

/*!
 * Colwerts a PERF_LIMIT_35 object's CLIENT_INPUT to ARBITRATION_INPUT.  This
 * colwerts a single CLIENT_INPUT criterium into a full specification of a min
 * and max (PSTATE, CLK_DOMAINs, VOLT_RAILs) tuples, taking into account all of
 * the various depended state - e.g. VF lwrve, PSTATEs, VPSTATEs, VFE equations,
 * etc.
 *
 * @param[in]    pLimits35     PERF_LIMITS_35 pointer
 * @param[in]    pLimit35      PERF_LIMIT_35 pointer
 * @param[in]    pArbOutput35
 *     PERF_LIMITS_ARBITRATION_OUTPUT_35 pointer.  This structure provides
 *     various input data, such as @ref
 *     PERF_LIMITS_ARBITRATION_OUTPUT_35::super.clkDomainsMask.
 *
 * @return FLCN_OK
 *     CLIENT_INPUT colwersion to ARBITATION_INPUT was successful.
 * @return Other errors
 *     Unexpected error encountered.
 */
static FLCN_STATUS
s_perfLimit35ClientInputToArbInput
(
    PERF_LIMITS_35                    *pLimits35,
    PERF_LIMIT_35                     *pLimit35,
    PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35
)
{
    PERF_LIMIT_ACTIVE_DATA_35 *pActiveData35;
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput;
    PERF_LIMIT_ARBITRATION_INPUT_35 *pArbInput35;
    FLCN_STATUS status  = FLCN_OK;

    if (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID == pLimit35->super.activeDataIdx)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_perfLimit35ClientInputToArbInput_exit;
    }

    pActiveData35 = (PERF_LIMIT_ACTIVE_DATA_35 *)perfLimitsActiveDataGet(
            &pLimits35->super, pLimit35->super.activeDataIdx);
    pClientInput  = &pActiveData35->super.clientInput;
    pArbInput35   = &pActiveData35->arbInput;

    //
    // 1. Init the ARBITRATION_INPUT_35 data to default/unset state.  Will be
    // filled in from CLIENT_INPUT below.
    //
    perfLimitArbitrationInput35Init(pArbInput35);

    // 2. Colwert CLIENT_INPUT to ARBITRATION_INPUT based on TYPE.
    switch (pClientInput->type)
    {
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_PSTATE_IDX:
        {
            status = s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX(pLimits35,
                        pLimit35, pArbOutput35, pArbInput35, pClientInput);
            break;
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_FREQUENCY:
        {
            status = s_perfLimit35ClientInputTypeToArbInput_FREQUENCY(pLimits35,
                        pLimit35, pArbOutput35, pArbInput35, pClientInput);
            break;
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VPSTATE_IDX:
        {
            status = s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX(pLimits35,
                        pLimit35, pArbOutput35, pArbInput35, pClientInput);
            break;
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VOLTAGE:
        {
            status = s_perfLimit35ClientInputTypeToArbInput_VOLTAGE(pLimits35,
                        pLimit35, pArbOutput35, pArbInput35, pClientInput);
            break;
        }
        // Un-supported/un-recognized TYPEs are an error.
        default:
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputToArbInput_exit;
        }
    }
    // Catch for errors in TYPE-specific colwersion.
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ClientInputToArbInput_exit;
    }

    // 3. Set ARBITRATION_INPUT's remaining un-set CLK_DOMAINs.
    status = s_perfLimit35ArbInputClkDomainsPropagate(pLimits35, pLimit35,
                pArbOutput35, pArbInput35);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ClientInputToArbInput_exit;
    }

    // 4. Sanity check ARBITRATION_INPUT data:
    {
        PERF_LIMITS_PSTATE_RANGE pstateRange;
        CLK_DOMAIN   *pDomain = NULL;
        LwBoardObjIdx d;

        //
        // a. All expected CLK_DOMAINS have been specified -
        // i.e. ARBITRATOIN_INPUT::clkDomainsSetMask ==
        // ARBITRATION_OUTPUT::clkDomainsMask.
        //
        if (!boardObjGrpMaskIsEqual(&(pArbInput35->clkDomainsMask),
                                    &(pArbOutput35->super.clkDomainsMask)))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto s_perfLimit35ClientInputToArbInput_exit;
        }
        //
        // b. Only expected VOLT_RAILs may have been specified -
        // i.e. ARBITRATOIN_INPUT::voltRailsSetMask is a subset of
        // ARBITRATION_OUTPUT::voltRailsMask.
        //
        if (!boardObjGrpMaskIsSubset(&(pArbInput35->voltRailsMask),
                                     &(pArbOutput35->super.voltRailsMask)))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto s_perfLimit35ClientInputToArbInput_exit;
        }
        //
        // c. ARBITRATION_INPUT min and max (PSTATE, CLK_DOMAINs) tuples are
        // coherent:
        //
        // 1) Tuple max PSTATE >= tuple min.
        //
        PERF_LIMIT_35_ARB_INPUT_PSTATE_GET_RANGE(pArbInput35, &pstateRange);
        if (!PERF_LIMITS_RANGE_IS_COHERENT(&pstateRange))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto s_perfLimit35ClientInputToArbInput_exit;
        }

        // 2) For each CLK_DOMAIN:
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
            &pArbOutput35->super.clkDomainsMask.super)
        {
            PERF_LIMITS_VF_RANGE vfDomainInputRange;
            PERF_LIMITS_VF_RANGE vfDomainPstateRange;
            LwU8                 i;

            vfDomainInputRange.idx = vfDomainPstateRange.idx =
                BOARDOBJ_GRP_IDX_TO_8BIT(d);

            //
            // a) Tuple max CLK_DOMAIN >= tuple min.
            //
            status = PERF_LIMIT_35_ARB_INPUT_CLK_DOMAIN_GET_RANGE(pArbInput35,
                &vfDomainInputRange);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_perfLimit35ClientInputToArbInput_exit;
            }
            if (!PERF_LIMITS_RANGE_IS_COHERENT(&vfDomainInputRange))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
                goto s_perfLimit35ClientInputToArbInput_exit;
            }

            //
            // b) CLK_DOMAIN tuple frequency within tuple PSTATE's CLK_DOMAIN
            // frequency range.
            //
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
            {
                PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pInputTuple35 =
                    &pArbInput35->tuples[i];
                PERF_LIMITS_PSTATE_RANGE pstateRange;

                PERF_LIMITS_RANGE_SET_VALUE(&pstateRange, pInputTuple35->pstateIdx);

                status = perfLimitsPstateIdxRangeToFreqkHzRange(
                            &pstateRange, &vfDomainPstateRange);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ClientInputToArbInput_exit;
                }
                if (!PERF_LIMITS_RANGE_CONTAINS_VALUE(&vfDomainPstateRange,
                        vfDomainInputRange.values[i]))
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto s_perfLimit35ClientInputToArbInput_exit;
                }
            }
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;
        }
        BOARDOBJGRP_ITERATOR_END;
    }

s_perfLimit35ClientInputToArbInput_exit:
    return status;
}

/*!
 * @ref LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_PSTATE_IDX specific
 * CLIENT_INPUT -> ARBITRATION_INPUT colwersion function.
 *
 * @copydoc PerfLimit35ClientInputTypeToArbInput
 */
static FLCN_STATUS
s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX
(
    PERF_LIMITS_35                           *pLimits35,
    PERF_LIMIT_35                            *pLimit35,
    PERF_LIMITS_ARBITRATION_OUTPUT_35        *pArbOutput35,
    PERF_LIMIT_ARBITRATION_INPUT_35          *pArbInput35,
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput
)
{
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_PSTATE *pInputPstate =
        &pClientInput->data.pstate;
    PSTATE_35          *pPstate35 = (PSTATE_35 *)PSTATE_GET(pInputPstate->pstateIdx);
    CLK_DOMAIN         *pDomain   = NULL;
    BOARDOBJGRPMASK_E32 clkDomainStrictPropagationMask;
    LwBoardObjIdx       d;
    FLCN_STATUS         status    = FLCN_OK;

    // Sanity check that the PSTATE index is valid.
    if (pPstate35 == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX_exit;
    }

    // Set the pstateIdx directly from the CLIENT_INPUT.
    PERF_LIMIT_35_ARB_INPUT_PSTATE_SET_VALUE(pArbInput35, pInputPstate->pstateIdx);

    status = perfLimit35ClkDomainStrictPropagationMaskGet(pLimit35,
                                                         &clkDomainStrictPropagationMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX_exit;
    }
    //
    // Strict CLK_DOMAINs are bound to the _POINT - a single frequency
    // value as both min and max tuple.
    //
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &clkDomainStrictPropagationMask.super)
    {
        const PERF_PSTATE_35_CLOCK_ENTRY *pcPstateClkFreq;
        PERF_LIMITS_VF vfDomain;

        // Query the frequency values for the PSTATE.
        status = perfPstate35ClkFreqGetRef(pPstate35,
                    BOARDOBJ_GRP_IDX_TO_8BIT(d), &pcPstateClkFreq);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX_exit;
        }

        // Pull out the correct frequency based on the POINT.
        vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);
        switch (pInputPstate->point)
        {
            case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_PSTATE_POINT_NOM:
            {
                vfDomain.value = pcPstateClkFreq->nom.freqVFMaxkHz;
                break;
            }
            case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_PSTATE_POINT_MIN:
            {
                vfDomain.value = pcPstateClkFreq->min.freqVFMaxkHz;
                break;
            }
            case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_PSTATE_POINT_MAX:
            {
                vfDomain.value = pcPstateClkFreq->max.freqVFMaxkHz;
                break;
            }
            case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_PSTATE_POINT_MID:
            {
                vfDomain.value =
                    (pcPstateClkFreq->min.freqVFMaxkHz + pcPstateClkFreq->max.freqVFMaxkHz) / 2;
                break;
            }
            default:
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                PMU_BREAKPOINT();
                goto s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX_exit;
            }
        }

        //
        // Quantize requested CLK_DOMAIN frequency to the availabe PSTATE ranges
        // and VF frequency steps.
        //
        status = perfLimitsFreqkHzQuantize(&vfDomain, NULL,
                    FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _MAX, _TRUE,
                        pLimit35->super.flags));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX_exit;
        }

        //
        // Set CLK_DOMAIN's both MIN and MAX tuple values to the PSTATE point
        // frequency.
        //
        status = PERF_LIMIT_35_ARB_INPUT_CLK_DOMAIN_SET_VALUE(pArbInput35, &vfDomain);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

s_perfLimit35ClientInputTypeToArbInput_PSTATE_IDX_exit:
    return status;
}

/*!
 * @ref LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_FREQUENCY specific
 * CLIENT_INPUT -> ARBITRATION_INPUT colwersion function.
 *
 * @copydoc PerfLimit35ClientInputTypeToArbInput
 */
static FLCN_STATUS
s_perfLimit35ClientInputTypeToArbInput_FREQUENCY
(
    PERF_LIMITS_35                           *pLimits35,
    PERF_LIMIT_35                            *pLimit35,
    PERF_LIMITS_ARBITRATION_OUTPUT_35        *pArbOutput35,
    PERF_LIMIT_ARBITRATION_INPUT_35          *pArbInput35,
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput
)
{
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_FREQ *pInputFreq =
        &pClientInput->data.freq;
    PERF_LIMITS_VF vfDomain;
    FLCN_STATUS    status  = FLCN_OK;

    // Check that provided CLK_DOMAIN index is valid in the ARBITRATION_OUTPUT.
    if (!boardObjGrpMaskBitGet(&pArbOutput35->super.clkDomainsMask,
            pInputFreq->clkDomainIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_perfLimit35ClientInputTypeToArbInput_FREQUENCY_exit;
    }

    //
    // 1. Quantize requested CLK_DOMAIN frequency to the availabe PSTATE ranges
    // and VF frequency steps.
    //
    vfDomain.idx   = pInputFreq->clkDomainIdx;
    vfDomain.value = pInputFreq->freqKHz;
    status = perfLimitsFreqkHzQuantize(&vfDomain, NULL,
                FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _MAX, _TRUE,
                    pLimit35->super.flags));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ClientInputTypeToArbInput_FREQUENCY_exit;
    }

    // 2. Set the frequency in the min/max tuples.
    status = PERF_LIMIT_35_ARB_INPUT_CLK_DOMAIN_SET_VALUE(pArbInput35, &vfDomain);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ClientInputTypeToArbInput_FREQUENCY_exit;
    }

    //
    // 3. Colwert the CLK_DOMAIN frequency to a corresponding min/max PSTATE
    // range for the MIN/MAX TUPLEs.
    //
    {
        PERF_LIMITS_PSTATE_RANGE pstateRange;
        status = perfLimitsFreqkHzToPstateIdxRange(&vfDomain,
                    &pstateRange);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputTypeToArbInput_FREQUENCY_exit;
        }

        PERF_LIMIT_35_ARB_INPUT_PSTATE_SET_RANGE(pArbInput35, &pstateRange);
    }

s_perfLimit35ClientInputTypeToArbInput_FREQUENCY_exit:
    return status;
}

/*!
 * @ref LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VPSTATE_IDX specific
 * CLIENT_INPUT -> ARBITRATION_INPUT colwersion function.
 *
 * @copydoc PerfLimit35ClientInputTypeToArbInput
 */
static FLCN_STATUS
s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX
(
    PERF_LIMITS_35                           *pLimits35,
    PERF_LIMIT_35                            *pLimit35,
    PERF_LIMITS_ARBITRATION_OUTPUT_35        *pArbOutput35,
    PERF_LIMIT_ARBITRATION_INPUT_35          *pArbInput35,
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput
)
{
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VPSTATE *pInputVpstate =
        &pClientInput->data.vpstate;
    VPSTATE_3X         *pVpstate3x;
    CLK_DOMAIN         *pDomain;
    BOARDOBJGRPMASK_E32 clkDomainsMask;
    LwBoardObjIdx       d;
    FLCN_STATUS         status         = FLCN_OK;
    LwU8                pstateIdx;

    // Check that VPSTATE idx is valid.
    if (!BOARDOBJGRP_IS_VALID(VPSTATE, pInputVpstate->vpstateIdx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX_exit;
    }

    // Fetch the VPSTATE pointer from the BOARDOBJGRP.
    pVpstate3x = (VPSTATE_3X *)BOARDOBJGRP_OBJ_GET(VPSTATE, pInputVpstate->vpstateIdx);
    if (pVpstate3x == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX_exit;
    }

    pstateIdx = PSTATE_GET_INDEX_BY_LEVEL(vpstate3xPstateIdxGet(pVpstate3x));
    if (pstateIdx == LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX_exit;
    }

    // Set the PSTATE index from the VPSTATE.
    PERF_LIMIT_35_ARB_INPUT_PSTATE_SET_VALUE(pArbInput35, pstateIdx);

    //
    // Only set CLK_DOMAIN values for CLK_DOMAINs set in both the
    // VPSTATE::clkDomainsMask and the ARIBTRATION_OUTPUT::clkDomainsMask.
    //
    status = perfLimitsVpstateIdxToClkDomainsMask(
                pInputVpstate->vpstateIdx, &clkDomainsMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX_exit;
    }
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskAnd(&clkDomainsMask, &clkDomainsMask,
            &pArbOutput35->super.clkDomainsMask),
        s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX_exit);
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &clkDomainsMask.super)
    {
        PERF_LIMITS_VF vfDomain;
        vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);

        // Set the VPSTATE's targetFreq as the value for this CLK_DOMAIN.
        status = perfLimitsVpstateIdxToFreqkHz(
                    pInputVpstate->vpstateIdx,
                    FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _MAX, _TRUE,
                        pLimit35->super.flags),
                    LW_FALSE,
                    &vfDomain);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX_exit;
        }
        status = PERF_LIMIT_35_ARB_INPUT_CLK_DOMAIN_SET_VALUE(pArbInput35, &vfDomain);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

s_perfLimit35ClientInputTypeToArbInput_VPSTATE_IDX_exit:
    return status;
}

/*!
 * @ref LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VOLTAGE specific
 * CLIENT_INPUT -> ARBITRATION_INPUT colwersion function.
 *
 * @copydoc PerfLimit35ClientInputTypeToArbInput
 */
static FLCN_STATUS
s_perfLimit35ClientInputTypeToArbInput_VOLTAGE
(
    PERF_LIMITS_35                           *pLimits35,
    PERF_LIMIT_35                            *pLimit35,
    PERF_LIMITS_ARBITRATION_OUTPUT_35        *pArbOutput35,
    PERF_LIMIT_ARBITRATION_INPUT_35          *pArbInput35,
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput
)
{
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE *pInputVoltage =
        &pClientInput->data.voltage;
    PERF_LIMITS_VF           vfRail;
    PERF_LIMITS_PSTATE_RANGE pstateRange;
    BOARDOBJGRPMASK_E32      clkDomainsProgMask;
    BOARDOBJGRPMASK_E32      clkDomainsStrictPropagationMask;
    VOLT_RAIL               *pRail;
    CLK_DOMAIN              *pDomain;
    FLCN_STATUS              status  = FLCN_OK;
    LwBoardObjIdx            d;
    LwU8                     i;

    pRail = VOLT_RAIL_GET(pInputVoltage->voltRailIdx);
    if (pRail == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
    }

    //
    // Build mask of CLK_DOMAIN_PROGs which PERF_LIMITS code is
    // attempting to set which has Vmin on the specified VOLT_RAIL.
    // This mask will be used to sanity check all F->V lookups for the
    // @ref LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE.
    //
    {
        BOARDOBJGRPMASK_E32 *pRailClkDomainsProgMask;
        boardObjGrpMaskInit_E32(&clkDomainsProgMask);

        pRailClkDomainsProgMask = voltRailGetClkDomainsProgMask(pRail);
        PMU_ASSERT_OK_OR_GOTO(status,
            ((pRailClkDomainsProgMask != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
            s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit);
        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskAnd(&clkDomainsProgMask,
                &pArbOutput35->super.clkDomainsMask, pRailClkDomainsProgMask),
            s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit);
    }

    // 1. Callwlate the actual intersect voltage (uV) value.
    vfRail.idx   = pInputVoltage->voltRailIdx;
    vfRail.value = 0;
    for (i = 0; i < pInputVoltage->numElements; i++)
    {
        LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT *pElement =
            &pInputVoltage->elements[i];
        PERF_LIMITS_VF vfRailElement;
        vfRailElement.idx   = vfRail.idx;
        vfRailElement.value = 0;

        // a. Callwlate type specific information from the ELEMENT.
        switch (pElement->type)
        {
            case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_LOGICAL:
            {
                vfRailElement.value = pElement->data.logical.voltageuV;
                break;
            }
            case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_VFE:
            {
                RM_PMU_PERF_VFE_EQU_RESULT result;
                //
                // Error case where no VFE_EQU_MONITOR handle is reserved.  Can
                // assume value = 0 above.
                //
                if (VFE_EQU_MONITOR_IDX_ILWALID == pElement->data.vfe.vfeEquMonIdx)
                {
                    status = FLCN_ERR_ILWALID_STATE;
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                    break;
                }

                // Fetch the cached result from the VFE_EQU_MONITOR.
                status = vfeEquMonitorResultGet(PERF_PMU_VFE_EQU_MONITOR_GET(),
                            pElement->data.vfe.vfeEquMonIdx, &result);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                }

                vfRailElement.value = result.voltuV;
                break;
            }
            case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_PSTATE:
            {
                const PERF_PSTATE_35_CLOCK_ENTRY *pcPstateClkFreq;
                PSTATE_35     *pPstate35 = (PSTATE_35 *)PSTATE_GET(pElement->data.pstate.pstateIdx);
                PERF_LIMITS_VF vfRailPstate;
                vfRailPstate.idx = vfRail.idx;

                if (pPstate35 == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                }

                //
                // Iterate over the PSTATE's CLK_DOMAIN values for set
                // of CLK_DOMAINs which Vmins on the given VOL_RAIL
                // and intersect from those frequencies.
                //
                BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
                    &clkDomainsProgMask.super)
                {
                    PERF_LIMITS_VF vfDomain;
                    vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);

                    // Query the frequency values for the PSTATE.
                    status = perfPstate35ClkFreqGetRef(pPstate35,
                                BOARDOBJ_GRP_IDX_TO_8BIT(d), &pcPstateClkFreq);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                    }
                    switch (pElement->data.pstate.freqType)
                    {
                        case LW2080_CTRL_PERF_VOLT_DOM_INFO_PSTATE_FREQ_TYPE_MIN:
                        {
                            vfDomain.value = pcPstateClkFreq->min.freqVFMaxkHz;
                            break;
                        }
                        case LW2080_CTRL_PERF_VOLT_DOM_INFO_PSTATE_FREQ_TYPE_MAX:
                        {
                            vfDomain.value = pcPstateClkFreq->max.freqVFMaxkHz;
                            break;
                        }
                        case LW2080_CTRL_PERF_VOLT_DOM_INFO_PSTATE_FREQ_TYPE_NOM:
                        {
                            vfDomain.value = pcPstateClkFreq->nom.freqVFMaxkHz;
                            break;
                        }
                        default:
                        {
                            status = FLCN_ERR_ILWALID_ARGUMENT;
                            PMU_BREAKPOINT();
                            goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                            break;
                        }
                    }

                    // F->V look up for the PSTATE intersect frequencies.
                    status = perfLimitsFreqkHzToVoltageuV(&vfDomain,
                                &vfRailPstate, LW_FALSE);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                    }

                    // Element will evaluate to the MAX of all PSTATE intersect voltages.
                    vfRailElement.value = LW_MAX(vfRailElement.value, vfRailPstate.value);
                }
                BOARDOBJGRP_ITERATOR_END;

                break;
            }
            case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_VPSTATE:
            {
                BOARDOBJGRPMASK_E32 clkDomainsMask;
                PERF_LIMITS_VF vfRailVpstate;
                vfRailVpstate.idx = vfRail.idx;

                // Fetch the CLK_DOMAIN mask for the given VPSTATE.
                status = perfLimitsVpstateIdxToClkDomainsMask(
                            pElement->data.vpstateIdx.vpstateIdx,
                            &clkDomainsMask);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                }
                PMU_ASSERT_OK_OR_GOTO(status,
                    boardObjGrpMaskAnd(&clkDomainsMask, &clkDomainsMask,
                        &pArbOutput35->super.clkDomainsMask),
                    s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit);

                //
                // Trim the returned VPSTATE CLK_DOMAIN mask to the
                // intersection with set of CLK_DOMAINs which have a
                // Vmin on the specified rail.
                //
                PMU_ASSERT_OK_OR_GOTO(status,
                    boardObjGrpMaskAnd(&clkDomainsMask, &clkDomainsMask,
                        &clkDomainsProgMask),
                    s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit);

                //
                // Iterate over the VPSTATE's CLK_DOMAIN values and intersect
                // from those frequencies.
                //
                BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
                    &clkDomainsMask.super)
                {
                    PERF_LIMITS_VF      vfDomain;
                    vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);

                    // Fetch CLK_DOMAIN frequency from the VPSTATE.
                    status = perfLimitsVpstateIdxToFreqkHz(
                                pElement->data.vpstateIdx.vpstateIdx,
                                FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _MAX, _TRUE,
                                    pLimit35->super.flags),
                                LW_FALSE,
                                &vfDomain);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                    }

                    // F->V lookup for the VPSTATE's intersect frequencies.
                    status = perfLimitsFreqkHzToVoltageuV(&vfDomain,
                                &vfRailVpstate, LW_FALSE);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                    }

                    // Element will evaluate to the MAX of all PSTATE intersect voltages.
                    vfRailElement.value = LW_MAX(vfRailElement.value, vfRailVpstate.value);
                }
                BOARDOBJGRP_ITERATOR_END;

                break;
            }
            case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_FREQUENCY:
            {
                PERF_LIMITS_VF vfDomain;
                vfDomain.idx = pElement->data.frequency.clkDomainIdx;
                vfDomain.value = pElement->data.frequency.freqKHz;

                //
                // If the specified CLK_DOMAIN does not have a Vmin on
                // the rail, must skip this input.
                //
                if (!boardObjGrpMaskBitGet(&clkDomainsProgMask, vfDomain.idx))
                {
                    // CRPTODO - TBD should this be considered an error?
                    break;
                }

                // Ensure that the specified input frequency is quantized.
                status = perfLimitsFreqkHzQuantize(&vfDomain, NULL,
                            FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _MAX, _TRUE,
                                pLimit35->super.flags));
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                }

                // F -> V lookup for the specified frequency.
                status = perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRailElement, LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                }
                break;
            }
            default:
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                PMU_BREAKPOINT();
                goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
                break;
            }
        }

        // Apply ELEMENT and global deltas and quantize to VOLT_RAIL step size.
        vfRailElement.value = (LwU32)LW_MAX(0,
                                (LwS32)vfRailElement.value + pElement->deltauV +
                                        pInputVoltage->deltauV);
        status = voltRailRoundVoltage(pRail,
                    (LwS32*)&vfRailElement.value,
                    FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _MIN, _TRUE,
                        pLimit35->super.flags),
                    LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
        }

        // Save off the ELEMENT voltage for debugging APIs.
        pElement->lwrrVoltageuV = vfRailElement.value;

        // Overall voltage is MAX of all ELEMENTs.
        vfRail.value = LW_MAX(vfRail.value, vfRailElement.value);
    }
    // a. Store the VOLT_RAIL voltage in the ARBITRATION_INPUT_35 struct.
    status = PERF_LIMIT_35_ARB_INPUT_VOLT_RAIL_SET_VALUE(pArbInput35, &vfRail);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
    }
    // Save off overall voltage for debugging APIs.
    pInputVoltage->lwrrVoltageuV = vfRail.value;

    // 2. Init to the entire PSTATE range, will bound more tightly from there.
    pstateRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN] =
        boardObjGrpMaskBitIdxLowest(&(PSTATES_GET()->super.objMask));
    pstateRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX] =
        boardObjGrpMaskBitIdxHighest(&(PSTATES_GET()->super.objMask));

    //
    // 3. Build STRICT CLK_DOMAINs mask from the intersection of the
    // PERF_LIMIT's STRICT propagation mask and the set of CLK_DOMAINs
    // which have a Vmin on the given CLK_DOMAIN.
    //
    PMU_ASSERT_OK_OR_GOTO(status,
        (perfLimit35ClkDomainStrictPropagationMaskGet(pLimit35,
             &clkDomainsStrictPropagationMask)),
        s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit);
    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskAnd(&clkDomainsStrictPropagationMask,
            &clkDomainsStrictPropagationMask, &clkDomainsProgMask),
        s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit);

    //
    // 4. Loop over STRICT CLK_DOMAINs and find their intersect frequencies and
    // their corresponding PSTATE ranges.
    //
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &clkDomainsStrictPropagationMask.super)
    {
        PERF_LIMITS_VF vfDomain;
        PERF_LIMITS_PSTATE_RANGE pstateRangeDomain;
        vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);

        // a. Voltage intersect to a frequency value in any PSTATE's range and
        // store in ARBITRATION_INPUT_35.
        status = perfLimitsVoltageuVToFreqkHz(&vfRail, NULL,
                    &vfDomain, LW_TRUE, LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
        }
        status = PERF_LIMIT_35_ARB_INPUT_CLK_DOMAIN_SET_VALUE(pArbInput35, &vfDomain);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
        }

        // b. Compute PSTATE range corresponding to intersected frequency value.
        status = perfLimitsFreqkHzToPstateIdxRange(&vfDomain,
                    &pstateRangeDomain);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
        }

        //
        // c. Bound the final PSTATE range with this CLK_DOMAIN frequency's
        // PSTATE range.  Accounting for overlap based on semantics of PERF_LIMIT.
        //
        PERF_LIMITS_RANGE_RANGE_BOUND(&pstateRangeDomain, &pstateRange);
        if (!PERF_LIMITS_RANGE_IS_COHERENT(&pstateRange))
        {
            if (FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _MAX, _TRUE, pLimit35->super.flags))
            {
                pstateRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN] =
                    pstateRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX];
            }
            else
            {
                pstateRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX] =
                    pstateRange.values[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN];
            }
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // 5. Set the final PSTATE range in the ARBITRATION_INPUT_35.
    PERF_LIMIT_35_ARB_INPUT_PSTATE_SET_RANGE(pArbInput35, &pstateRange);

    //
    // 6. Trim the intersected frequencies in ARBITRATION_INPUT_35 to the final
    // PSTATE range.
    //
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &clkDomainsStrictPropagationMask.super)
    {
        PERF_LIMITS_VF_RANGE vfDomainRange;
        vfDomainRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);

        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
        {
            PERF_LIMITS_PSTATE_RANGE pstateRangeDomain;
            PERF_LIMITS_RANGE_SET_VALUE(&pstateRangeDomain, pstateRange.values[i]);

            // Query the TUPLE's PSTATE's CLK_DOMAIN frequency range.
            status = perfLimitsPstateIdxRangeToFreqkHzRange(&pstateRangeDomain, &vfDomainRange);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
            }

            // Trim the ARBITRATION_INPUT_35 TUPLE CLK_DOMAIN value to the TUPLE PSTATE's range.
            status = PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_BOUND_VALUE(
                        &pArbInput35->tuples[i], &vfDomainRange);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit;
            }
        }
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;
    }
    BOARDOBJGRP_ITERATOR_END;

s_perfLimit35ClientInputTypeToArbInput_VOLTAGE_exit:
    return status;
}

/*!
 * Propagates ARBITRATION_INPUT TUPLE CLK_DOMAIN frequency (kHz) values.
 * Propagation uses the PSTATE index and CLK_DOMAIN frequency values already set
 * in the TUPLEs to determine all the rest of the unset CLK_DOMAIN frequency
 * values.
 *
 * There are two types of propagation:
 * 1. Strict - Uses MAX(TUPLE's set CLK_DOMAINs frequency -> voltage) -> frequency
 * intersection to determine the frequency of TUPLE's unset CLK_DOMAINs.
 *
 * 2. Loose - Uses the TUPLE's PSTATE's CLK_DOMAIN frequency ranges to determine
 * the frequency of TUPLE's unset CLK_DOMAINs.
 *
 * This interface determines whether to use strict vs. loose propagation for a
 * given CLK_DOMAIN based on whether the CLK_DOMAIN's index is set in @ref
 * PERF_LIMIT_35_<XY>::clkDomainStrictPropagationMask.
 *
 * @param[in]     pLimits35     PERF_LIMITS_35 pointer
 * @param[in]     pLimit35      PERF_LIMIT_35 pointer
 * @param[in]     pArbOutput35
 *     PERF_LIMITS_ARBITRATION_OUTPUT_35 pointer.  This structure provides
 *     various input data, such as @ref
 *     PERF_LIMITS_ARBITRATION_OUTPUT_35::super.clkDomainsMask.
 * @param[out]    pArbInput35
 *     PERF_LIMIT_ARBITRATION_INPUT_35 structure into which _TUPLEs this
 *     interface will propagate CLK_DOMAIN frequency (kHz) values.
 *
 * @return FLCN_OK
 *     CLIENT_INPUT colwersion to ARBITATION_INPUT was successful.
 * @return Other errors
 *     Unexpected error encountered.
 */
static FLCN_STATUS
s_perfLimit35ArbInputClkDomainsPropagate
(
    PERF_LIMITS_35                    *pLimits35,
    PERF_LIMIT_35                     *pLimit35,
    PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35,
    PERF_LIMIT_ARBITRATION_INPUT_35   *pArbInput35
)
{
    BOARDOBJGRPMASK_E32  clkDomainsUnsetMask;
    BOARDOBJGRPMASK_E32  clkDomainStrictPropagationMask;
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx d;
    FLCN_STATUS   status = FLCN_OK;

    boardObjGrpMaskInit_E32(&clkDomainsUnsetMask);

    //
    // If no CLK_DOMAINs have been set yet, strict propagation is not possible.
    // Proceed to loose.
    //
    if (boardObjGrpMaskIsZero(&pArbInput35->clkDomainsMask))
    {
        goto s_perfLimit35ArbInputClkDomainsPropagate_loose;
    }
    //
    // 1. Voltage-intersect strict propagation.
    // Build the mask of unset CLK_DOMAINs which require strict propagation.
    //
    status = boardObjGrpMaskCopy(&clkDomainsUnsetMask, &pArbInput35->clkDomainsMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
    }
    status = boardObjGrpMaskIlw(&clkDomainsUnsetMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
    }

    status = perfLimit35ClkDomainStrictPropagationMaskGet(pLimit35,
                                                         &clkDomainStrictPropagationMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
    }

    status = boardObjGrpMaskAnd(&clkDomainsUnsetMask, &clkDomainsUnsetMask,
                &clkDomainStrictPropagationMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
    }
    // If no strict CLK_DOMAINS are unset, can proceed on to LOOSE propagation.
    if (!boardObjGrpMaskIsZero(&clkDomainsUnsetMask))
    {
        PERF_LIMITS_VF              vfDomain;
        PERF_LIMITS_VF_RANGE        vfDomainRange;
        PERF_LIMITS_PSTATE_RANGE    pstateRange;
        LwU8                        i;

        // a. Perform strict clock domain propagation.
        {
            PERF_LIMITS_VF_ARRAY    vfArrayIn;
            PERF_LIMITS_VF_ARRAY    vfArrayOut[LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS];

            // Build pstate range tuple.
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
            {
                PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pInputTuple35 =
                    &pArbInput35->tuples[i];

                if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT_40))
                {
                    switch (i)
                    {
                        //
                        // MIN TUPLE is bound to the range of PSTATEs
                        // which support the CLIENT input criteria
                        // (i.e. the PSTATE range in the MIN/MAX
                        // TUPLEs).
                        //
                        case LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN:
                        {
                            PERF_LIMIT_35_ARB_INPUT_PSTATE_GET_RANGE(pArbInput35, &pstateRange);
                            break;
                        }
                        //
                        // MAX TUPLE is bound to only the higest
                        // PSTATE (i.e. already set in the _MAX
                        // TUPLE).
                        //
                        case LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX:
                        {
                            // Default the intersection range to the already-arbitrated PSTATE.
                            PERF_LIMITS_RANGE_SET_VALUE(&pstateRange, pInputTuple35->pstateIdx);
                            break;
                        }
                        default:
                        {
                            status = FLCN_ERR_ILWALID_ARGUMENT;
                            PMU_BREAKPOINT();
                            goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
                            break;
                        }
                    }
                }
                else if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT_35_10))
                {
                    // Default the intersection range to the already-arbitrated PSTATE.
                    PERF_LIMITS_RANGE_SET_VALUE(&pstateRange, pInputTuple35->pstateIdx);
                    //
                    // If unconstrained to the PSTATE range, expand the intersection
                    // range to higher/lower pstates as applicable.
                    //
                    if (!pLimit35->bStrictPropagationPstateConstrained)
                    {
                        switch (i)
                        {
                            //
                            // MIN limits can raise the PSTATE up, so move PSTATE
                            // upper bound to the highest pstate.
                            //
                            case LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN:
                            {
                                pstateRange.values[
                                    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX] =
                                    boardObjGrpMaskBitIdxHighest(&(PSTATES_GET()->super.objMask));
                                break;
                            }
                            //
                            // MAX limits can lower the PSTATE down, so move the
                            // PSTATE lower boudn to the lowest pstate.
                            //
                            case LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX:
                            {
                                pstateRange.values[
                                    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN] =
                                    boardObjGrpMaskBitIdxLowest(&(PSTATES_GET()->super.objMask));
                                break;
                            }
                            default:
                            {
                                status = FLCN_ERR_ILWALID_ARGUMENT;
                                PMU_BREAKPOINT();
                                goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
                                break;
                            }
                        }
                    }
                }

                // Init the input/output struct.
                PERF_LIMITS_VF_ARRAY_INIT(&vfArrayIn);
                PERF_LIMITS_VF_ARRAY_INIT(&vfArrayOut[i]);

                vfArrayIn.pMask = &pArbInput35->clkDomainsMask;
                BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
                    &pArbInput35->clkDomainsMask.super)
                {
                    PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pInputTuple35 =
                        &pArbInput35->tuples[i];
                    PERF_LIMITS_VF vfDomain;

                    vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);
                    status = PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_GET_VALUE(
                                pInputTuple35, &vfDomain);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
                    }

                    vfArrayIn.values[d] = vfDomain.value;
                }
                BOARDOBJGRP_ITERATOR_END;

                vfArrayOut[i].pMask = &clkDomainsUnsetMask;

                // Perform clock frequency propagation.
                status = perfLimitsFreqPropagate(
                            &pstateRange, &vfArrayIn, &vfArrayOut[i],
                            LW2080_CTRL_CLK_CLK_PROP_TOP_IDX_ILWALID);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
                }
            }
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;

            //
            // Loop over all unset strict CLK_DOMAINs and update their TUPLES'
            // target frequencies.
            //
            BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
                &clkDomainsUnsetMask.super)
            {
                vfDomainRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);
                PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
                {
                    vfDomainRange.values[i] = vfArrayOut[i].values[d];
                }
                PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;

                //
                // Set the CLK_DOMAIN's TUPLE values from the callwlated set of
                // intersect values.
                //
                status = PERF_LIMIT_35_ARB_INPUT_CLK_DOMAIN_SET_RANGE(pArbInput35,
                    &vfDomainRange);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
                }
            }
            BOARDOBJGRP_ITERATOR_END;
        }

        //
        // b. If intersection was unconstrained to the target PSTATE range, must
        // renormalize the PSTATE and CLK_DOMAINs to ensure a coherent result
        // was found.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT_35_10) &&
            pLimit35->bStrictPropagationPstateConstrained)
        {
            goto s_perfLimit35ArbInputClkDomainsPropagate_loose;
        }
        // b1) Renormalize the PSTATE based on the intersected CLK_DOMAINs.
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
            &clkDomainsUnsetMask.super)
        {
            vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
            {
                PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pInputTuple35 =
                    &pArbInput35->tuples[i];

                status = PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_GET_VALUE(
                            pInputTuple35, &vfDomain);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
                }

                // Look up the PSTATE range for this CLK_DOMAIN's frequency.
                status = perfLimitsFreqkHzToPstateIdxRange(
                            &vfDomain, &pstateRange);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
                }

                //
                // Apply the CLK_DOMAIN's PSTATE range value to the TUPLE
                // pstateIdx with the appropriate semantics.
                //
                switch (i)
                {
                    // MIN limits -> MAX() of MINs.
                    case LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN:
                    {
                        pInputTuple35->pstateIdx = LW_MAX(pInputTuple35->pstateIdx,
                                                           pstateRange.values[i]);
                        break;
                    }
                    //
                    // MAX limits -> MIN() of MAXs.
                    //
                    case LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX:
                    {
                        pInputTuple35->pstateIdx = LW_MIN(pInputTuple35->pstateIdx,
                                                           pstateRange.values[i]);
                        break;
                    }
                    // Coverity[deadcode]
                    default:
                    {
                        //
                        // Keeping the switch present, even though it may be marked
                        // as dead code. Want this to be present in case someone
                        // updates the enumeration but doesn't add a handler here.
                        //
                        status = FLCN_ERR_ILWALID_ARGUMENT;
                        PMU_BREAKPOINT();
                        goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
                        break;
                    }
                }
            }
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;
        }
        BOARDOBJGRP_ITERATOR_END;
        //
        // b2) If new PSTATE range is incoherent (i.e. overlap), correct based
        // on MIN vs. MAX limit semantics.
        //
        PERF_LIMIT_35_ARB_INPUT_PSTATE_GET_RANGE(pArbInput35, &pstateRange);
        if (!PERF_LIMITS_RANGE_IS_COHERENT(&pstateRange))
        {
            // MAX limits choose the MAX tuple.
            if (FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _MAX, _TRUE,
                    pLimit35->super.flags))
            {
                PERF_LIMIT_35_ARB_INPUT_PSTATE_SET_VALUE(pArbInput35,
                    pstateRange.values[
                        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX]);
            }
            // MIN limits choose the MIN tuple.
            else
            {
                PERF_LIMIT_35_ARB_INPUT_PSTATE_SET_VALUE(pArbInput35,
                    pstateRange.values[
                        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN]);
            }
        }
        //
        // b3) Apply the normalized PSTATE values to all of the set CLK_DOMAIN
        // values (@ref PERF_LIMIT_ARBITRATION_INPUT_35::clkDomainsMask) -
        // i.e. floor/ceiling to the PSTATE's CLK_DOMAIN ranges.
        //
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
            &pArbInput35->clkDomainsMask.super)
        {
            vfDomainRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
            {
                PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pInputTuple35 =
                    &pArbInput35->tuples[i];

                // Look up the tuple's PSTATE's CLK_DOMAIN range.
                PERF_LIMITS_RANGE_SET_VALUE(&pstateRange, pInputTuple35->pstateIdx);
                status = perfLimitsPstateIdxRangeToFreqkHzRange(
                            &pstateRange, &vfDomainRange);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
                }

                //
                // Bound the tuple CLK_DOMAIN value to the PSTATE's CLK_DOMAIN
                // range.
                //
                status = PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_BOUND_VALUE(
                            pInputTuple35, &vfDomainRange);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
                }
            }
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;
        }
        BOARDOBJGRP_ITERATOR_END;
    }

s_perfLimit35ArbInputClkDomainsPropagate_loose:
    //
    // 2. Loose propagation for the remaining unset CLK_DOMAINs' tuple values to
    // the tuple's PSTATE's CLK_DOMAIN frequency range.
    //
    status = boardObjGrpMaskCopy(&clkDomainsUnsetMask, &pArbInput35->clkDomainsMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
    }
    status = boardObjGrpMaskIlw(&clkDomainsUnsetMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
    }
    status = boardObjGrpMaskAnd(&clkDomainsUnsetMask, &clkDomainsUnsetMask,
                &pArbOutput35->super.clkDomainsMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
    }
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &clkDomainsUnsetMask.super)
    {
        PERF_LIMITS_VF_RANGE vfDomainRange;
        LwU8 i;

        vfDomainRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);

        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
        {
            PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE *pInputTuple35 =
                &pArbInput35->tuples[i];
            PERF_LIMITS_PSTATE_RANGE pstateRange;
            PERF_LIMITS_VF_RANGE     vfDomainPstateRange;

            //
            // Pack the pstate range to the tuple's PSTATE.  Will be used to
            // retrieve the frequency range below.
            //
            PERF_LIMITS_RANGE_SET_VALUE(&pstateRange, pInputTuple35->pstateIdx);

            // Look up the frequency range corresponding to the PSTATE range.
            vfDomainPstateRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);
            status = perfLimitsPstateIdxRangeToFreqkHzRange(
                        &pstateRange, &vfDomainPstateRange);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
            }

            vfDomainRange.values[i] = vfDomainPstateRange.values[i];
        }
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;

        status = PERF_LIMIT_35_ARB_INPUT_CLK_DOMAIN_SET_RANGE(pArbInput35, &vfDomainRange);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimit35ArbInputClkDomainsPropagate_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

s_perfLimit35ArbInputClkDomainsPropagate_exit:
    return status;
}

/*!
 * Applys a PERF_LIMIT_35 object's ARBITRATION_INPUT to the ARBITRATION_OUTPUT
 * structure.  This applies the ARBITRATION_INPUT's min and max tuples to the
 * ARBITRATION_OUTPUT's tuples - i.e. max of mins, min of maxes and arbitrating
 * min vs. max overlaps based on priority.
 *
 * @param[in]     pLimits35     PERF_LIMITS_35 pointer
 * @param[in]     pLimit35      PERF_LIMIT_35 pointer
 * @param[in/out] pArbOutput35
 *     PERF_LIMITS_ARBITRATION_OUTPUT_35 pointer.  This structure provides
 *     various input data, such as @ref
 *     PERF_LIMITS_ARBITRATION_OUTPUT_35::super.clkDomainsMask.  The
 *     ARBITRATION_OUTPUT tuples will be updated by the application of the
 *     PERF_LIMIT_35's ARBITRATION_INPUT.
 *
 * @return FLCN_OK
 *     ARBITRATION_INPUT application to ARBITATION_OUTPUT was successful.
 * @return Other errors
 *     Unexpected error encountered.
 */
static FLCN_STATUS
s_perfLimit35ArbInputToArbOutput
(
    PERF_LIMITS_35                    *pLimits35,
    PERF_LIMIT_35                     *pLimit35,
    PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35
)
{
    PERF_LIMIT_ACTIVE_DATA_35              *pActiveData35;
    PERF_LIMIT_ARBITRATION_INPUT_35        *pArbInput35;
    PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE  *pInputTuple35;
    BOARDOBJGRPMASK_E32                     clkDomainStrictPropagationMask;
    CLK_DOMAIN   *pDomain;
    VOLT_RAIL    *pRail;
    LwBoardObjIdx d;
    LwBoardObjIdx r;
    FLCN_STATUS   status = FLCN_OK;

    if (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID == pLimit35->super.activeDataIdx)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_perfLimit35ArbInputToArbOutput_exit;
    }

    pActiveData35 = (PERF_LIMIT_ACTIVE_DATA_35 *)perfLimitsActiveDataGet(
            &pLimits35->super, pLimit35->super.activeDataIdx);
    pArbInput35   = &pActiveData35->arbInput;

    status = perfLimit35ClkDomainStrictPropagationMaskGet(pLimit35,
                                                         &clkDomainStrictPropagationMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimit35ArbInputToArbOutput_exit;
    }

    //
    // If a MIN PERF_LIMIT, apply the _MIN ARBITRATION_INPUT tuple to
    // ARBITRATION_OUTPUT tuples.
    //
    if (FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _MIN, _TRUE,
            pLimit35->super.flags))
    {
        pInputTuple35 = &pArbInput35->tuples[
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN];

        // a. pstateIdx
        PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MAX(pArbOutput35, pstateIdx,
            pInputTuple35->pstateIdx, BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));

        // b. clkDomains[]
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
            &pArbInput35->clkDomainsMask.super)
        {
            PERF_LIMITS_VF vfDomain;

            vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);
            status = PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_GET_VALUE(
                        pInputTuple35, &vfDomain);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_perfLimit35ArbInputToArbOutput_exit;
            }

            // 1) Target frequency value.
            PMU_ASSERT_OK_OR_GOTO(status,
                PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_BOUND(
                    pArbOutput35, &vfDomain,
                    PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_KHZ,
                    LW_FALSE, pLimit35),
                s_perfLimit35ArbInputToArbOutput_exit);

            // 2) If frequency is noise-unaware, apply to noise-unaware minimum.
            {
                LwBool bNoiseUnaware = LW_FALSE;

                status = perfLimitsFreqkHzIsNoiseUnaware(
                            &vfDomain, &bNoiseUnaware);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ArbInputToArbOutput_exit;
                }
                if ((bNoiseUnaware) ||
                    (pLimit35->bForceNoiseUnawareStrictPropgation &&
                     boardObjGrpMaskBitGet(&clkDomainStrictPropagationMask, d)))
                {
                    PMU_ASSERT_OK_OR_GOTO(status,
                        PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_BOUND(
                            pArbOutput35, &vfDomain,
                            PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_NOISE_UNAWARE_KHZ,
                            LW_FALSE, pLimit35),
                        s_perfLimit35ArbInputToArbOutput_exit);
                }
            }
        }
        BOARDOBJGRP_ITERATOR_END;

        //
        // c. voltRails[] - Loose voltage mins.  Store the maximum loose voltage
        // minimum in
        // LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35::voltageMinLooseuV.
        //
        if (boardObjGrpMaskIsZero(&clkDomainStrictPropagationMask))
        {
            BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, r,
                &pArbInput35->voltRailsMask.super)
            {
                PERF_LIMITS_VF vfRail;

                vfRail.idx = BOARDOBJ_GRP_IDX_TO_8BIT(r);
                status = PERF_LIMIT_35_ARB_INPUT_TUPLE_VOLT_RAIL_GET_VALUE(
                            pInputTuple35, &vfRail);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ArbInputToArbOutput_exit;
                }

                PMU_ASSERT_OK_OR_GOTO(status,
                    PERF_LIMITS_ARB_OUTPUT_35_TUPLES_VOLT_RAIL_BOUND(
                        pArbOutput35, &vfRail,
                        PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM_VOLTAGE_MIN_LOOSE_UV,
                        LW_FALSE, pLimit35),
                    s_perfLimit35ArbInputToArbOutput_exit);
            }
            BOARDOBJGRP_ITERATOR_END;
        }
    }
    //
    // If a MAX PERF_LIMIT, apply the _MAX ARBITRATION_INPUT tuple to
    // ARBITRATION_OUTPUT tuples.
    //
    if (FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _MAX, _TRUE,
            pLimit35->super.flags))
    {
        pInputTuple35 = &pArbInput35->tuples[
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX];

        // a. pstateIdx
        PERF_LIMITS_35_ARB_OUTPUT_TUPLES_MIN(pArbOutput35, pstateIdx,
            pInputTuple35->pstateIdx, BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));

        // b. clkDomains[]
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
            &pArbInput35->clkDomainsMask.super)
        {
            PERF_LIMITS_VF vfDomain;

            vfDomain.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);
            status = PERF_LIMIT_35_ARB_INPUT_TUPLE_CLK_DOMAIN_GET_VALUE(
                        pInputTuple35, &vfDomain);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_perfLimit35ArbInputToArbOutput_exit;
            }

            // 1) Target frequency value.
            PMU_ASSERT_OK_OR_GOTO(status,
                PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_BOUND(
                    pArbOutput35, &vfDomain,
                    PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_KHZ,
                    LW_TRUE, pLimit35),
                s_perfLimit35ArbInputToArbOutput_exit);

            //
            // 2) Noise-unaware handling.  If MAX ARBITRATION_INPUT TUPLE
            // overlaps, must override either ARBITRATION_OUTPUT TUPLE
            // regardless of whether the frequency is noise-unaware.
            //
            PMU_ASSERT_OK_OR_GOTO(status,
                PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_BOUND(
                    pArbOutput35, &vfDomain,
                    PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_NOISE_UNAWARE_KHZ,
                    LW_TRUE, pLimit35),
                s_perfLimit35ArbInputToArbOutput_exit);

            //
            // a) MAX ARBITRATION_OUTPUT Tuple: If ARBITRATION_INPUT frequency
            // equal to the ARBITRATION_OUTPUT frequency and is noise-unaware,
            // include it in the noise-unaware min frequency.
            //
            {
                PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35
                    *pOutputTuple35 = &pArbOutput35->tuples[
                        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX];
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
                   *pTuple35ClkDomain;

                //
                // Retrieve the
                // LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35f
                // pointer.
                //
                PMU_ASSERT_OK_OR_GOTO(status,
                    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35_GET_CLK_DOMAIN(
                        pOutputTuple35, d, &pTuple35ClkDomain),
                    s_perfLimit35ArbInputToArbOutput_exit);

                if (pTuple35ClkDomain->super.freqkHz.value ==
                        vfDomain.value)
                {
                    LwBool bNoiseUnaware = LW_FALSE;

                    status = perfLimitsFreqkHzIsNoiseUnaware(
                                &vfDomain, &bNoiseUnaware);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                        goto s_perfLimit35ArbInputToArbOutput_exit;
                    }
                    if (bNoiseUnaware)
                    {
                        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MAX(
                            &pTuple35ClkDomain->freqNoiseUnawareMinkHz,
                            vfDomain.value, BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));
                    }
                }
            }
        }
        BOARDOBJGRP_ITERATOR_END;

        //
        // c. voltRails[] - Loose voltage maximum applied to the loose voltage
        // min here (handling cases of overlap).  However, the loose voltage
        // maximum must also be handled within @ref
        // s_perfLimits35ArbOutputVoltages() with respect to PERF_LIMIT priority.
        //
        if (boardObjGrpMaskIsZero(&clkDomainStrictPropagationMask) &&
            !boardObjGrpMaskIsZero(&pArbInput35->voltRailsMask))
        {
            PERF_LIMIT                      *pLimit;
            PERF_LIMIT_ARBITRATION_INPUT_35 *pArbInputBelow35;
            BOARDOBJGRPMASK_E255 limitMaskBelow;
            LwBoardObjIdx l;

            boardObjGrpMaskInit_E255(&limitMaskBelow);
            for (l = 0; l < BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super); ++l)
            {
                boardObjGrpMaskBitSet(&limitMaskBelow, l);
            }
            boardObjGrpMaskAnd(&limitMaskBelow, &limitMaskBelow,
                &pLimits35->super.limitMaskActive);

            BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, r,
                &pArbInput35->voltRailsMask.super)
            {
                PERF_LIMITS_VF vfRail;

                vfRail.idx = BOARDOBJ_GRP_IDX_TO_8BIT(r);
                status = PERF_LIMIT_35_ARB_INPUT_TUPLE_VOLT_RAIL_GET_VALUE(
                            pInputTuple35, &vfRail);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ArbInputToArbOutput_exit;
                }

                PMU_ASSERT_OK_OR_GOTO(status,
                    PERF_LIMITS_ARB_OUTPUT_35_TUPLES_VOLT_RAIL_BOUND(
                        pArbOutput35, &vfRail,
                        PERF_LIMITS_ARB_OUTPUT_TUPLE_35_VOLT_RAIL_ENUM_VOLTAGE_MIN_LOOSE_UV,
                        LW_TRUE, pLimit35),
                    s_perfLimit35ArbInputToArbOutput_exit);

                //
                // Propagate the loose voltage maximum to all the lower
                // priority limits.
                //
                BOARDOBJGRP_ITERATOR_BEGIN(PERF_LIMIT, pLimit, l, &limitMaskBelow.super)
                {
                    PERF_LIMIT_35 *pLimitBelow35 = (PERF_LIMIT_35 *)pLimit;
                    pActiveData35 = (PERF_LIMIT_ACTIVE_DATA_35 *)perfLimitsActiveDataGet(
                                        &pLimits35->super, pLimitBelow35->super.activeDataIdx);
                    pArbInputBelow35 = &pActiveData35->arbInput;

                    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MIN(
                        &pArbInputBelow35->voltageMaxLooseuV[r], vfRail.value,
                        BOARDOBJ_GET_GRP_IDX(&pLimit35->super.super));
                }
                BOARDOBJGRP_ITERATOR_END;
            }
            BOARDOBJGRP_ITERATOR_END;
        }
    }

    // Sanity check ARBITRATION_OUTPUT for coherency.
    {
        PERF_LIMITS_PSTATE_RANGE pstateRange;
        PERF_LIMITS_35_ARB_OUTPUT_TUPLES_GET_RANGE(pArbOutput35, pstateIdx,
            &pstateRange);

        // 1. Pstate range coherent - max >= min.
        if (!PERF_LIMITS_RANGE_IS_COHERENT(&pstateRange))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto s_perfLimit35ArbInputToArbOutput_exit;
        }

        // 2.  For each CLK_DOMAIN confirm the following:
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
            &pArbOutput35->super.clkDomainsMask.super)
        {
            PERF_LIMITS_VF_RANGE vfDomainRange;
            PERF_LIMITS_VF_RANGE vfDomainNoiseUnawareMinRange;
            LwU8                 i;

            vfDomainRange.idx = vfDomainNoiseUnawareMinRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);

            PMU_ASSERT_OK_OR_GOTO(status,
                PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_GET_RANGE(
                    pArbOutput35, &vfDomainRange,
                    PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_KHZ),
                s_perfLimit35ArbInputToArbOutput_exit);
            PMU_ASSERT_OK_OR_GOTO(status,
                PERF_LIMITS_ARB_OUTPUT_35_TUPLES_CLK_DOMAIN_GET_RANGE(
                    pArbOutput35, &vfDomainNoiseUnawareMinRange,
                    PERF_LIMITS_ARB_OUTPUT_TUPLE_35_CLK_DOMAIN_ENUM_FREQ_NOISE_UNAWARE_KHZ),
                s_perfLimit35ArbInputToArbOutput_exit);

            // a. Target freq tuples are within PSTATE range
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
            {
                PERF_LIMITS_PSTATE_RANGE pstateRangeCheck;
                PERF_LIMITS_VF_RANGE     vfDomainRangeCheck;

                PERF_LIMITS_RANGE_SET_VALUE(&pstateRangeCheck, pstateRange.values[i]);
                vfDomainRangeCheck.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);

                // Pull out the TUPLE's PSTATE's CLK_DOMAIN range.
                status = perfLimitsPstateIdxRangeToFreqkHzRange(
                            &pstateRangeCheck, &vfDomainRangeCheck);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimit35ArbInputToArbOutput_exit;
                }

                //
                // Ensure that the target frequency is within TUPLE's PSTATE's
                // CLK_DOMAIN range.
                //
                if (!PERF_LIMITS_RANGE_CONTAINS_VALUE(&vfDomainRangeCheck,
                        vfDomainRange.values[i]))
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto s_perfLimit35ArbInputToArbOutput_exit;
                }
            }
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;

            // b. Target freq range is coherent.
            if (!PERF_LIMITS_RANGE_IS_COHERENT(&vfDomainRange))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
                goto s_perfLimit35ArbInputToArbOutput_exit;
            }

            // c. Noise-unaware min freq range is coherent
            if (!PERF_LIMITS_RANGE_IS_COHERENT(&vfDomainNoiseUnawareMinRange))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_STATE;
                goto s_perfLimit35ArbInputToArbOutput_exit;
            }

            // d. For each TUPLE: target freq >= noise-unaware min freq.
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
            {
                if (vfDomainRange.values[i] < vfDomainNoiseUnawareMinRange.values[i])
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto s_perfLimit35ArbInputToArbOutput_exit;
                }
            }
            PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;
        }
        BOARDOBJGRP_ITERATOR_END;
    }

s_perfLimit35ArbInputToArbOutput_exit:
    return status;
}

/*!
 * Helper function to compute the ARBITRATION_OUTPUT_TUPLE_35 voltages for the
 * tuples in the ARBITRATION_OUTPUT_35 struct.  This function uses the
 * frequencies in the ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35 structs, as well as
 * various loose voltage values in the ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35
 * structs to compute this value.  These values will have been determined by the
 * various PERF_LIMIT_35 sturctures' ARBITRATION_INPUT_35 values.
 *
 * @note This function may only be called after processing all PERF_LIMIT_35
 * objects' ARBITRATION_INPUT_35.
 *
 * @param[in]     pLimits35    PERF_LIMITS_35 pointer
 * @param[in/out] pArbOutput35
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_35 struct for which to compute
 *     the voltages.
 *
 * @return FLCN_OK
 *     ARBITRATION_OUTPUT_35 voltages successfully computed.
 * @return Other errors
 *     Unexpected error encountered.
 */
static FLCN_STATUS
s_perfLimits35ArbOutputVoltages
(
    PERF_LIMITS_35                    *pLimits35,
    PERF_LIMITS_ARBITRATION_OUTPUT_35 *pArbOutput35
)
{
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 *pTuple35 = NULL;
    VOLT_RAIL    *pRail   = NULL;
    CLK_DOMAIN   *pDomain = NULL;
    LwBoardObjIdx r;
    LwBoardObjIdx d;
    FLCN_STATUS   status  = FLCN_OK;
    LwU8          limitIdx;
    LwU8          i;

    // Loop over min and max tuple.
    PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR(pArbOutput35, i, pTuple35)
    {
        // Compute voltages for each voltage rail.
        BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, r,
            &pArbOutput35->super.voltRailsMask.super)
        {
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35
               *pTupleVoltRail35;
            PERF_LIMITS_VF vfRail;
            BOARDOBJGRPMASK_E32 clkDomainsProgMask;

            vfRail.idx = BOARDOBJ_GRP_IDX_TO_8BIT(r);

            //
            // Build mask of CLK_DOMAINs to consider for the given
            // VOLT_RAIL's Vmin(s).  This set is determined as the
            // intersection of theset of CLK_DOMAINs which PERF_LIMIT
            // code is attempting to arbitrate with the set of
            // CLK_DOMAINs which have Vmin on the given VOLT_RAIL.
            //
            {
                BOARDOBJGRPMASK_E32 *pRailClkDomainsProgMask;
                boardObjGrpMaskInit_E32(&clkDomainsProgMask);

                pRailClkDomainsProgMask = voltRailGetClkDomainsProgMask(pRail);
                PMU_ASSERT_OK_OR_GOTO(status,
                    ((pRailClkDomainsProgMask != NULL) ? FLCN_OK : FLCN_ERR_ILWALID_STATE),
                    s_perfLimits35ArbOutputVoltages_exit);
                PMU_ASSERT_OK_OR_GOTO(status,
                    boardObjGrpMaskAnd(&clkDomainsProgMask,
                        &pArbOutput35->super.clkDomainsMask, pRailClkDomainsProgMask),
                    s_perfLimits35ArbOutputVoltages_exit);
            }

            // Retrieve the LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35 pointer.
            PMU_ASSERT_OK_OR_GOTO(status,
                PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35_GET_VOLT_RAIL(
                    pTuple35, r, &pTupleVoltRail35),
                s_perfLimits35ArbOutputVoltages_exit);

            // Init the voltage value as the loose voltage min.
            pTupleVoltRail35->super.voltageuV =
            pTupleVoltRail35->super.voltageNoiseUnawareMinuV =
                pTupleVoltRail35->voltageMinLooseuV;

            // For each CLK_DOMAIN process its various Vmins on this VOLT_RAIL.
            BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
                &clkDomainsProgMask.super)
            {
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
                   *pTupleClkDomain35;
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE
                    clkDomailwminWithLooseMaxuV;
                PERF_LIMIT_35 *pLimit35;
                PERF_LIMIT_ACTIVE_DATA_35 *pActiveData35;
                PERF_LIMIT_ARBITRATION_INPUT_35 *pArbInput35;
                PERF_LIMITS_VF vfDomain;

                vfDomain.idx   = BOARDOBJ_GRP_IDX_TO_8BIT(d);

                //
                // Retrieve the
                // LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
                // pointer.
                //
                PMU_ASSERT_OK_OR_GOTO(status,
                    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35_GET_CLK_DOMAIN(
                        pTuple35, d, &pTupleClkDomain35),
                    s_perfLimits35ArbOutputVoltages_exit);

                // Look up the V_{min} for the tuple frequency.
                vfDomain.value = pTupleClkDomain35->super.freqkHz.value;
                status = perfLimitsFreqkHzToVoltageuV(
                            &vfDomain, &vfRail, LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimits35ArbOutputVoltages_exit;
                }

                //
                // Bound the CLK_DOMAIN V_{min} by all higher priority loose
                // voltage maxes.
                //
                limitIdx = pTupleClkDomain35->super.freqkHz.limitIdx;
                pLimit35 = (PERF_LIMIT_35 *)PERF_PERF_LIMIT_GET(limitIdx);
                if (pLimit35 == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto s_perfLimits35ArbOutputVoltages_exit;
                }
                if (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID == pLimit35->super.activeDataIdx)
                {
                    // Sanity check that the limit is active
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto s_perfLimits35ArbOutputVoltages_exit;
                }

                pActiveData35 = (PERF_LIMIT_ACTIVE_DATA_35 *)perfLimitsActiveDataGet(
                                    &pLimits35->super, pLimit35->super.activeDataIdx);
                pArbInput35 = &pActiveData35->arbInput;
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_SET(
                    &clkDomailwminWithLooseMaxuV, vfRail.value, limitIdx);
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MIN(
                    &clkDomailwminWithLooseMaxuV,
                    pArbInput35->voltageMaxLooseuV[r].value,
                    pArbInput35->voltageMaxLooseuV[r].limitIdx);

                // Take max of all V_{mins}.
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MAX(
                    &pTupleVoltRail35->super.voltageuV,
                    clkDomailwminWithLooseMaxuV.value,
                    clkDomailwminWithLooseMaxuV.limitIdx);

                //
                // Look up the V_{min} for the tuple noise-unaware minimum
                // frequency, if applicable.
                //
                if (pTupleClkDomain35->freqNoiseUnawareMinkHz.value == 0)
                {
                    continue;
                }
                vfDomain.value = pTupleClkDomain35->freqNoiseUnawareMinkHz.value;
                status = perfLimitsFreqkHzToVoltageuV(
                            &vfDomain, &vfRail, LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfLimits35ArbOutputVoltages_exit;
                }

                //
                // Bound the CLK_DOMAIN V_{min} by all higher priority loose
                // voltage maxes.
                //
                limitIdx = pTupleClkDomain35->freqNoiseUnawareMinkHz.limitIdx;
                pLimit35 = (PERF_LIMIT_35 *)PERF_PERF_LIMIT_GET(limitIdx);
                if (pLimit35 == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto s_perfLimits35ArbOutputVoltages_exit;
                }
                if (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID == pLimit35->super.activeDataIdx)
                {
                    // Sanity check that the limit is active
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_STATE;
                    goto s_perfLimits35ArbOutputVoltages_exit;
                }

                pArbInput35 = &pActiveData35->arbInput;
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_SET(
                    &clkDomailwminWithLooseMaxuV, vfRail.value, limitIdx);
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MIN(
                    &clkDomailwminWithLooseMaxuV,
                    pArbInput35->voltageMaxLooseuV[r].value,
                    pArbInput35->voltageMaxLooseuV[r].limitIdx);

                // Take max of all V_{mins}.
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_MAX(
                    &pTupleVoltRail35->super.voltageNoiseUnawareMinuV,
                    clkDomailwminWithLooseMaxuV.value,
                    clkDomailwminWithLooseMaxuV.limitIdx);
            }
            BOARDOBJGRP_ITERATOR_END;
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    PERF_LIMITS_ARBITRATION_OUTPUT_35_TUPLE_ITERATOR_END;

    // CRPTODO - Sanity check the ARBITRATION_OUTPUT voltages

s_perfLimits35ArbOutputVoltages_exit:
    return status;
}

/*!
 * @brief Initializes the loose voltage maximum values/limit IDs for the limits.
 *
 * @param[in,out]  pLimits35  Pointer to the PERF_LIMITS structure. Initializes
 *                            the voltageMaxLooseuV values of individual
 *                            limits.
 *
 * @return FLCN_OK if the loose voltage values have been initialized;
 *         FLCN_ERR_ILWALID_STATE if an active limit does not have a
 *         valid pointer to the PERF_LIMIT_ACTIVE_DATA_35 information.
 */
static FLCN_STATUS
s_perfLimits35ArbInputLooseVoltageInit
(
    PERF_LIMITS_35 *pLimits35
)
{
    PERF_LIMIT   *pLimit;
    LwBoardObjIdx l;
    FLCN_STATUS   status = FLCN_OK;

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_LIMIT, pLimit, l,
        &pLimits35->super.limitMaskActive.super)
    {
        PERF_LIMIT_ACTIVE_DATA_35 *pActiveData35;
        VOLT_RAIL    *pRail;
        LwBoardObjIdx r;

        if (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID == pLimit->activeDataIdx)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto s_perfLimits35ArbInputLooseVoltageInit_exit;
        }

        pActiveData35 = (PERF_LIMIT_ACTIVE_DATA_35 *)perfLimitsActiveDataGet(
                            &pLimits35->super, pLimit->activeDataIdx);
        if (pActiveData35 == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto s_perfLimits35ArbInputLooseVoltageInit_exit;
        }
        BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, r,
            &pLimits35->arbOutput35Apply.super.voltRailsMask.super)
        {
            LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE_SET(
                &pActiveData35->arbInput.voltageMaxLooseuV[r],
                LW_U32_MAX, LW2080_CTRL_PERF_PERF_LIMIT_ID_NOT_SET);
        }
        BOARDOBJGRP_ITERATOR_END;
    }
    BOARDOBJGRP_ITERATOR_END;

s_perfLimits35ArbInputLooseVoltageInit_exit:
    return status;
}
