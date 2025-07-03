#include <gtest/gtest.h>

#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <algorithm>
#include "fs_lib.h"
#include "fs_chip_core.h"
#include "fs_chip_factory.h"
#include "fs_test_utils.h"
#include "fs_chip_hopper.h"
#include <iomanip>
#include <sstream>
#include <cassert>
#include <random>
#include "SkylineSpareSelector.h"

namespace fslib {

class FSChipDownbinHopper : public ::testing::Test {
protected:
    void SetUp() override {
        fbpSettings = {0};
        gpcSettings = {0};
        msg = "";
    }
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    std::string msg;
};


TEST_F(FSChipDownbinHopper, Test_getHBMEnableCount) {
    {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->returnModuleList(module_type_id::fbp)[0]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::fbp)[3]->setEnabled(false);

        EXPECT_EQ(dynamic_cast<GH100_t*>(fsChip.get())->getHBMEnableCount(), 6);
    }
    {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->returnModuleList(module_type_id::fbp)[1]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::fbp)[4]->setEnabled(false);

        EXPECT_EQ(dynamic_cast<GH100_t*>(fsChip.get())->getHBMEnableCount(), 5);
    }
}

TEST_F(FSChipDownbinHopper, Test_isInSKU1) {
    gpcSettings.tpcPerGpcMasks[1] = 0xcc;
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 8};
    skuconfig.fsSettings["gpc"] = {8, 8};
    EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
}

TEST_F(FSChipDownbinHopper, Test_isInSKU2) {
    for (int i = 0; i < 8; i++){
        gpcSettings.tpcPerGpcMasks[i] = 0x1;
    }
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {64, 64, 8};
    skuconfig.fsSettings["gpc"] = {8, 8};

    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
}

TEST_F(FSChipDownbinHopper, Test_isInSKU3) {
    for (int i = 0; i < 8; i++){
        gpcSettings.tpcPerGpcMasks[i] = 0x1;
    }
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    {   //fail. mustEnableList is wrong
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 8, UNKNOWN_ENABLE_COUNT, 0, 0, {27}, {28, 29, 30}};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    }
    {   //pass. check mustEnableList, mustDisableList
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 8, UNKNOWN_ENABLE_COUNT, 0, 0, {28}, {27, 0, 9, 18}};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    }
    {   //fail because enableCountPerGroup is wrong
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, UNKNOWN_ENABLE_COUNT, 7, 0, 0, {28}, {27, 0, 9, 18}};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    }
    {   //fail because minEnablePerGroup is wrong
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 9, UNKNOWN_ENABLE_COUNT, 0, 0, {28}, {27, 0, 9, 18}};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    }
}

TEST_F(FSChipDownbinHopper, Test_isInSKU4) {
    gpcSettings.tpcPerGpcMasks[1] = 0x1;
    gpcSettings.gpcMask = 0x4;
    gpcSettings.tpcPerGpcMasks[2] = 0x1ff;
    gpcSettings.cpcPerGpcMasks[2] = 0x7;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    {   //pass
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {62, 62, 6};
        skuconfig.fsSettings["gpc"] = {7, 8};
        EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_isInSKUAsymmetric) {
    for (int i = 0; i < 8; i++){
        gpcSettings.tpcPerGpcMasks[i] = 0x1;
    }
    gpcSettings.tpcPerGpcMasks[3] = 0x3; // make gpc3 have 2 disabled TPCs
    gpcSettings.tpcPerGpcMasks[2] = 0x0; // make gpc2 have 0 disabled TPCs

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    {   //fail. the chip has 64 TPCs, but does not have the correct TPC per GPC count
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 8};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    }
    {   //pass. this time we do have enough TPCs per GPC
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 7};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    }
    {   //fail. the SKU requires symmetric GPCs
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, UNKNOWN_ENABLE_COUNT, 8};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    }
}

TEST_F(FSChipDownbinHopper, Test_moduleCanFitSKU1) {
    for (int i = 0; i < 8; i++){
        gpcSettings.tpcPerGpcMasks[i] = 0x1;
    }
    gpcSettings.tpcPerGpcMasks[3] = 0x3; // make gpc3 have 2 disabled TPCs

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    FSModule_t* gpc3 = fsChip->returnModuleList(module_type_id::gpc)[3];

    //fail. gpc3 has 7 enabled TPCs, but we need min 8
    EXPECT_FALSE(gpc3->moduleCanFitSKU(module_type_id::tpc, {64, 64, 8}, msg));
    EXPECT_FALSE(gpc3->moduleCanFitSKU(module_type_id::tpc, {64, 64, UNKNOWN_ENABLE_COUNT, 8}, msg));
    
    //pass. gpc3 has 7 enabled TPCs, but we only need 7
    EXPECT_TRUE(gpc3->moduleCanFitSKU(module_type_id::tpc, {64, 64, 7}, msg)) << msg;
    EXPECT_TRUE(gpc3->moduleCanFitSKU(module_type_id::tpc, {64, 64, UNKNOWN_ENABLE_COUNT, 7}, msg)) << msg;

    //fail. gpc3 has 7 enabled TPCs, we need 7, but we also must disable a good one via mustDisableList
    EXPECT_FALSE(gpc3->moduleCanFitSKU(module_type_id::tpc, {64, 64, 7, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {31}}, msg));
    EXPECT_FALSE(gpc3->moduleCanFitSKU(module_type_id::tpc, {64, 64, UNKNOWN_ENABLE_COUNT, 7, 0, 0, {}, {31}}, msg));

    //pass. gpc3 has 7 enabled TPCs, we need 7, but we need to make sure the disableList is compatible
    EXPECT_TRUE(gpc3->moduleCanFitSKU(module_type_id::tpc, {64, 64, 7, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {27}}, msg)) << msg;
    EXPECT_TRUE(gpc3->moduleCanFitSKU(module_type_id::tpc, {64, 64, UNKNOWN_ENABLE_COUNT, 7, 0, 0, {}, {27}}, msg)) << msg;

    //pass. don't specify the group enable count
    EXPECT_TRUE(gpc3->moduleCanFitSKU(module_type_id::tpc, {64, 64}, msg)) << msg;
}

// These tests are the same cases as above, but use the higher level function
TEST_F(FSChipDownbinHopper, Test_moduleCanFitSKU2) {
    for (int i = 0; i < 8; i++){
        gpcSettings.tpcPerGpcMasks[i] = 0x1;
    }
    gpcSettings.tpcPerGpcMasks[3] = 0x3; // make gpc3 have 2 disabled TPCs

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    FSModule_t* gpc3 = fsChip->returnModuleList(module_type_id::gpc)[3];

    {   //fail. gpc3 has 7 enabled TPCs, but we need min 8
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 8};
        EXPECT_FALSE(gpc3->moduleCanFitSKU(skuconfig, msg));
    }
    {   //fail. gpc3 has 7 enabled TPCs, but we need min 7
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 7};
        EXPECT_TRUE(gpc3->moduleCanFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_moduleIsInSKU1) {
    for (int i = 0; i < 8; i++){
        gpcSettings.tpcPerGpcMasks[i] = 0x1;
    }
    gpcSettings.tpcPerGpcMasks[3] = 0x3; // make gpc3 have 2 disabled TPCs

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    FSModule_t* gpc3 = fsChip->returnModuleList(module_type_id::gpc)[3];

    //fail. gpc3 has 7 enabled TPCs, but we need min 8
    EXPECT_FALSE(gpc3->moduleIsInSKU(module_type_id::tpc, {64, 64, 8}, msg));

    //fail. gpc3 has 7 enabled TPCs, but we need exactly 8
    EXPECT_FALSE(gpc3->moduleIsInSKU(module_type_id::tpc, {64, 64, UNKNOWN_ENABLE_COUNT, 8}, msg));
    
    //pass. gpc3 has 7 enabled TPCs, but we only need 7
    EXPECT_TRUE(gpc3->moduleIsInSKU(module_type_id::tpc, {64, 64, 7}, msg)) << msg;

    //pass gpc3 has 7 enabled TPCs, and we need exactly 7
    EXPECT_TRUE(gpc3->moduleIsInSKU(module_type_id::tpc, {64, 64, UNKNOWN_ENABLE_COUNT, 7}, msg)) << msg;

    //pass. gpc3 has 7 enabled TPCs, but we only need 6
    EXPECT_TRUE(gpc3->moduleIsInSKU(module_type_id::tpc, {64, 64, 7}, msg)) << msg;

    //fail gpc3 has 7 enabled TPCs, and we need exactly 6
    EXPECT_FALSE(gpc3->moduleIsInSKU(module_type_id::tpc, {64, 64, UNKNOWN_ENABLE_COUNT, 6}, msg)) << msg;

    //pass. don't specify the group enable count
    EXPECT_TRUE(gpc3->moduleIsInSKU(module_type_id::tpc, {64, 64}, msg)) << msg;
}

TEST_F(FSChipDownbinHopper, Test_moduleIsInSKU2) {
    for (int i = 0; i < 8; i++){
        gpcSettings.tpcPerGpcMasks[i] = 0x1;
    }
    gpcSettings.tpcPerGpcMasks[3] = 0x3; // make gpc3 have 2 disabled TPCs

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    FSModule_t* gpc3 = fsChip->returnModuleList(module_type_id::gpc)[3];
    {   //fail. gpc3 has 7 enabled TPCs, but we need min 8
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 8};
        EXPECT_FALSE(gpc3->moduleCanFitSKU(skuconfig, msg));
    }
    {   //fail. gpc3 has 7 enabled TPCs, but we need min 7
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 7};
        EXPECT_TRUE(gpc3->moduleCanFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKU) {
    for (int i = 0; i < 8; i++){
        gpcSettings.tpcPerGpcMasks[i] = 0x1;
    }
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    {   //fail. mustEnableList conflicts with the defect in gpc3 tpc0
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 8, UNKNOWN_ENABLE_COUNT, 0, 0, {27}, {28, 29, 30}};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
    {   //pass. the SKU requires fewer TPCs per GPC (and total) than we have
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {56, 56, 7};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUAssymetric) {
    for (int i = 0; i < 8; i++){
        gpcSettings.tpcPerGpcMasks[i] = 0x1;
    }
    gpcSettings.tpcPerGpcMasks[2] = 0x0; // make gpc2 have 0 disabled TPCs

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    {   //pass. the chip has 65 TPCs, we can disable 1 and fit
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, 8};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
    {   //pass. the chip has 65 TPCs, we can disable 1 and fit
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, UNKNOWN_ENABLE_COUNT, 8};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUImpossibleSKU1) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    {   //fail. this is an impossible SKU. 8*8 > 56
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {56, 56, 8};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
    {   //fail. this is an impossible SKU. 8*8 > 63
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {63, 63, UNKNOWN_ENABLE_COUNT, 8};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
    {   //fail. this is an impossible SKU. 8*8 < 65
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {65, 65, UNKNOWN_ENABLE_COUNT, 8};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
    {   //fail. this is an impossible SKU. can't have 8 GPCs if we also have mustDisable gpc2
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {72, 72, 9};
        skuconfig.fsSettings["gpc"] = {8, 8, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {2}};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
    {   //fail. this is an impossible SKU. can't have 5 or less GPCs if we also have mustEnable 0,1,2,3,4,5
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["gpc"] = {4, 5, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {0, 1, 2, 3, 4, 5}, {}};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
    {   //fail. this is an impossible SKU. can't have GPC3 in both the mustEnable and mustDisable list
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["gpc"] = {1, 8, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {0, 1, 2, 3}, {3, 4, 5}};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
    {   // mustEnableCount conflicts with enableCountPerGroup in GPC0
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {1, 72, UNKNOWN_ENABLE_COUNT, 6, 0, 0, {0, 1, 2, 3, 4, 5, 6}, {}};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
    {   // mustEnableCount conflicts with enableCountPerGroup in GPC2
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {1, 72, UNKNOWN_ENABLE_COUNT, 5, 0, 0, {18, 20, 21, 22, 23, 24}, {}};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
    {   // TPC mustEnableList conflicts with GPC mustDisableList
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {1, 72, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {9, 10}, {}};
        skuconfig.fsSettings["gpc"] = {1, 8, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {1}};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
}


TEST_F(FSChipDownbinHopper, Test_canFitSKUCorner1) {
    for (int i = 0; i < 8; i++){
        gpcSettings.tpcPerGpcMasks[i] = 0x1f0; // all gpcs have 5 disabled TPCs
    }

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   // This SKU can't fit because the mustDisableList would force GPC1 to have too few TPCs
        // GPC1 has 0..4 enabled, but the sku wants to disable 0,1, then we wouldn't have enough enabled to meet minEnablePerGroup=4
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {32, 32, 4, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {9, 10}};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUCorner2) {

    gpcSettings.tpcPerGpcMasks[1] = 0xf; // make gpc1 have 4 disabled TPCs
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   // This SKU can't fit because the mustDisableList would force the chip to have too few TPCs
        // GPC1 has 0..4 enabled, but because the sku wants to disable global TPC 0,1,2,3,45, we wouldn't have enough enabled to get to 64
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {0, 1, 2, 3, 45}};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
    {   // This SKU can fit because the mustDisableList overlaps with the disabled tpcs enough
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {64, 64, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {9, 10, 11, 12, 45}};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100slices1) {
    fbpSettings.l2SlicePerFbpMasks[0] = 0x01;
    fbpSettings.l2SlicePerFbpMasks[3] = 0x10;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["l2slice"] = {94, 94, 7};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100slices2) {
    fbpSettings.l2SlicePerFbpMasks[0] = 0x01;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["l2slice"] = {94, 94, 7};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_94slices_error) {
    fbpSettings.l2SlicePerFbpMasks[0] = 0x01;
    fbpSettings.l2SlicePerFbpMasks[1] = 0x01;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["l2slice"] = {94, 94, 7};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_uGPUPairing_slice) {
    fbpSettings.l2SlicePerFbpMasks[0] = 0x01;
    fbpSettings.l2SlicePerFbpMasks[1] = 0x01;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   // This one is bad because we need to disablethe pairs, and then we'd be over the limit
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["l2slice"] = {94, 94, 7};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
    {   // This one is ok because we can disable the pairs and still be valid
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["l2slice"] = {92, 92, 7};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
    {   // This one is ok, because the mustDisable slice is a pair of a defective slice
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["l2slice"] = {92, 92, 7, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {28}};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
    {   // Can't fit this  SKU because the SKU requires slice 29 to be disabled, which, if we do its pair,
        // would cause FBP0 to have two slices disabled
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["l2slice"] = {92, 92, 7, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {29}};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_uGPUPairing_slice2) {
    fbpSettings.l2SlicePerFbpMasks[0] = 0x01;
    fbpSettings.l2SlicePerFbpMasks[3] = 0x20;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   // This config is not allowed because it would require two slices in one FBP to be disabled
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["l2slice"] = {92, 92, 7};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
}


TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_uGPUPairing_ltc) {
    fbpSettings.ltcPerFbpMasks[0] = 0x01;
    fbpSettings.ltcPerFbpMasks[7] = 0x01;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["ltc"] = {22, 22, 1};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["ltc"] = {20, 20, 1};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_uGPUPairing_fbp) {
    fbpSettings.fbpMask = 0x3;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["fbp"] = {10, 10};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["ltc"] = {8, 8};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_uGPUPairing_fbp2) {
    fbpSettings.fbpMask = 0x9;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["fbp"] = {10, 10};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["ltc"] = {8, 8};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_92slices) {
    fbpSettings.l2SlicePerFbpMasks[0] = 0x01;
    fbpSettings.l2SlicePerFbpMasks[1] = 0x01;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["l2slice"] = {92, 92, 7};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_FSGPC) {
    gpcSettings.gpcMask = 0x4;
    gpcSettings.tpcPerGpcMasks[2] = 0x1ff;
    gpcSettings.cpcPerGpcMasks[2] = 0x7;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    {   //pass
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {62, 62, 6};
        skuconfig.fsSettings["gpc"] = {7, 8};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_tpc_count) {
    for (int i : {0, 3, 4, 6}){
        gpcSettings.tpcPerGpcMasks[i] = 0x1; // some gpcs have 8 TPCs enabled
    }

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {60, 60};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
    {   
        FUSESKUConfig::SkuConfig skuconfig; // you can't get 60 TPCs with 6 GPCs. 6*9
        skuconfig.fsSettings["tpc"] = {60, 60};
        skuconfig.fsSettings["gpc"] = {6, 6};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
    {   
        FUSESKUConfig::SkuConfig skuconfig; // this is possible. you can get 60 TPCs
        skuconfig.fsSettings["tpc"] = {60, 60};
        skuconfig.fsSettings["gpc"] = {7, 7};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg));
    }
    {   
        FUSESKUConfig::SkuConfig skuconfig; // this is not possible. you cannot get 61 TPCs. too many would be FSed after FSing a GPC with 8
        skuconfig.fsSettings["tpc"] = {61, 61};
        skuconfig.fsSettings["gpc"] = {7, 7};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_tpc_count_gpc0) {
    
    gpcSettings.tpcPerGpcMasks[0] = 0x70; // gpc0 has the least enabled tpcs
    gpcSettings.tpcPerGpcMasks[4] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig; // this is not possible. you cannot get 61 TPCs, since you have to floorsweep gpc1
        skuconfig.fsSettings["tpc"] = {61, 61};
        skuconfig.fsSettings["gpc"] = {7, 7};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_tpc_count_gpc_fs) {
    
    gpcSettings.tpcPerGpcMasks[1] = 0xf0;
    gpcSettings.tpcPerGpcMasks[4] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig; // this is possible, floorsweep gpc1 and now all gpcs have at least 6 TPCs
        skuconfig.fsSettings["tpc"] = {50, 50, 6};
        skuconfig.fsSettings["gpc"] = {7, 7};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
    {   
        FUSESKUConfig::SkuConfig skuconfig; // this is possible, floorsweep gpc1 and you have 62 TPCs
        skuconfig.fsSettings["tpc"] = {62, 62, 6};
        skuconfig.fsSettings["gpc"] = {7, 7};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_l2slice_count_fbp_fs) {
    
    fbpSettings.l2SlicePerFbpMasks[4] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig; // should be possible, disable fbp 1&4
        skuconfig.fsSettings["l2slice"] = {80, 80, UNKNOWN_ENABLE_COUNT, 8};
        skuconfig.fsSettings["fbp"] = {10, 10};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

// detect that the mustEnableList implicitly conflicts with existing defects
TEST_F(FSChipDownbinHopper, Test_canFitSKUgh100_must_enable_list_conflict) {
    gpcSettings.gpcMask = 0x2;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    {   
        FUSESKUConfig::SkuConfig skuconfig; // this is not possible, gpc1 is dead
        skuconfig.fsSettings["tpc"] = {1, 72};
        skuconfig.fsSettings["gpc"] = {1, 8, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {1}, {}};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
    {   
        FUSESKUConfig::SkuConfig skuconfig; // this is possible, gpc1 is dead so tpc9 is dead
        skuconfig.fsSettings["tpc"] = {1, 72, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {9}, {}};
        skuconfig.fsSettings["gpc"] = {1, 8};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_1) {
    // This config is not valid because it has unpaired slices
    fbpSettings.l2SlicePerFbpMasks[0] = 0x01;
    fbpSettings.l2SlicePerFbpMasks[1] = 0x01;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_EQ(fsChip->returnModuleList(module_type_id::fbp)[3]->returnModuleList(module_type_id::l2slice)[4]->getEnabled(), true);
    EXPECT_EQ(fsChip->returnModuleList(module_type_id::fbp)[4]->returnModuleList(module_type_id::l2slice)[4]->getEnabled(), true);
    fsChip->funcDownBin();
    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_EQ(fsChip->returnModuleList(module_type_id::fbp)[3]->returnModuleList(module_type_id::l2slice)[4]->getEnabled(), false);
    EXPECT_EQ(fsChip->returnModuleList(module_type_id::fbp)[4]->returnModuleList(module_type_id::l2slice)[4]->getEnabled(), false);
    EXPECT_EQ(fsChip->getNumEnabledLTCs(), 24);
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 92);
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_2) {
    // To make this config valid, we have to go to LTC FS, since there's too many bad slices for slice FS
    fbpSettings.l2SlicePerFbpMasks[0] = 0x01;
    fbpSettings.l2SlicePerFbpMasks[1] = 0x01;
    fbpSettings.l2SlicePerFbpMasks[2] = 0x01;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    fsChip->funcDownBin();
    EXPECT_EQ(fsChip->getNumEnabledLTCs(), 18);
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 72);
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::fbp)), 12);
    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_3) {
    // To make this config valid, we have to go to LTC FS, since there's too many bad slices in one LTC
    fbpSettings.l2SlicePerFbpMasks[0] = 0x0a;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    fsChip->funcDownBin();
    EXPECT_EQ(fsChip->getNumEnabledLTCs(), 22);
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 88);
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::fbp)), 12);
    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_4) {
    // To make this config valid, we have to go to LTC FS, since there's too many bad slices in one LTC after fixing the unpaired slices
    fbpSettings.l2SlicePerFbpMasks[0] = 0x01;
    fbpSettings.l2SlicePerFbpMasks[3] = 0x20;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    fsChip->funcDownBin();
    EXPECT_EQ(fsChip->getNumEnabledLTCs(), 22);
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 88);
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::fbp)), 12);
    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_5) {
    // To make this config valid, we have to go to LTC FS, since there's too many bad slices in one FBP
    fbpSettings.l2SlicePerFbpMasks[0] = 0x11;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    fsChip->funcDownBin();
    EXPECT_EQ(fsChip->getNumEnabledLTCs(), 20);
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 80);
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::fbp)), 10);
    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_6) {
    // To make this config valid, we have to go to FBP FS, since there's too many bad slices in one FBP after fixing the unpaired slices
    fbpSettings.l2SlicePerFbpMasks[0] = 0x01;
    fbpSettings.l2SlicePerFbpMasks[3] = 0x01;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    fsChip->funcDownBin();
    EXPECT_EQ(fsChip->getNumEnabledLTCs(), 20);
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 80);
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::fbp)), 10);
    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_7) {
    // GPCs with no TPCs should be floorwswept
    gpcSettings.tpcPerGpcMasks[1] = 0x1ff;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::cpc)), 3);
    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::cpc)), 0);
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_8) {
    // TPC and CPCs in a disabled GPC should be floorswept
    gpcSettings.gpcMask = 0x2;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)), 9);
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::cpc)), 3);
    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)), 0);
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::cpc)), 0);
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_9) {
    // TPCs in a disabled CPC should be floorswept
    gpcSettings.cpcPerGpcMasks[1] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)), 9);
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::cpc)), 2);
    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[2]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[3]->getEnabled());
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::cpc)), 2);
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_10) {
    // CPCs with no TPCs should be floorswept
    gpcSettings.tpcPerGpcMasks[1] = 0x7;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)), 6);
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::cpc)), 3);
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::cpc)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());
    EXPECT_EQ(getNumEnabled(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::cpc)), 2);
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_11) {
    // FBPs with no LTCs should be floorswept
    fbpSettings.ltcPerFbpMasks[1] = 0x3;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::ltc)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::ltc)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_12) {
    // FBPs with no L2Slices should be floorswept
    fbpSettings.l2SlicePerFbpMasks[1] = 0xff;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_13) {
    // LTCs with no L2Slices should be floorswept
    fbpSettings.l2SlicePerFbpMasks[1] = 0xf;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::ltc)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::ltc)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_14) {
    // Slices should be floorswept when the LTC is floorswept
    fbpSettings.ltcPerFbpMasks[1] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[1]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[2]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[3]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::ltc)[0]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[2]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[3]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::ltc)[0]->getEnabled());
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_15) {
    // FBPs with no FBIOs should be floorswept
    fbpSettings.fbioPerFbpMasks[1] = 0x3;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[4]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::fbio)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::fbio)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[4]->returnModuleList(module_type_id::fbio)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[4]->returnModuleList(module_type_id::fbio)[1]->getEnabled());
}

TEST_F(FSChipDownbinHopper, Test_funcDownBingh100_16) {
    // If one FBIO is floorswept, the other one in the same FBP must be floorswept also
    fbpSettings.fbioPerFbpMasks[1] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[4]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::fbio)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::fbio)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[4]->returnModuleList(module_type_id::fbio)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[4]->returnModuleList(module_type_id::fbio)[1]->getEnabled());
}

TEST_F(FSChipDownbinHopper, Test_downBin_gh100_gpc_1) {
    
    //Floorsweep the GPC with the fewest enabled TPCs
    init_array(gpcSettings.tpcPerGpcMasks, {0x1, 0xf0, 0x1, 0x1, 0x1, 0x1, 0x1}); //gpc1 has the fewest enabled TPCs
    gpcSettings.cpcPerGpcMasks[1] = 0x2;
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig; // floorsweep a GPC
    skuconfig.fsSettings["tpc"] = {2, 72, 1};
    skuconfig.fsSettings["gpc"] = {7, 7};

    fsChip->downBin(skuconfig, msg);
    GpcMasks actualGPCDownBinMask = {0};
    fsChip->getGPCMasks(actualGPCDownBinMask);

    // It should be the GPC with the fewest TPCs that gets floorswept. GPC1
    GpcMasks expectedGpcDownBinnedSettings = {0};
    expectedGpcDownBinnedSettings.gpcMask = 0x2;
    init_array(expectedGpcDownBinnedSettings.tpcPerGpcMasks, {0x1, 0x1ff, 0x1, 0x1, 0x1, 0x1, 0x1});
    expectedGpcDownBinnedSettings.cpcPerGpcMasks[1] = 0x7;
    EXPECT_EQ(actualGPCDownBinMask, expectedGpcDownBinnedSettings);

}

TEST_F(FSChipDownbinHopper, Test_downBin_gh100_tpc_1) {

    // floorsweep one TPC from the GPC with the most enabled TPCs
    init_array(gpcSettings.tpcPerGpcMasks, {0x1, 0x70, 0x1, 0x1, 0x1, 0x1, 0x1, 0x0}); //gpc7 has the fewest enabled TPCs
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 1};
    skuconfig.fsSettings["gpc"] = {8, 8};

    fsChip->downBin(skuconfig, msg);
    GpcMasks actualGPCDownBinMask = {0};
    fsChip->getGPCMasks(actualGPCDownBinMask);

    GpcMasks expectedGpcDownBinnedSettings = {0};
    expectedGpcDownBinnedSettings.gpcMask = 0x0;
    init_array(expectedGpcDownBinnedSettings.tpcPerGpcMasks, {0x1, 0x70, 0x1, 0x1, 0x1, 0x1, 0x1, 0x1});

    EXPECT_EQ(actualGPCDownBinMask, expectedGpcDownBinnedSettings) << msg;
}

TEST_F(FSChipDownbinHopper, Test_downBin_gh100_tpc_2) {

    // floorsweep one TPC from the GPC with the most enabled TPCs
    init_array(gpcSettings.tpcPerGpcMasks, {0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x11, 0x49}); //gpc6 has fewest tpc defects

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig; // floorsweep one TPC from the CPC with the most good TPCs
    skuconfig.fsSettings["tpc"] = {48, 48, 5};
    skuconfig.fsSettings["gpc"] = {8, 8};

    EXPECT_TRUE(fsChip->downBin(skuconfig, msg));
    GpcMasks actualGPCDownBinMask = {0};
    fsChip->getGPCMasks(actualGPCDownBinMask);

    GpcMasks expectedGpcDownBinnedSettings = {0};
    expectedGpcDownBinnedSettings.gpcMask = 0x0;

    // we want to floorsweep TPC6 out of GPC6. This is to balance the GPCs and CPCs within GPC6
    init_array(expectedGpcDownBinnedSettings.tpcPerGpcMasks, {0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x51, 0x49});

    EXPECT_EQ(actualGPCDownBinMask, expectedGpcDownBinnedSettings) << msg;
}

TEST_F(FSChipDownbinHopper, Test_downBin_gh100_fbp_most_defective_slices) {

    // floorsweep the FBP with the most defective slices
    fbpSettings.l2SlicePerFbpMasks[8] = 0x1;
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["fbp"] =     {10, 10};
    skuconfig.fsSettings["l2slice"] = {80, 80};
    
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg));

    FbpMasks actualFBPDownBinMask = {0};
    fsChip->getFBPMasks(actualFBPDownBinMask);

    FbpMasks expectedFbpDownBinnedSettings = {0};
    expectedFbpDownBinnedSettings.fbpMask = 0x180; // FBP 7 and 8 floorswept
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[7] = 0xff;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[8] = 0xff;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[7] = 0x3;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[8] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[7] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[8] = 0x3;

    EXPECT_EQ(actualFBPDownBinMask, expectedFbpDownBinnedSettings) << msg;
}

TEST_F(FSChipDownbinHopper, Test_downBin_gh100_fbp_most_defective_ltcs) {

    // floorsweep the FBP with the most defective slices
    fbpSettings.ltcPerFbpMasks[9] = 0x1;
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["fbp"] = {10, 10};
    skuconfig.fsSettings["ltc"] = {20, 20};
    
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg));

    FbpMasks actualFBPDownBinMask = {0};
    fsChip->getFBPMasks(actualFBPDownBinMask);

    FbpMasks expectedFbpDownBinnedSettings = {0};
    expectedFbpDownBinnedSettings.fbpMask = 0x600; // FBP 9 and 10 floorswept
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[9] = 0xff;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[10] = 0xff;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[9] = 0x3;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[10] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[9] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[10] = 0x3;

    EXPECT_EQ(actualFBPDownBinMask, expectedFbpDownBinnedSettings) << msg;
}

// prefer whole sites when there are no memsys defects
TEST_F(FSChipDownbinHopper, Test_downBin_gh100_fbp_prefer_whole_sites) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["fbp"] = {10, 10};
    
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg));

    FbpMasks actualFBPDownBinMask = {0};
    fsChip->getFBPMasks(actualFBPDownBinMask);

    FbpMasks expectedFbpDownBinnedSettings = {0};
    expectedFbpDownBinnedSettings.fbpMask = 0x840; // FBP 6 and 11 floorswept
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[6] = 0xff;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[11] = 0xff;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[6] = 0x3;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[11] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[6] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[11] = 0x3;

    EXPECT_EQ(actualFBPDownBinMask, expectedFbpDownBinnedSettings) << msg;
}

// prefer whole sites when going down to 8 FBPs
TEST_F(FSChipDownbinHopper, Test_downBin_gh100_prefer_whole_sites_defective_slice) {

    fbpSettings.ltcPerFbpMasks[5] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["fbp"] = {8, 8};
    
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg));

    FbpMasks actualFBPDownBinMask = {0};
    fsChip->getFBPMasks(actualFBPDownBinMask);

    FbpMasks expectedFbpDownBinnedSettings = {0};
    expectedFbpDownBinnedSettings.fbpMask = 0x2d; // FBP 0, 2, 3, 5 disabled
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[0] = 0xff;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[2] = 0xff;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[3] = 0xff;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[5] = 0xff;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[0] = 0x3;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[2] = 0x3;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[3] = 0x3;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[5] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[0] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[2] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[3] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[5] = 0x3;

    EXPECT_EQ(actualFBPDownBinMask, expectedFbpDownBinnedSettings) << msg;
}

// If the chip is perfect, floorsweep slices out of HBM E and B
TEST_F(FSChipDownbinHopper, Test_downBin_gh100_92_slice) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["l2slice"] = {92, 92, 7};
    
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg));

    FbpMasks actualFBPDownBinMask = {0};
    fsChip->getFBPMasks(actualFBPDownBinMask);

    FbpMasks expectedFbpDownBinnedSettings = {0};
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[1]  = 0x1;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[4]  = 0x10;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[6]  = 0x1;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[11] = 0x10;

    EXPECT_EQ(actualFBPDownBinMask, expectedFbpDownBinnedSettings) << msg;
}

// If the chip is perfect, floorsweep slices out of HBM E
TEST_F(FSChipDownbinHopper, Test_downBin_gh100_94_slice) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["l2slice"] = {94, 94, 7};
    
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg));

    FbpMasks actualFBPDownBinMask = {0};
    fsChip->getFBPMasks(actualFBPDownBinMask);

    FbpMasks expectedFbpDownBinnedSettings = {0};
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[6] = 0x1;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[11] = 0x10;

    EXPECT_EQ(actualFBPDownBinMask, expectedFbpDownBinnedSettings) << msg;
}

// If the chip is perfect, floorsweep LTCs out of HBM E for 88 slice mode
TEST_F(FSChipDownbinHopper, Test_downBin_gh100_88_slice) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["ltc"] = {22, 22, 1};
    
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg));

    FbpMasks actualFBPDownBinMask = {0};
    fsChip->getFBPMasks(actualFBPDownBinMask);

    FbpMasks expectedFbpDownBinnedSettings = {0};
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[6] = 0xf;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[11] = 0xf0;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[6] = 0x1;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[11] = 0x2;

    EXPECT_EQ(actualFBPDownBinMask, expectedFbpDownBinnedSettings) << msg;
}

// If the chip is perfect, floorsweep FBPs out of HBM E for 80 slice mode
TEST_F(FSChipDownbinHopper, Test_downBin_gh100_80_slice) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["ltc"] = {20, 20, 1};
    skuconfig.fsSettings["fbp"] = {10, 12};
    
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg));

    FbpMasks actualFBPDownBinMask = {0};
    fsChip->getFBPMasks(actualFBPDownBinMask);

    FbpMasks expectedFbpDownBinnedSettings = {0};
    expectedFbpDownBinnedSettings.fbpMask = 0x840;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[6] = 0xff;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[11] = 0xff;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[6] = 0x3;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[11] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[6] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[11] = 0x3;

    EXPECT_EQ(actualFBPDownBinMask, expectedFbpDownBinnedSettings) << msg;
}

// Make sure mustEnableList is followed for HBM FS
TEST_F(FSChipDownbinHopper, Test_downBin_gh100_must_enable_HBM_E) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["ltc"] = {20, 20, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {12, 23}, {}};
    skuconfig.fsSettings["fbp"] = {10, 10};
    
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;

    FbpMasks actualFBPDownBinMask = {0};
    fsChip->getFBPMasks(actualFBPDownBinMask);

    FbpMasks expectedFbpDownBinnedSettings = {0};
    expectedFbpDownBinnedSettings.fbpMask = 0x12;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[1] = 0xff;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[4] = 0xff;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[1] = 0x3;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[4] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[1] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[4] = 0x3;

    EXPECT_EQ(actualFBPDownBinMask, expectedFbpDownBinnedSettings) << msg;
}

// Make sure mustEnableList is followed combined with prefer wholes
TEST_F(FSChipDownbinHopper, Test_downBin_gh100_must_enable_HBM_E_prefer_wholes) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["ltc"] = {20, 20};
    skuconfig.fsSettings["fbp"] = {10, 12, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {6, 11}, {}};
    
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;

    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;

    FbpMasks actualFBPDownBinMask = {0};
    fsChip->getFBPMasks(actualFBPDownBinMask);

    FbpMasks expectedFbpDownBinnedSettings = {0};
    expectedFbpDownBinnedSettings.fbpMask = 0x12;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[1] = 0xff;
    expectedFbpDownBinnedSettings.l2SlicePerFbpMasks[4] = 0xff;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[1] = 0x3;
    expectedFbpDownBinnedSettings.ltcPerFbpMasks[4] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[1] = 0x3;
    expectedFbpDownBinnedSettings.fbioPerFbpMasks[4] = 0x3;

    EXPECT_EQ(actualFBPDownBinMask, expectedFbpDownBinnedSettings);
}

// Try floorsweeping a perfect chip into sku a, 62 tpcs, 7 or 8 gpcs, 80 slices
// HBM E should be picked
TEST_F(FSChipDownbinHopper, Test_downBinFromPerfect) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62};
    skuconfig.fsSettings["gpc"] = {7, 8};
    skuconfig.fsSettings["ltc"] = {20, 20};
    skuconfig.fsSettings["fbp"] = {10, 12};
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg));
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;

    EXPECT_EQ(fsChip->getTotalEnableCount(module_type_id::tpc), 62);
    EXPECT_EQ(fsChip->getTotalEnableCount(module_type_id::ltc), 20);
    EXPECT_EQ(fsChip->getTotalEnableCount(module_type_id::l2slice), 80);
    EXPECT_EQ(fsChip->getTotalEnableCount(module_type_id::fbp), 10);

    EXPECT_FALSE(fsChip->getModuleFlatIdx(module_type_id::fbp, 6)->getEnabled());
    EXPECT_FALSE(fsChip->getModuleFlatIdx(module_type_id::fbp, 11)->getEnabled());
}

TEST_F(FSChipDownbinHopper, Test_gh100_ugpu_imbalance_skuIsPossible) {
    {   // This should be possible, if you must floorsweep 2 TPCs in uGPU0 , it's still possible to floorsweep a TPC in uGPU1
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {69, 69, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {0, 1}, 2, {}};
        EXPECT_TRUE(fsChip->skuIsPossible(skuconfig, msg)) << msg;
    }
    {   // impossible because the mustDisableList doesn't allow for any balancing on the other uGPU
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {69, 69, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {4, 5, 6}, 2, {}};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
}

// Test that the max ugpu imbalance for TPCs is enforced
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_imbalance_1) {
    {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, 5, {}};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
    {   // this should be false, because ugpu0 has 6 tpcs floorswept, but ugpu1 has 0 tpcs floorswept
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[1]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[2]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[3]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[4]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::gpc)[6]->returnModuleList(module_type_id::tpc)[0]->setEnabled(false);
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, 5, {}};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
    {   // this should be true, because ugpu0 has 5 tpcs floorswept, but ugpu1 has 0 tpcs floorswept
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[1]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[2]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[4]->setEnabled(false);
        fsChip->returnModuleList(module_type_id::gpc)[6]->returnModuleList(module_type_id::tpc)[0]->setEnabled(false);
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, 5, {}};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_imbalance_2) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[4]->setEnabled(false);
    fsChip->returnModuleList(module_type_id::gpc)[6]->returnModuleList(module_type_id::tpc)[3]->setEnabled(false);
    FUSESKUConfig::SkuConfig skuconfig;

    {
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {4}, 2, {}};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
    {
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {4}, 3, {}};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
    {   // this config should be possible because the mustDisableList overlaps with one of the disabled TPCs
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {13}, 2, {}};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }   
}

// 0s in the skyline
TEST_F(FSChipDownbinHopper, Test_gh100_getOptimalTPCDownBinForSpares_1) {
    {   
        std::vector<uint32_t> vgpcSkyline = { 0, 0, 6, 8, 9, 9, 9, 9 };
        std::vector<uint32_t> aliveTPCs = { 6, 9, 9, 9, 9, 9, 9, 9 };
        std::vector<uint32_t> bestSolution;
        std::string msg;

        int result = getOptimalTPCDownBinForSpares(vgpcSkyline, aliveTPCs, 2, 0, 9, 0, bestSolution, msg);
        EXPECT_TRUE(result > 0) << msg;
    }
}

// 0 in the skyline, but it's impossible to fit because of min tpc per gpc
TEST_F(FSChipDownbinHopper, Test_gh100_getOptimalTPCDownBinForSpares_2) {
    {   
        std::vector<uint32_t> vgpcSkyline = { 0, 6, 8, 8, 8, 8, 8, 9 };
        std::vector<uint32_t> aliveTPCs = { 6, 6, 8, 8, 8, 8, 8, 9 };
        std::vector<uint32_t> bestSolution;

        int result = getOptimalTPCDownBinForSpares(vgpcSkyline, aliveTPCs, 2, 0, 9, 4, bestSolution, msg);
        EXPECT_FALSE(result > 0);
    }
}

// What happens when you have a 0 in the skyline? It is allowed to FS that GPC
TEST_F(FSChipDownbinHopper, Test_gh100_getOptimalTPCDownBinForSpares_3) {
    {
        std::vector<uint32_t> vgpcSkyline = { 0, 5, 8, 8, 8, 8, 8, 9 };
        std::vector<uint32_t> aliveTPCs = { 6, 7, 8, 8, 8, 8, 8, 9 };
        std::vector<uint32_t> bestSolution;

        int result = getOptimalTPCDownBinForSpares(vgpcSkyline, aliveTPCs, 2, 0, 9, 4, bestSolution, msg);
        EXPECT_TRUE(result > 0) << msg;
        EXPECT_EQ(bestSolution, (std::vector<uint32_t>{ 0,7,8,8,8,8,8,9 }));
    }
}

// This function cannot support skylines of different size from the aliveTPCs list
TEST_F(FSChipDownbinHopper, Test_gh100_getOptimalTPCDownBinForSpares_4) {
    {
        std::vector<uint32_t> vgpcSkyline = { 5, 8, 8, 8, 8, 8, 9 };
        std::vector<uint32_t> aliveTPCs = { 6, 7, 8, 8, 8, 8, 8, 9 };
        std::vector<uint32_t> bestSolution;

        int result = getOptimalTPCDownBinForSpares(vgpcSkyline, aliveTPCs, 2, 0, 9, 4, bestSolution, msg);
        EXPECT_FALSE(result > 0);
    }
}

TEST_F(FSChipDownbinHopper, Test_gh100_skyline_skuIsPossible) {
    {   // Not possible because the max tpc count isn't enough to fit the skyline
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 7, 7, 9, 9, 9, 9, 9}};
        EXPECT_FALSE(fsChip->skuIsPossible(skuconfig, msg));
    }
}

TEST_F(FSChipDownbinHopper, Test_gh100_isInSKU_imbalance_1) {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, 0};

    {   // imbalance of 0
        init_array(gpcSettings.tpcPerGpcMasks, {0x70, 0x3, 0x70, 0x3, 0x0, 0x0, 0x0, 0x0});
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->applyGPCMasks(gpcSettings);
        EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    }
    {   // imbalance of 2
        init_array(gpcSettings.tpcPerGpcMasks, {0x30, 0x3, 0x170, 0x3, 0x0, 0x0, 0x0, 0x0});
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->applyGPCMasks(gpcSettings);
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    }
}

TEST_F(FSChipDownbinHopper, Test_gh100_isInSKU_imbalance_2) {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, 2};

    {   // imbalance of 2
        init_array(gpcSettings.tpcPerGpcMasks, {0x70, 0x1, 0x70, 0x3, 0x1, 0x0, 0x0, 0x0});
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->applyGPCMasks(gpcSettings);
        EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    }
    {   // imbalance of 4 not in the sku
        init_array(gpcSettings.tpcPerGpcMasks, {0x70, 0x0, 0x70, 0x3, 0x3, 0x0, 0x0, 0x0});
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->applyGPCMasks(gpcSettings);
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    }
}

// just test that UNKNOWN_ENABLE_COUNT does not reject anything
TEST_F(FSChipDownbinHopper, Test_gh100_isInSKU_imbalance_3) {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 5};

    {   // imbalance of 4
        init_array(gpcSettings.tpcPerGpcMasks, {0x70, 0x0, 0x70, 0x3, 0x3, 0x0, 0x0, 0x0});
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->applyGPCMasks(gpcSettings);
        EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_gh100_isInSKU_skyline_1) {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 7, 8, 8, 8, 8, 8}};

    {   // skyline can't fit because there's too many gpcs with more than 2 dead tpcs
        init_array(gpcSettings.tpcPerGpcMasks, {0x70, 0x3, 0x30, 0x3, 0x0, 0x0, 0x0, 0x1});
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->applyGPCMasks(gpcSettings);
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg)) << msg;
    }
    {   // not in the sku, because there's too many tpcs
        init_array(gpcSettings.tpcPerGpcMasks, {0x70, 0x1, 0x30, 0x3, 0x0, 0x0, 0x0, 0x1});
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->applyGPCMasks(gpcSettings);
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    }
    {   // not in the sku, because the skyline doesn't fit
        init_array(gpcSettings.tpcPerGpcMasks, {0x70, 0x70, 0x70, 0x1, 0x0, 0x0, 0x0, 0x0});
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->applyGPCMasks(gpcSettings);
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    }
    {   // skyline can't fit because there's too many gpcs with more than 2 dead tpcs
        init_array(gpcSettings.tpcPerGpcMasks, {0x70, 0x3, 0x70, 0x3, 0x0, 0x0, 0x0, 0x0});
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->applyGPCMasks(gpcSettings);
        EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    }
    {   // skyline can fit
        init_array(gpcSettings.tpcPerGpcMasks, {0x30, 0x3, 0x70, 0x8, 0x0, 0x0, 0x4, 0x2});
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        fsChip->applyGPCMasks(gpcSettings);
        EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    }
}

TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_1) {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {6,7,8,8,8,8,8}};

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
}


TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_2) {
    init_array(gpcSettings.tpcPerGpcMasks, {0x30, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7});
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {6,7,8,8,8,8,8}};

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}


// This config should fit
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_3) {
    init_array(gpcSettings.tpcPerGpcMasks, {0xf0, 0x1, 0x2, 0x3, 0x4, 0x0, 0x0, 0x1});
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 7, 8, 8, 8, 8, 8}};

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
}

// This config would not fit because there are not enough 8 TPC GPCs
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_4) {
    init_array(gpcSettings.tpcPerGpcMasks, {0xf0, 0x7, 0x3, 0x3, 0x4, 0x0, 0x0, 0x1});
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] =  {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {7,7,9,9,9,9,9}};

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// Combining the mustDisableList with thie config will fail to fit the skyline
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_5) {
    init_array(gpcSettings.tpcPerGpcMasks, {0x70, 0xf, 0x3, 0x3, 0x4, 0x0, 0x0, 0x1});
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {0, 72, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {8}, UNKNOWN_ENABLE_COUNT, {6,7,8,8,8,8,8}};

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// This can fit by floorsweeping some TPCs out of the heavily floorswept GPC
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_6) {
    init_array(gpcSettings.tpcPerGpcMasks, {0x0, 0x0, 0x3, 0x3, 0x4, 0x0, 0x0, 0x0});
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] =  {62, 62, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {7,7,9,9,9,9,9}};

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
}

// This can't fit because we need to floorsweep TPCs out of GPC0 and won't meet the minimum TPC per GPC count
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_7) {
    init_array(gpcSettings.tpcPerGpcMasks, {0xf0, 0x0, 0x3, 0x3, 0x0, 0x0, 0x0, 0x0});
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {7, 7, 9, 9, 9, 9, 9}};

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}


// This one can't fit.
// GPC0 can't be floorswept and no TPCs can be floorswept out of GPC0 because of min 5
// The minimum TPC count we can get to and still fit he skyline is 64
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_8) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    // we can't knock out a GPC
    init_array(gpcSettings.tpcPerGpcMasks, {0xf0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {7, 7, 9, 9, 9, 9, 9}};

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// This one can't fit.
// no TPCs can be floorswept out of GPC1 because of min 5
// GPC1 can't be floorswept because then there would only be 61 TPCs
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_9) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    // we can't knock out gpc0,
    init_array(gpcSettings.tpcPerGpcMasks, {0x0, 0xf0, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {7, 7, 9, 9, 9, 9, 9}};
    skuconfig.fsSettings["gpc"] = {7, 8};

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// Similar to the test above, except the skyline is more relaxed such that the config should fit
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_10) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    // we can't knock out a GPC
    init_array(gpcSettings.tpcPerGpcMasks, {0xf0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {7, 7, 8, 8, 9, 9, 9}};

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
}

TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_11) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    const GH100_t* gh100_ptr = dynamic_cast<const GH100_t*>(fsChip.get());
    assert(gh100_ptr != nullptr);

    init_array(gpcSettings.tpcPerGpcMasks, {0x0, 0xf, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {52, 52, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {9, 9, 9, 9, 9}};
    skuconfig.fsSettings["gpc"] = {6, 8};
    EXPECT_TRUE(gh100_ptr->skylineFits({9, 9, 9, 9, 9}, msg)) << msg;
    EXPECT_TRUE(gh100_ptr->canFitSKU(skuconfig, msg)) << msg;
}

// This is not possible to fit because we need 5 GPCs with 9 TPCS, which gives 45 TPCs
// The sku requires a max of 52, however, gpc0 only has 5 enabled, and gpc0 cannot be floorswept
// This means another gpc would need to have 2 enabled (45+5+2 == 52), which is not allowed, because of min 5
// This test case has the worst runtime without an obvious fix
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_12) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, {0xf0, 0x0, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {52, 52, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {9, 9, 9, 9, 9}};
    skuconfig.fsSettings["gpc"] = {6, 8};
    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// This one is similar to the one above, except that it can fit because gpc0 is perfect and gpc1 has dead TPCs
// Floorsweep gpc1 and gpc6, and floorsweep a tpc out of gpc3 to get 52
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_skyline_13) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    init_array(gpcSettings.tpcPerGpcMasks, {0x0, 0xf, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {52, 52, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {9, 9, 9, 9, 9}};
    skuconfig.fsSettings["gpc"] = {5, 8};
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
}

TEST_F(FSChipDownbinHopper, Test_gh100_canFitSkylineGPC0Screen_1) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    init_array(gpcSettings.tpcPerGpcMasks, { 0x0, 0x1ff, 0x0, 0x9, 0x0, 0x0, 0x1ff, 0x0 });
    gpcSettings.gpcMask = 0x42;
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {9, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 5, 8 };
    EXPECT_TRUE(dynamic_cast<GH100_t*>(fsChip.get())->canFitSkylineGPC0Screen(skuconfig, msg)) << msg;
}

// this cannot fit, 2 gfx tpcs needed, and we have 1
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_gfxtpcs_1) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    init_array(gpcSettings.tpcPerGpcMasks, {0x3, 0xf, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.misc_params[min_gfx_tpcs_str] = 2;
    
    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// this can fit, only 1 gfx tpc needed, and we have 1
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_gfxtpcs_2) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    init_array(gpcSettings.tpcPerGpcMasks, {0x3, 0xf, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.misc_params[min_gfx_tpcs_str] = 1;
    
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
}

// can't fit the sku, min gfx tpcs requirement not met
TEST_F(FSChipDownbinHopper, Test_gh100_canFitSKU_gfxtpcs_3) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    init_array(gpcSettings.tpcPerGpcMasks, {0x1, 0xf, 0x0, 0x1, 0x0, 0x0, 0x1, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.misc_params[min_gfx_tpcs_str] = 3;
    
    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// in the sku, min gfx tpcs requirement is met
TEST_F(FSChipDownbinHopper, Test_gh100_isInSKU_gfxtpcs_1) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    init_array(gpcSettings.tpcPerGpcMasks, {0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.misc_params[min_gfx_tpcs_str] = 2;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
}

// not in the sku, min gfx tpcs requirement not met
TEST_F(FSChipDownbinHopper, Test_gh100_isInSKU_gfxtpcs_2) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    init_array(gpcSettings.tpcPerGpcMasks, {0x2, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.misc_params[min_gfx_tpcs_str] = 3;
    EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
}

// in the sku, min gfx tpcs requirement is exceeded
TEST_F(FSChipDownbinHopper, Test_gh100_isInSKU_gfxtpcs_3) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    init_array(gpcSettings.tpcPerGpcMasks, {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.misc_params[min_gfx_tpcs_str] = 2;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
}

// sanity test on a perfect config
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_skyline_1) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {7, 7, 9, 9, 9, 9, 9}};
    skuconfig.fsSettings["gpc"] = {7, 7};

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;

    fsChip->downBin(skuconfig, msg);
    
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
}

//Same skyline as above, but with gpc1 disabled first
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_skyline_2) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, { 0x0, 0x1ff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 });
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 62, 62, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {7, 7, 9, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 7, 7 };

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;

    fsChip->downBin(skuconfig, msg);

    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
}

TEST_F(FSChipDownbinHopper, Test_gh100_downBin_skyline_impossible_sku) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    // this sku is not possible with 8 GPCs, because there are only 3 singletons to put in the last GPC, which is less than the min of 5
    skuconfig.fsSettings["tpc"] = {62, 62, 5, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {7, 7, 9, 9, 9, 9, 9}};
    skuconfig.fsSettings["gpc"] = {8, 8};


    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// tricky runtime
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_skyline_impossible_sku_2) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    // This sku is not possible with 8 GPCs because it requires there to be 2 GPCs that only have singletons
    // The problem is there are only 8 singletons, so each of those GPCs can't have 6 TPCs each
    // Disabling a GPC isn't an option because of the min 8 GPC requirement in the sku
    skuconfig.fsSettings["tpc"] = {62, 62, 6, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {9, 9, 9, 9, 9, 9}};
    skuconfig.fsSettings["gpc"] = {8, 8};

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

//return sku A
static FUSESKUConfig::SkuConfig makeSkuA() {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {62, 62, 6, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 6, 7, 7, 8, 8, 8}};
    skuconfig.fsSettings["gpc"] = {7, 8};
    skuconfig.fsSettings["fbp"] = {12, 12};
    skuconfig.fsSettings["ltc"] = {22, 22, 1};
    skuconfig.misc_params[min_gfx_tpcs_str] = 2;
    return skuconfig;
}

static FUSESKUConfig::SkuConfig makeSkuARepair() {
    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 62, 62, 1, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 6, 7, 7, 8, 8, 8} };
    skuconfig.fsSettings["gpc"] = { 7, 8 };
    skuconfig.fsSettings["fbp"] = { 12, 12 };
    skuconfig.fsSettings["ltc"] = { 22, 22, 1 };
    skuconfig.misc_params[min_gfx_tpcs_str] = 2;
    return skuconfig;
}

//downbin to sku A from perfect
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_skyline_3) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    FUSESKUConfig::SkuConfig skuconfig = makeSkuA();

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
}

//downbin to sku A. This config requires GPC floorsweeping
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_skyline_4) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, {0x0, 0x1f0, 0x0, 0x0, 0x0, 0x1, 0x0, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig = makeSkuA();

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
}

//downbin to sku A. This config requires GPC floorsweeping, but it can't work because gpc0 cannot be floorswept
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_skyline_5) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, {0x1f0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0});
    fsChip->applyGPCMasks(gpcSettings);
    FUSESKUConfig::SkuConfig skuconfig = makeSkuA();

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

static void attemptRepair(const std::unique_ptr<FSChip_t>& fsChip) {
    for (uint32_t gpc_idx = 0; gpc_idx < fsChip->GPCs.size(); gpc_idx++) {
        for (uint32_t tpc_idx = 0; tpc_idx < fsChip->GPCs[gpc_idx]->TPCs.size(); tpc_idx++) {
            if (((fsChip->spare_gpc_mask.tpcPerGpcMasks[gpc_idx] >> tpc_idx) & 0x1) == 1) {
                fsChip->GPCs[gpc_idx]->TPCs[tpc_idx]->setEnabled(true);
            }
        }
    }
}

// Happy path test that checks that spare selection is correct
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_repair_skyline_sanity_1) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 62, 62, 1, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 7, 8, 8, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 7, 8 };

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;

    fsChip->downBin(skuconfig, msg);

    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;

    // The optimal repair config is 3, 8, 8, 9, 9, 9, 9, 9
    // And our algorithm selects spares from the GPCs with the highest TPC count
    // Ties are broken by picking the GPC with the lowest physical ID
    EXPECT_EQ(fsChip->getLogicalTPCCounts(), (std::vector<uint32_t>{3, 8, 8, 8, 8, 9, 9, 9}));

    GpcMasks expectedSpareGPCSettings = { 0 };
    init_array(expectedSpareGPCSettings.tpcPerGpcMasks, { 0x0, 0x0, 0x0, 0x1, 0x1, 0x0, 0x0, 0x0 });
    EXPECT_EQ(fsChip->spare_gpc_mask, expectedSpareGPCSettings);
}

// Happy path test that checks that spare selection is correct
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_repair_skyline_sanity_2) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 62, 62, 1, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 7, 8, 8, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 7, 8 };

    init_array(gpcSettings.tpcPerGpcMasks, { 0xf0, 0x1, 0x0, 0x1, 0x1, 0x0, 0x1, 0x0 });
    fsChip->applyGPCMasks(gpcSettings);

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;

    fsChip->downBin(skuconfig, msg);

    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;

    // The optimal repair config is 5, 8, 8, 8, 8, 9, 9, 9
    // And our algorithm selects spares from the GPCs with the highest TPC count
    // Ties are broken by picking the GPC with the lowest physical ID
    EXPECT_EQ(fsChip->getLogicalTPCCounts(), (std::vector<uint32_t>{5, 7, 7, 8, 8, 9, 9, 9}));

    GpcMasks expectedSpareGPCSettings = { 0 };
    // We need to pick GPCs with 1 TPC bad
    // So pick the two with the lowest physical IDs
    // We can't pick TPC0 because that TPC is already dead, so pick TPC1
    init_array(expectedSpareGPCSettings.tpcPerGpcMasks, { 0x0, 0x8, 0x0, 0x8, 0x0, 0x0, 0x0, 0x0 });
    EXPECT_EQ(fsChip->spare_gpc_mask, expectedSpareGPCSettings);
}

// Happy path test that checks that spare selection is correct
// this time with a skyline without a 0
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_repair_skyline_sanity_3) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 62, 62, 1, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 6, 7, 7, 7, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 7, 8 };

    init_array(gpcSettings.tpcPerGpcMasks, { 0x70, 0x1, 0x0, 0x1, 0x1, 0x0, 0x1, 0x0 });
    fsChip->applyGPCMasks(gpcSettings);

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;

    fsChip->downBin(skuconfig, msg);

    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;

    // The optimal repair config is 6, 7, 8, 8, 8, 9, 9, 9
    // And our algorithm selects spares from the GPCs with the highest TPC count
    // Ties are broken by picking the GPC with the lowest physical ID
    EXPECT_EQ(fsChip->getLogicalTPCCounts(), (std::vector<uint32_t>{6, 7, 7, 7, 8, 9, 9, 9}));

    GpcMasks expectedSpareGPCSettings = { 0 };
    // We need to pick GPCs with 1 TPC bad
    // So pick the two with the lowest physical IDs
    // GPC2 has had a TPC disabled during the downbinning, so we would pick the next GPC
    // We can't pick TPC0 because that TPC is already dead, so pick TPC1
    init_array(expectedSpareGPCSettings.tpcPerGpcMasks, { 0x0, 0x0, 0x0, 0x8, 0x8, 0x0, 0x0, 0x0 });
    EXPECT_EQ(fsChip->spare_gpc_mask, expectedSpareGPCSettings);
}

// this is a case where repair was not possible
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_repair_skyline_sanity_4) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig = makeSkuARepair();

    init_array(gpcSettings.tpcPerGpcMasks, {0x0, 0x0, 0x164, 0x0, 0x0, 0x120, 0x0, 0x0});
    fsChip->applyGPCMasks(gpcSettings);

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;

    fsChip->downBin(skuconfig, msg);

    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;

    // The optimal repair config is 5, 7, 7, 9, 9, 9, 9, 9
    // And our algorithm selects spares from the GPCs with the highest TPC count
    // Ties are broken by picking the GPC with the lowest physical ID
    EXPECT_EQ(fsChip->getLogicalTPCCounts(), (std::vector<uint32_t>{5, 7, 7, 8, 8, 9, 9, 9}));

    GpcMasks expectedSpareGPCSettings = { 0 };
    init_array(expectedSpareGPCSettings.tpcPerGpcMasks, { 0x0, 0x1, 0x0, 0x1, 0x0, 0x0, 0x0, 0x0 });
    EXPECT_EQ(fsChip->spare_gpc_mask, expectedSpareGPCSettings);

    fsChip->getModuleFlatIdx(module_type_id::tpc, 46)->setEnabled(false);
    fsChip->getModuleFlatIdx(module_type_id::tpc, 47)->setEnabled(false);

    attemptRepair(fsChip);
    EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
}

// This test has bad runtime without the skyline screening function "canFitSkylineGPC0Screen"
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_bad_runtime_1) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x100 });
    init_array(gpcSettings.cpcPerGpcMasks, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1 });

    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 4, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {7, 8, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 6, 8 };
    skuconfig.fsSettings["fbp"] = { 8, 12 };
    skuconfig.fsSettings["ltc"] = { 16, 16 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;

}

// Bad runtime case without the skyline screening function "canFitSkylineGPC0Screen"
// it's not possible for the config to fit the sku because gpc0 does not have enough tpcs to fit the skyline.
// The skyline does not have any singletons, so it's not possible to leave gpc0 alive either, but it can't be floorswept because it's gpc0
// TODO add some logic to handle this case quickly
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_bad_runtime_2) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, { 0x160, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 });

    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 4, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {8, 8, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 6, 8 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg)) << msg;
}

// This config takes forever without the skyline screening function "canFitSkylineGPC0Screen"
// This one can't fit because of the skyline requirement
// We need to disable 20 TPCs. If you disable a GPC1, you now have 17 dead TPCs.
// physical gpc2 is the 6 in the skyline and physical gpc3 is the 8.
// We can only disable 1 more TPC in gpc0 because of min4, so there is no way to disable enough TPCs to fit the sku
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_bad_runtime_3) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, { 0x8, 0x0, 0x0, 0x10});
    init_array(gpcSettings.cpcPerGpcMasks, { 0x4, 0x0, 0x2, 0x0});

    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 4, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 8, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 6, 8 };
    skuconfig.fsSettings["fbp"] = { 8, 12 };
    skuconfig.fsSettings["ltc"] = { 16, 16 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// Takes around 6 seconds without the skyline screening function "canFitSkylineGPC0Screen"
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_bad_runtime_4) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, { 0x0, 0x0, 0x0, 0x2, 0x100, 0x80, 0x0, 0x20 });

    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 4, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 8, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 6, 8 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
}

// This one runs fast after some fixes. Leave the test to protect this case in the future
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_bad_runtime_5) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.ropPerGpcMasks, { 0x1 });

    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 4, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 8, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 6, 8 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// takes 28 seconds without the skyline screening function "canFitSkylineGPC0Screen"
// phys gpc 0 is the 6
// phys gpc 3 is the 8
// disable gpc 4
// now there's 12 disabled, need 8 more
// disable a tpc in phys 0
// now there's 13 disabled
// disable 4 tpcs in gpc 5 (so it now has 4 total enabled)
// now there's 17 disabled
// we can't disable any more and fit the skyline
// we also can't just disable 2 GPCs, because then we would have too many dead TPCs

// try removing gpc0, and then say the rest need to have max 48, min 45 tpcs
// 0x90, 0x0, 0x0, 0x8, 0x100, 0x40 
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_bad_runtime_6) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, { 0x90, 0x0, 0x0, 0x8, 0x100, 0x40 });

    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 4, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 8, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 6, 8 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// Same as above, but with memsys in the sku
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_bad_runtime_7) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, { 0x90, 0x0, 0x0, 0x8, 0x100, 0x40 });
    init_array(fbpSettings.ltcPerFbpMasks, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1 });

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 4, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 8, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 6, 8 };
    skuconfig.fsSettings["fbp"] = { 8, 12 };
    skuconfig.fsSettings["ltc"] = { 16, 16 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// Takes a long time without using the downbin heuristics function
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_bad_runtime_8) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(fbpSettings.ltcPerFbpMasks, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1 });

    fsChip->applyFBPMasks(fbpSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["fbp"] = { 8, 12 };
    skuconfig.fsSettings["ltc"] = { 16, 16 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
}

// Really bad runtime for this case without the skyline screening function "canFitSkylineGPC0Screen"
// GPC0 is the 6, GPC7 is the 8
// We need to disable 16 more  TPCs
// We can FS a GPC, but then we can't FS 7 TPCs in the other "0" GPC, because of min 4 so not possible to fit
// TODO, we need some kind of heuristic to detect this case
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_bad_runtime_9) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80 });
    init_array(gpcSettings.cpcPerGpcMasks, { 0x4 });

    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 52, 52, 4, UNKNOWN_ENABLE_COUNT, 0, 2, {}, {}, UNKNOWN_ENABLE_COUNT, {6, 8, 9, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 6, 8, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {0} };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;

    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// Another bad runtime case discovered
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_bad_runtime_10) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    init_array(gpcSettings.tpcPerGpcMasks, { 0x120, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 });

    fsChip->applyGPCMasks(gpcSettings);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = { 62, 62, 1, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {}, UNKNOWN_ENABLE_COUNT, {8, 8, 8, 8, 9, 9, 9} };
    skuconfig.fsSettings["gpc"] = { 7, 8 };
    skuconfig.misc_params[fslib::min_gfx_tpcs_str] = 2;

    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
}


// =================================================================================
// Randomized testing
// =================================================================================
static const int NUM_RANDOM_ITER = 100;


// Downbin to skyline after 6 random TPCs are floorswept
// Either a GPC is floorswept, or two GPCs have 6 TPCs in the most antagonistic cases
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_skyline_random) {
    fslib::GpcMasks gpcSettings = {0};
    fslib::FbpMasks fbpSettings= {0};
    FUSESKUConfig::SkuConfig skuconfig = makeSkuA();

    for (int i = 0; i < NUM_RANDOM_ITER; i++) {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        int num_to_kill = 5;
        disableNRandom(gpcSettings.tpcPerGpcMasks, num_to_kill, 8, 9);
        uint32_t num_gfx_tpcs_disabled = 0;
        num_gfx_tpcs_disabled += ((gpcSettings.tpcPerGpcMasks[0] >> 0) & 1);
        num_gfx_tpcs_disabled += ((gpcSettings.tpcPerGpcMasks[0] >> 1) & 1);
        num_gfx_tpcs_disabled += ((gpcSettings.tpcPerGpcMasks[0] >> 2) & 1);
        int32_t num_gfx_tpcs_alive = 3 - num_gfx_tpcs_disabled;
        fsChip->applyGPCMasks(gpcSettings);

        bool insufficient_gfx_tpcs = num_gfx_tpcs_alive < skuconfig.misc_params[min_gfx_tpcs_str];
        bool insufficient_tpcs = minTPCEnableCount(gpcSettings, 8, 9) < skuconfig.fsSettings["tpc"].minEnablePerGroup &&
            minTPCEnableCount(gpcSettings, 8, 9) < static_cast<uint32_t>(num_to_kill - 1);
                              
        if (insufficient_gfx_tpcs || insufficient_tpcs) {
            EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg)) << msg;
        } else {
            EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
            EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
            EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
            EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
        }
        // Reset
        msg = "";
        gpcSettings = {0};    
        fbpSettings = {0};
    }

}

// SKU doesn't support more than 10 TPCs floorswept.
TEST_F(FSChipDownbinHopper, Test_gh100_downBin_skyline_random_fail) {
    fslib::GpcMasks gpcSettings = {0};
    fslib::FbpMasks fbpSettings= {0};
    FUSESKUConfig::SkuConfig skuconfig = makeSkuA();

    for (int i = 0; i < NUM_RANDOM_ITER; i++) {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        floorsweepRandomTPCs(gpcSettings, fsChip->getChipPORSettings(), 11, false);
        fsChip->applyGPCMasks(gpcSettings);

        bool is_valid = fslib::IsFloorsweepingValid(
            fslib::Chip::GH100, gpcSettings, fbpSettings, msg, fslib::FSmode::PRODUCT);
        EXPECT_TRUE(is_valid) << "A chip with 11 TPCs floorswept was considered invalid!"
                              << " msg: " << msg;

        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg)) << msg;
        // Reset
        msg = "";
        gpcSettings = {0};    
        fbpSettings = {0};
    }

}

static bool fitsSkyline(std::vector<uint32_t> skyline, const std::vector<uint32_t>& logical_config) {
    while (skyline.size() < logical_config.size()) {
        skyline.insert(skyline.begin(), 0);
    }
    for (uint32_t i = 0; i < skyline.size(); i++) {
        if (logical_config[i] < skyline[i]) {
            return false;
        }
    }
    return true;
}

static void gh100_downBin_skyline_random_repair_iterations(int num_iterations){

    std::default_random_engine generator;

    fslib::GpcMasks gpcSettings = { 0 };
    fslib::FbpMasks fbpSettings = { 0 };
    FUSESKUConfig::SkuConfig skuconfig = makeSkuARepair();
    std::string msg;

    for (int i = 0; i < num_iterations; i++) {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        floorsweepRandomTPCs(gpcSettings, fsChip->getChipPORSettings(), 6, false);
        fsChip->applyGPCMasks(gpcSettings);

        bool is_valid = fslib::IsFloorsweepingValid(
            fslib::Chip::GH100, gpcSettings, fbpSettings, msg, fslib::FSmode::FUNC_VALID);
        ASSERT_TRUE(is_valid) << "A chip with six TPCs floorswept was considered invalid!"
            << " msg: " << msg;

        uint32_t gfx_tpcs_alive = getNumEnabledRange(fsChip->getFlatModuleList(module_type_id::tpc), 0, 2);

        // Downbin the chip into the sku and select spares
        EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
        EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
        EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;

        // figure out what the alive TPC idxs are
        std::vector<uint32_t> alive_tpcs;
        auto tpcs = fsChip->getFlatModuleList(module_type_id::tpc);
        for (uint32_t tpc_idx = 0; tpc_idx < tpcs.size(); tpc_idx++) {
            if (tpcs[tpc_idx]->getEnabled()) {
                alive_tpcs.push_back(tpc_idx);
            }
        }

        // Kill two random TPCs that were previously alive
        std::shuffle(alive_tpcs.begin(), alive_tpcs.end(), generator);
        for (uint32_t i = 0; i < skuconfig.fsSettings["tpc"].repairMaxCount; i++) {
            uint32_t tpc_to_kill = alive_tpcs[i];
            fsChip->getModuleFlatIdx(module_type_id::tpc, tpc_to_kill)->setEnabled(false);
            if (tpc_to_kill == 0 || tpc_to_kill == 1 || tpc_to_kill == 2) {
                gfx_tpcs_alive -= 1;
            }
        }


        // Only try repairing if we have enough gfx tpcs
        if (gfx_tpcs_alive >= static_cast<uint32_t>(skuconfig.misc_params[min_gfx_tpcs_str])) {
            // Now try to repair the chip by enabling the spares
            EXPECT_EQ(fsChip->getTotalEnableCount(module_type_id::tpc), skuconfig.fsSettings["tpc"].maxEnableCount - skuconfig.fsSettings["tpc"].repairMaxCount);
            attemptRepair(fsChip);

            fsChip->funcDownBin();

            // Verify that the chip is back in the sku again
            std::vector<uint32_t> logical = fsChip->getLogicalTPCCounts();
            if (fitsSkyline(skuconfig.fsSettings["tpc"].skyline, fsChip->getLogicalTPCCounts())) {
                EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
                if (!fsChip->isInSKU(skuconfig, msg)) {
                    std::cout << gpcSettings << std::endl;
                    std::cout << fsChip->spare_gpc_mask << std::endl;
                    std::cout << "killed: " << alive_tpcs[0] << " " << alive_tpcs[1] << std::endl;
                }
            } else {
                EXPECT_FALSE(fsChip->isInSKU(skuconfig, msg));
            }
            EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
        }

        // Reset
        msg = "";
        gpcSettings = { 0 };
        fbpSettings = { 0 };
    }
}

TEST_F(FSChipDownbinHopper, Test_gh100_downBin_skyline_random_repair_100x) {
    gh100_downBin_skyline_random_repair_iterations(100);
}

TEST_F(FSChipDownbinHopper, DISABLED_Test_gh100_downBin_skyline_random_repair_10000x) {
    gh100_downBin_skyline_random_repair_iterations(10000);
}



}