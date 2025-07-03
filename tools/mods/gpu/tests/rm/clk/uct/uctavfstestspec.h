/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCTAVFSTESTSPEC_H_
#define UCTAVFSTESTSPEC_H_

#include <vector>

#include "gpu/tests/rm/utility/rmtstring.h"

#include "uctexception.h"
#include "uctdomain.h"
#include "uctpstate.h"
#include "ucttestspec.h"

namespace uct
{

    //!
    //! \brief      AVFS AdcVolt Test Specifications
    //! \see        TBD - https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#Partial_P-State_Definition
    //!
    //! \details    TBD
    //!
    //!
    //!
    //!
    //!
    //! Order of events:
    //! -
    //!
    struct AvfsTestSpec: public TestSpec
    {
        AvfsTestSpec():TestSpec(TestSpec::AvfsTestSpec)
        {   }

        struct LutFreqSetting;
        struct AdcVoltSetting;

        //!
        //! \brief      Base class for 'FreqSetting' and 'VoltSetting'
        //! \see        NumericOperator
        //!
        //! \details    Objects of this class contain a 'NumericOperator' to
        //!             be applied to either a frequency or voltage value.
        //!             This class also defines the protocol ('fullPState') for
        //!             applying this and other operators to a fully-defined
        //!             p-state.
        //!
        struct Setting
        {
            //!
            //! \brief      Frequency/Voltage Setting Vectors
            //!
            //! \details    Objects of this class actually contain two vectors:
            //!             one for the frequency/flag settings ('FreqSetting')
            //!             and one for voltage settings ('VoltSetting').
            //!
            class Vector
            {
            protected:
                // \brief       Union of all AdcVolt clock domains in this vector
                DomainBitmap adcvoltDomainSet;

                // \brief       Map from AdcVolt domains to settings objects.
                rmt::Vector<LutFreqSetting> lutfreqDomainMap[Domain_Count];

                rmt::Vector<AdcVoltSetting> adcvoltDomainMap[Domain_Count];

            public:

                //! \brief      Construction
                inline Vector()
                {
                    adcvoltDomainSet = DomainBitmap_None;
                }

                //! \brief      Add a frequency setting to this vector.
                void push(const LutFreqSetting &setting);

                //! \brief      Add a voltage setting to this vector.
                void push(const AdcVoltSetting &setting);

                //! \brief      Number of elements in a non-empty domain
                unsigned long long cardinality() const;

                //!
                //! \brief      Apply the 'i'th setting.
                //!
                //! \details    Settings are applied to all applicable voltage
                //!             domains.
                //!
                //! param[in/out]   fullPState      P-state to modify
                //! param[in]       i               Index into the cardinality
                //! param[in]       freqMask        Zero bits indicate pruned domains
                //!
                ExceptionList fullPState(FullPState &fullPState, unsigned long long i, FreqDomainBitmap freqMask) const;

                //! \brief      Have any settings been added to this object?
                inline bool empty() const
                {
                    return !adcvoltDomainSet;
                }

                //! \brief      Human-readable representation of the objects in this vector
                rmt::String string() const;
            };

            //! \brief      AdcVolt clock domain
            Domain          domain;

            //! \brief      Numeric operator
            rmt::ConstClonePointer<NumericOperator> numeric;

            //! \brief      Destructor
            virtual ~Setting();

            //! \brief      Apply the operator(s) to the full p-state.
            virtual SolverException fullPState(FullPState &fullPState) const = 0;

            //! \brief      Human-readable representation of this object
            virtual rmt::String string() const = 0;
        };

        //!
        //! \brief      Frequency and Flag Setting
        //!
        //! \details    Objects of this class contain the operators to be applied
        //!             to the frequency (clock) domain of a fully-defined p-state.
        //!             Specifically, each contains a 'NumericOperator' to for
        //!             the frequency and a 'FlagOperator' for the flags.
        //!
        struct LutFreqSetting: public Setting
        {
            //!
            //! \brief      Absolute Frequency Range
            //! \see        fullPState
            //!
            //! \details    Tests are skipped when the frequency falls outside
            //!             this range.
            //!
            static const NumericOperator::Range range;

            //! \brief      Frequency Text
            static rmt::String dec(NumericOperator::value_t kilohertz);

            //! \brief      Flag operator
            FlagOperator flag;

            //! \brief      Apply the operators to the full p-state.
            virtual SolverException fullPState(FullPState &fullPState) const;

            //! \brief      Human-readable representation of this object
            virtual rmt::String string() const;
        };

        //!
        //! \brief      Voltage Setting
        //!
        //! \details    Objects of this class contain the operator ('NumericOperator')
        //!             to be applied to the voltage domain of a fully-defined p-state.
        //!
        struct AdcVoltSetting: public Setting
        {
            //!
            //! \brief      Absolute Voltage Range
            //! \see        fullPState
            //!
            //! \details    Tests are skipped when the voltage falls outside
            //!             this range.
            //!
            static const NumericOperator::Range range;

            //! \brief      Voltage Text
            static rmt::String dec(NumericOperator::value_t microvolts);

            //! \brief      Apply the operator to the full p-state.
            virtual SolverException fullPState(FullPState &fullPState) const;

            //! \brief      Human-readable representation of this object
            virtual rmt::String string() const;
        };

        //!
        //! \brief      LUT Frequency Setting
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#freq_Field
        //!
        //! \details    This field indicates the frequencies to test for specific
        //!             clocks domain.
        //!
        //! Syntax:     <lutfreq-field>  ::= { <adcvolt-domain> "." } <adcvolt-domain> ".freq="
        //!                                  <freq-spec> { "," <freq-spec> }
        //! Where:      <adcvolt-domain>   ::= "gpc2clk" | "xbar2clk" | "ltc2clk" | "sys2clk"
        //!             <freq-spec>      ::= ( "+" | "-" ) <number> [ <freq-units> | "%" ]
        //!                                  { ":" <freq-flag-spec> }
        //!             <freq-units>     ::= "Hz" | "KHz" | "MHz" | "GHz"
        //!
        struct LutFreqField: public Field
        {
            //! \brief      Default construction
            inline LutFreqField(): Field()     { }

            //! \brief      Construction from a statement
            inline LutFreqField(const CtpStatement &statement): Field(statement) { }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the partial p-state
            virtual ExceptionList apply(AvfsTestSpec &avfstestspec) const;

            //! \brief      Simple human-readable text
            rmt::String string() const;
        };

        //!
        //! \brief      ADC Voltage Setting
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#volt_Field
        //!
        //! \details    This field indicates the voltages to test for specific
        //!             AdcVolt enabled voltage domains.  Since the domains are bitmapped,
        //!             multiple domains per field are supported.
        //!
        //! Syntax:     <adcvolt-field>  ::= { <adcvolt-domain> "." } <adcvolt-domain> ".adcvolt="
        //!                               <volt-spec> { "," <volt-spec> }
        //! Where:      <adcvolt-domain>   ::= "gpc2clk" | "xbar2clk" | "ltc2clk" | "sys2clk"
        //!             <volt-spec>      ::= ( "+" | "-" ) <number> [ <volt-units> | "%" ]
        //!             <volt-units>     ::= "uV" | "mV" | "V"
        //!
        struct AdcVoltField: public Field
        {
            //! \brief      Default construction
            inline AdcVoltField(): Field()     { }

            //! \brief      Construction from a statement
            inline AdcVoltField(const CtpStatement &statement): Field(statement) { }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the partial p-state
            virtual ExceptionList apply(AvfsTestSpec &avfstestspec) const;

            //! \brief      Simple human-readable text
            rmt::String string() const;
        };

        //! \brief      Frequency and voltage settings
        Setting::Vector settingVector;

        //!
        //! \brief      Can this pstate can be supported by PSTATE RMAPI?
        //!
        //! \details    This is important because if there is a clock/voltage
        //!             domain in this pstate class, then setting the pstate
        //!             using RM will fail
        //!
        //! \param[in]  vbios   Fully-defined (VBIOS) pstate to be used to see
        //!                     which domains are supported
        //!
        //! \retval     true    This pstate is supported by RM's SET_PSTATE API.
        //!
        bool supportPStateMode(const VbiosPState &vbios) const;

        //!
        //! \brief      Number of subfield combinations
        //!
        //! \details    This function returns the cardinality of the cartesian
        //!             product of all frequency and voltage subfields.  In
        //!             effect, this is the same as the number of fully-defined
        //!             p-states that would result from applying this partially-
        //!             defined p-state to one fully-defined p-state.
        //!
        unsigned long long cardinality() const
        {
            return settingVector.cardinality();
        }

        //!
        //! \brief      Modify the full p-state
        //!
        //! \details    Compute the 'i'th fully-defined p-state in the result
        //!             set using the specified p-state as the basis.
        //!
        ExceptionList fullPState(FullPState &fullPState, unsigned long long i, FreqDomainBitmap freqMask) const;

        //! \brief      Human-readable representation of this object
        rmt::String debugString(const char *info = NULL) const;
    };
}
#endif

