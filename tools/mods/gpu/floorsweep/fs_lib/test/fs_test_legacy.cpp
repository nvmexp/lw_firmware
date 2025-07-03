#include <gtest/gtest.h>

#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <algorithm>
#include "fs_lib.h"
#include <iomanip>
#include <sstream>
#include <cassert>

#define MAX_GPCS 8
#define MAX_FBPS 12

class FSLegacyTest : public ::testing::Test {
    protected:
        void SetUp() override {
            is_valid = false;
            msg = "";
            count = 0;
            fbConfig0 = {0};
            gpcConfig0 = {0};
            for (auto i : {0}) {
                fbConfig0.fbioPerFbpMasks[i] = 0x0;
                fbConfig0.fbpaPerFbpMasks[i] = 0x0;
                fbConfig0.ltcPerFbpMasks[i] = 0x0;
            }
        }

        // void TearDown() override {}

        bool is_valid;
        std::string msg;
        unsigned int count;
        fslib::FbpMasks fbConfig0 = {0};
        fslib::GpcMasks gpcConfig0 = {0};

};

std::string print_gpc(fslib::GpcMasks gpcConfig) {
    std::stringstream outstr;
    outstr << std::hex ;
    outstr << "gpcMask: 0x" << gpcConfig.gpcMask << " | ";
    for (int i=0; i<MAX_GPCS; i++) {
        outstr << "GPC" << i << ": tpcPerGpcMask: 0x" << gpcConfig.tpcPerGpcMasks[i] << ", pesPerGpcMask: 0x" << gpcConfig.pesPerGpcMasks[i] << ", ropPerGpcMask: 0x" << gpcConfig.ropPerGpcMasks[i] << " | " ;
    }
    outstr << "\n";
    outstr << "------------------------------------------------\n";
    return outstr.str();
}

std::string print_fbp(fslib::FbpMasks fbConfig) {
    std::stringstream outstr;
    outstr << std::hex ;
    outstr << "fbpMask: 0x" << fbConfig.fbpMask << " | ";
    for (int i=0; i<MAX_FBPS; i++) {
        outstr << "FBP" << i << ": ltcPerFbpMask: 0x" << fbConfig.ltcPerFbpMasks[i] << ", l2SlicePerFbpMask: 0x" << fbConfig.l2SlicePerFbpMasks[i] << ", fbioPerFbpMask: 0x" << fbConfig.fbioPerFbpMasks[i] << " | " ;
    }
    outstr << "\n";
    outstr << "------------------------------------------------\n";
    return outstr.str();
}

TEST_F(FSLegacyTest, PerfectFBConfig) {
    //------------------------------------------------------------------------------
    // Perfect FB config
    // (Perfect is always valid)
    // Varying FBP masks, keeping GPC mask same = {0}
    //------------------------------------------------------------------------------
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_EQ(is_valid, true) << msg;
    msg = "";

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GV100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_EQ(is_valid, true) << msg;
    msg = "";

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GP104, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_EQ(is_valid, true) << msg;
}

TEST_F(FSLegacyTest, FBP0Disabled) {
    //------------------------------------------------------------------------------
    // FBP0 disabled consistently assuming 2 FBPA/FBIO per FBP, PRODUCT
    //------------------------------------------------------------------------------

    fbConfig0 = {0};
    fbConfig0.fbpMask = 0x1;
    for (auto i : {0}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }

    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << msg;  // true for FUNC_VALID, false for PRODUCT
}

TEST_F(FSLegacyTest, FBP0Disabled_IGNORE_FSLIB) {
    //------------------------------------------------------------------------------
    // FBP0 disabled consistently, IGNORE_FSLIB
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x1;
    for (auto i : {0}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }

    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::IGNORE_FSLIB);
    EXPECT_EQ(is_valid, true) << msg;  // Any config is true for IGNORE_FSLIB
}

TEST_F(FSLegacyTest, FBP0FBP3Disabled) {
    //------------------------------------------------------------------------------
    // FBP0 and FBP3 disabled consistently
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x9;
    for (auto i : {0, 3}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);  // true for FUNC_VALID, false for PRODUCT
    // Reason: HBM pairing is incosistent
}

TEST_F(FSLegacyTest, DisableFBP6and11) {
    //------------------------------------------------------------------------------
    // FBP6 and FBP11 disabled consistently
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x840;
    for (auto i : {6, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);  // true for PRODUCT, 5HBM case
    // Reason: L2NoC and HBM pairing is consistent
}

TEST_F(FSLegacyTest, DisableFBP6and11_FUNC) {
    //------------------------------------------------------------------------------
    // FBP6 and FBP11 disabled consistently
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x840;
    for (auto i : {6, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);  // true for PRODUCT, FUNC_VALID
    // Reason: L2NoC and HBM pairing is consistent
}

TEST_F(FSLegacyTest, 2HBMIlwalid) {
    //------------------------------------------------------------------------------
    // FBP 1,3,4,5,6,7,9,11 disabled => 0,2,8,10 enabled
    // Product invalid config for 2 hbms
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);  // false for PRODUCT, true for FUNC_VALID, and GA101
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};

    //------------------------------------------------------------------------------
    // FBP 1,3,4,5,6,7,9,11 disabled => 0,2,8,10 enabled
    // Functionally invalid config with 2 hbms
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);  // false for PRODUCT,FUNC_VALID, need to fix gpcmask
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};

    //------------------------------------------------------------------------------
    // FBP 1,3,4,5,6,7,9,11 disabled => 0,2,8,10 enabled
    // Functionally invalid config with 2 hbms
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA101, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    // Since GA101 is temporarily passthrough for ga101, this should pass
    EXPECT_TRUE(is_valid);  // false for GA101 product because GPCmask is {0}
}

TEST_F(FSLegacyTest, DisableFBP1and4) {

    //------------------------------------------------------------------------------
    // FBP1 and FBP4 disabled
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x12;
    for (auto i : {1, 4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);  // true for PRODUCT, 5 hbm
    // Reason: L2NoC and HBM pairing is cosistent

}

TEST_F(FSLegacyTest, DisableFBP0and2and3and5) {
    //------------------------------------------------------------------------------
    // FBP 0,2,3,5 disabled
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x2D;
    for (auto i : {0, 2, 3, 5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);  // true for PRODUCT, 4 hbm
    msg = "";
    fbConfig0 = {0};  // resetting
    gpcConfig0 = {0};

    // Reason: L2NoC and HBM pairing is cosistent
}

TEST_F(FSLegacyTest, DisableFBP9and1and4) {
    //------------------------------------------------------------------------------
    // FBP 0,1,4 disabled
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x13;
    for (auto i : {0, 1, 4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);  // false for PRODUCT, true for FUNC_VALID
    // Reason: L2NoC pairing is incosistent
}

TEST_F(FSLegacyTest, GA101Tests) {
    //------------------------------------------------------------------------------
    // GA101 config
    // GA101 tests, make sure to relax the l2noc rules
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    gpcConfig0.gpcMask = 0x3C;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {2, 3, 4, 5}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA101, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);  // true for GA101 PRODUCT
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // GA101 config as GA100 PRODUCT
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    gpcConfig0.gpcMask = 0x3C;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {2, 3, 4, 5}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);  // true for GA101 PRODUCT
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting


    //------------------------------------------------------------------------------
    // GA101 config as GA100 FUNC_VALID
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xAFA;
    gpcConfig0.gpcMask = 0x3C;
    for (auto i : {1, 3, 4, 5, 6, 7, 9, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {2, 3, 4, 5}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);  // true for GA101 PRODUCT
}

TEST_F(FSLegacyTest, UnsupportedLtcs) {
    //------------------------------------------------------------------------------
    // GA100 checking unsupported number of ltcs by AMAP
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {0,1,2}) {
        //fbConfig0.fbioPerFbpMasks[i] = 0x3;
        //fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);  // false as it breaks AMAP rule for supporting #ltcs
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // GA100 checking unsupported number of ltcs by AMAP
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x7bf;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {0,1,2,3,4,5,7,8,9,10}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);  // false as it breaks AMAP rule for supporting #ltcs
}

TEST_F(FSLegacyTest, OneuGPUGA100) {
    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfed;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    
    //------------------------------------------------------------------------------
    // One ugpu FS with half Hbmsite B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfef;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with wrong half Hbmsite B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xffd;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,4,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and E and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x7ad;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,7,8,9,10}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite A,B,E,F and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x2a8;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {3,5,7,9}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite A and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xffa;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,3,4,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1,2,3,4,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0xfe;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with All Hbmsites and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,4,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0xfe;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfed;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0xfe;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with half Hbmsite A and B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfee;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0xfe;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu with Hbmsite A,F and GPC0 and GPC7, func_valid and product_ilwalid in ga100, but product_valid in ga101
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xafa;
    gpcConfig0.gpcMask = 0x7e;
    for (auto i : {1,3,4,5,6,7,9,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1,2,3,4,5,6}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA101, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu with Hbmsite A and GPC0 and GPC7, func_valid and product_ilwalid in ga100, but product_valid in ga101
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xaff;
    gpcConfig0.gpcMask = 0x7e;
    for (auto i : {0,1,2,3,4,5,6,7,9,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1,2,3,4,5,6}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA101, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu with Hbmsite A and GPC0 and GPC7, func_valid and product_ilwalid in ga100, but product_valid in ga101
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xa;
    gpcConfig0.gpcMask = 0x3c;
    for (auto i : {1,3}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {2,3,4,5}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
}

TEST_F(FSLegacyTest, TwouGPUGA100) {
    //------------------------------------------------------------------------------
    // Two ugpu with Hbmsite A,B and GPC0 and GPC4, but A's complementary HBM C missing
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfe8;
    gpcConfig0.gpcMask = 0xee;
    for (auto i : {3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    for (auto i : {1,2,3,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0xfe;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
}

TEST_F(FSLegacyTest, FF0Disabled) {
    //------------------------------------------------------------------------------
    // FF0 disabled consistently (still assuming 2 FBPA/FBIO per FBP)
    //------------------------------------------------------------------------------
    fbConfig0 = {0};
    fbConfig0.fbpMask = 0x3;
    for (auto i : {0, 1}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    // Reason: It does not follow GA100 rules
}

TEST_F(FSLegacyTest, InconsistentGPC) {
    //------------------------------------------------------------------------------
    // inconsistent GPC floorsweeping (assuming 3 PES, 5 TPC per GPC)
    //------------------------------------------------------------------------------
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0, 1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0xff;
    }
    gpcConfig0.tpcPerGpcMasks[0] = 0xfe;  // Only making tpc0 of gpc0 enabled

    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GA100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
}

TEST_F(FSLegacyTest, InconsistentPES) {
    //------------------------------------------------------------------------------
    // Inconsistent PES
    //------------------------------------------------------------------------------
    gpcConfig0 = {0};
    gpcConfig0.gpcMask = 0xfe;
    gpcConfig0.pesPerGpcMasks[0] = 0x7;
    gpcConfig0.pesPerGpcMasks[1] = 0x7;
    gpcConfig0.pesPerGpcMasks[2] = 0x7;
    gpcConfig0.pesPerGpcMasks[3] = 0x7;
    gpcConfig0.pesPerGpcMasks[4] = 0x7;
    gpcConfig0.pesPerGpcMasks[5] = 0x7;
    gpcConfig0.pesPerGpcMasks[6] = 0x7;
    gpcConfig0.pesPerGpcMasks[7] = 0x7;

    gpcConfig0.tpcPerGpcMasks[0] = 0xfe;
    gpcConfig0.tpcPerGpcMasks[1] = 0xff;
    gpcConfig0.tpcPerGpcMasks[2] = 0xff;
    gpcConfig0.tpcPerGpcMasks[3] = 0xff;
    gpcConfig0.tpcPerGpcMasks[4] = 0xff;
    gpcConfig0.tpcPerGpcMasks[5] = 0xff;
    gpcConfig0.tpcPerGpcMasks[6] = 0xff;
    gpcConfig0.tpcPerGpcMasks[7] = 0xff;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
}

TEST_F(FSLegacyTest, GA102Tests) {
    //------------------------------------------------------------------------------
    // Unit tests for GA10x:
    //------------------------------------------------------------------------------

    fbConfig0 = {0};
    gpcConfig0 = {0};
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,2,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
        fbConfig0.fbpaPerFbpMasks[i] = 0x0;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xc3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xc3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;

    fbConfig0.l2SlicePerFbpMasks[0] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x42;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;

    fbConfig0.l2SlicePerFbpMasks[0] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x24;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;

    fbConfig0.l2SlicePerFbpMasks[0] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x18;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;

    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x81;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Check 1 FBP FS + l2slice FS
    fbConfig0.fbpMask = 0x4;

    fbConfig0.fbioPerFbpMasks[2] = 0x1;
    fbConfig0.fbpaPerFbpMasks[2] = 0x1;
    fbConfig0.ltcPerFbpMasks[2] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x18;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Check 1 FBP FS + l2slice FS
    fbConfig0.fbpMask = 0x1;

    fbConfig0.fbioPerFbpMasks[0] = 0x1;
    fbConfig0.fbpaPerFbpMasks[0] = 0x1;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x81;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    // Check 2 FBP FS + l2slice FS
    fbConfig0.fbpMask = 0x14;
    for (auto i : {2,4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x24;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    
    // Check 2 FBP FS + l2slice FS
    fbConfig0.fbpMask = 0x24;
    for (auto i : {2,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[5] = 0xff;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    // Check 2 FBP FS + l2slice FS, invalid pattern
    fbConfig0.fbpMask = 0x24;
    for (auto i : {2,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[5] = 0xff;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    
    // Check 2 FBP FS + l2slice FS + ltc FS -- NOT SUPPORTED YET
    fbConfig0.fbpMask = 0x14;
    for (auto i : {2,4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
    }
    fbConfig0.ltcPerFbpMasks[0] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x1;

    fbConfig0.l2SlicePerFbpMasks[0] = 0x1f;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x24;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    
    fbConfig0.fbpMask = 0x3a;
    for (auto i : {0,2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
    }
    for (auto i : {1,3,4,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    fbConfig0.fbpMask = 0x3a;
    for (auto i : {0,2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x21;
    }
    for (auto i : {1,3,4,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    fbConfig0.fbpMask = 0x3a;
    for (auto i : {0}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x24;
    }
    for (auto i : {2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x81;
    }
    for (auto i : {1,3,4,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.fbpaPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);  //Marked false for new product valid pattern, as of 03/05/20
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1f;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // fs0_rand_snap, enforcing l2slicemask and ltcmask mutually compatible
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[5] = 0xfc;
    fbConfig0.ltcPerFbpMasks[5] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x1;
    fbConfig0.fbioPerFbpMasks[5] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[5] = 0x3c;
    fbConfig0.ltcPerFbpMasks[5] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x1;
    fbConfig0.fbioPerFbpMasks[5] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
    }
    fbConfig0.l2SlicePerFbpMasks[5] = 0x1;
    fbConfig0.ltcPerFbpMasks[5] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x1;
    fbConfig0.fbioPerFbpMasks[5] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    fbConfig0.fbpMask = 0x0;
    for (auto i : {1,2,3,4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
    }
    fbConfig0.l2SlicePerFbpMasks[5] = 0xff;
    fbConfig0.ltcPerFbpMasks[5] = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x0;
    fbConfig0.fbioPerFbpMasks[5] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Enforcing half_fbpa mask is ignored during balanced-ltc floorsweeping
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xf3;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[5] = 0xcf;
    fbConfig0.ltcPerFbpMasks[5] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x0;
    fbConfig0.fbioPerFbpMasks[5] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.ropPerGpcMasks[1] = 0x1;
    gpcConfig0.ropPerGpcMasks[4] = 0x3;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.ropPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.ropPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    gpcConfig0.gpcMask = 0x7e;
    for (auto i : {1,2,3,4,5,6}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    //gpcConfig0.ropPerGpcMasks[1] = 0x1;
    gpcConfig0.tpcPerGpcMasks[0] = 0x3f;
    gpcConfig0.pesPerGpcMasks[0] = 0x7;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSLegacyTest, GA103Tests) {
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[4] = 0xfc;
    fbConfig0.ltcPerFbpMasks[4] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[4] = 0x1;
    fbConfig0.fbioPerFbpMasks[4] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA103, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    

    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA103, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSLegacyTest, GA104Tests) {
    // GA104 unit tests
#ifdef LW_MODS_SUPPORTS_GA104_MASKS
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[3] = 0xfc;
    fbConfig0.ltcPerFbpMasks[3] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[3] = 0x1;
    fbConfig0.fbioPerFbpMasks[3] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA104, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    

    // GA104 as2 test which caught the MODS bug
    fbConfig0.fbpMask = 0x0;    
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA104, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0xf;
        gpcConfig0.pesPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA104, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

#endif // GA104 tests
}

TEST_F(FSLegacyTest, GA106Tests) {
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[2] = 0xfc;
    fbConfig0.ltcPerFbpMasks[2] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[2] = 0x1;
    fbConfig0.fbioPerFbpMasks[2] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA106, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    
    
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA106, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSLegacyTest, GA107Tests) {
    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xff;
    fbConfig0.ltcPerFbpMasks[0] = fbConfig0.ltcPerFbpMasks[1] = 0x3;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};



    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xf;
    fbConfig0.ltcPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[1] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x0F;
    fbConfig0.ltcPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[1] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSLegacyTest, G000DummyTest) {
    // Random wrong config intentionally created to test the disabled chips
    fbConfig0.fbpMask = 0x33;
    for (auto i : {0,1,2,3,4,15}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1f;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::G000, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSLegacyTest, HalfFBPAMaskTest) {
// Stray half_fbpa mask should throw error
#ifdef LW_MODS_SUPPORTS_HALFFBPA_MASK
    fbConfig0.fbpMask = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[5] = 0x1;
    fbConfig0.ltcPerFbpMasks[5] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xFF;
    fbConfig0.fbioPerFbpMasks[0] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;
    fbConfig0.ltcPerFbpMasks[1] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x10;
    fbConfig0.fbioPerFbpMasks[1] = 0x0;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xFF;
    fbConfig0.fbioPerFbpMasks[0] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;
    fbConfig0.ltcPerFbpMasks[1] = 0x2;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xf0;
    fbConfig0.fbioPerFbpMasks[1] = 0x0;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    fbConfig0.fbpMask = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xFF;
    fbConfig0.fbioPerFbpMasks[0] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x0;
    fbConfig0.ltcPerFbpMasks[1] = 0x2;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xf0;
    fbConfig0.fbioPerFbpMasks[1] = 0x0;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
#endif      
}

TEST_F(FSLegacyTest, GA10BTests) {
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0xf;
        gpcConfig0.pesPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0xc;
    gpcConfig0.pesPerGpcMasks[1] = 0x2;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA10B, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0xf;
        gpcConfig0.pesPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x8;
    gpcConfig0.pesPerGpcMasks[1] = 0x2;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GA10B, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSLegacyTest, NewGH100Tests) {
    fbConfig0.fbpMask = 0x12;
    gpcConfig0.gpcMask = 0x3c;
    for (auto i : {1,4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {2,3,4,5}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting


    fbConfig0.fbpMask = 0x12;
    gpcConfig0.gpcMask = 0x3c;
    for (auto i : {1,4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    fbConfig0.ltcPerFbpMasks[0] = 0x1;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xf;
    fbConfig0.fbioPerFbpMasks[0] = 0x0;
    fbConfig0.fbpaPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[3] = 0x2;
    fbConfig0.l2SlicePerFbpMasks[3] = 0xf0;
    fbConfig0.fbioPerFbpMasks[3] = 0x0;
    fbConfig0.fbpaPerFbpMasks[3] = 0x0;

    for (auto i : {2,3,4,5}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << msg;
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    fbConfig0.fbpMask = 0xfc0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        //fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {0,1,2,3,4,5,6,7}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1f8;
        gpcConfig0.pesPerGpcMasks[i] = 0x6;
        gpcConfig0.cpcPerGpcMasks[i] = 0x6;
        gpcConfig0.ropPerGpcMasks[i] = 0x1;
    }
    gpcConfig0.ropPerGpcMasks[0] = 0x0;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << msg;
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    gpcConfig0.tpcPerGpcMasks[0] = 0x7;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    gpcConfig0.ropPerGpcMasks[0] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    gpcConfig0.pesPerGpcMasks[0] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    // cpc with no tpcs
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    gpcConfig0.cpcPerGpcMasks[1] = 0x0;
    gpcConfig0.tpcPerGpcMasks[1] = 0x7;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    // gpc with no tpcs
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    gpcConfig0.tpcPerGpcMasks[1] = 0x1ff;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    // don't floorsweep FBIOs unless the FBP is floorswept
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    fbConfig0.ltcPerFbpMasks[0] = 0x1;
    fbConfig0.ltcPerFbpMasks[3] = 0x2;
    fbConfig0.fbioPerFbpMasks[0] = 0x0;
    fbConfig0.fbioPerFbpMasks[3] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xf;
    fbConfig0.l2SlicePerFbpMasks[3] = 0xf0;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << msg;
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    // floorsweep FBIOs if the FBP is floorswept
    fbConfig0.fbpMask = 0x9;
    gpcConfig0.gpcMask = 0x0;
    fbConfig0.ltcPerFbpMasks[0] = 0x3;
    fbConfig0.ltcPerFbpMasks[3] = 0x3;
    fbConfig0.fbioPerFbpMasks[0] = 0x3;
    fbConfig0.fbioPerFbpMasks[3] = 0x3;
    fbConfig0.l2SlicePerFbpMasks[0] = 0xff;
    fbConfig0.l2SlicePerFbpMasks[3] = 0xff;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    // LTC cannot be disabled without also disabling slices!
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    fbConfig0.ltcPerFbpMasks[0] = 0x1;
    fbConfig0.ltcPerFbpMasks[3] = 0x2;
    fbConfig0.fbioPerFbpMasks[0] = 0x0;
    fbConfig0.fbioPerFbpMasks[3] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[0] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x0;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
}

TEST_F(FSLegacyTest, GA100TestsForGH100) {
    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and GPC0
    // Second uGPU is technically enabled
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfed;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    
    //------------------------------------------------------------------------------
    // One ugpu FS with half Hbmsite B and GPC0
    // GH100 disallows 1uGPU configs  
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfef;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with wrong half Hbmsite B and GPC0
    // Second uGPU is still enabled 
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xffd;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,4,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and E and GPC0
    // GH100 disallows 1uGPU configs
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x7ad;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,7,8,9,10}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite A,C,D,F and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x852;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,4,6,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite A and GPC0
    // GH100 disallows 1uGPU configs
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xffa;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,3,4,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1,2,3,4,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with All Hbmsites and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,4,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with Hbmsite B and GPC0
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfed;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu FS with half Hbmsite A and B and GPC0
    // GH100 disallows 1uGPU configs
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfee;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // Two ugpu with Hbmsite A,B and GPC0 and GPC4, but A's complementary HBM C missing
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xfe8;
    gpcConfig0.gpcMask = 0xee;
    for (auto i : {3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1,2,3,5,6,7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;


    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // One ugpu with Hbmsite A and GPC0 and GPC7, func_valid and product_ilwalid in ga100, but product_valid in ga101
    //------------------------------------------------------------------------------
    fbConfig0.fbpMask = 0xa;
    gpcConfig0.gpcMask = 0x3c;
    for (auto i : {1,3}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {2,3,4,5}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // FF0 disabled consistently (still assuming 2 FBPA/FBIO per FBP)
    //------------------------------------------------------------------------------
    fbConfig0 = {0};
    fbConfig0.fbpMask = 0x3;
    for (auto i : {0, 1}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    // Reason: It does not follow GH100 rules

    //------------------------------------------------------------------------------
    // inonsistent GPC floorsweeping (assuming 3 PES, 5 TPC per GPC)
    //------------------------------------------------------------------------------
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0, 1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[0] = 0x7;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;  // Only making tpc0 of gpc0 valid
    gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid =
        fslib::IsFloorsweepingValid(fslib::Chip::GH100, gpcConfig0, fbConfig0,
                                    msg, fslib::FSmode::FUNC_VALID);
    // FIXME ignore all GPC FS failures for GH100 
    // EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //------------------------------------------------------------------------------
    // Inconsistent PES
    //------------------------------------------------------------------------------
    gpcConfig0 = {0};
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {0, 1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
    gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
    gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
}

TEST_F(FSLegacyTest, GH100L2SliceTests) {
    //l2slice tests
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //94 slices
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //92 slices, different FBP
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1,8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {4,7}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //92 slices, but same FBP
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x0C;
    }
    for (auto i : {4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xC0;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //90 slices, different FBP
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1,8,2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {4,7,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    
    //76 slices, FBP + slice FS
    fbConfig0.fbpMask = 0x840;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {6, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xFF;
    }
    for (auto i : {1,8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {4,7}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
    
    //84 slices, LTC + slice FS
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {6}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
    }
    for (auto i : {11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x2;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
    }
    for (auto i : {1,8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {4,7}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //68 slices, FBP + LTC + slice FS
    fbConfig0.fbpMask = 0x12;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {1, 4}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xFF;
    }
    for (auto i : {6}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
    }
    for (auto i : {11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x2;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
    }
    for (auto i : {0,8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x2;
    }
    for (auto i : {3,7}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x20;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //64 slices, FBP + LTC FS, no slice FS
    fbConfig0.fbpMask = 0x840;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {6, 11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xFF;
    }
    for (auto i : {1,8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    for (auto i : {4,7}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
     is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //72 slices, only LTC FS, no slice FS
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {0,8,9}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    for (auto i : {3,7,10}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //72 slices, only LTC FS, no slice FS, wrong LTCs
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {0,8,11}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.fbioPerFbpMasks[i] = 0x2;
    }
    for (auto i : {3,7,10}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //92 slices, only LTC FS, no slice FS, AMAP should complain
    fbConfig0.fbpMask = 0x0;
    gpcConfig0.gpcMask = 0x0;
    for (auto i : {8}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF0;
        fbConfig0.ltcPerFbpMasks[i] = 0x2;
        fbConfig0.fbioPerFbpMasks[i] = 0x2;
    }

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
}

TEST_F(FSLegacyTest, GH100uGPUTests) {
    //single uGPU
    // GH100 disallows 1uGPU configs
    fbConfig0.fbpMask = 0xfee;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }

        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //single uGPU + LTC FS
    // GH100 disallows 1uGPU configs
    fbConfig0.fbpMask = 0xfee;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
    for (auto i : {0}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    } 
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting

    //single uGPU + LTC FS + slice FS
    fbConfig0.fbpMask = 0xfee;
    gpcConfig0.gpcMask = 0xfe;
    for (auto i : {1,2,3,5,6,7,8,9,10,11}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x3;
        fbConfig0.fbpaPerFbpMasks[i] = 0x3;
        fbConfig0.ltcPerFbpMasks[i] = 0x3;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    for (auto i : {1, 2, 3, 4, 5, 6, 7}) {
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.tpcPerGpcMasks[i] = 0x1ff;
        gpcConfig0.cpcPerGpcMasks[i] = 0x7;
    }
    for (auto i : {0}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xF;
        fbConfig0.ltcPerFbpMasks[i] = 0x1;
        fbConfig0.fbioPerFbpMasks[i] = 0x1;
    } 
     for (auto i : {4}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1;
    } 
        gpcConfig0.pesPerGpcMasks[0] = 0x6;
        gpcConfig0.tpcPerGpcMasks[0] = 0x1fe;
        gpcConfig0.cpcPerGpcMasks[0] = 0x6;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);
    msg = "";
    fbConfig0 = {0};   // resetting
    gpcConfig0 = {0};  // resetting
}

// CPC0 and CPC1 cannot both be floorswept in gh100
TEST_F(FSLegacyTest, GH100_2cpcs) {
    gpcConfig0.tpcPerGpcMasks[2] = 0x3f;
    gpcConfig0.cpcPerGpcMasks[2] = 0x3;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid);

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
}

// GH100 products need at least 2 CPCs enabled
TEST_F(FSLegacyTest, GH100_2cpcs_product) {
    gpcConfig0.tpcPerGpcMasks[2] = 0x1c7;
    gpcConfig0.cpcPerGpcMasks[2] = 0x5;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << msg;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::GH100, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid);
}