/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2018, 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef UCTTRIALSPEC_H_
#define UCTTRIALSPEC_H_

#include <stdio.h>
#include <climits>
#include <vector>
#include <sstream>
#include <string>
#include <map>
#include <list>

#include "gpu/tests/rm/utility/rmtstring.h"
#include "uctexception.h"
#include "uctstatement.h"
#include "uctfield.h"
#include "ucttestspec.h"
#include "ucttestreference.h"

#ifndef ULLONG_MAX
#define ULLONG_MAX ((unsigned long long) -1)
#endif

namespace uct
{
    //!
    //! \brief      Trial specifiation
    //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#Trial_Specification
    //!
    //! \details    Consists of a set of pstate references (which will be resolved
    //!             into fully-defined p-states of all permutations) and several
    //!             settings.
    //!
    //!             Each object of this class corresponds to a trial specification
    //!             block in a sourced Clock Test Profile (ctp) file.
    //!
    struct TrialSpec
    {
        //!
        //! \brief      Test Reference Field
        //!
        //! \details    List of colon-delimited p-states starting with a
        //!             fully-defined VBIOS p-state plus zero or more partial
        //!             p-states or sine tests defined in the partial
        //!             p-state or sine test definition.
        //!
        //! Syntax:     <test-ref> ::= <vbios-p-state> { ":" <test-spec> }
        //!
        struct TestField: public Field
        {
            typedef TextSubfield ReferenceText;

            //! \brief      Each <vbios-p-state> { ":" <test-spec> } is a test reference text
            //!             and is stored in this vector
            ReferenceText::Vector referenceTextList;

            //! \brief      Default construction
            inline TestField(): Field() { }

            //! \brief      Construction from a statement
            inline TestField(const CtpStatement &statement): Field(statement) { }

            //! \brief      Create a new 'enable' field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the trial spec
            virtual ExceptionList apply(TrialSpec &trial) const;
        };

        //!
        //! \brief      RMAPI Setting
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#rmapi_Field
        //!
        //! \details    This field indicates which RMAPI points should be used to
        //!             set the frequencies, voltages, etc. as:
        //!
        //!             'clk'       LW2080_CTRL_CMD_CLK_SET_INFO (default)
        //!             'perf'      LW2080_CTRL_CMD_PERF_SET_FORCE_PSTATE
        //!             'config'    LW2080_CTRL_CMD_CLK_CONFIG_INFO_V2
        //!             'chseq'     LW2080_CTRL_CMD_PERF_CHANGE_SEQ_(G|S)ET_CONTROL
        //!
        //!             Formerly, there was 'vfinject' which called LW2080_CTRL_CMD_PERF_VF_CHANGE_INJECT.
        //!
        //! Syntax:     <rmapi-field> ::= "rmapi=" ( "clk" | "perf" )
        //!
        //!             'clocks' and 'pstate' are respectively accepted as synonyms
        //!             for 'clk' and 'perf'.
        //!
        //! @note       The functionality of the RMAPI point sets differ and may or
        //!             may not be compatible with the 'pstate' field setting.
        //!
        //! @todo       Add syntax and functionality to make this setting a preference
        //!             rather than a requirement so that the RMAPI point set is chosen
        //!             based on the requirements of the 'pstate' field.
        //!
        struct RmapiField: public Field
        {
            enum Mode
            {
                Clk,            //!< LW2080_CTRL_CMD_CLK_SET_INFO control call
                Pmu,            //!< LW2080_CTRL_CMD_CLK_SET_INFOS_PMU control call
                Perf,           //!< LW2080_CTRL_PERF_ control calls
                Config,         //!< LW2080_CTRL_CMD_CLK_CONFIG_INFO_V2 control call
                ChSeq           //!< LW2080_CTRL_CMD_PERF_CHANGE_SEQ_(G|S)ET_CONTROL control call
            };

            Mode    mode;

            //! \brief      Default construction
            inline RmapiField(): Field()
            {
                mode = Clk;
            }

            //! \brief      Construction from a statement
            inline RmapiField(const CtpStatement &statement): Field(statement)
            {
                mode = Clk;
            }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the trial spec
            virtual ExceptionList apply(TrialSpec &trial) const;

            //! \brief      Simple human-readable text
            rmt::String string() const;
        };

        struct ToleranceSetting
        {
            float measured;                 //!< Actual v. Measured
            float actual;                   //!< Target v. Actual

            //! \brief      Default Construction
            inline ToleranceSetting()
            {
                measured = 0.05f;   //  5%
                actual   = 0.25f;   // 25%
            }
        };

        //!
        //! \brief      Tolerance Setting
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#tolerance_Field
        //!
        //! \details    Basically it is a field that specifies the deviation allowed
        //!             between what is specified in the ctp file vs what can be
        //!             programmed, and also what can be programmed vs what the clock
        //!             actually achieved (2 numbers).
        //!
        //! Syntax:     <tolerance-field> ::= "tolerance=" <actual-tolerance> "," <measured-tolerance>
        //!
        struct ToleranceField: public Field
        {
            //!
            ToleranceSetting    setting;

            //! \brief      Default construction
            inline ToleranceField(): Field() { }

            //! \brief      Construction from a statement
            inline ToleranceField(const CtpStatement &statement): Field(statement)
            {
                setting.actual = setting.measured = 0.0f;   // Zero until parsed
            }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the trial spec
            virtual ExceptionList apply(TrialSpec &trial) const;

            //! \brief      Simple human-readable text
            rmt::String string() const;
        };

        //!
        //! \brief      Order (sequence v. random) Field
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#order_Field
        //!
        //! \details    Specifies if a trial spec should be exelwted in random order
        //!             or sequentially.
        //!
        //! Syntax:     <order-field> ::= "order=" ( "sequence" | "random" )
        //!
        //! \todo       Add an "alternate" option that alternates among the p-state
        //!             references.  For example, if there are three p-state
        //!             references defined, one based on each of P0, P8 and P12;
        //!             "alternate" means do one from P0, then one from P8, then
        //!             one from P12, and repeat.  If combined with "random",
        //!             it should randomly choose a fully-defined p-state from
        //!             each reference.
        //!
        //! \todo       Add a "shuffle" option which is just like "random" except
        //!             that it keeps track of the indices it chooses so that
        //!             it doesn't repeat.  Because of memory limitations, there
        //!             is a limit to how much it can remember.
        //!
        struct OrderField: public Field
        {
            //!
            //! \brief      Iteraion Mode
            //! \see        Iterator
            //! \see        PStateReference::Vector::fullPState
            //!
            //! \details    This enum specifies how an 'Iterator' object selects the
            //!             next index to pass to PStateReference::Vector::fullPState.
            //!
            enum Mode
            {
                //!
                //! \brief      Sequential Iteraion
                //! \see        Iterator::nextSequence
                //! \see        PStateReference::Vector::fullPState
                //!
                //! \details    Iterate in the order specified in the CTP file.
                //!             Specifially, run each fully-defined p-state in
                //!             the first p-state reference, then each in the
                //!             second, and so on.
                //!
                //!             Since PStateReference::Vector::fullPState uses
                //!             this order when interpreting the 'index' parameter,
                //!             this mode is the same as incrementing the
                //!             index at each iteration, which is what
                //!             Iterator::nextSequence does.
                //!
                Sequence,

                //!
                //! \brief      Random Iteraion
                //! \see        Iterator::nextRandom
                //!
                //! \details    Randomly choose fully-define p-states from
                //!             randomly chosen p-state references.
                //!
                //!             There is no attempt to avoid repetition or to
                //!             ensure that each fully-defined p-state is run
                //!             at least once.
                //!
                //!             Since PStateReference::Vector::fullPState uses
                //!             this order when interpreting the 'index' parameter,
                //!             this mode is the same as choosing a random index
                //!             at each iteration, which is what Iterator::nextRandom.
                //!
                Random
            };

            Mode    mode;

            //! \brief      Default construction
            inline OrderField(): Field()
            {
                mode = Sequence;
            }

            //! \brief      Construction from a statement
            inline OrderField(const CtpStatement &statement): Field(statement)
            {
                mode = Sequence;
            }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the trial spec
            virtual ExceptionList apply(TrialSpec &trial) const;

            //! \brief      Simple human-readable text
            rmt::String string() const;
        };

        //!
        //! \brief      Random Seed Field
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#seed_Field
        //!
        //! \details    In the case of random order, specify the seed to use.
        //!
        //! Syntax:     <seed-field> ::= "seed=" <nonnegative-integer>
        //!
        struct SeedField: public Field
        {
            //! \brief      Value passed to 'srand'
            unsigned seed;

            //! \brief      Default construction
            inline SeedField(): Field()
            {
                seed = 0;
            }

            //! \brief      Construction from a statement
            inline SeedField(const CtpStatement &statement): Field(statement)
            {
                seed = 0;
            }

            //! \brief      Create a new field
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the trial spec
            virtual ExceptionList apply(TrialSpec &trial) const;

            //! \brief      Simple human-readable text
            rmt::String string() const;
        };

        //! \brief      Settings that affect the order of iteration
        struct SortSetting
        {
            //! \brief      Order (Random v. Sequence)
            OrderField      order;

            //! \brief      Random seed
            SeedField       seed;

            //! \brief      Do these fields agree?
            ExceptionList check() const;
        };

        //! \brief      Settings that affect the iteration range
        struct RangeSetting
        {
            //! \brief      Iteration Number
            typedef unsigned long long Iteration;

            //! \brief      Maximum value for 'Iteration'
            static const Iteration Iteration_Max = ULLONG_MAX;

            //!
            //! \brief      Beginning Iteration Field
            //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#begin_Field
            //!
            //! \details    Specify the first iteration to execute.
            //!
            //! Syntax:     <begin-field> ::= "begin=" ( "first" | <nonnegative-integer> )
            //!
            struct BeginField: public Field
            {
                //!
                //! \brief      Beginning Iteration
                //!
                //! \details    Zero means to execute starting with the first p-state (the default).
                //!
                RangeSetting::Iteration begin;

                //! \brief      Default construction
                inline BeginField(): Field()
                {
                    begin = 0;
                }

                //! \brief      Construction from a statement
                inline BeginField(const CtpStatement &statement): Field(statement)
                {
                    begin = 0;
                }

                //! \brief      Create a new field
                static Field *newField(const CtpStatement &statement);

                //! \brief      Create an exact copy of 'this'.
                virtual Field *clone() const;

                //! \brief      Interpret the statement argument and/or domain.
                virtual ExceptionList parse();

                //! \brief      Apply the field to the trial spec
                virtual ExceptionList apply(TrialSpec &trial) const;

                //! \brief      Simple human-readable text
                rmt::String string() const;
            };

            //!
            //! \brief      Ending Iteration Field
            //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#end_Field
            //!
            //! \details    Specify the number of iterations to execute.
            //!
            //!             Do not confuse this class with 'eof' field, a pseudo-directive
            //!             that marks the end of the CTP input file.
            //!
            //! Syntax:     <end-field> ::= "end=" ( "last" | <nonnegative-integer> )
            //!
            struct EndField: public Field
            {
                //!
                //! \brief      Iteration end
                //!
                //! \details    ULONG_MAX means to end with the last p-state (the default).
                //!
                RangeSetting::Iteration end;

                //! \brief      Default construction
                inline EndField(): Field()
                {
                    end = RangeSetting::Iteration_Max;
                }

                //! \brief      Construction from a statement
                inline EndField(const CtpStatement &statement): Field(statement)
                {
                    end = RangeSetting::Iteration_Max;
                }

                //! \brief      Create a new field
                static Field *newField(const CtpStatement &statement);

                //! \brief      Create an exact copy of 'this'.
                virtual Field *clone() const;

                //! \brief      Interpret the statement argument and/or domain.
                virtual ExceptionList parse();

                //! \brief      Apply the field to the trial spec
                virtual ExceptionList apply(TrialSpec &trial) const;

                //! \brief      Simple human-readable text
                rmt::String string() const;
            };

            //! \brief      Starting iteration
            BeginField      begin;

            //! \brief      Ending iteration
            EndField        end;

            //! \brief      Do these fields agree?
            ExceptionList check() const;

            //! \brief      Number of iterations
            inline Iteration count() const
            {
                return 1 + end.end - begin.begin;
            }
        };

        //!
        //! \brief      Enable/Disable Feature Field
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#enable_Field
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#disable_Field
        //!
        //! \details    Specify features that must be enabled or disabled for the
        //!             trial to be exelwted.
        //!
        //!             This statement may be specified multiple times.  Each
        //!             'EnableField' object implies a logical OR and the vector of
        //!             objects (from multiple statements) implies a logical AND.
        //!
        //! Syntax:     <enable-field>  ::= "enable=" <feature> { "," <feature> }
        //!             <disable-field> ::= "disable=" <feature> { "," <feature> }
        //!             <feature>       ::= ( "ThermalSlowdown" | "IdleSlowdown"
        //!                                 | "BrokenFB" | "Training2T" )
        //!
        struct FeatureField: public Field
        {
            //! \brief      Feature Index
            enum Index
            {
                ThermalSlowdown = 0,
                DeepIdle,
                BrokenFB,
                Training2T,
            };

            //! \brief      Index Bitmap
            typedef UINT08 Bitmap;

            //! \brief      Vector of Features
            struct Vector: public rmt::Vector<FeatureField>
            {
                //! \brief      Simple human-readable text
                inline rmt::String string() const
                {
                    const_iterator p;
                    rmt::String s;
                    for (p = begin(); p != end(); ++p)
                        s += p->string();
                    return s;
                }
            };

            //! \brief      Map from Keword Text to Index Bitmap
            typedef rmt::StringMap<Bitmap> KeywordMap;

            //! \brief      Map from Keword Text to Index Bitmap
            static const KeywordMap keywordMap;

            //!
            //! \brief      Enable v. Disable flag
            //!
            //! \details    When -1, the test can proceed if/when any one of the
            //!             features is enabled.  When 0, the test can proceed
            //!             if/when any one of the features is disabled.
            //!
            //!             Each zero bit represents that the corresponding feature
            //!             is disabled while a one bit indicates that it is enabled.
            //!             With current syntax, each bit of 'enable' is the same.
            //!
            //!             Why use FeatureFlag instead of bool?  So that we can
            //!             (if needed) create a new field type keyword that uses
            //!             ordinary booean expressions, something like:
            //!             required = (ThermalSlowdown | !IdleSlowdown) & !BrokenFB
            //!             We have no such need at this time, but if we did, this
            //!             data structure would handle it.  The evaluation logic
            //!             would not have to change.
            //!
            Bitmap     enable;

            //!
            //! \brief      Features flag
            //!
            Bitmap     feature;

            //!
            //! \brief      Construction from a statement
            //!
            //! \param[in]  enable      true for 'enable' field; false for 'disable' field.
            //!
            inline FeatureField(bool enable, const CtpStatement &statement): Field(statement)
            {
                this->enable  = (enable? (Bitmap) -1: (Bitmap) 0);
                this->feature = (Bitmap) 0;
            }

            //! \brief      Create a new 'enable' field
            static Field *newEnableField(const CtpStatement &statement);

            //! \brief      Create a new 'disable' field
            static Field *newDisableField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the trial spec
            virtual ExceptionList apply(TrialSpec &trial) const;

            //! \brief      Is this feature eitehr enabled or disabled?
            inline bool isSpecified(Index index) const
            {
                return !!(feature & BIT(index));
            }

            //! \brief      Is this feature enabled?
            inline bool isEnabled(Index index) const
            {
                return (feature & BIT(index)) && (enable & BIT(index));
            }

            //! \brief      Is this feature disabled?
            inline bool isDisabled(Index index) const
            {
                return (feature & BIT(index)) && !(enable & BIT(index));
            }

            //! \brief      Simple human-readable text
            rmt::String string() const;
        };

        //!
        //! \brief      RAM Type Field
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#ramtype_Field
        //!
        //! \details    Specify the required memory type(s) for this trial.
        //!
        //!             If multiple memory types are specified on the same statement,
        //!             any one of those memory types enable the trial.
        //!
        //! Syntax:     <ram-type-field>  ::= "ramtype=" <ram-type> { "," <ram-type> }
        //! Where:      <ram-type> is defined in the 'KeywordMap'.
        //!
        struct RamTypeField: public Field
        {
            // \brief       LW2080_CTRL_FB_INFO_RAM_TYPE_xxx Bitmap
            typedef UINT32 Bitmap;

            // \brief       Map from Keyword Text to LW2080_CTRL_FB_INFO_RAM_TYPE_xxx
            typedef rmt::StringMap<Bitmap> KeywordMap;

            //
            // \brief       Map from Keyword Text to LW2080_CTRL_FB_INFO_RAM_TYPE_xxx
            //
            // \ilwariant   Each entry defines exactly one bit.
            //
            static const KeywordMap keywordMap;

            //!
            //! \brief      RAM type flag set
            //!
            //! \details    The trial is enabled only if one if we're running on
            //!             any of these memory types
            //!
            Bitmap     bitmap;

            //!
            //! \brief      Construction default object
            //!
            //! \details    By default, any RAM type is okay.
            //!
            inline RamTypeField(): Field()
            {
                bitmap = 0xffffffff;
            }

            //!
            //! \brief      Construction from a statement
            //!
            inline RamTypeField(const CtpStatement &statement): Field(statement)
            {
                bitmap = 0xffffffff;
            }

            //! \brief      Create a new field object
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //! \brief      Apply the field to the trial spec
            virtual ExceptionList apply(TrialSpec &trial) const;

            //! \brief      Is any RAM type okay?
            inline bool isAny() const
            {
                return bitmap == 0xffffffff;
            }

            //! \brief      Simple human-readable text
            rmt::String string() const;

            //! \brief      Text for specified RAM type
            inline rmt::String typeString() const
            {
                return typeString(bitmap);
            }

            //! \brief      Text for specified RAM type
            static rmt::String typeString(Bitmap bitmap);
        };

        //!
        //! \brief      Domain Pruning Field
        //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#prune_Field
        //! \see        CtpFileReader::freqDomainSet
        //! \see        DomainNameMap::freq
        //!
        //! \details    Specify the domains that should be omitted from the test.
        //!             Multiple domains may be specified, seperated by commas.
        //!
        //!             The keyword 'auto' means that unsupported domains are
        //!             pruned per CtpFileReader::freqDomainSet.
        //!
        //!             The keyword 'none' disables pruning.
        //!
        //! Syntax:     <prune-field> ::= "prune=" [ "auto" | "none" | <domain> { "," <domain> } ]
        //! Where:      <domain> is defined in the 'DomainNameMap::freq'.
        //!
        //! \todo       Add support for voltage domains.
        //!
        struct PruneField: public Field
        {
            //! \brief      No domains pruned
            static const FreqDomainBitmap noPrune   = 0xffffffff;

            //!
            //! \brief      Prune domains automatically
            //!
            //! \details    This sentinel value is used only before the call to
            //!             'apply' since 'apply' resolves the mask from the
            //!             context.  That is, 'apply' knows what 'auto' means.
            //!
            static const FreqDomainBitmap autoPrune = 0x00000000;

            //!
            //! \brief      Pruned Domains
            //!
            //! \details    Zero bits indicate domains to be pruned.
            //!             0xffffffff means 'none'.
            //!             0x00000000 means 'auto'.
            //!
            //! \details    The trial is enabled only if one if we're running on
            //!             any of these memory types
            //!
            FreqDomainBitmap mask;

            //!
            //! \brief      Construction of default object
            //!
            //! \details    By default, nothing is pruned.
            //!
            inline PruneField(): Field()
            {
                mask = noPrune;
            }

            //!
            //! \brief      Construction from a statement
            //!
            inline PruneField(const CtpStatement &statement): Field(statement)
            {
                mask = noPrune;
            }

            //! \brief      Create a new field object
            static Field *newField(const CtpStatement &statement);

            //! \brief      Create an exact copy of 'this'.
            virtual Field *clone() const;

            //! \brief      Interpret the statement argument and/or domain.
            virtual ExceptionList parse();

            //!
            //! \brief      Apply the field to the trial spec
            //!
            //! \details    The mask is applied to the trial specification.
            //!             If the 'auto' keyword was used, the mask applied is
            //!             the set of programmable frequency domains from the
            //!             context.
            //!
            virtual ExceptionList apply(TrialSpec &trial) const;

            //!
            //! \brief      Simple human-readable text
            //!
            //! \param[in]  programmableMask    Programmable domains -- others are omited
            //!
            rmt::String string(FreqDomainBitmap programmableMask = DomainBitmap_All) const;

            //! \brief      Text for pruned domains
            static rmt::String domainString(FreqDomainBitmap pruneMask, FreqDomainBitmap programmableMask = DomainBitmap_All);
        };

        //!
        //! \brief      P-State Reference Iterator
        //!
        //! \note       This class interface does not conform to any STL-style iterator.
        //!
        //! \pre        Before construction, 'sort' and 'range' in the bound
        //!             trial specification must be initialized.
        //!
        //! \pre        Before usage, 'resolve' must have been successfully
        //!             called on the bound trial specification so that
        //!             'pstateReferenceVector' is properly populated.
        //!
        class Iterator
        {
            //! \brief      Integer Type if Iteration Number
            typedef RangeSetting::Iteration Iteration;

            //! \brief      Function to choose the next iteration
            typedef void (Iterator::*Next)();

        protected:

            //! \brief      Trial specification to iterate over
            const TrialSpec *pTrial;

            //! \brief      Index to pass to PStateReference::Vector::fullPState
            Iteration       index;

            //! \brief      Number if iterations until the end
            Iteration       remaining;

            //!
            //! \brief      Function to choose the next iteration
            //!
            //! \details    Either 'nextSequence' or 'nextRandom'.
            //!
            Next            next;

            //! \brief      Add one to the index
            void nextSequence();

            //! \brief      Choose a random index
            void nextRandom();

        public:

            //! \brief      Construct to the beginning
            Iterator(const TrialSpec *pTrial);

            //! \brief      Prefix Increment Operator
            void operator++()
            {
                (this->*next)();
                --remaining;
            }

            //!
            //! \brief      End-Of-List?
            //!
            //! \retval     true    Iterator is valid
            //! \retval     false   Last p-state has been fetched
            //!
            operator bool() const
            {
                return remaining > 0;
            }

            //! \brief      Index of p-state within result set
            Iteration iterationIndex() const
            {
                return index;
            }

            //! \brief      Iteration count before end of result set
            Iteration iterationRemaining() const
            {
                return remaining;
            }

            //!
            //! \brief      Current Fully-Defined P-State
            //!
            //! \pre        'resolve' must have been successfully called on the
            //!             trial specification so that 'pstateReferenceVector'
            //!             is properly populated.
            //!
            //!             Typical call chain:
            //!             TrialSpec::Iterator::fullPState =>
            //!             TestReference::Vector::fullPState =>
            //!             TestReference::fullPState =>
            //!             PartialPState::fullPState =>
            //!             PartialPState::Setting::Vector::fullPState =>
            //!             FreqSetting/VoltSetting::fulPState =>
            //!             NumericOperator/FlagOperator::apply
            //!
            inline ExceptionList fullPState(FullPState &pstate) const
            {
                //
                // 'auto' should have been resolved when PruneField::apply was
                // called.  At least, it's illegal to prune away all domains
                // from the fully-defined p-state.
                //
                MASSERT(pTrial->prune.mask);

                // Prune the domains from the fully-defined p-state
                pstate.prune(pTrial->prune.mask);

                // Apply the partially-defined p-states to the fully-defined p-state.
                return pTrial->testReferenceVector.fullPState(pstate, index, pTrial->prune.mask);
            }
        };

        //!
        //! \brief      List of trial specifications with name mape
        //!
        //! \details    This class augments list functionality with map to find
        //!             trial specifications by name.  It also enforces ilwariants
        //!             to disallow anonymous and duplicate names.
        //!
        //! \ilwariant  Each trials specification must have a nonempty name.
        //!
        //! \ilwariant  No two trials may have the same name.
        //!
        //! \ilwariant  Names may not be changed after insertion.  THis ilwariant
        //!             is not completely enforced by this class.
        //!
        //! \todo       Consider making this list contain 'TrialSpec *' rather
        //!             that 'TrialSpec' do avoid duplicating the object duing
        //!             'push'.  'push' would be replaced with 'take' since
        //!             this class would delete the object on destruction.
        //!
        class List: protected std::list<TrialSpec>
        {
        protected:

            //! \brief      Map from Names to Trial Specifications
            struct Map: public rmt::StringMap<TrialSpec *>    {   };

            //! \brief      Map from Names to Trial Specifications
            Map nameMap;

            //! \brief      Non-Constant Iterator at Begining
            iterator begin()
            {
                return std::list<TrialSpec>::begin();
            }

            //! \brief      Non-Constant Iterator at End
            iterator end()
            {
                return std::list<TrialSpec>::end();
            }

        public:
            //! \brief      List Iterator
            typedef std::list<TrialSpec>::const_iterator const_iterator;

            //! \brief      Construct an empty list
            inline List()   {   }

            //! \brief      Construct a copy
            inline List(const List &that)
            {
                push(that);
            }

            //! \brief      Destruction
            virtual ~List();

            //! \brief      Make this equal that
            inline List &operator=(const List &that)
            {
                clear();
                push(that);
                return *this;
            }

            //! \brief      Make this list empty
            inline void clear()
            {
                nameMap.clear();
                std::list<TrialSpec>::clear();
            }

            //! \brief      Is this list empty?
            inline bool empty() const
            {
                return std::list<TrialSpec>::empty();
            }

            //! \brief      Copy each field into this list
            inline void push(const List &that)
            {
                const_iterator pTrial;
                for (pTrial = that.begin(); pTrial != that.end(); ++pTrial)
                    push(*pTrial);
            }

            //!
            //! \brief      Add the trial to the end of this list.
            //!
            //! \details    It is illegal to insert an anonymous trial
            //!             specification; an assertion results.
            //!
            //!             If the name of the trial specification is a duplicate,
            //!             the it is not inserted into this list and 'false'
            //!             is returned.
            //!
            //! \retval     false       No insertion due to a duplicate name
            //! \retval     true        Trial spec has been inserted.
            //!
            inline bool push(const TrialSpec &trial)
            {
                // Anonymous names may not be added to the list.
                MASSERT(!trial.name.empty());

                // Duplicate trials are not added to the list.
                if (nameMap.find(trial.name) != nameMap.end())
                    return false;

                // Add to the list and the name map
                push_back(trial);
                nameMap.insert(back().name, &back());
                return true;
            }

            const_iterator begin() const
            {
                return std::list<TrialSpec>::begin();
            }

            const_iterator end() const
            {
                return std::list<TrialSpec>::end();
            }

            //! \brief      Resolve P-State References for All Trials
            ExceptionList resolve(
                const VbiosPStateArray         &vbiosPStateMap,
                const TestSpec::NameMap        &testSpecMap);
        };

        //! \brief      File name and line number, as well as the trial name
        NameField       name;

        //! \brief      Chip- or plaform-specific context
        ChipContext     context;

        //!
        //! \brief      Unresolved Test References as Text
        //!
        //! \example    vbiosp0:halfFreqAlt
        //!
        //! \details    This list is filled by 'TestField::apply' with the text
        //!             of test references that have not yet been resolved
        //!             while the trial spec is being parsed from the CTP file.
        //!
        //!             As these references are resolved, they are removed from
        //!             this vector.  Holding the text here during parsing
        //!             enables the use of forward references to partial p-states
        //!             and sine specs.
        //!
        TestField::ReferenceText::Vector testReferenceText;

        //!
        //! \brief      Resolved P-State References
        //!
        //! \details    Once all the partial p-state definitions have been parsed
        //!             from the CTP (clock test profile) files. this vector is
        //!             filled with the resolved p-state references.  Doing so
        //!             enables the use of forward references.
        //!
        TestReference::Vector testReferenceVector;

        //! \brief      RMAPI points to use (CLK v. PERF)
        RmapiField      rmapi;

        //! \brief      Tolerances
        ToleranceField  tolerance;

        //! \brief      Order and seed
        SortSetting     sort;

        //! \brief      Iteration range
        RangeSetting    range;

        //! \brief      Features which must be enabled and/or disabled.
        FeatureField::Vector    featureVector;

        //! \brief      Ram type settings
        RamTypeField    ramtype;

        //! \brief      Prune settings
        PruneField      prune;

        //!
        //! \brief      Resolve Test References
        //! \see        testReferenceText
        //! \see        testReferenceVector
        //!
        //! \details    Move the test references from 'testReferenceText'
        //!             to 'testReferenceVector', resolving them in the process.
        //!
        ExceptionList resolve(
            const VbiosPStateArray         &vbiosPStateMap,
            const TestSpec::NameMap        &testSpecMap);

        //! \brief      Number of fully-defined p-states in this trial
        inline RangeSetting::Iteration cardinality() const
        {
            return testReferenceVector.cardinality();
        }

        //! \brief      Human-readable representation of this
        rmt::String debugString(const char *info = NULL) const;
    };
}
#endif

