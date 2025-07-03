/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCTVFLWRVE_H_
#define UCTVFLWRVE_H_

#include <vector>

#include "gpu/tests/rm/utility/rmtstring.h"

#include "uctexception.h"
#include "uctdomain.h"
#include "uctpstate.h"
#include "ucttestspec.h"
#include "uctfield.h"

extern LW2080_CTRL_CLK_CLK_DOMAINS_INFO clkDomainsInfo;
extern UINT32 clkDomailwfLwrveIndex;
extern LW2080_CTRL_CLK_CLK_PROGS_INFO   clkProgsInfo;
extern LW2080_CTRL_CLK_CLK_VF_RELS_INFO clkVfRelsInfo;
extern LW2080_CTRL_CLK_CLK_ENUMS_INFO clkEnumsInfo;
extern LW2080_CTRL_VOLT_VOLT_RAILS_STATUS_PARAMS voltRailsStatus;

namespace uct
{
    struct VfLwrve: public TestSpec
    {
        VfLwrve(): TestSpec(TestSpec::VfLwrve)
        {   }

        struct FreqSetting;
        struct VoltSetting;
        struct SourceSetting;

        struct Setting
        {
            class Vector
            {
            protected:
                // \brief       Union of all frequency (clock) domains in this vector
                DomainBitmap freqDomainSet;

                DomainBitmap voltDomainSet;

                // \brief       Map from frequency domains to settings objects.
                rmt::Vector<FreqSetting> freqDomainMap[Domain_Count];

                rmt::Vector<VoltSetting> voltDomainMap[Domain_Count];

                // \brief       Map from frequency domains to source settings objects.
                rmt::Vector<SourceSetting> sourceDomainMap[Domain_Count];

            public:
                //! \brief      Construction
                inline Vector()
                {
                    freqDomainSet =
                    voltDomainSet = DomainBitmap_None;
                }

                //! \brief      Add a frequency setting to this vector.
                void push(const FreqSetting &setting);

                //! \brief      Add a voltage setting to this vector.
                void push(const VoltSetting &setting);

                //! \brief      Add a source setting to this vector
                void push(const SourceSetting &setting);

                //! \brief      Number of elements in the cartesian product
                unsigned long long cardinality() const;

                ExceptionList fullPState(FullPState &fullPState, unsigned long long i, FreqDomainBitmap freqMask) const;

                //! \brief      Human-readable representation of the objects in this vector
                rmt::String string() const;

            };

            //! \brief      Frequency (clock) or voltage domain
            Domain domain;

            Domain domailwolt;

            //Base P-state
            long pstateNumber;

            //! \brief      Numeric operator
            rmt::ConstClonePointer<NumericOperator> numeric;

            //! \brief      Destructor
            virtual ~Setting();

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

        struct VfLwrveField: public Field
        {
            //! \brief      Default construction
            inline VfLwrveField(): Field()     { }

            //! \brief      Construction from a statement
            inline VfLwrveField(const CtpStatement &statement): Field(statement) { }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the partial p-state
            virtual ExceptionList apply(VfLwrve &vflwrve,GpuSubdevice *m_pSubdevice) const;

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

