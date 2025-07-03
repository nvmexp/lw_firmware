/*
 */

#include "gpu/repair/hbm/hbm_interface/vendor/samsung/samsung_hbm2_xdie_interface.h"
#include "gtest/gtest.h"

using namespace Memory;
using namespace Hbm;

using FuseScanLookupTable = SamsungHbm2XDie::EnableFuseScanLookupTable;
using EnableFuseScanData  = SamsungHbm2XDie::EnableFuseScanData;
using HbmChannel          = Hbm::Channel;

TEST(SamsungXDieHbm2, EnableFuseScanLookupTable_GetWdrData)
{
    // Channels with same offset
    //
    // Channel A/B/E/F, spare 0
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(0), PseudoChannel(0), Bank(0), 0),
              EnableFuseScanData(0b00101001000000001000000000)) << "channel A/B/E/F, spare 0";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(1), PseudoChannel(0), Bank(0), 0),
              EnableFuseScanData(0b00101001000000001000000000)) << "channel A/B/E/F, spare 0";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(4), PseudoChannel(0), Bank(0), 0),
              EnableFuseScanData(0b00101001000000001000000000)) << "channel A/B/E/F, spare 0";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(5), PseudoChannel(0), Bank(0), 0),
              EnableFuseScanData(0b00101001000000001000000000)) << "channel A/B/E/F, spare 0";

    // Channel A/B/E/F, spare 1
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(0), PseudoChannel(0), Bank(0), 1),
              EnableFuseScanData(0b00101001000000001010000000)) << "channel A/B/E/F, spare 1";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(1), PseudoChannel(0), Bank(0), 1),
              EnableFuseScanData(0b00101001000000001010000000)) << "channel A/B/E/F, spare 1";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(4), PseudoChannel(0), Bank(0), 1),
              EnableFuseScanData(0b00101001000000001010000000)) << "channel A/B/E/F, spare 1";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(5), PseudoChannel(0), Bank(0), 1),
              EnableFuseScanData(0b00101001000000001010000000)) << "channel A/B/E/F, spare 1";

    // Channel C/D/G/H, spare 0
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(2), PseudoChannel(0), Bank(0), 0),
              EnableFuseScanData(0b00101001010000001000000000)) << "channel C/D/G/H, spare 0";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(3), PseudoChannel(0), Bank(0), 0),
              EnableFuseScanData(0b00101001010000001000000000)) << "channel C/D/G/H, spare 0";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(6), PseudoChannel(0), Bank(0), 0),
              EnableFuseScanData(0b00101001010000001000000000)) << "channel C/D/G/H, spare 0";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(7), PseudoChannel(0), Bank(0), 0),
              EnableFuseScanData(0b00101001010000001000000000)) << "channel C/D/G/H, spare 0";

    // Channel C/D/G/H, spare 1
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(2), PseudoChannel(0), Bank(0), 1),
              EnableFuseScanData(0b00101001010000001010000000)) << "channel C/D/G/H, spare 1";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(3), PseudoChannel(0), Bank(0), 1),
              EnableFuseScanData(0b00101001010000001010000000)) << "channel C/D/G/H, spare 1";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(6), PseudoChannel(0), Bank(0), 1),
              EnableFuseScanData(0b00101001010000001010000000)) << "channel C/D/G/H, spare 1";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(7), PseudoChannel(0), Bank(0), 1),
              EnableFuseScanData(0b00101001010000001010000000)) << "channel C/D/G/H, spare 1";

    // Different stack
    //
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(0), PseudoChannel(0), Bank(0), 0),
              EnableFuseScanData(0b01001001000000001000000000));
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(0), PseudoChannel(0), Bank(0), 1),
              EnableFuseScanData(0b01001001000000001010000000));

    // Pseudo channel 0, lower banks
    //
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(5), PseudoChannel(0), Bank(3), 0),
              EnableFuseScanData(0b01001001000000110000000000)) << "pseudo channel 0, lower banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(5), PseudoChannel(0), Bank(4), 1),
              EnableFuseScanData(0b00101001001000001010000000)) << "pseudo channel 0, lower banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(5), PseudoChannel(0), Bank(7), 1),
              EnableFuseScanData(0b01001001001000110010000000)) << "pseudo channel 0, lower banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(3), PseudoChannel(0), Bank(7), 0),
              EnableFuseScanData(0b00101001011000110000000000)) << "pseudo channel 0, lower banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(0), PseudoChannel(0), Bank(7), 1),
              EnableFuseScanData(0b01001001001000110010000000)) << "pseudo channel 0, lower banks";

    // Pseudo channel 1, lower banks
    //
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(5), PseudoChannel(1), Bank(3), 0),
              EnableFuseScanData(0b01001001010000110000000000)) << "pseudo channel 1, lower banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(5), PseudoChannel(1), Bank(4), 1),
              EnableFuseScanData(0b00101001011000001010000000)) << "pseudo channel 1, lower banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(5), PseudoChannel(1), Bank(7), 1),
              EnableFuseScanData(0b01001001011000110010000000)) << "pseudo channel 1, lower banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(3), PseudoChannel(1), Bank(7), 0),
              EnableFuseScanData(0b00101001001000110000000000)) << "pseudo channel 1, lower banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(0), PseudoChannel(1), Bank(7), 1),
              EnableFuseScanData(0b01001001011000110010000000)) << "pseudo channel 1, lower banks";

    // Pseudo channel 0, upper banks
    //
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(5), PseudoChannel(0), Bank(10), 0),
              EnableFuseScanData(0b01001010000000101000000000)) << "pseudo channel 0, upper banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(5), PseudoChannel(0), Bank(8), 1),
              EnableFuseScanData(0b00101010000000001010000000)) << "pseudo channel 0, upper banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(5), PseudoChannel(0), Bank(15), 1),
              EnableFuseScanData(0b01001010001000110010000000)) << "pseudo channel 0, upper banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(3), PseudoChannel(0), Bank(15), 0),
              EnableFuseScanData(0b00101010011000110000000000)) << "pseudo channel 0, upper banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(0), PseudoChannel(0), Bank(15), 1),
              EnableFuseScanData(0b01001010001000110010000000)) << "pseudo channel 0, upper banks";

    // Pseudo channel 1, upper banks
    //
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(5), PseudoChannel(1), Bank(10), 0),
              EnableFuseScanData(0b01001010010000101000000000)) << "pseudo channel 1, upper banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(5), PseudoChannel(1), Bank(8), 1),
              EnableFuseScanData(0b00101010010000001010000000)) << "pseudo channel 1, upper banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(5), PseudoChannel(1), Bank(15), 1),
              EnableFuseScanData(0b01001010011000110010000000)) << "pseudo channel 1, upper banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(0), HbmChannel(3), PseudoChannel(1), Bank(15), 0),
              EnableFuseScanData(0b00101010001000110000000000)) << "pseudo channel 1, upper banks";
    EXPECT_EQ(FuseScanLookupTable::GetWdrData(Stack(1), HbmChannel(0), PseudoChannel(1), Bank(15), 1),
              EnableFuseScanData(0b01001010011000110010000000)) << "pseudo channel 1, upper banks";
}
