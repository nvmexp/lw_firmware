/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "protobuf/pbwriter.h" // Need to include before other example_writer.h file below
#include "example_handler.h"
#include "example_reader.h"
#include "example_writer.h"
#include "inc/bytestream.h"
#include "lwdiagutils.h"

LwDiagUtils::EC ExampleHandler::HandleExampleMessage
(
    const ExampleStructs::ExampleMessage & message,
    void * pvContext
)
{
    printf("Entering ExampleHandler::HandleExampleMessage\n");
    printf("message.major_version = %u\n", message.major_version);
    printf("message.minor_version = %u\n", message.minor_version);

    for (unsigned int index = 0; index < message.headers.size(); index++)
    {
        const auto& header = message.headers[index];
        printf("message.header[%u].node_id = %u\n", index, header.node_id);
        switch (header.app_type)
        {
            case ExampleStructs::MessageHeader::AppType::gdm:
                printf("message.header[%u].app_type = gdm\n", index);
                break;
            case ExampleStructs::MessageHeader::AppType::onediag:
                printf("message.header[%u].app_type = onediag\n", index);
                break;
            case ExampleStructs::MessageHeader::AppType::mods:
                printf("message.header[%u].app_type = mods\n", index);
                break;
            case ExampleStructs::MessageHeader::AppType::unknown:
                printf("message.header[%u].app_type = unknown\n", index);
                break;
        }
    }

    printf("message.link_info.b_valid = %u\n", message.link_info.b_valid);
    printf("message.link_info.b_active = %u\n", message.link_info.b_active);
    printf("message.link_info.b_lane_reversed = %u\n", message.link_info.b_lane_reversed);
    printf("message.link_info.version = %u\n", message.link_info.version);
    printf("message.link_info.line_rate_mbps = %u\n", message.link_info.line_rate_mbps);
    printf("message.link_info.ref_clk_mbps = %u\n", message.link_info.ref_clk_mbps);

    for (unsigned int index = 0; index < message.end_points.size(); index++)
    {
        switch (message.end_points[index].dev_type)
        {
            case ExampleStructs::PerEndPointInfo::Type::type_lwidia_gpu:
                printf("message.end_points[%u].dev_type gpu\n", index);
                break;
            case ExampleStructs::PerEndPointInfo::Type::type_lwidia_lwswitch:
                printf("message.end_points[%u].dev_type lwswitch\n", index);
                break;
            case ExampleStructs::PerEndPointInfo::Type::type_lwidia_trex:
                printf("message.end_poinst[%u].dev_type trex\n", index);
                break;
        }

        printf("message.end_points[%u].bdf.domain = 0x%x\n",
            index, message.end_points[index].bdf.domain);
        printf("message.end_points[%u].bdf.bus = 0x%x\n",
            index, message.end_points[index].bdf.bus);
        printf("message.end_points[%u].bdf.device = 0x%x\n",
            index, message.end_points[index].bdf.device);
        printf("message.end_points[%u].bdf.function = 0x%x\n",
            index, message.end_points[index].bdf.function);

        printf("message.end_points[%u].bdf.devdetails.vendor_id = 0x%x\n",
            index, message.end_points[index].bdf.devdetails.vendor_id);
        printf("message.end_points[%u].bdf.devdetails.device_id = 0x%x\n",
            index, message.end_points[index].bdf.devdetails.device_id);
        for (unsigned int barId = 0;
             barId < message.end_points[index].bdf.devdetails.bars.size();
             barId++)
        {
            printf("message.end_points[%u].bdf.devdetails.bars[%u] = %u\n",
                index, barId, message.end_points[index].bdf.devdetails.bars[barId]);
        }

        for (unsigned int otherBdfsId = 0;
             otherBdfsId < message.end_points[index].otherBdfs.size();
             otherBdfsId++)
        {
            printf("message.end_points[%u].otherBdfs[%u].domain = 0x%x\n",
                index, otherBdfsId, message.end_points[index].otherBdfs[otherBdfsId].domain);
            printf("message.end_points[%u].otherBdfs[%u].bus = 0x%x\n",
                index, otherBdfsId, message.end_points[index].otherBdfs[otherBdfsId].bus);
            printf("message.end_points[%u].otherBdfs[%u].device = 0x%x\n",
                index, otherBdfsId, message.end_points[index].otherBdfs[otherBdfsId].device);
            printf("message.end_points[%u].otherBdfs[%u].function = 0x%x\n",
                index, otherBdfsId, message.end_points[index].otherBdfs[otherBdfsId].function);
        }

        for (unsigned int linkId = 0; linkId < message.end_points[index].link_ids.size(); linkId++)
        {
            printf("message.end_points[%u].link_ids[%u] = %u\n",
                index, linkId, message.end_points[index].link_ids[linkId]);
        }
    }

    return LwDiagUtils::EC::OK;
}

int main(int argc, char **argv)
{
    LwDiagUtils::EC ec;
    ByteStream bs;
    auto msg = ExampleWriter::Messages::example_msg(&bs);
    msg.major_version(5).minor_version(2);

    // The writing routines only insert submessage into the bytestream
    // when the submessage is destructed
    // First header app 1 node 9
    {
        msg
          .headers()
              .app_type(1)
              .node_id(9);
    }
    // Second header app 2 node 3
    {
        msg
          .headers()
              .app_type(2)
              .node_id(3);
    }

    {
        msg
          .link_info()
              .b_valid(true)
              .b_active(true)
              .b_lane_reversed(false)
              .version(4)
              .line_rate_mbps(120)
              .ref_clk_mbps(100);
    }

    {
        vector<unsigned int> links{3, 4};
        vector<unsigned int> bars{0, 1};

        auto end_points_entry = msg.end_points();
        end_points_entry
              .dev_type(ExampleStructs::PerEndPointInfo::Type::type_lwidia_gpu)
              .link_ids(links)
              .bdf()
                  .domain(1)
                  .bus(7)
                  .device(3)
                  .devdetails()
                      .vendor_id(0x10de)
                      .device_id(0x20b0)
                      .bars(bars);

        end_points_entry
            .otherBdfs()
                .domain(0x9)
                .bus(0xa)
                .device(0xb)
                .function(0xc);

        end_points_entry
            .otherBdfs()
                .domain(2)
                .bus(3)
                .device(4);
    }

    {
        vector<unsigned int> links{5, 6};
        vector<unsigned int> bars{2, 3};
        msg
          .end_points()
              .dev_type(ExampleStructs::PerEndPointInfo::Type::type_lwidia_lwswitch)
              .link_ids(links)
              .bdf()
                  .domain(2)
                  .bus(5)
                  .device(8)
                  .function(1)
                  .devdetails()
                      .vendor_id(0x10de)
                      .device_id(0x20b5)
                      .bars(bars);
    }

    msg.Finish();
    CHECK_EC(ExampleHandler::HandleMessages(bs, nullptr));
    return 0;
}
