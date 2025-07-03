/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCTTESTSPEC_H_
#define UCTTESTSPEC_H_

#include <memory>
#include <vector>

#include "gpu/tests/rm/utility/rmtstring.h"

#include "uctexception.h"
#include "uctdomain.h"
#include "uctpstate.h"

//! \brief Constants for checking the frequency value range
#define FREQ_RANGE_MIN          0
#define FREQ_RANGE_MAX  100000000

namespace uct{

    //! \brief      Abstract base class for test fields
    //!
    //! \details    Each of the different tests a trial can specify should inherit this
    //!             base class.
    //!
    struct TestSpec
    {
        //! \brief      The common freq domain source field
        //!
        //! \details    All test specs support the source field, which can specify
        //!             which source to use for each clock domain in the test
        //!
        struct SourceField: public Field
        {
            //! \brief      Default construction
            inline SourceField(): Field()
            {   }

            //! \brief      Construction from a statement
            inline SourceField(const CtpStatement &statement): Field(statement)
            {   }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //!
            //! \brief      Apply the field to the partial p-state
            //! \see        uctpstate.cpp for implementation
            //!
            virtual ExceptionList apply(uct::PartialPState &pstate) const;

            //!
            //! \brief      Apply the field to the vflwrve
            //! \see        uctvflwrve.cpp for implementation
            //!
            virtual ExceptionList apply(uct::VfLwrve &vflwrve) const;

            //!
            //! \brief      Apply the field to the sine spec
            //! \see        uctsinespec.cpp for implementation
            //!
            virtual ExceptionList apply(uct::SineSpec &sinespec) const;
        };

        //! \brief      Defines vector of test specs
        typedef rmt::Vector<shared_ptr<TestSpec>> Vector;

        //! \brief      Map from unit text to exponent for frequency fields
        const static NumericOperator::UnitMap unitMapuV;

        //! \brief      Map from unit text to exponent for voltage fields
        const static NumericOperator::UnitMap unitMapKHz;

        //! \brief      Map from flag names to bitmapped masks for frequency fields
        const static FlagOperator::NameMap flagNameMap;

        //! \brief      The types of tests there are, this is used to
        //!             keep track of which keyword (for example pstate= or sine=)
        //!             to generate the test spec.
        //!
        //!             Values used should match Field::Type
        enum TestType
        {
            Unknown = 0,
            SineSpec,
            PartialPState,
            VfLwrve
        };

        //! \brief      Each test spec can be looked up by name by using a NameMap
        //!             to store them
        struct NameMap: public rmt::StringMap<shared_ptr<TestSpec>>
        {
            inline bool push(const shared_ptr<TestSpec> &pTestSpec)
            {
                MASSERT(!pTestSpec->name.empty());

                if (find(pTestSpec->name) != end())
                    return false;

                insert(pTestSpec->name, pTestSpec);

                return true;
            }
        };

        //! \brief      Constructor must set type of the test spec
        TestSpec(TestType t): type(t)
        {   }

        virtual ~TestSpec() {   };

        //! \brief      Type of keyword this test spec belongs to (sine/pstate)
        TestType type;

        //! \brief      Name of the test spec
        NameField name;

        //! \brief      Given a VBios PState, method checks to see if settings in the
        //!             test spec can be applied to the vbios. This is needed because
        //!             if a domain is specified in the test that doest not exist in
        //!             the VBios PState then API calls to set pstate will fail
        virtual bool supportPStateMode(const VbiosPState &vbios) const = 0;

        //! \brief      Compute how many iterations this test spec contains
        virtual unsigned long long cardinality() const = 0;

        //! \brief      Generate the current (ith) iteration based on test spec settings
        //!             and apply the settings to a base full pstate
        virtual ExceptionList fullPState(FullPState &fullPState,
            unsigned long long i, FreqDomainBitmap freqMask) const = 0;

        //! \brief      Human readable representation of this test spec
        virtual rmt::String debugString(const char *info = NULL) const = 0;
    };
}
#endif

