/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef MODS_RM_CLK_UCT_OPERATOR_H_
#define MODS_RM_CLK_UCT_OPERATOR_H_

#include <limits.h>
#include "core/include/utility.h"
#include "uctexception.h"

namespace uct
{
    //!
    //! \brief      Flag Operator
    //!
    //! \details    Objects of this class represent operations performed on
    //!             individual bits within a bitmapped flag value.  The operations
    //!             are each one of:
    //!             - Identity (no operattion)
    //!             - Set (make the bit 1)
    //!             - Reset (make the bit 0)
    //!
    //!             As of November 2012, this operator is used only in 'freq'
    //!             fields, although there is nothing in this class that
    //!             prevents it from being used elsewhere.
    //!
    struct FlagOperator
    {
        //! \brief      Bitmap of flags
        typedef UINT32 Bitmap;

        //! \brief      Map from bits to flag names
        typedef rmt::StringBitmap32 NameMap;

        //!
        //! \brief      Bits to alter
        //!
        //! \details    For each bit, 'mask' values correspond to operations as:
        //!             - 0 => Identity
        //!             - 1 => Set or Reset
        //!
        Bitmap mask;

        //! \brief      Value applied to altered bits
        //!
        //! \details    For each bit, 'mask' values correspond to operations as:
        //!             - 0 => Identity or Reset
        //!             - 1 => Set
        //!
        Bitmap value;

        //! \brief      Construct as not-applicable
        inline FlagOperator()
        {
            mask = value = 0;
        }

        //!
        //! \brief      Interpret the text as a flag operator
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#freq_Field
        //!
        //! \details    This function initializes this flag operator object by
        //!             parsing on the text passed to it.
        //!
        //!             The text in 'flagNameMap' must be in all lowercase since
        //!             the 'text' parameter is colwerted to lowercase before
        //!             it is used to lookup the flag.
        //!
        //! Syntax:     [ '+' | '-' ] <flag-name>
        //!
        //! \param[in]  text        Text to parse
        //! \param[in]  location    File location used to instantiate exceptions
        //! \param[in]  flagNameMap Map from lowercase flag keyword to bit
        //!
        //! \todo       Consider using a case-insensitive search of 'flagNameMap'
        //!
        ExceptionList parse(rmt::String text,
            const rmt::FileLocation &location, const NameMap &flagNameMap);

        //! \brief      Apply this flag setting
        inline Bitmap apply(Bitmap flag) const
        {
            return (flag & ~mask) | (value & mask);
        }

        //!
        //! \brief      Flags as text
        //!
        //! \details    Each flag name is prefixed with a colon and either
        //!             a plus (set) or minus (reset) sign.
        //!
        inline rmt::String string(const NameMap &map) const
        {
            return map.textListWithSign(mask,value).concatenate(":", ":", NULL);
        }
    };

    //!
    //! \brief      Numeric Operator
    //!
    //! \details    Objects of this class represent an operation and a constant
    //!             which is applied to a frequency or voltage value.  The
    //!             operation is defined by subclasses.
    //!
    //!             As of November 2012, this operator is used in 'freq' and
    //!             'volt' fields, although there is nothing in this class that
    //!             prevents it from being used elsewhere.
    //!
    //!             This class is based on the prior 'UCTFreqField' and
    //!             'UCTSubField' classes.  The class model replaces prior
    //!             'FieldOp' enum.  The vector is based on prior 'UCTCtpBlock'.
    //!
    struct NumericOperator
    {
        //! \brief      Type of freq/volt in RMAPI
        typedef UINT32 value_t;

        //! \brief      Maximum value of 'value_t'
        static const value_t value_max = 0xffffffff;

        //! \brief      Reasonable range of freq/volt
        struct Range
        {
            value_t min;
            value_t max;
        };

        //! \brief      Map from unit text to exponent
        typedef rmt::StringMap<short> UnitMap;

        //!
        //! \brief      Numeric Operator or Exception
        //!
        //! \details    The pointer and exception are mutually exclusive.
        //!             That is, either the pointer is NULL or the exception
        //!             is Null, or both.  This is useful in returning either
        //!             from 'parse'.
        //!
        struct PointerOrException
        {
            //! \brief      Numeric Operator (if any)
            NumericOperator *pointer;

            //! \brief      Exception (if any)
            Exception  exception;

            //! \brief      Construct from a Numeric Operator
            inline PointerOrException(NumericOperator *pointer = NULL)
            {
                this->pointer = pointer;    // Exception is null
            }

            //! \brief      Construct from an Exception
            inline PointerOrException(const Exception &exception)
            {
                this->pointer   = NULL;
                this->exception = exception;
            }
        };

        //!
        //! \brief      Interpret the text as a numeric operator.
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#freq_Field
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#volt_Field
        //!
        //! \details    This function creates a numeric operator object by
        //!             parsing on the text passed to it.
        //!
        //!             The text in 'unitMap' must be in all lowercase since
        //!             the 'text' parameter is colwerted to lowercase before
        //!             it is used to lookup the unit.
        //!
        //! Syntax:     <numeric-oper>  ::= ( <absolute-oper> | <additive-oper> | <multiplicitive-oper> )
        //!             <absolute-oper> ::= <nonneg-integer> [ <units> ]
        //!             <additive-oper> ::= [ "+" | "-" ] <nonneg-integer> [ <units> ]
        //!             <relative-oper> ::= ( "+" | "-" ) <floating-point> [ <units> ] "%"
        //!             <nonneg-integer> is a nonnegative integer (using standard notation).
        //!             <floating-point> is a floating-point number (using standard notation).
        //!
        //! \param[in/out]  text        Text to parse; lowercase flag text on return (if any)
        //! \param[in]      location    File location used to instantiate exceptions
        //! \param[in]      unitMap     Map from lowercase unit text to exponent
        //!
        //! \todo       Consider using a case-insensitive search of 'unitMap'
        //!
        //! \todo       Consider adding support for noninteger <number> text.
        //!
        static PointerOrException parse(rmt::String &text, const UnitMap &unitMap);

        //! \brief      Destruction
        virtual ~NumericOperator();

        //! \brief      Create an exact copy of 'this'.
        virtual NumericOperator *clone() const = 0;

        //! \brief      Is it okay if the 'prior' value is missing?
        virtual bool isAbsolute() const = 0;

        //!
        //! \brief      Is the result within range?
        //!
        //! \details    The range in interpreted as semi-inclusive in that
        //!             this function returns true when the return value of
        //!             'apply' a will be min < a <= max.
        //!
        //!             This function is not exactly the same as calling 'apply'
        //!             and comparing the return value to 'range' because
        //!             this function handles overflow/underflow, but 'apply'
        //!             may or may not.
        //!
        virtual bool validRange(value_t prior, Range range) const = 0;

        //! \brief      Compute the new value based on the prior value
        virtual value_t apply(value_t prior) const = 0;

        //! \brief      Subfield without flags as text
        virtual rmt::String string() const = 0;
    };

    //!
    //! \brief      Absolute Numeric Value
    //!
    //! \details    When aplied, this operator replaces the frequency/voltage
    //!             value with a constant ('value').  The prior value is ignored.
    //!
    struct AbsoluteNumericOperator: public NumericOperator
    {
        //! \brief      Constant Value
        value_t         value;

        //! \brief      Create an exact copy of 'this'.
        virtual NumericOperator *clone() const;

        //! \brief      true because we don't use the 'prior' value.
        virtual bool isAbsolute() const;

        //! \brief      Is the result within range?
        virtual bool validRange(value_t prior, Range range) const;

        //!
        //! \brief      Field value
        //!
        //! \param[in]  prior       Unused in this implementation
        //!
        virtual value_t apply(value_t prior) const;

        //! \brief      Subfield without flags as text
        virtual rmt::String string() const;
    };

    //!
    //! \brief      Additive (Relative) Numeric Value
    //!
    //! \details    When aplied, this operator adds a constant ('addend') to
    //!             the prior frequency/voltage value.
    //!
    struct AdditiveNumericOperator: public NumericOperator
    {
        //! \brief      Type of 'addend'
        typedef long addend_t;

        //! \brief      Amount to add (or subtract)
        addend_t        addend;

        //! \brief      Maximum value of 'addend'
        static const addend_t addend_max = MIN(value_max, LONG_MAX);

        //! \brief      Minimum value of 'addend'
        static const addend_t addend_min = -addend_max;

        //! \brief      Create an exact copy of 'this'.
        virtual NumericOperator *clone() const;

        //! \brief      false because we need the 'prior' value.
        virtual bool isAbsolute() const;

        //! \brief      Is the result within range?
        virtual bool validRange(value_t prior, Range range) const;

        //! \brief      Prior value plus addend
        virtual value_t apply(value_t prior) const;

        //! \brief      Subfield without flags as text
        virtual rmt::String string() const;
    };

    //!
    //! \brief      Multiplicative (Relative) Numeric Value
    //!
    //! \details    When aplied, this operator multiplies the prior
    //!             frequency/voltage value by a constant ('factor').
    //!
    struct MultiplicativeNumericOperator: public NumericOperator
    {
        //! \brief      Multiplicative factor
        double          factor;

        //! \brief      Create an exact copy of 'this'.
        virtual NumericOperator *clone() const;

        //! \brief      false because we need the 'prior' value.
        virtual bool isAbsolute() const;

        //! \brief      Is the result within range?
        virtual bool validRange(value_t prior, Range range) const;

        //! \brief      Prior value times factor
        virtual value_t apply(value_t prior) const;

        //! \brief      Subfield without flags as text
        virtual rmt::String string() const;
    };
}

#endif /* MODS_RM_CLK_UCT_OPERATOR_H_ */

