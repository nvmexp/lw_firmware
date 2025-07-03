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

//!
//! \file
//!
//! \brief      Base implementation of CTP fields (freq, volt, name, etc.)
//!

#include <cmath>
#include <math.h>
#include <climits>
#include <sstream>
#include <iomanip>

#include "gpu/tests/rm/utility/rmtmemory.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tests/rm/utility/rmtstring.h"

#include "uctfield.h"
#include "uctstatement.h"

/*******************************************************************************
    Field (base class)
*******************************************************************************/

// Destruction -- nothing to do
uct::Field::~Field()
{   }

// By default, we do nothing in the base class.
uct::ExceptionList uct::Field::apply(CtpFileReader &reader) const
{
    MASSERT(!"This field object cannot be applied to a CtpFileReader.");
    return uct::ExceptionList();
}

// By default, we do nothing in the base class.
uct::ExceptionList uct::Field::apply(PartialPState &ptate) const
{
    MASSERT(!"This field object cannot be applied to a PartialPState.");
    return uct::ExceptionList();
}

// By default, we do nothing in the base class.
uct::ExceptionList uct::Field::apply(SineSpec &sinespec) const
{
    MASSERT(!"This field object cannot be applied to a SineSpec.");
    return uct::ExceptionList();
}

// By default, we do nothing in the base class.
uct::ExceptionList uct::Field::apply(VfLwrve &vflwrve, GpuSubdevice *m_pSubdevice) const
{
    MASSERT(!"This field object cannot be applied to a VfLwrve.");
    return uct::ExceptionList();
}

// By default, we do nothing in the base class.
uct::ExceptionList uct::Field::apply(TrialSpec &trial) const
{
    MASSERT(!"This field object cannot be applied to a TrialSpec.");
    return uct::ExceptionList();
}

/*******************************************************************************
    NameField
*******************************************************************************/

// Characters permitted in names
const char *const uct::NameField::charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.";

// Create a new field
uct::Field *uct::NameField::newField(const CtpStatement &statement)
{
    return new NameField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::NameField::clone() const
{
    return new NameField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::NameField::parse()
{
    // It makes no sense to have a 'name' field with no name.
    if (empty())
        return ReaderException("Name is required in a 'name' field.",
            *this, __PRETTY_FUNCTION__);

    // Error if there is something other than an alphanumeric/period
    if (find_first_not_of(charset) != npos)
        return ReaderException(*this + ": Only alphanumerics/periods are permitted in a 'name' field.",
            *this, __PRETTY_FUNCTION__);

    // Okay
    return Exception();
}

/*******************************************************************************
    TextSubfield
*******************************************************************************/

// Destruction -- nothing to do
uct::TextSubfield::~TextSubfield()
{   }

// Clone
const uct::TextSubfield *uct::TextSubfield::clone() const
{
    return new TextSubfield(*this);
}

/*******************************************************************************
    FieldVector
*******************************************************************************/

// Delete the fields when the vector is deleted.
uct::FieldVector::~FieldVector()
{
    clear();
}

// Delete the fields when the vector is cleared.
void uct::FieldVector::clear()
{
    const_iterator ppField;

    // Delete each field from the heap
    for (ppField = begin(); ppField != end(); ++ppField)
        if (*ppField)
            delete *ppField;

    // clear the pointers.
    std::vector<Field *>::clear();

    // Reset the block type since it is now empty.
    type = Unknown;
}

// Copy all elements from 'that' to 'this'
void uct::FieldVector::push(const uct::FieldVector &that)
{
    const_iterator ppField;

    // Clone each field
    for (ppField = that.begin(); ppField != that.end(); ppField++)
        push_back((*ppField)->clone());

    // Update the union of statement types
    type |= that.type;
}

// Parse statement and push fields
uct::ExceptionList uct::FieldVector::push(const uct::CtpStatement &statement)
{
    ExceptionList status;

    // Create a field object based on the statement
    Field *pField = statement.newField();
    MASSERT(pField);

    // Parse the field argument.
    status = pField->parse();

    // Add it to this list unless there is an error.
    if (status.isOkay())
    {
        // Get the bitmapped statement attributes that correspond to the block type
        Type statementType = (statement.type->attribute  &
                               (DirectiveBlock        |
                                PartialPStateBlock    |
                                SineSpecBlock         |
                                VfLwrveBlock          |
                                TrialSpecBlock));

        // Use the statement only if it uniquly identifies the block type
        if (ONEBITSET(statementType))
            type = (Type)(type | statementType);

        // Add the field to this vector.
        push_back(pField);
    }
    else
    {
        // Otherwise we no longer have any need for this field and should free it
        // since it will no longer be reachable outside of this method
        delete pField;
    }

    // Done
    return status;
}

// Human-readable summary of this block
rmt::String uct::FieldVector::debugString(const char *info) const
{
    const Field        *start = this->start();
    size_t              size  = this->size();
    std::ostringstream  oss;

    // File name and line number of starting statement if applicable
    if (start)
        oss << start->locationString();

    // Additional info if any
    if (info && *info)
        oss << info << ": ";

    // Empty
    if (!size)
        oss << "size=0  type=empty";

    // Non empty
    else
    {
        Type         type      = this->type;
        const Field *name      = this->name();
        const Field *directive = this->directive();

        // Number of statements in the block
        oss << "size=" << dec << size;

        // Express concern if the directive is not a singleton
        if (type == DirectiveBlock && size != 1)
            oss << '!';

        // Statement type (verb)
        oss << "  type=";
        switch (type)
        {
            case Unknown:             oss << "unknown";   break;
            case DirectiveBlock:      oss << "directive"; break;
            case PartialPStateBlock:  oss << "pstate";    break;
            case VfLwrveBlock:        oss << "vflwrve";   break;
            case TrialSpecBlock:      oss << "trial";     break;
            default: oss << "0x" << std::hex << std::setw(2) << std::setfill('0')
                         << type << std::setw(0) << '!';
        }

        // Directive keyword if applicable
        if (directive && directive->type)
            oss << "  keyword=" << directive->type->keyword;

        // Name if applicable
        if (name && !name->empty())
            oss << "  name=" << *name;
        else if (type & (PartialPStateBlock | TrialSpecBlock| VfLwrveBlock))
            oss << "  name=[anonymous]";
    }

    // Done
    return oss.str();
}

// Human-readable details of this block
rmt::String uct::FieldVector::detailString(const char *info) const
{
    rmt::String s = debugString(info);

    // Get the details of each statement.
    for (const_iterator it = begin(); it != end(); ++it)
        s += '\n' + (*it)->debugString();

    return s;
}

