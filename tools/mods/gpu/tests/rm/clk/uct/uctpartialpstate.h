/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2014,2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCTPARTIALPSTATE_H_
#define UCTPARTIALPSTATE_H_

#include <vector>

#include "gpu/tests/rm/utility/rmtstring.h"
#include "uctexception.h"
#include "uctdomain.h"
#include "uctpstate.h"
#include "ucttestspec.h"

namespace uct
{

    //!
    //! \brief      Partial P-State Definition
    //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#Partial_P-State_Definition
    //!
    //! \details    Each partial p-state object represents an enumerated set of
    //!             modifications that can be made to a fully-defined p-state
    //!             to produce a set of one or more fully-defined p-states.
    //!
    //!             The modifications can be made to any clock (frequency) or
    //!             voltage domain.  The result set of fully-defined p-states is
    //!             computed by applying every possible combination of these
    //!             modifications (i.e. the cartesian product).  Elements in the
    //!             result set are enumerated.
    //!
    //!             For example, let's assume we have a partial p-state
    //!             specification with these settings:
    //!                 xbar2clk.freq = +10%, -10%
    //!                 ltcclk.freq = +10%, +0%, -10%
    //!
    //!             Let's assume this partial p-state were applied to a fully-
    //!             defined p-state where both xbar2clk and ltcclk were set to
    //!             1000MHz.  The result set would contain six fully-defined
    //!             p-states with these values:
    //!
    //!               index    xbar2clk      ltcclk
    //!                 0       1100MHz     1100MHz
    //!                 1        900MHz     1100MHz
    //!                 2       1100MHz     1000MHz
    //!                 3        900MHz     1000MHz
    //!                 4       1100MHz      900MHz
    //!                 5        900MHz      900MHz
    //!
    //! Order of events:
    //! - A PartialPState object is created by CtpFileReader::scanFile.
    //! - Field objects are parsed by Field::parse,
    //! - The PartialPState object gets the Field object data with Field::apply.
    //! - PStateReference::resolve is used to link the p-state references to PartialPState objects.
    //! - PartialPState::fullPState gets the fully-defined p-state definition to test.
    //!
    struct PartialPState: public TestSpec
    {
        PartialPState(): TestSpec(TestSpec::PartialPState)
        {   }

        struct FreqSetting;
        struct VoltSetting;
        struct SourceSetting;

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
                // \brief       Union of all frequency (clock) domains in this vector
                DomainBitmap freqDomainSet;

                // \brief       Union of all voltage domains in this vector
                DomainBitmap voltDomainSet;

                // \brief       Map from frequency domains to settings objects.
                rmt::Vector<FreqSetting> freqDomainMap[Domain_Count];

                // \brief       Map from frequency domains to source settings objects.
                rmt::Vector<SourceSetting> sourceDomainMap[Domain_Count];

                // \brief       Map from voltage domains to settnigs objects.
                rmt::Vector<VoltSetting> voltDomainMap[Domain_Count];

            public:

                //! \brief      Construction
                inline Vector()
                {
                    freqDomainSet =
                    voltDomainSet = DomainBitmap_None;
                }

                //! \brief      Add a frequency setting to this vector.
                void push(const FreqSetting &setting);

                //! \brief      Add a source setting to this vector
                void push(const SourceSetting &setting);

                //! \brief      Add a voltage setting to this vector.
                void push(const VoltSetting &setting);

                //! \brief      Number of elements in the cartesian product
                unsigned long long cardinality() const;

                //!
                //! \brief      Apply the 'i'th setting in the cartesian product.
                //!
                //! \details    Settings are applied to all applicable voltage
                //!             domains.
                //!
                //!             Settings are applied only to those frequency
                //!             domains for which its bit in 'freqMask' on one.
                //!             This has the effect of suppressing exceptions
                //!             for the domains that have been pruned.
                //!
                //!             To reduce memory usage, the cartesian product
                //!             is not computed in its entirety.  Only the i'th
                //!             element within the cartesian product is processed.
                //!
                //!             The scheme to enumerate the elements within the
                //!             cartesian product can best be illustrated by
                //!             example.  Assume:
                //!
                //!             mclk.freq   = 1111, 2222, 3333
                //!             hostclk.freq = 444, 555
                //!
                //!             Since LW2080_CTRL_CLK_DOMAIN_MCLK < LW2080_CTRL_CLK_DOMAIN_HOSTCLK,
                //!             the elements are enumerated thusly:
                //!                 i   mclk    hostclk
                //!                 0   1111    444
                //!                 1   2222    444
                //!                 2   3333    444
                //!                 3   1111    555
                //!                 4   2222    555
                //!                 5   3333    555
                //!
                //! param[in/out]   fullPState      P-state to modify
                //! param[in]       i               Index into the cartesian product
                //! param[in]       freqMask        Zero bits indicate pruned domains
                //!
                ExceptionList fullPState(FullPState &fullPState, unsigned long long i, FreqDomainBitmap freqMask) const;

                //! \brief      Is there a domain intersection?
                inline bool containsAnyDomain(DomainBitmap freqDomainSet, DomainBitmap voltDomainSet) const
                {
                    return !!(this->freqDomainSet & freqDomainSet ||
                              this->voltDomainSet & voltDomainSet);
                }

                //! \brief      Have any settings been added to this object?
                inline bool empty() const
                {
                    return !freqDomainSet && !voltDomainSet;
                }

                //! \brief      Human-readable representation of the objects in this vector
                rmt::String string() const;
            };

            //! \brief      Frequency (clock) or voltage domain
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
        //! \brief      Source Setting
        //!
        //! \details    Objects of this class contain the clock source to be used
        //!             the values can be LW2080_CTRL_CLK_SOURCE_xxx
        //!
        struct SourceSetting: public Setting
        {
            //!
            //! \brief      Source field
            UINT32 source;

            //! \brief      Apply the operators to the full p-state.
            virtual SolverException fullPState(FullPState &fullPState) const;

            //! \brief      Human-readable representation of this object
            virtual rmt::String string() const;
        };

        //!
        //! \brief      Frequency and Flag Setting
        //!
        //! \details    Objects of this class contain the operators to be applied
        //!             to the frequency (clock) domain of a fully-defined p-state.
        //!             Specifically, each contains a 'NumericOperator' to for
        //!             the frequency and a 'FlagOperator' for the flags.
        //!
        struct FreqSetting: public Setting
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
        struct VoltSetting: public Setting
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
        //! \brief      Frequency/Flag Setting
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#freq_Field
        //!
        //! \details    This field indicates the frequencies to test for specific
        //!             clocks domain.
        //!
        //! Syntax:     <freq-field>     ::= { <freq-domain> "." } <freq-domain> ".freq="
        //!                                  <freq-spec> { "," <freq-spec> }
        //! Where:      <freq-domain> is one of the clock domains
        //!             <freq-spec>      ::= ( "+" | "-" ) <number> [ <freq-units> | "%" ]
        //!                                  { ":" <freq-flag-spec> }
        //!             <freq-units>     ::= "Hz" | "KHz" | "MHz" | "GHz"
        //!             <freq-flag-spec> ::= ( "+" | "-" ) ( "FORCE_PLL" | "FORCE_BYPASS" )
        //!
        struct FreqField: public Field
        {
            //! \brief      Default construction
            inline FreqField(): Field()     { }

            //! \brief      Construction from a statement
            inline FreqField(const CtpStatement &statement): Field(statement) { }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the partial p-state
            virtual ExceptionList apply(PartialPState &pstate) const;

            //! \brief      Simple human-readable text
            rmt::String string() const;
        };

        //!
        //! \brief      Voltage Setting
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#volt_Field
        //!
        //! \details    This field indicates the voltages to test for specific
        //!             voltage domains.  Since the domains are bitmapped,
        //!             multiple domains per field are supported.
        //!
        //! Syntax:     <volt-field>  ::= { <volt-domain> "." } <volt-domain> ".volt="
        //!                               <volt-spec> { "," <volt-spec> }
        //! Where:      <volt-domain> ::= "lwvdd" | "fbvdd"
        //!             <volt-spec>   ::= ( "+" | "-" ) <number> [ <clock-freq-units> | "%" ]
        //!             <volt-units>  ::= "uV" | "mV" | "V"
        //!
        struct VoltField: public Field
        {
            //! \brief      Default construction
            inline VoltField(): Field()     { }

            //! \brief      Construction from a statement
            inline VoltField(const CtpStatement &statement): Field(statement) { }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the partial p-state
            virtual ExceptionList apply(PartialPState &pstate) const;

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

