/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2022 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwdiagutils.h"
#include "inc/bytestream.h"
#include "protobuf/pbreader.h"
#include "protobuf/pbwriter.h"
#include "example_reader.h"
#include "example_writer.h"
#include "example_structs.h"
#include "protobuf/pbreader_parse.h"
#include <memory>
#include <unistd.h>

static LwDiagUtils::EC ParseMessage(const ByteStream & messageData, ExampleStructs::Example & message)
{
    ProtobufReader::PBInput input(nullptr, &messageData, 0, messageData.size(), LwDiagUtils::PriError);

    ProtobufReader::FieldHdr fldhdr = ProtobufReader::GetPBFieldHdr(input);

    if ((fldhdr.index != ExampleReader::ExampleTop::example_msg) ||
        (fldhdr.wire != ProtobufCommon::Wire::Bytes))
    {
        return LwDiagUtils::SOFTWARE_ERROR;
    }
    // Remove the size from the stream
    size_t entrySize = input.GetPBSize(input.pos);
    entrySize = entrySize;

    // Parse context fields, save all other fields in pSubEntries
    for (const ProtobufReader::FieldHdr& hdr : input)
    {
        switch (hdr.index)
        {
            DECLARE_SIMPLE_FIELD(ExampleReader, Example, example_type, UINT08);
            DECLARE_SIMPLE_FIELD(ExampleReader, Example, print, string);

            BEGIN_STRUCTURED_FIELD(ExampleReader, Example, example_submessage)
                DECLARE_MEMBER_FIELD(ExampleReader, ExampleSubmessage, example_enum,   UINT08)
                DECLARE_MEMBER_FIELD(ExampleReader, ExampleSubmessage, example_int,    INT64)
                DECLARE_MEMBER_FIELD(ExampleReader, ExampleSubmessage, example_string, string)
            END_STRUCTURED_FIELD(ExampleSubmessage)

            default:
                break;
        }
    }
    return LwDiagUtils::OK;
}

static void PrintMessage(const ExampleStructs::Example & ex)
{
    switch (ex.example_type)
    {
        case ExampleStructs::Example::et_print:
            printf("example print message received : %s\n", ex.print.c_str());
            break;
        case ExampleStructs::Example::et_submessage:
            printf("example submessage received\n");
            switch (ex.example_submessage.example_enum)
            {
                case ExampleStructs::ExampleSubmessage::sm_foo:
                    printf("   submessage enum   : foo\n");
                    break;
                case ExampleStructs::ExampleSubmessage::sm_bar:
                    printf("   submessage enum   : bar\n");
                    break;
                default:
                    break;
            }
            printf("   submessage int    : %llu\n", ex.example_submessage.example_int);
            printf("   submessage string : %s\n", ex.example_submessage.example_string.c_str());
            break;
        default:
            break;
    }
}

int main(int argc, char **argv)
{
    ByteStream bs;
    auto hello = ExampleWriter::ExampleTop::example_msg(&bs);
    hello.example_type(ExampleWriter::Example::et_print).print("Hello world");
    hello.Finish();

    LwDiagUtils::EC ec;
    ExampleStructs::Example ex;
    CHECK_EC(ParseMessage(bs, ex));
    PrintMessage(ex);

    ByteStream newBs;
    auto submessage = ExampleWriter::ExampleTop::example_msg(&newBs);
    submessage.example_type(ExampleWriter::Example::et_submessage).print("Hello");
    {
        // The writing routines only insert the submessage into the bytestream
        // when the submessage is destructed, alternatively this could be a the
        // top level and after creating the submessage you can call
        // submessage.example_submessage().Finish()
        submessage
            .example_submessage()
                .example_enum(ExampleWriter::ExampleSubmessage::sm_foo)
                .example_int(23LL)
                .example_string("Submessage string");
    }
    submessage.Finish();

    CHECK_EC(ParseMessage(newBs, ex));
    PrintMessage(ex);

    return 0;
}
