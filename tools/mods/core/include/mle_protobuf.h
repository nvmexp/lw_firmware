/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "inc/bytestream.h"
#include "massert.h"
#include "protobuf/pbwriter.h"
#include "mle_protobuf_writer.h"

namespace Tee
{
    void PrintMle(ByteStream*);
}

namespace Mle
{
    //! Prints protobuf messages into the MLE log.
    //!
    //! This class is not supposed to be instantiated directly, instead use
    //! the Mle::Print() function!
    template<typename T, unsigned fieldIndex, bool isFieldPublic>
    struct Printer: private ByteStream, public Dumper<T, fieldIndex, isFieldPublic>
    {
        Printer()
        : Dumper<T, fieldIndex, isFieldPublic>(this, DumpPos(0))
        {
        }
        ~Printer()
        {
            if (isFieldPublic || !Tee::IsMleLimited())
            {
                Dumper<T, fieldIndex, isFieldPublic>::Finish();
                Tee::PrintMle(this);
            }
        }
        Printer(const Printer&)            = delete;
        Printer& operator=(const Printer&) = delete;

        Printer(Printer&& other)
        : ByteStream(other),
        Dumper<T, fieldIndex, isFieldPublic>(&other, other.GetPos())
        {
        }
        Printer& operator=(Printer&& other)
        {
            ByteStream::operator=(move(other));
            Dumper<T, fieldIndex, isFieldPublic>::operator=(
                Dumper<T, fieldIndex, isFieldPublic>(this, other.GetPos()));
            return *this;
        }
    };

    //! Prints a complex message (binary entry) into MLE log.
    //!
    //! For textual printfs, Printf() (from Tee) must be used.
    //! This function can be ilwoked only for messages which are fields defined
    //! in the Entry message.
    //!
    //! The argument is a named field in the Entry message.
    //!
    //! The argument is really a dummy function which gets the parent message
    //! reference, this function does not exist anywhere, but we use it to
    //! enforce that Print() is used only with Entry fields.
    //!
    //! Returns printer objects, which actually write the binary log entry into
    //! the MLE log in the destructor.  A typical usage of these objects is to
    //! use them as temporary objects, otherwise end of scope should be observed
    //! to make sure the MLE log entry is written at the time expected.
    template<typename T, unsigned fieldIndex, bool isFieldPublic>
    Printer<T, fieldIndex, isFieldPublic> Print(Deductor<T, fieldIndex, isFieldPublic> (*)(Entry&))
    {
        return Printer<T, fieldIndex, isFieldPublic>();
    }
}
