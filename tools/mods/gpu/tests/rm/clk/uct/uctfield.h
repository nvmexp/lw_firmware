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

#ifndef MODS_RM_CLK_UCT_FIELD_H_
#define MODS_RM_CLK_UCT_FIELD_H_

#include "gpu/tests/rm/utility/rmtmemory.h"
#include "gpu/tests/rm/utility/rmtvector.h"
#include "gpu/include/gpusbdev.h"
#include "uctchip.h"
#include "uctstatement.h"
#include "uctexception.h"

namespace uct
{
    class  CtpFileReader;
    struct TrialSpec;
    struct PartialPState;
    struct SineSpec;
    struct VfLwrve;

    //!
    //! \brief      Base class for any setting fields in a CTP file
    //! \see        uctfilereader.h
    //! \see        ucttrialspec.h
    //! \see        uctpstate.h
    //!
    //! \details    There are several types of fields in Clock Test Profile
    //!             (CTP) file, depending on the class(es) of objects to which
    //!             that may be applied (which in turn depends on type of
    //!             statement block where they may be found).  In general,
    //!             Field subclasses are declared/defined in the same header/source
    //!             files as the class to which they apply.
    //!
    //!             For example, RamTypeField is specific to trial specifications
    //!             and therefore can be applied only to TrialSpec objects.
    //!             Its declaration is in ucttrialspec.h and its definition is
    //!             in ucttrialspec.cpp.
    //!
    //!             This base class definition allows for subclasses that apply
    //!             to more than one class of objects (e.g. NameField).  Such
    //!             subclasses are declared here and defined in uctfield.cpp.
    //!
    //! \todo       Change this class to extend FileLocation rather than
    //!             'CtpStatement'.  It would be necessary to add a
    //!             'const CtpStatement &' parameter to 'parse'.
    //!
    struct Field: public CtpStatement
    {
        //! \brief      Default construction
        inline Field(): CtpStatement()      {   }

        //! \brief      Construction from a statement
        inline Field(const CtpStatement &statement):
            CtpStatement(statement)         {   }

        //! \brief      Destruction
        virtual ~Field();

        //! \brief      Create an exact copy of 'this'.
        virtual Field *clone() const = 0;

        //! \brief      Interpret the statement argument and/or domain.
        virtual ExceptionList parse() = 0;

        //!
        //! \brief      Apply the field to the reader
        //!
        //! \details    This function is a nop if this field does not apply
        //!             to a file reader.
        //!
        virtual ExceptionList apply(CtpFileReader &reader) const;

        //!
        //! \brief      Apply the field to the partial p-state definition
        //!
        //! \details    This function is a nop if this field does not apply
        //!             to a partial p-state.
        //!
        virtual ExceptionList apply(PartialPState &pstate) const;

        //!
        //! \brief      Apply the field to the sine spec
        //!
        //! \details    This function is a nop if this field does not apply
        //!             to a sine spec.
        //!
        virtual ExceptionList apply(SineSpec &sineSpec) const;

        //!
        //! \brief      Apply the field to the vflwrve definition
        //!
        //! \details    This function is a nop if this field does not apply
        //!             to a vflwrve field.
        //!
        virtual ExceptionList apply(VfLwrve &vflwrve, GpuSubdevice *m_pSubdevice) const;

        //!
        //! \brief      Apply the field to the trial spec
        //!
        //! \details    This function is a nop if this field does not apply
        //!             to a trial specification.
        //!
        virtual ExceptionList apply(TrialSpec &trial) const;
    };

    //!
    //! \brief      Canonical Name Field
    //! \see        https://wiki.lwpu.com/engwiki/index.php/Resman/Resman_Components/Clocks/UniversalClockTest/Writing#name_Field
    //!
    //! Syntax:     <name-field> ::= "name=" <canonical-name>
    //!
    struct NameField: public Field
    {
        //!
        //! \brief      Permitted characters
        //!
        //! \details    This set of characters, which includes ASCII alphanumerics
        //!             and period, are the only characters permitted in a name.
        //!
        static const char *const charset;

        //! \brief      Default construction
        inline NameField(): Field()
        {   }

        //! \brief      Construction from a statement
        inline NameField(const CtpStatement &statement): Field(statement)
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
        virtual ExceptionList apply(PartialPState &pstate) const;

        //!
        //! \brief      Apply the field to the sine spec
        //! \see        uctsinespec.cpp for implementation
        //!
        virtual ExceptionList apply(SineSpec &sinespec) const;

        //!
        //! \brief      Apply the field to the vflwrve
        //! \see        uctvflwrve.cpp for implementation
        //!
        virtual ExceptionList apply(VfLwrve &vflwrve, GpuSubdevice *m_pSubdevice) const;

        //!
        //! \brief      Apply the field to the trial spec
        //! \see        ucttrialspec.cpp for implementation
        //
        virtual ExceptionList apply(TrialSpec &trial) const;
    };

    //!
    //! \brief          Text subfield
    //!
    //! \details        Objects of this class represent text values (e.g. p-state
    //!                 references.
    //!
    struct TextSubfield: public rmt::FileLocation
    {
        //! \brief      Values for a field
        struct Vector: public rmt::Vector<rmt::ConstClonePointer<TextSubfield> >
        {
            //!
            //! \brief      All subfields as text
            //!
            inline rmt::String string() const
            {
                rmt::StringList list;
                const_iterator pp;
                for (pp = begin(); pp != end(); ++pp)
                    list.push_back((*pp)->text);
                return list.concatenate(", ");
            }
        };

        //! \brief      The text of the subfield
        rmt::String text;

        //! \brief      Construction of an empty subfield
        inline TextSubfield(const rmt::FileLocation &location):
            rmt::FileLocation(location)     { }

        //! \brief      Destruction
        virtual ~TextSubfield();

        //! \brief      Create an exact copy of 'this'.
        virtual const TextSubfield *clone() const;
    };

    //!
    //! \brief          Fields of a Statement Block
    //!
    //! \details        This class supercedes CtpStatementBlock from previous
    //!                 versions.
    //!
    //! \ilwariant      All objects in this vector have been parsed.
    //! \ilwariant      None of the pointers in this vector are NULL.
    //!
    class FieldVector: protected std::vector<Field *>
    {
    public:

        //!
        //! \brief      Aclwmulated statement types
        //!
        //! \details    This bitmap of the anonymous enum constants indicates
        //!             what type of statement block is represented by this
        //!             vector.
        //!
        //!             If more than one bit is set, the block type is ambiguous
        //!             due to the existance of statements for more than one
        //!             type of block (e.g. 'begin' for TrialSpecBlock and 'freq'
        //!             for PartialPStateBlock).  This would constitute a syntax
        //!             error in the CTP file.
        //!
        //!             The block type may also be ambiguous if no bits are set,
        //!             indicating that none of the statements are unique to a
        //!             type of block (e.g. 'name' which could be either
        //!             TrialSpecBlock or PartialPStateBlock).  Unless the block
        //!             is empty, this would also constitute a syntax error in
        //!             the CTP file.
        //!
        //!             Note that comments and empty statements are not typically
        //!             added to a statement block.
        //!
        typedef UINT08 Type;

        // Bitmapped values for 'Type'
        enum
        {
            Unknown            = CtpStatement::Type::None,               //!< Unknown -- probably empty
            DirectiveBlock     = CtpStatement::Type::DirectiveBlock,     //!< Singleton directive block
            PartialPStateBlock = CtpStatement::Type::PartialPStateBlock, //!< Partial p-state definition
            SineSpecBlock      = CtpStatement::Type::SineTestBlock,      //!< Sine test specification
            VfLwrveBlock       = CtpStatement::Type::VfLwrveBlock,       //!< VfLwrve test specification
            TrialSpecBlock     = CtpStatement::Type::TrialSpecBlock,     //!< Trial specification
        };

    protected:
        //! \brief      Aclwmulated statement types
        Type    type;

    public:
        //! \brief      Construct an empty vector
        inline FieldVector(): std::vector<Field *>()
        {
            type = Unknown;
        }

        //! \brief      Copy each field into newly constructed vector
        inline FieldVector(const FieldVector &that)
        {
            type = Unknown;
            push(that);
        }

        //! \brief      Destruction
        virtual ~FieldVector();

        //! \brief      Make this vector empty.
        void clear();

        //! \brief      Number of statements in this block
        inline size_type size() const
        {
            return std::vector<Field *>::size();
        }

        //! \brief      Copy each field into newly constructed vector
        inline FieldVector &operator=(const FieldVector &that)
        {
            clear();
            push(that);
            return *this;
        }

        //! \brief      Copy each field from 'that' to 'this'.
        void push(const FieldVector &that);

        //! \brief      Parse statement and push fields
        ExceptionList push(const CtpStatement &statement);

        //!
        //! \brief      Apply each field to the partial p-state definition.
        //! \see        uctpstate.cpp for implementation
        //!
        //! \pre        Each field must have been passed before being applied,
        //!             which follows from the fact that the field is in this
        //!             vector.
        //!
        //! \pre        This field vector must have PartialPStateBlock type.
        //!
        ExceptionList apply(PartialPState &pstate) const;

        //!
        //! \brief      Apply each field to the sine spec.
        //! \see        uctsinespec.cpp for implementation
        //!
        //! \pre        Each field must have been passed before being applied,
        //!             which follows from the fact that the field is in this
        //!             vector.
        //!
        //! \pre        This field vector must have SineSpecBlock type.
        //!
        ExceptionList apply(SineSpec &sinespec) const;

        //!
        //! \brief      Apply each field to the vflwrve definition.
        //! \see        uctvflwrve.cpp for implementation
        //!
        //! \pre        Each field must have been passed before being applied,
        //!             which follows from the fact that the field is in this
        //!             vector.
        //!
        //! \pre        This field vector must have VfLwrveBlock type.
        //!
        ExceptionList apply(VfLwrve &vflwrve, GpuSubdevice *m_pSubdevice) const;

        //!
        //! \brief      Apply each field to the trial spec.
        //! \see        ucttrialspec.cpp for implementation
        //!
        //! \pre        Each field must have been passed before being applied,
        //!             which follows from the fact that the field is in this
        //!             vector.
        //!
        //! \pre        This field vector must have TrialSpecBlock type.
        //!
        ExceptionList apply(TrialSpec &trial) const;

        //!
        //! \brief      Identify the block type
        //!
        //! \retval     Unknown
        //!                 Contains no identifying statements (block may be empty)
        //! \retval     DirectiveBlock
        //!                 All statements are consistent with directives (i.e. include).
        //! \retval     TrialSpecBlock
        //!                 All statements are consistent with trial specifications.
        //! \retval     PartialPStateBlock
        //!                 All statements are consistent with partial p-state specifications.
        //! \retval     Any other value
        //!                 Error: Statements of more than one type exist in this block.
        //!
        inline Type blockType() const
        {
            return type;
        }

        //!
        //! \brief      Value from the starting field (if any)
        //!
        //! \return     Starting statement
        //! \retval     NULL       No such field exists.
        //!
        inline const Field *start() const
        {
            for (const_iterator it = begin(); it != end(); ++it)
                if ((*it)->isBlockStart())
                    return *it;             // BlockStart found
            return NULL;
        }

        //!
        //! \brief      Value from the 'name' field (if any)
        //!
        //! \return     Name statement
        //! \retval     NULL       No such field exists.
        //!
        inline const Field *name() const
        {
            for (const_iterator it = begin(); it != end(); ++it)
                if ((*it)->isBlockName())
                    return *it;             // BlockName found
            return NULL;
        }

        //!
        //! \brief      Value from the directive field (if any)
        //!
        //! \return     Directive statement
        //! \retval     NULL       No such field exists.
        //!
        inline const Field *directive() const
        {
            for (const_iterator it = begin(); it != end(); ++it)
                if ((*it)->isDirective())
                    return *it;             // Directive found
            return NULL;
        }

        //!
        //! \brief      Human-readable summary of this block
        //!
        //! \details    No new-line is appended to the string.
        //!
        rmt::String debugString(const char *info = NULL) const;

        //!
        //! \brief      Human-readable representation of this block
        //!
        //! \details    All statements are included after a summary.
        //!             Newlines are inserted between lines (before statements),
        //!             but not after the text.
        //!
        rmt::String detailString(const char *info = NULL) const;

    };
}
#endif /* MODS_RM_CLK_UCT_FIELD_H_ */

