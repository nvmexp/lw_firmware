/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.
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
#include "example_handler.h"
#include <memory>
#include <unistd.h>

LwDiagUtils::EC ExampleHandler::HandleExampleMessage(const ExampleStructs::ExampleMessage & ex, void *pvContext)
{
    switch (ex.example_header.example_type)
    {
        case ExampleStructs::ExampleHeader::et_submessage:
            printf("example header print : %s\n", ex.example_header.print.c_str());
            printf("example submessage received\n");
            switch (ex.example_enum)
            {
                case ExampleStructs::ExampleMessage::sm_foo:
                    printf("   submessage enum   : foo\n");
                    break;
                case ExampleStructs::ExampleMessage::sm_bar:
                    printf("   submessage enum   : bar\n");
                    break;
                default:
                    break;
            }
            printf("   submessage int    : %llu\n", ex.example_int);
            printf("   submessage string : %s\n", ex.example_string.c_str());
            break;
        default:
            break;
    }
    return LwDiagUtils::OK;
}

int main(int argc, char **argv)
{
    LwDiagUtils::EC ec;
    ByteStream newBs;
    auto submessage = ExampleWriter::Example::example_msg(&newBs);
    {
        // The writing routines only insert the submessage into the bytestream
        // when the submessage is destructed, alternatively this could be a the
        // top level and after creating the submessage you can call
        // submessage.example_submessage().Finish()
        submessage
            .example_header()
                .example_type(ExampleWriter::ExampleHeader::et_submessage)
                .print("Hello");
    }
    submessage
        .example_enum(ExampleWriter::ExampleMessage::sm_foo)
        .example_int(23LL)
        .example_string("Submessage string");
    submessage.Finish();

    CHECK_EC(ExampleHandler::HandleExample(newBs, nullptr));

    return 0;
}
