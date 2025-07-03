#include <gtest/gtest.h>

#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <map>

#include "fs_lib.h"
#include "fs_chip_core.h"
#include "fs_chip_ada.h"
#include "fs_test_utils.h"

#define NUM_RANDOM_ITER 10


class FSAdaTest : public ::testing::Test {
    protected:
        fslib::AD102_t ad102_config;
        fslib::AD103_t ad103_config;
        fslib::AD104_t ad104_config;
        fslib::AD106_t ad106_config;
        fslib::AD107_t ad107_config;
        std::map<fslib::Chip, fslib::ADLit1Chip_t*> chips_in_litter = {
            {fslib::Chip::AD102, &ad102_config},
            {fslib::Chip::AD103, &ad103_config},
            {fslib::Chip::AD104, &ad104_config},
            {fslib::Chip::AD106, &ad106_config},
            {fslib::Chip::AD107, &ad107_config},};
        bool is_valid = false;
        std::string msg = "";
        fslib::FbpMasks fbConfig0 = {0};
        fslib::GpcMasks gpcConfig0 = {0};
};

// ================================================================================================
// Tests that demonstrate random physical floorsweeping functions
// ================================================================================================


TEST_F(FSAdaTest, AdaRandomGPCFS) {
    // Floorsweep one random GPC for each chip
    for (auto &chip : chips_in_litter) {
        for (int i = 0; i < NUM_RANDOM_ITER; i++) {
            if (chip.first == fslib::Chip::AD103) {
                gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
                gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
            }
            floorsweepRandomGPCs(gpcConfig0, chip.second->getChipPORSettings(), 1);
            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
            EXPECT_TRUE(is_valid) << "A chip with one GPC floorswept was considered invalid!"
                                << " msg: " << msg;
            msg = "";
            fbConfig0 = {0};    // resetting
            gpcConfig0 = {0};
        }
    }
}

TEST_F(FSAdaTest, AdaRandomFBPFS) {
    // Floorsweep one random FBP for each chip
    for (auto &chip : chips_in_litter) {
        for (int i = 0; i < NUM_RANDOM_ITER; i++) {
            if (chip.first == fslib::Chip::AD103) {
                gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
                gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
            }
            floorsweepRandomFBPs(fbConfig0, chip.second->getChipPORSettings(), 1);
            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
            EXPECT_TRUE(is_valid) << "A chip with one FBP floorswept was considered invalid!"
                                << " msg: " << msg;
            msg = "";
            fbConfig0 = {0};    // resetting
            gpcConfig0 = {0};
        }   
    }
}

TEST_F(FSAdaTest, AdaOneRandomGPCandFBPFS) {
    // Floorsweep one random GPC for each chip
    // Also floorsweep one random FBP from each chip
    for (auto &chip : chips_in_litter) {
        for (int i = 0; i < NUM_RANDOM_ITER; i++) {
            if (chip.first == fslib::Chip::AD103) {
                gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
                gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
            }
            floorsweepRandomGPCs(gpcConfig0, chip.second->getChipPORSettings(), 1);
            floorsweepRandomFBPs(fbConfig0, chip.second->getChipPORSettings(), 1);
            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
            EXPECT_TRUE(is_valid) << "A chip with one FBP floorswept was considered invalid!"
                                << " msg: " << msg;
            msg = "";
            fbConfig0 = {0};    // resetting
            gpcConfig0 = {0};
        }
    }
}

TEST_F(FSAdaTest, AdaOneGPCandFBPandPES) {
    // Floorsweep one random GPC for each chip
    // Also floorsweep one random FBP from each chip
    // Also floorsweep one random PES from an active GPC
    for (auto &chip : chips_in_litter) {
        for (int i = 0; i < NUM_RANDOM_ITER; i++) {
            if (chip.first == fslib::Chip::AD103) {
                gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
                gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
            }
            floorsweepRandomGPCs(gpcConfig0, chip.second->getChipPORSettings(), 1);
            floorsweepRandomPESs(gpcConfig0, chip.second->getChipPORSettings(), 1, true);
            floorsweepRandomFBPs(fbConfig0, chip.second->getChipPORSettings(), 1);
            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
            EXPECT_TRUE(is_valid) << "A chip with one FBP, one GPC and one PES " 
                                << "floorswept was considered invalid!"
                                << " msg: " << msg;
            msg = "";
            fbConfig0 = {0};    // resetting
            gpcConfig0 = {0};
        }
    }
}

TEST_F(FSAdaTest, AdaOneGPCLeft) {
    // Floorsweep all but one GPC
    for (auto &chip : chips_in_litter) {
        for (int i = 0; i < NUM_RANDOM_ITER; i++) {
            if (chip.first == fslib::Chip::AD103) {
                gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
                gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
            }
            floorsweepRandomGPCs(gpcConfig0, chip.second->getChipPORSettings(), chip.second->getChipPORSettings().num_gpcs - 1);
            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
            EXPECT_TRUE(is_valid) << "A chip with all but one GPC floorswept was considered invalid!"
                                << " msg: " << msg;
            msg = "";
            fbConfig0 = {0};    // resetting
            gpcConfig0 = {0};
        }
    }
}

TEST_F(FSAdaTest, AdaOneFBPLeft) {
    // Floorsweep all but one FBP 
    for (auto &chip : chips_in_litter) {
        for (int i = 0; i < NUM_RANDOM_ITER; i++) {
            if (chip.first == fslib::Chip::AD103) {
                gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
                gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
            }
            floorsweepRandomFBPs(fbConfig0, chip.second->getChipPORSettings(), chip.second->getChipPORSettings().num_fbps - 1);
            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
            EXPECT_TRUE(is_valid) << "A chip with all but one FBP floorswept was considered invalid!"
                                << " msg: " << msg;
            msg = "";
            fbConfig0 = {0};    // resetting
            gpcConfig0 = {0};
        }
    }
}

TEST_F(FSAdaTest, AdaOneFBPandGPCLeft) {
    // Floorsweep all but one FBP 
    for (auto &chip : chips_in_litter) {
        for (int i = 0; i < NUM_RANDOM_ITER; i++) {
            if (chip.first == fslib::Chip::AD103) {
                gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
                gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
            }
            floorsweepRandomGPCs(gpcConfig0, chip.second->getChipPORSettings(), chip.second->getChipPORSettings().num_gpcs - 1);
            floorsweepRandomFBPs(fbConfig0, chip.second->getChipPORSettings(), chip.second->getChipPORSettings().num_fbps - 1);
            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
            EXPECT_TRUE(is_valid) << "A chip with all but one FBP and GPC floorswept was considered invalid!"
                                << " msg: " << msg;
            msg = "";
            fbConfig0 = {0};    // resetting
            gpcConfig0 = {0};
        }
    }
}

TEST_F(FSAdaTest, AdaDeadChip) {
    // Floorsweep all components
    for (auto &chip : chips_in_litter) {
        for (int i = 0; i < NUM_RANDOM_ITER; i++) {
            if (chip.first == fslib::Chip::AD103) {
                gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
                gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
            }
            floorsweepRandomGPCs(gpcConfig0, chip.second->getChipPORSettings(), chip.second->getChipPORSettings().num_gpcs);
            floorsweepRandomFBPs(fbConfig0, chip.second->getChipPORSettings(), chip.second->getChipPORSettings().num_fbps);
            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
            EXPECT_FALSE(is_valid) << "A chip with everything floorswept was considered valid!";
            msg = "";
            fbConfig0 = {0};    // resetting
            gpcConfig0 = {0};
        }
    }
}


// New ad10x tests

TEST_F(FSAdaTest, AD103UnequalGPCs) {
    // Perfect config should fail since GPC 3 should only have 4 TPCs and 2 PESs

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD103, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << "No TPCs/PESs floorswept config for AD103"
                           << "considered valid for FUNC_VALID!";

    gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
    gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD103, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "Perfect config with 4 TPCs and 2 PESs in GPC 3 considered invalid! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSAdaTest, AD102FuncValid) {
    // Tests that are confirmed PRODUCT invalid, but functionally valid

    // Config with l2 slices not the same across LTCs
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,2,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
        fbConfig0.fbpaPerFbpMasks[i] = 0x0;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xc3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << "A configuration with different L2 slices across FBPs" 
                           << " was considered valid!";

    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Check 2 FBP FS + l2slice FS + ltc FS 
    // Not supported for PRODUCT AD102
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC, "
                          << "but two FPBs and one LTC floorswept "
                          << "was considered valid for mode PRODUCT!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Floorsweep all but two FPBs, enable 3 L2 slices per LTC
    // The l2 slice mask is the same across enabled FBPs, so this is invalid for PRODUCT
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC, "
                           << "with all but two FBPs floorswept, with an invalid "
                           << "L2 slice mask "
                           << "was considered valid for mode PRODUCT!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Floorsweep all but two FPBs, enable 3 L2 slices per LTC
    // This is func_valid, but not PRODUCT.
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC, "
                           << "with all but two FBPs floorswept, with an invalid "
                           << "L2 slice mask "
                           << "was considered valid for mode PRODUCT!"; 
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

        // Different number of L2 slices enabled across LTCs
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << "A configuration with different numbers of L2 slices "
                           << "across LTCs was considered valid!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Same idea, ilwert the masks.
    // This is also invalid since LTCs are enabled with no L2 slices.
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1f;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << "A configuration with different numbers of L2 slices "
                           << "across LTCs was considered valid!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // The  product_Valid  would have 1xROP for all GPCs or 2xROP across all the GPCs
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.ropPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "A configuration with one ROP floorswept in an active GPC "
                           << "was considered valid for PRODUCT!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

}

TEST_F(FSAdaTest, AD107FuncValid) {
    //------------------------------------------------------------------------------
    // Unit tests for AD107:
    // Ported from GA107 fs_lib tests
    //------------------------------------------------------------------------------


    // Disable one LTC and its L2 slices
    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xf;
    fbConfig0.ltcPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[1] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "Configuration with one LTC disabled " 
                          << "considered invalid for mode FUNC_VALID! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Disable one LTC, 3 L2 slices per live LTC
    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x1F;
    fbConfig0.ltcPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[1] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "Configuration with one LTC disabled and 3 L2 slices/LTC" 
                           << "considered invalid for mode FUNC_VALID. msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Disable 1 GPC and 2 TPCs
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "Configuration with one GPC and two TPCs floorswept "
                          << "considered invalid for FUNC_VALID! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSAdaTest, AdaBalancedLTCMode) {
    for (auto &chip : chips_in_litter) {
        for (uint32_t ltc = 0x0; ltc < (chip.second->getChipPORSettings().num_ltcs / chip.second->getChipPORSettings().num_fbps); ltc++) {
            if (chip.first == fslib::Chip::AD103) {
                gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
                gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
            } 
            for (uint32_t fbp = 0; fbp < chip.second->getChipPORSettings().num_fbps; fbp++) {
                fbConfig0.ltcPerFbpMasks[fbp] = 0x1 << ltc;
                fbConfig0.l2SlicePerFbpMasks[fbp] = 0xF << (4*ltc);
            }

            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
            EXPECT_TRUE(is_valid) << "Balanced LTC mode should be FUNC_VALID for ad10x! "
                        << " msg: " << msg; 
            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
            if (chip.first == fslib::Chip::AD107) {
                EXPECT_TRUE(is_valid) << "Balanced LTC mode should be PRODUCT valid for ad107! "
                    << " msg: " << msg;
            } else {
                EXPECT_FALSE(is_valid) << "Balanced LTC mode should not be PRODUCT valid for ad10x!";
            }

            msg = "";
            fbConfig0 = {0};    // resetting
            gpcConfig0 = {0};
        }

    }   
}

// ga10x tests ported to ad10x

TEST_F(FSAdaTest, AD102TestsFromGA102) {
    //------------------------------------------------------------------------------
    // Unit tests for AD102:
    // Ported from GA102 fs_lib tests
    //------------------------------------------------------------------------------


    // Perfect config
    fbConfig0 = {0};
    gpcConfig0 = {0};
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A perfect chip is considered invalid! msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    // Config with l2 slices not the same across LTCs
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,2,5}) {
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
        fbConfig0.fbpaPerFbpMasks[i] = 0x0;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.l2SlicePerFbpMasks[i] = 0xc3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid) << "A configuration with different L2 slices across FBPs" 
                           << " was considered valid!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Config with same number of l2 slices in all LTCs
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xc3;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 2 l2 slices per LTC was considered invalid! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // The same test but with the l2 mask ilwerted
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 2 l2 slices per LTC was considered invalid! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Disable 1 L2 slice per LTC.
    fbConfig0.fbpMask = 0x0;
    fbConfig0.l2SlicePerFbpMasks[0] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x42;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC was considered invalid! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // The same test with re-ordered masks
    fbConfig0.fbpMask = 0x0;
    fbConfig0.l2SlicePerFbpMasks[0] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x24;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC was considered invalid! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // The same test with re-ordered masks
    fbConfig0.fbpMask = 0x0;
    fbConfig0.l2SlicePerFbpMasks[0] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x18;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC was considered invalid! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // The same test with re-ordered masks
    fbConfig0.fbpMask = 0x0;
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x81;
    fbConfig0.l2SlicePerFbpMasks[2] = 0x42;
    fbConfig0.l2SlicePerFbpMasks[3] = 0x24;
    fbConfig0.l2SlicePerFbpMasks[4] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[5] = 0x81;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC was considered invalid! "
                          << "msg: " << msg;
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC, but one FPB "
                          << "floorswept was considered invalid! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Check 1 FBP FS + l2slice FS
    // Same test, different FBP disabled
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC, but one FPB "
                          << "floorswept was considered invalid! "
                          << "msg: " << msg;
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC, but two FPBs "
                          << "floorswept was considered invalid! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    
    // Check 2 FBP FS + l2slice FS
    // Same test
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC, but two FPBs "
                          << "floorswept was considered invalid! "
                          << "msg: " << msg;
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid) << "A configuration with 3 l2 slices per LTC, "
                           << "but two FPBs floorswept, with an invalid L2 slice "
                           << "pattern was considered valid for mode PRODUCT!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Check 2 FBP FS + l2slice FS, invalid pattern
    // Repeat with FUNC_VALID
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "A configuration with 3 l2 slices per LTC, "
                           << "but two FPBs floorswept, with an invalid L2 slice "
                           << "pattern was considered invalid for mode FUNC_VALID! "
                           << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    
    // Check 2 FBP FS + l2slice FS + ltc FS -- NOT SUPPORTED YET for GA102
    // Not supported for PRODUCT AD102
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid) << "A configuration with 3 l2 slices per LTC, "
                          << "but two FPBs and one LTC floorswept "
                          << "was considered valid for mode PRODUCT!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Floorsweep all but 2 FBPs
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "A configuration with 2 l2 slices per LTC, "
                          << "with all but two FBPs floorswept "
                          << "was considered invalid for mode PRODUCT! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Floorsweep all but two FPBs, enable 3 L2 slices per LTC
    // The l2 slice mask is the same across enabled FBPs, so this is invalid for PRODUCT
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid) << "A configuration with 3 l2 slices per LTC, "
                           << "with all but two FBPs floorswept, with an invalid "
                           << "L2 slice mask "
                           << "was considered valid for mode PRODUCT!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Floorsweep all but two FPBs, enable 3 L2 slices per LTC
    // This is func_valid, but not PRODUCT.
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid) << "A configuration with 3 l2 slices per LTC, "
                           << "with all but two FBPs floorswept, with an invalid "
                           << "L2 slice mask "
                           << "was considered valid for mode PRODUCT!"; 
    //Marked false for new product valid pattern, as of 03/05/20
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Different number of L2 slices enabled across LTCs
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid) << "A configuration with different numbers of L2 slices "
                           << "across LTCs was considered valid!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Same idea, ilwert the masks.
    // This is also invalid since LTCs are enabled with no L2 slices.
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0,1,2,3,4,5}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x1f;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid) << "A configuration with different numbers of L2 slices "
                           << "across LTCs was considered valid!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // fs0_rand_snap, enforcing l2slicemask and ltcmask mutually compatible
    // FS one LTC, keep 2 L2 slices per enabled FBP
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "A configuration with 2 l2 slices per LTC, "
                          << "with one LTC floorswept "
                          << "was considered invalid for mode FUNC_VALID! "
                          << "msg: " << msg; 
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // L2 slice mask that enabled L2 slices in FS'd LTC
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << "A configuration with 2 l2 slices per LTC, "
                           << "with one LTC floorswept and L2 slices enabled in it"
                           << "was considered valid for mode FUNC_VALID!"; 
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // One LTC enabled, but L2 slices enabled in dead LTC
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << "A configuration with one LTC enabled, "
                           << "but L2 slices enabled in a floorswept LTC "
                           << "was considered valid for mode FUNC_VALID!"; 
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // All L2 slices floorswept
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << "A configuration with all L2 slices floorswept "
                           << "was considered valid!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Enforcing half_fbpa mask is ignored during balanced-ltc floorsweeping
    // This is func_valid for all AD10x but PRODUCT for AD107
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
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "A configuration with 2 L2 slices per LTC " 
                          << "but one LTC floorswept per FBP "
                          << "was considered invalid! " 
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    // Valid config of TPC/PES
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "A configuration with one PES and two connected TPCs "
                          << "was considered invalid! msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // FS two ROPs in a GPC
    // The current design does not support floorsweeping both ROPs in a GPC. 
    // The chip would hang since screen work will still be sent to that GPC 
    // but that work cannot drain
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.ropPerGpcMasks[1] = 0x1;
    gpcConfig0.ropPerGpcMasks[4] = 0x3;
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << "A configuration with two ROPs floorswept in an active GPC "
                           << "was considered valid!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    // FS one ROP in one active GPC
    // Mixed mode(1xROP+2xROP)  would be functional_valid.
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.ropPerGpcMasks[1] = 0x1;
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "A configuration with one ROP floorswept in an active GPC "
                          << "was considered invalid for FUNC_VALID! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // The  product_Valid  would have 1xROP for all GPCs or 2xROP across all the GPCs
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.ropPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid) << "A configuration with one ROP floorswept in an active GPC "
                           << "was considered valid for PRODUCT!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // FS all TPCs in live GPC
    gpcConfig0.gpcMask = 0x7e;
    for (auto i : {1,2,3,4,5,6}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    //gpcConfig0.ropPerGpcMasks[1] = 0x1;
    gpcConfig0.tpcPerGpcMasks[0] = 0x3f;
    gpcConfig0.pesPerGpcMasks[0] = 0x7;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << "A configuration with all TPCs in a live GPC floorswept "
                           << "was considered valid!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSAdaTest, AD103TestsFromGA103) {
    //------------------------------------------------------------------------------
    // Unit tests for AD103:
    // Ported from GA103 fs_lib tests
    //------------------------------------------------------------------------------

    // 2-L2 slice per LTC
    // FS one LTC, FUNC_VALID but not PRODUCT
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

    gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
    gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD103, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "2-L2 slice configuration with one LTC "
                          << "considered invalid for FUNC_VALID! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    
    // FS one GPC and two TPCs
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;

    gpcConfig0.tpcPerGpcMasks[3] |= 0x30;
    gpcConfig0.pesPerGpcMasks[3] |= 0x4;  
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD103, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "Configuration with one GPC and two TPCs floorswept "
                          << "considered invalid for FUNC_VALID! " 
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSAdaTest, AD104TestsFromGA104) {
    //------------------------------------------------------------------------------
    // Unit tests for AD104:
    // Ported from GA104 fs_lib tests
    //------------------------------------------------------------------------------

    // 2 L2 slices per LTC, FS one LTC
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
        fslib::Chip::AD104, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "2-L2 slice configuration with one LTC floorswept "
                          << "considered invalid for FUNC_VALID! msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    

    // Disable all L2 slices
    fbConfig0.fbpMask = 0x0;    
    for (auto i : {0,1,2}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0xff;
    }
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD104, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_FALSE(is_valid) << "Configuration with all l2 slices disabled" 
                           << "considered valid!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    
    // FS one PES and its two connected TPCs, along with one GPC
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD104, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "Configuration with one GPC and one PES/TPC set floorswept "
                          << "considered invalid! msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

}

TEST_F(FSAdaTest, AD106TestsFromGA106) {
    //------------------------------------------------------------------------------
    // Unit tests for AD106:
    // Ported from GA106 fs_lib tests
    //------------------------------------------------------------------------------

    // Disable one LTC, with 2 L2 slices per LTC
    fbConfig0.fbpMask = 0x0;
    for (auto i : {0}) {
        fbConfig0.l2SlicePerFbpMasks[i] = 0x3c;
        fbConfig0.ltcPerFbpMasks[i] = 0x0;
        fbConfig0.halfFbpaPerFbpMasks[i] = 0x0;
        fbConfig0.fbioPerFbpMasks[i] = 0x0;
    }
    fbConfig0.l2SlicePerFbpMasks[1] = 0xfc;
    fbConfig0.ltcPerFbpMasks[1] = 0x2;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;
    fbConfig0.fbioPerFbpMasks[1] = 0x0;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD106, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "2-L2 slice configuration with one LTC floorswept "
                          << "considered invalid for FUNC_VALID! msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
    
    // One GPC disabled, two TPCs and a connected PES disabled
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x3f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD106, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "Configuration with one GPC and two TPCs floorswept "
                          << "considered invalid for FUNC_VALID! msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

TEST_F(FSAdaTest, AD107TestsFromGA107) {
    //------------------------------------------------------------------------------
    // Unit tests for AD107:
    // Ported from GA107 fs_lib tests
    //------------------------------------------------------------------------------

    // 2 L2 slices per LTC, FS one LTC
    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xff;
    fbConfig0.ltcPerFbpMasks[0] = fbConfig0.ltcPerFbpMasks[1] = 0x3;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid) << "Configuration with 2 L2 slices per LTC and one LTC "
                           << "floorswept considered valid for mode PRODUCT!";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};


    // Disable one LTC and its L2 slices
    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x0;
    fbConfig0.l2SlicePerFbpMasks[1] = 0xf;
    fbConfig0.ltcPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[1] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_TRUE(is_valid) << "Configuration with one LTC disabled " 
                          << "considered invalid for mode PRODUCT! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Disable one LTC, 3 L2 slices per live LTC
    fbConfig0.fbpMask = 0x0;       
    fbConfig0.l2SlicePerFbpMasks[0] = 0x18;
    fbConfig0.l2SlicePerFbpMasks[1] = 0x0F;
    fbConfig0.ltcPerFbpMasks[0] = 0x0;
    fbConfig0.ltcPerFbpMasks[1] = 0x1;
    fbConfig0.halfFbpaPerFbpMasks[0] = 0x0;
    fbConfig0.halfFbpaPerFbpMasks[1] = 0x1;

    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD107, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
    EXPECT_FALSE(is_valid) << "Configuration with one LTC disabled and 3 L2 slices/LTC" 
                           << "considered valid for mode PRODUCT";
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};

    // Disable 1 GPC and 2 TPCs
    gpcConfig0.gpcMask = 0x1;
    for (auto i : {0}) {
        gpcConfig0.tpcPerGpcMasks[i] = 0x1f;
        gpcConfig0.pesPerGpcMasks[i] = 0x7;
        gpcConfig0.ropPerGpcMasks[i] = 0x3;
    }
    gpcConfig0.tpcPerGpcMasks[1] = 0x3;
    gpcConfig0.pesPerGpcMasks[1] = 0x1;
    
    is_valid = fslib::IsFloorsweepingValid(
        fslib::Chip::AD107, gpcConfig0, fbConfig0, msg, fslib::FSmode::FUNC_VALID);
    EXPECT_TRUE(is_valid) << "Configuration with one GPC and two TPCs floorswept "
                          << "considered invalid for FUNC_VALID! "
                          << "msg: " << msg;
    msg = "";
    fbConfig0 = {0};    // resetting
    gpcConfig0 = {0};
}

// Test all the 3slice possibilities
TEST_F(FSAdaTest, Test_ada3slice) {
    for (int slice_offset = 0; slice_offset < 4; slice_offset++){
        msg = "";
        fbConfig0 = {0};    // resetting
        gpcConfig0 = {0};
        for (int fbp_idx = 0; fbp_idx < 6; fbp_idx++) {
            uint32_t slice_idx = (fbp_idx + slice_offset) % 4;
            fbConfig0.l2SlicePerFbpMasks[fbp_idx] |= (1 << slice_idx);
            fbConfig0.l2SlicePerFbpMasks[fbp_idx] |= (1 << (7 - slice_idx));
        }

        bool isvalid = fslib::IsFloorsweepingValid(fslib::Chip::AD102, gpcConfig0, fbConfig0, msg, fslib::FSmode::PRODUCT);
        EXPECT_TRUE(isvalid) << msg;
    }
}