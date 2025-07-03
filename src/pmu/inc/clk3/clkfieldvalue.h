/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2019 by LWPU Corporation.  All rights reserved.  All
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

#ifndef CLK3_FIELDVALUE_H
#define CLK3_FIELDVALUE_H

#ifndef CLK_SD_CHECK        // See pmu_sw/prod_app/clk/clk3/clksdcheck.c
#include "pmusw.h"
#endif                      // CLK_SD_CHECK

#include "lwtypes.h"


/*******************************************************************************
    Field Value Structure
*******************************************************************************/

/*!
 * @brief       Field value with mask
 *
 * @details     Objects of this class encode the value and position of one or
 *              more fields within a register.
 *
 *              For example, for the LW_PTRIM_SYS_CLK_SWITCH register and the
 *              _FINALSEL field, the _ONESRCCLK value can be encoded as an object
 *              where:
 *              mask  = DRF_SHIFTMASK(LW_PTRIM_SYS_CLK_SWITCH_FINALSEL);
 *              value = DRF_DEF(_PTRIM, _SYS_CLK_SWITCH, _FINALSEL, _ONESRCCLK);
 *
 *              The values of multiple fields are possible by using the bitwise
 *              OR operator.  See ClkOsm1 or ClkOsm3 for examples.
 *
 *              Although the individual members of this class are neither final
 *              nor const, objects of this class generally const.
 *
 * @todo        Consider making ClkBitFieldValue which has the same functionality
 *              as ClkFieldValue, but saves memory by working only with single-
 *              bit (boolean) fields.  We need only five bits for the field
 *              position and one bit for the value.
 */
typedef struct ClkFieldValue
{
/*!
 * @brief       Mask indicating bit positions for the field(s)
 *
 * @details     On-bits correspond to the position of the field(s) and are
 *              generally initialized by one or more DRF_SHIFTMASK constants
 *              ORed together.
 *
 * @note        public:         Functions of any class may read its value.
 * @note        final:          Value does not change after construction.
 */
    LwU32       mask;

/*!
 * @brief       Value for the field(s)
 *
 * @details     For the position of field(s) in 'mask', the bits in this member
 *              indicate the applicable value(s).  Generally, this member is
 *              initialized by one or more DRF_DEF or DRF_NUM constant ORed
 *              together.
 *
 *              Insigificant bits (i.e. those that correspond to off-bits in
 *              'mask') should be off in 'value'.
 *
 * @note        public:         Functions of any class may read or change its value.
 */
    LwU32       value;

} ClkFieldValue;


/*******************************************************************************
    Field Value Macros
*******************************************************************************/

/*!
 * @memberof    ClkFieldValue
 * @brief       True iff register data matches the field value
 *
 * @note        public:         Any function may call this function.
 * @note        macro:          Arguments may be evaluated more than once.
 *
 * @param[in]   this            ClkFieldValue object
 * @param[in]   data            Register data
 *
 * @retval      LW_TRUE         The field value is represented in the register data.
 */
#define CLK_MATCHES__FIELDVALUE(this, data)             \
    (((data) & (this).mask) == (this).value)

/*!
 * @memberof    ClkFieldValue
 * @brief       Apply the field value to the register data.
 *
 * @note        public:         Any function may call this function.
 * @note        macro:          Arguments may be evaluated more than once.
 *
 * @param[in]   this            ClkFieldValue object
 * @param[in]   data            Original register data
 *
 * @return      'data' with the field value applied.
 */
#define CLK_APPLY__FIELDVALUE(this, data)               \
    (((data) & ~(this).mask) | (this).value)

/*!
 * @memberof    ClkFieldValue
 * @brief       Apply the field value to the register data.
 *
 * @note        public:         Any function may call this function.
 * @note        macro:          Arguments may be evaluated more than once.
 *
 * @param[in]   this            ClkFieldValue object
 * @param[in]   data            Original register data
 *
 * @return      'data' with the field value applied.
 */
#define CLK_APPLY_ILWERSE__FIELDVALUE(this, data)       \
    (((data) | (this).mask) & ~(this).value)


/*******************************************************************************
    Field Value Map
*******************************************************************************/

typedef const struct ClkFieldValue *ClkFieldValueMap;


/*******************************************************************************
    Constant Field Value Macro
*******************************************************************************/

/*!
 * @memberof    ClkFieldValue
 * @brief       Value Map for Specified Register Field Value
 *
 * @details     This macro translates the standard DRF notation for a register
 *              field value to an initializer for a 'ClkFieldValue' value.
 *
 * @note        public:         Any function may call this macro.
 * @note        macro:          Arguments may be evaluated more than once.
 *
 * @param[in]   d               Device
 * @param[in]   r               Register
 * @param[in]   f               Field location within register
 * @param[in]   v               Field value
 */
#define CLK_DRF__FIELDVALUE(d, r, f, v)                                        \
    {                                                                          \
        DRF_SHIFTMASK(LW ## d ## r ## f),                                      \
        DRF_SHIFTMASK(LW ## d ## r ## f) & DRF_DEF(d, r, f, v)                 \
    }


/*!
 * @memberof    ClkFieldValue
 * @brief       Ilwerse Value Map for Specified Register Field Value
 *
 * @details     This macro translates the standard DRF notation for a register
 *              field value to an initializer for a 'ClkFieldValue' value which
 *              flips all the bits in the value.
 *
 *              This is useful in 2x1 multiplexers where CLK_DRF__FIELDVALUE is
 *              used for zero (e.g. _BYPASS) and this macros is used for one
 *              (e.g. VCO).
 *
 * @note        public:         Any function may call this macro.
 * @note        macro:          Arguments may be evaluated more than once.
 *
 * @param[in]   d               Device
 * @param[in]   r               Register
 * @param[in]   f               Field location within register
 * @param[in]   v               Field value to be ilwerted
 */
#define CLK_DRF_ILW__FIELDVALUE(d, r, f, v)                                    \
    {                                                                          \
        DRF_SHIFTMASK(LW ## d ## r ## f),                                      \
        DRF_SHIFTMASK(LW ## d ## r ## f) & ~DRF_DEF(d, r, f, v)                \
    }


/*!
 * @memberof    ClkFieldValue
 * @brief       Value Map for Specified Register Field Value
 *
 * @details     This macro translated the standard DRF notation for a register
 *              field value to an initializer for a 'ClkFieldValue' value.
 *
 * @note        public:         Any initializer may use this macro.
 */
#define CLK_NOT_APPLICABLE__FIELDVALUE  { 0, 0 }

#endif // CLK3_FIELDVALUE_H

