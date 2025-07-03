/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef MODS_RM_CLK_UCT_STATEMENT_H_
#define MODS_RM_CLK_UCT_STATEMENT_H_

#include <fstream>
#include <deque>
#include <algorithm>
#include <set>

#include "core/include/tee.h"
#include "gpu/tests/rm/utility/rmtstream.h"
#include "gpu/tests/rm/utility/rmtstring.h"
#include "uctdomain.h"

namespace uct
{
    struct ReaderException;
    class ExceptionList;
    struct Field;

    //!
    //! \brief      Clock Test Profile (CTP) Statement
    //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest#Clock_Test_Profile
    //!
    //! \details    Objects of this class differ from ordinary strings in
    //!             that the meta data is stripped from the text when read.
    //!
    //! Syntax:     <statement> ::= [<domain> "."] <field-type> "=" <value>
    //!
    //!             Among the meta data are the file name and line number
    //!             inherited as 'file' and 'line'.  <domain> and <field-type>
    //!             are respectively stored as 'domain' and 'type'.  <value>
    //!             remains in this string object after the other data are
    //!             stripped away.
    //!
    //!             Since each statement can generate only one type of field
    //!             <field-type> is a statement type as well as a field type.
    //!
    struct CtpStatement: rmt::TextLine
    {
        //! \brief          <field-type>
        class Type
        {
        public:

            //!
            //! \brief      Field types accepted from the CTP file
            //!
            //! \details    These enum values index the 'all' array.
            //!
            enum Index
            {
                Unknown = 0,
                Comment,        //!< Comment including empty lines
                Eof,            //!< End of CTP (clock test profile) file
                Include,        //!< Source another CTP file
                DryRun,         //!< Parse the CTP files, but do not run trials
                Name,           //!< Canonical name of pseudo-p-state or trial spec
                Freq,           //!< Frequencies for the pseudo-p-state
                Source,         //!< Source for the frequency domain in a pseudo-p-state
                Volt,           //!< Voltages for the pseudo-p-state
                Test,           //!< Test reference in the trial spec
                Rmapi,          //!< RMAPI points used in the trials
                Tolerance,      //!< How much tolerance is allowed between defined ctp value vs actual
                Order,          //!< P-State order (e.g. random v. sequential)
                Seed,           //!< P-State order (e.g. random v. sequential)
                Begin,          //!< Number of iterations to skip
                End,            //!< Iteration count in the trials
                Enable,         //!< Features that must be enabled for the trial
                Disable,        //!< Features that must be disabled for the trial
                RamType,        //!< Required memory types for the trial
                Prune,          //!< Domains to prune from the trial
                DomainFlag,     //!< Defines the domain and flags for this sine block
                Alpha,          //!< Starting frequency of Sine Test
                Beta,           //!< Step size of Sine Test
                Gamma,          //!< Time scaling factor of Sine Test
                Omega,          //!< Decay rate of Sine Test
                Iterations,     //!< Number of iterations this Sine Test has
                VfLwrve,        //!< Keyword used for vflwrve test spec
                TypeCount       //!< Number of types -- Must be last
            };

            //!
            //! \brief      Bitmapped Statement Attributes
            //!
            //! \details    These bits classify the CTP (Clock Test Profile) statement.
            //!             The first five of these symbols indicate the type of block
            //!             to which the statement belongs and are returned by
            //!             CtpStatementBlock::type.
            //!
            enum
            {
                None                = 0,        //!< None of the attributes apply
                DirectiveBlock      = BIT(0),   //!< No other statements in this block -- implies BlockStart
                PartialPStateBlock  = BIT(1),   //!< Appears in partial p-state spec
                TrialSpecBlock      = BIT(2),   //!< Appears in trial spec
                SineTestBlock       = BIT(3),   //!< Appears in sine test spec
                VfLwrveBlock        = BIT(4),   //!< Appears in vflwrve test spec
                BlockStart          = BIT(5),   //!< New block has started
                EofDirective        = BIT(6),   //!< End-of-file -- implies DirectiveBlock
            };

            //! \brief      Detailed information
            struct Info
            {
                //! \brief      'Field' object factory
                typedef Field *NewField(const CtpStatement &statement);

                //! \brief      <field-type> as a string
                const char     *keyword;

                //! \brief      Attributes which follow from <field-type>
                UINT08          attribute;

                //! \brief      Function to create a new object
                NewField       *newField;

                //! \brief      Map from domain keywords to bits
                const DomainNameMap *domainNameMap;
            };

        protected:
            //! \brief      All possible types
            static const Info all[];

            //! \brief      Index into 'all' array
            Index index;

        public:
            //! \brief      Construct from the index
            inline Type(Index i = Unknown)
            {
                index = i;
            }

            //!
            //! \brief      The type is the index.
            //!
            //! \details    This operator function works as a boolean where
            //!             nonzero means it is not 'Unknown' since 'Unknown'
            //!             is defined as zero.
            //!
            inline operator Index() const
            {
                return index;
            }

            //! \brief      Details about this type
            inline const Info *operator->() const
            {
                return all + index;
            }

            //!
            //! \brief      Find the type corresponding to the keyword
            //!
            //! \brief      This function returns a pointer rather to the static
            //!             'Type' object that matches the specified keyword.
            //!             The referenced object persists.
            //!
            //! \retval     Unknown     No such type exists.
            //!
            static inline const Type find(const rmt::String &keyword)
            {
                int i = TypeCount;
                while (i--)
                    if (keyword == all[i].keyword)
                        return (Type)(Index) i;
                return Unknown;
            }
        };

        //!
        //! \brief      Statement type
        //!
        //! \details    This is the type infered from <field-type> per CTP syntax.
        //!             Other values are possible to handle special
        //!             cirlwmstances such as end-of-file or syntax error.
        //!
        //!             Since all 'Type' objects are allocated during construction
        //!             of a static 'TypeSet' object, the pointer value itself
        //!             can be used to determine the exact type of the statement.
        //!             NULL indicates an unknown type.
        //!
        //!             Assuming it is not NULL, the object pointed to by 'type'
        //!             contains additional type-specific information.
        //!
        Type            type;

        //!
        //! \brief      Voltage or clock domain
        //! \see        PartialPState::FreqField::domainNameMap
        //! \see        PartialPState::VoltField::domainNameMap
        //! \see        Type::Info::domainNameMap
        //! \see        uct::CtpStatement::Type::all
        //!
        //! \details    The <domain> per CTP syntax is represented as a
        //!             bitmap per either LW2080_CTRL_CLK_DOMAIN_xxx or
        //!             LW2080_CTRL_PERF_VOLTAGE_DOMAINS_xxx, depending on
        //!             the field type.  Zero indicates that the domain
        //!             does not apply to this statement.
        //!
        //!             This class doesn't actially define the bitmapped values,
        //!             but uses the map referenced by Type::Info::domainNameMap,
        //!             which is initialized in uct::CtpStatement::Type::all.
        //!             To define other sorts of domains, simply add it to 'all'.
        //!
        //! \todo       Lwrrently, CTP syntax permits only one domain to be
        //!             specified on a single statement.  We could change
        //!             [<domain> "."] to {<domain> "."} and allow more
        //!             that one domain to be used here.
        //!
        DomainBitmap    domain;

        //!
        //! \brief      Construct an empty object
        //!
        inline CtpStatement()
        {
            line   = 0;
            type   = Type::Unknown;
            domain = DomainBitmap_None;
        }

        //! \brief      Destruct
        virtual ~CtpStatement();

        //!
        //! \brief      Read a statement and begin parsing
        //!
        //! \details    This function reads a line from the input stream and
        //!             parses away the meta data per CTP syntax. Specifically,
        //!             it extracts the <domain> (if any) and the <field-type>
        //!             as metadata, leaving the <value>.  It uses Type::find
        //!             (which uses Type::find::all) to determine these metadata.
        //!
        //!             All keywords are case-insensitive.  Superfluous spaces
        //!             are removed.
        //!
        //!             This function also prints the statements are they are
        //!             read.
        //!
        //!             End-of-file is indicated by a pseudo-directive of EOF
        //!             type.  An end-of-file condition never resultsin an
        //!             exception, so it is safe to call this function using
        //!             a stream that has already reached its end.  In effect,
        //!             every file ends with such an infinitely many such directives.
        //!
        ReaderException read(rmt::TextFileInputStream &is);

        //! \brief          Reset everything to the constructed state
        inline void clear()
        {
            rmt::String::erase();
            file.erase();
            line = 0;
            type = Type::Unknown;
            domain = DomainBitmap_None;
        }

        //!
        //! \brief      Does the object contain no data except location?
        //!
        //! \details    This function returns true iff <domain>, <field-type>,
        //!             and <value> are empty.  This is in contrast to the
        //!             inherited 'empty' function, which tests only <value>.
        //!
        inline bool isEmpty() const
        {
            return rmt::TextLine::empty() && !type && !domain;
        }

        //! \brief      Types that indicate the start of a new block
        inline bool isBlockStart() const
        {
            return type && !!(type->attribute & Type::BlockStart);
        }

        //! \brief      Types that indicate the name of the block
        inline bool isBlockName() const
        {
            return type == Type::Name;
        }

        //!
        //! \brief      Should this be added to a block?
        //!
        //! \details    Basically, this is anything other than a comment or
        //!             blank line.
        //!
        inline bool isBlockType() const
        {
            return type && !!(type->attribute           &
                             (Type::DirectiveBlock      |
                              Type::TrialSpecBlock      |
                              Type::SineTestBlock       |
                              Type::VfLwrveBlock        |
                              Type::PartialPStateBlock));
        }

        //! \brief      Types that indicate the name of the block
        inline bool isDirective() const
        {
            return type && !!(type->attribute & Type::DirectiveBlock);
        }

        //! \brief      Directive that indicates end-of-file
        inline bool isEof() const
        {
            return type && !!(type->attribute & Type::EofDirective);
        }

        //! \brief      Does the value indicate a boolean true?
        inline bool isTrue() const
        {
            return !empty()                             &&
               (rmt::String("true").startsWith(*this)   ||
                rmt::String("yes").startsWith(*this));
        }

        //! \brief      Does the value indicate a boolean false?
        inline bool isFalse() const
        {
            return !empty()                             &&
               (rmt::String("false").startsWith(*this)  ||
                rmt::String("no").startsWith(*this));
        }

        //!
        //! \brief      Create a new 'Field' object
        //!
        //! \details    The caller owns the newly created stack object and
        //!             is responsible to make sure it gets deleted.
        //!
        //! \retval     NULL        No 'Field' class is associated with this type.
        //!
        inline Field *newField() const
        {
            if (type->newField)
                return type->newField(*this);
            return NULL;
        }

        //!
        //! \brief      Human-readable string representation of this statement
        //!
        //! \details    The meta data is included along with the value.
        //!             No new-line is appended to the string.
        //!
        //! \param[in]  info    Optional text (e.g. __FUNCTION__) to be included.
        //!
        virtual rmt::String debugString(const char *info = NULL) const;
    };
}

#endif /* MODS_RM_CLK_UCT_STATEMENT_H_ */

