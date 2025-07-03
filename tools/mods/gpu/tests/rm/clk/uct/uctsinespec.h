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

#ifndef UCTSINESPEC_H_
#define UCTSINESPEC_H_

#include <vector>

#include "gpu/tests/rm/utility/rmtstring.h"

#include "uctexception.h"
#include "uctdomain.h"
#include "uctpstate.h"
#include "ucttestspec.h"

namespace uct
{

    //!
    //! \brief      Sine Spec Definition
    //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#Sine_Spec_Definition
    //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Features#Sine_Test
    //!
    //! \details    Each sine spec object represents a set of domains to be modified
    //!             with frequency values computed based on the function given in the wiki
    //!
    //!             For each domain specified in a sine spec, the frequency value for that
    //!             domain will be computed by a elweloped sine function:
    //!                 f = alpha + beta * i^omega * sin(gamma * i)
    //!
    //!             Where:
    //!                 - f is the computed frequency at test iteration i, to be applied to
    //!                      domain specified in the sine spec
    //!                 - alpha is the initial frequency of the function (usually POR for that domain)
    //!                 - beta is the range parameter controlling the baseline rate of change
    //!                 - omega is the growth rate factor of the envelope
    //!                 - gamma is the time scaling factor controlling the periodicity
    //!
    //!             During each iteration, f is computed for each domain specified in the sine spec
    //!             and applied to the base full pstate it is inheriting from
    //!
    //!             Because sine is a continuous function, each domain specified in the sine spec
    //!             can generate an infinite number of frequencies (there will always be an f for
    //!             each i). This means that an iterations setting is required to specify how many
    //!             iterations should be generated for this sine spec.
    //!
    //!             For example, the following sine spec:
    //!                 xbar2clk.alpha = 1620MHz
    //!                 xbar2clk.beta  = 40MHz
    //!                 xbar2clk.gamma = 0.2
    //!                 xbar2clk.omega = 0.5
    //!                 iterations = 5
    //!
    //!             Will produce the following 5 values to be applied to xbar2clk
    //!               index    xbar2clk
    //!                 0       1620.0MHz
    //!                 1       1627.9MHz
    //!                 2       1642.0MHz
    //!                 3       1659.1MHz
    //!                 4       1677.4MHz
    //!                 5       1695.3MHz
    //!
    //!             For an interactive example, please refer to https://p4viewer.lwpu.com/get/sw/docs/resman/chips/Maxwell/docs/clk/SineTest.xls
    //!
    //! Order of events:
    //! - A SineSpec object is created by CtpFileReader::scanFile.
    //! - Field objects are parsed by Field::parse,
    //! - The SineSpec object gets the Field object data with Field::apply.
    //! - SineReference::resolve is used to link the p-state references to PartialPState objects.
    //! - SineSpec::fullPState gets the fully-defined p-state definition to test.
    //!
    struct SineSpec: public TestSpec
    {
        SineSpec():TestSpec(TestSpec::SineSpec)
        {
            iterations = 0;
        }

        //!
        //! \brief      Sine Params
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#sine_param_field
        //!
        //! \details    Each param field sets one parameter for one of the clock domains
        //!
        //! Syntax:     <sine-param-field>     ::= <freq-domain> "." <sine-param-name> "=" <sine-param-val>
        //! Where:      <freq-domain> is one of the clock domains
        //!             <sine-param-name>   ::= "alpha" | "beta" | "gamma" | "omega"
        //!             <sine-param-val>    ::=
        //!                 - alpha: <number> [ <freq-units> ]
        //!                 - beta: [ "+" | "-" ] <number> [ <freq-units> ]
        //!                 - gamma: <number>
        //!                 - omega: <number>
        //!             <freq-units>     ::= "Hz" | "KHz" | "MHz" | "GHz"
        //!
        struct ParamField: public Field
        {
            //! \brief      Default construction
            inline ParamField(): Field()     { }

            //! \brief      Construction from a statement
            inline ParamField(const CtpStatement &statement): Field(statement) { }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the Sine Spec
            virtual ExceptionList apply(SineSpec &sineSpec) const;
        };

        //!
        //! \brief      Flags Field
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#sine_domain_flags
        //!
        //! \details    Sets the flags for a particular domain
        //!
        //! Syntax:     <sine-flag-field>     ::= <freq-domain> ".flags=" <freq-flag-spec>
        //! Where:      <freq-domain> is one of the clock domains
        //!             <freq-flag-spec> ::= ( "+" | "-" ) ( "FORCE_PLL" | "FORCE_BYPASS" )
        //!
        struct FlagField: public Field
        {
            //! \brief      Default construction
            inline FlagField(): Field()     { }

            //! \brief      Construction from a statement
            inline FlagField(const CtpStatement &statement): Field(statement) { }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the Sine Spec
            virtual ExceptionList apply(SineSpec &sineSpec) const;
        };

        //!
        //! \brief      Sine Spec Interations
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#sine_iterations
        //!
        //! \details    Sets the number of iterations this sine spec should be able to produce
        //!             (limits range of i)
        //!
        //! Syntax:     <sine-iterations-field>     ::= "iterations=" <positive integer>
        //!
        struct IterationsField: public Field
        {
            //! \brief      Default construction
            inline IterationsField(): Field()     { }

            //! \brief      Construction from a statement
            inline IterationsField(const CtpStatement &statement): Field(statement) { }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the Sine Spec
            virtual ExceptionList apply(SineSpec &sineSpec) const;
        };

        //!
        //! \brief      Sine test setting
        //!
        //! \details    Objects of this class contain the parameters and flags
        //!             to be applied to the frequency (clock) domain of a
        //!             fully-defined p-state.
        //!
        struct SineSetting
        {
            //! \brief  Domain id
            FreqDomain                                   domain;

            //! \brief  Clock Source
            std::vector<UINT32>                          source;

            //! \brief  Flag operator for parsing and applying flag
            FlagOperator                                 flags;

            //! \brief  Parameter values
            double                                       alpha;
            double                                       beta;
            double                                       gamma;
            double                                       omega;

            //! \brief  Construct a setting object with default values
            SineSetting();

            //! \brief  Determine if the setting is valid
            ExceptionList checkErrors() const;

            //!
            //! \brief      Modify the full p-state
            //!
            //! \details    Compute the 'i'th fully-defined p-state in the result
            //!             set using the specified p-state as the basis.
            //!
            SolverException fullPState(
                FullPState &fullPState,
                unsigned long long i,
                FreqDomainBitmap freqMask) const;

            //! \brief      Human readable format of this setting
            rmt::String string() const;
        };

        //! \brief      How manu iterations should this sine spec contain
        unsigned long iterations;

        //! \brief      The settings that this sine spec has for different domains
        std::map<FreqDomain, SineSetting> settingMap;

        //! \brief      Create a setting for a particular domain,
        //!             fetch existing otherwise
        SineSetting &createSettingIfMissing(Domain domain);

        //! \brief      Check if the entire sine spec contain errors
        ExceptionList checkErrors() const;

        //!
        //! \brief      Modify the full p-state
        //!
        //! \details    Delegates to the individual SineSetting to compute and apply
        //!
        ExceptionList fullPState(
            FullPState &fullPState,
            unsigned long long i,
            FreqDomainBitmap freqMask) const;

        //! \brief      Human readable output of the sine spec
        rmt::String debugString(const char *info = NULL) const;

        //! \brief      Find the number of iterations this sine spec has
        //!             which is simply the iterations value set in the ctp
        inline unsigned long long cardinality() const
        {
            return iterations;
        }

        bool supportPStateMode(const VbiosPState &vbios) const;
    };
}
#endif

