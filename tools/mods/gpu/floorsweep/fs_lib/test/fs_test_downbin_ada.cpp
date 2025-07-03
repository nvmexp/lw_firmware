#include <gtest/gtest.h>

#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <algorithm>
#include "fs_lib.h"
#include "fs_chip_core.h"
#include "fs_chip_ada.h"
#include "fs_chip_factory.h"
#include "fs_test_utils.h"
#include <iomanip>
#include <sstream>
#include <cassert>

namespace fslib {

class FSChipDownbinAda : public ::testing::Test {
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


TEST_F(FSChipDownbinAda, Test_canFitSKUad102_1) {
    for (int i : {0, 3, 4, 6, 8}){
        gpcSettings.tpcPerGpcMasks[i] = 0x1; // some gpcs have 5 TPCs enabled
    }

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    {   
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {60, 60};
        skuconfig.fsSettings["gpc"] = {12, 12};
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    }
    {   
        FUSESKUConfig::SkuConfig skuconfig; // you can't get 60 TPCs with 8 GPCs. 8*6
        skuconfig.fsSettings["tpc"] = {60, 60, 5};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
    }
}

// test enable list conflict with 3 slice pattern
// this should not fit the sku because the enablelist conflicts with the slice that's already disabled
TEST_F(FSChipDownbinAda, Test_canFitSKUad102_2_check_must_enable_conflict) {

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["l2slice"] = {36, 36, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {0}, {}};
    skuconfig.fsSettings["ltc"] = {12, 12};
    skuconfig.fsSettings["fbp"] = {6, 6};

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[1]->setEnabled(false);
    EXPECT_FALSE(fsChip->canFitSKU(skuconfig, msg));
}

// test that an enable list can work with 3 slice pattern
TEST_F(FSChipDownbinAda, Test_canFitSKUad102_3_check_must_enable_nonconflict) {

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["l2slice"] = {36, 36, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {0}};
    skuconfig.fsSettings["ltc"] = {12, 12};
    skuconfig.fsSettings["fbp"] = {6, 6};

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::l2slice)[2]->setEnabled(false);
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg));
}

TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_1) {
    // GPCs with no ROPs should be floorswept
    gpcSettings.ropPerGpcMasks[1] = 0x3;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::rop)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::pes)[0]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::rop)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::pes)[0]->getEnabled());
}

TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_2) {
    // GPCs with no PESs should be floorswept
    gpcSettings.pesPerGpcMasks[1] = 0x7;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::rop)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::pes)[0]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::rop)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::pes)[0]->getEnabled());
}

TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_3) {
    // floorswept GPCs should floorsweep the tpcs, pess, rops
    gpcSettings.gpcMask = 0x2;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::rop)[1]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[3]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::pes)[1]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::rop)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[3]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::pes)[1]->getEnabled());
}

TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_4) {
    // TPCs connected to a floorswept PESs should be floorswept
    gpcSettings.pesPerGpcMasks[1] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[1]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[2]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::pes)[0]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[1]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[2]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::pes)[0]->getEnabled());
}

TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_6) {
    // floorswept FBIO means the FBP must be floorswept
    fbpSettings.fbioPerFbpMasks[1] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[1]->returnModuleList(module_type_id::fbio)[0]->getEnabled());
}

TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_7) {
    // floorswept FBP means the FBIO must be floorswept
    fbpSettings.fbpMask = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[0]->returnModuleList(module_type_id::fbio)[0]->getEnabled());
}

TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_8) {
    // floorsweep both LTCs means the FBP must be floorswept
    fbpSettings.ltcPerFbpMasks[0] = 0x3;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg));
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::fbp)[0]->getEnabled());

    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[0]->getEnabled());
}

TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_9) {
    // Slices should be floorswept when the LTC is floorswept
    fbpSettings.ltcPerFbpMasks[1] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
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

// if 4 slices are bad in different LTCs, it could be better to do 3 slice mode instead of 32 bit mode
TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_3slice_fs_1) {

    for(int start_fbp_idx = 0; start_fbp_idx < 3; start_fbp_idx++){
        for (int start_slice_idx = 0; start_slice_idx < 8; start_slice_idx++){
            std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

            // test a defect on 4 slices in different ltcs
            for (int k = 0; k < 4; k++){
                uint32_t slice_idx = (start_slice_idx + k) % 4;
                uint32_t fbp_idx = start_fbp_idx + k;
                fsChip->returnModuleList(module_type_id::fbp)[fbp_idx]->returnModuleList(module_type_id::l2slice)[slice_idx]->setEnabled(false);
            }
            fsChip->funcDownBin();

            std::string msg;
            EXPECT_TRUE(static_cast<DDRGPU_t*>(fsChip.get())->canDoSliceProduct(3, msg)) << msg;
            EXPECT_TRUE(fsChip->isValid(fslib::FSmode::PRODUCT, msg)) << msg;

            //check that every fbp has 6 slices now
            EXPECT_EQ(fsChip->getNumEnabledSlices(), 36);
            for(const auto& fbp_ptr : fsChip->returnModuleList(module_type_id::fbp)){
                EXPECT_EQ(static_cast<FBP_t*>(fbp_ptr)->getNumEnabledSlices(), 6);
            }
        }
    }
}

// test that disabling a slice in both ltcs in an FBP results in FBP FS
TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_fbp_fs_1) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    // test a defect on 2 slices
    fsChip->returnModuleList(module_type_id::fbp)[0]->returnModuleList(module_type_id::l2slice)[0]->setEnabled(false);
    fsChip->returnModuleList(module_type_id::fbp)[0]->returnModuleList(module_type_id::l2slice)[7]->setEnabled(false);

    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::PRODUCT, msg));
    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::PRODUCT, msg)) << msg;

    EXPECT_EQ(fsChip->getNumEnabledSlices(), 40);
}

// test that disabling some slice patterns can result in 2 slice mode after func downbin in some cases
TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_2slice_fs_1) {
    {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
        // test a defect on 2 slices
        for(auto fbp_ptr : fsChip->returnModuleList(module_type_id::fbp)){
            fbp_ptr->returnModuleList(module_type_id::l2slice)[0]->setEnabled(false);
            fbp_ptr->returnModuleList(module_type_id::l2slice)[7]->setEnabled(false);
        }

        EXPECT_FALSE(fsChip->isValid(fslib::FSmode::PRODUCT, msg));
        fsChip->funcDownBin();

        EXPECT_TRUE(fsChip->isValid(fslib::FSmode::PRODUCT, msg)) << msg;

        //check that every fbp has 4 slices now
        for(const auto& fbp_ptr : fsChip->returnModuleList(module_type_id::fbp)){
            EXPECT_EQ(static_cast<FBP_t*>(fbp_ptr)->getNumEnabledSlices(), 4);
        }
    }
    {
        // test the other pattern
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
        // test a defect on 2 slices
        for(auto fbp_ptr : fsChip->returnModuleList(module_type_id::fbp)){
            fbp_ptr->returnModuleList(module_type_id::l2slice)[3]->setEnabled(false);
            fbp_ptr->returnModuleList(module_type_id::l2slice)[4]->setEnabled(false);
        }

        EXPECT_FALSE(fsChip->isValid(fslib::FSmode::PRODUCT, msg));
        fsChip->funcDownBin();

        EXPECT_TRUE(fsChip->isValid(fslib::FSmode::PRODUCT, msg)) << msg;

        //check that every fbp has 4 slices now
        for(const auto& fbp_ptr : fsChip->returnModuleList(module_type_id::fbp)){
            EXPECT_EQ(static_cast<FBP_t*>(fbp_ptr)->getNumEnabledSlices(), 4);
        }
    }
}

// if there's only one bad slice in one ltc, just fs the ltc on ad102
TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_32bit) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    // test a defect on 2 slices
    fsChip->returnModuleList(module_type_id::fbp)[2]->returnModuleList(module_type_id::l2slice)[0]->setEnabled(false);

    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::PRODUCT, msg));
    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::PRODUCT, msg)) << msg;

    //check that there are 4 slices dead
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 44);
}

// if an fbio is bad, fs the whole fbp
TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_fbiofs) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

    // test a defect on fbio
    fsChip->returnModuleList(module_type_id::fbp)[2]->returnModuleList(module_type_id::fbio)[0]->setEnabled(false);

    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::PRODUCT, msg));
    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::PRODUCT, msg)) << msg;

    EXPECT_EQ(fsChip->getNumEnabledSlices(), 40);
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[2]->getEnabled());
}

// if two ltcs in the same fbp have bad slices, fs the fbp
TEST_F(FSChipDownbinAda, Test_funcDownBin_ad102_fbp_fs_2slices) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

    // test a defect on 2 slices
    fsChip->returnModuleList(module_type_id::fbp)[2]->returnModuleList(module_type_id::l2slice)[0]->setEnabled(false);
    fsChip->returnModuleList(module_type_id::fbp)[2]->returnModuleList(module_type_id::l2slice)[5]->setEnabled(false);

    EXPECT_FALSE(fsChip->isValid(fslib::FSmode::PRODUCT, msg));
    fsChip->funcDownBin();

    EXPECT_TRUE(fsChip->isValid(fslib::FSmode::PRODUCT, msg)) << msg;

    EXPECT_EQ(fsChip->getNumEnabledSlices(), 40);
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[2]->getEnabled());
}


// SKU fitting donwbin function testing
TEST_F(FSChipDownbinAda, Test_downBin_ad102_3slice_downbin_defective_slices) {

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["l2slice"] = {36, 36};
    skuconfig.fsSettings["ltc"] = {12, 12};
    skuconfig.fsSettings["fbp"] = {6, 6};


    for(int start_fbp_idx = 0; start_fbp_idx < 3; start_fbp_idx++){
        for (int start_slice_idx = 0; start_slice_idx < 8; start_slice_idx++){
            std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

            // test a defect on 4 slices in different ltcs
            for (int k = 0; k < 4; k++){
                uint32_t slice_idx = (start_slice_idx + k) % 4;
                uint32_t fbp_idx = start_fbp_idx + k;
                fsChip->returnModuleList(module_type_id::fbp)[fbp_idx]->returnModuleList(module_type_id::l2slice)[slice_idx]->setEnabled(false);
            }
            EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
            EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;

            std::string msg;
            EXPECT_TRUE(static_cast<DDRGPU_t*>(fsChip.get())->canDoSliceProduct(3, msg)) << msg;
            EXPECT_TRUE(fsChip->isValid(fslib::FSmode::PRODUCT, msg)) << msg;

            //check that every fbp has 6 slices now
            EXPECT_EQ(fsChip->getNumEnabledSlices(), 36);
            for(const auto& fbp_ptr : fsChip->returnModuleList(module_type_id::fbp)){
                EXPECT_EQ(static_cast<FBP_t*>(fbp_ptr)->getNumEnabledSlices(), 6);
            }
        }
    }
}

// check that a perfect config can be downbinned with 3 slice FS
TEST_F(FSChipDownbinAda, Test_downBin_ad102_3slice_downbin_from_perfect) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["l2slice"] = {36, 36};
    skuconfig.fsSettings["ltc"] = {12, 12};
    skuconfig.fsSettings["fbp"] = {6, 6};
    
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg));
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 36);
    EXPECT_EQ(fsChip->getEnableCount(module_type_id::fbp), 6);
    

    for(const auto& fbp_ptr : fsChip->returnModuleList(module_type_id::fbp)){
        EXPECT_EQ(static_cast<FBP_t*>(fbp_ptr)->getNumEnabledSlices(), 6);
    }
}

// check that a perfect config can be downbinned with 3 slice FS
// make sure mustDisableList is honered for all slices
TEST_F(FSChipDownbinAda, Test_downBin_ad102_3slice_downbin_from_perfect_must_disable) {

    for (uint32_t i = 0; i < 48; i++) {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["l2slice"] = {36, 36, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {i}};
        skuconfig.fsSettings["ltc"] = {12, 12};
        skuconfig.fsSettings["fbp"] = {6, 6};
        
        EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
        EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
        EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
        EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
        EXPECT_EQ(fsChip->getNumEnabledSlices(), 36);
        EXPECT_EQ(fsChip->getEnableCount(module_type_id::fbp), 6);

        EXPECT_FALSE(fsChip->getFlatModuleList(module_type_id::l2slice)[i]->getEnabled());

        for(const auto& fbp_ptr : fsChip->returnModuleList(module_type_id::fbp)){
            EXPECT_EQ(static_cast<FBP_t*>(fbp_ptr)->getNumEnabledSlices(), 6);
        }
    }
}

// check that a perfect config can be downbinned with 2 slice FS
TEST_F(FSChipDownbinAda, Test_downBin_ad102_2slice_downbin_from_perfect) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["l2slice"] = {24, 24};
    skuconfig.fsSettings["ltc"] = {12, 12};
    skuconfig.fsSettings["fbp"] = {6, 6};
    
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 24);
    EXPECT_EQ(fsChip->getEnableCount(module_type_id::fbp), 6);
    

    for(const auto& fbp_ptr : fsChip->returnModuleList(module_type_id::fbp)){
        EXPECT_EQ(static_cast<FBP_t*>(fbp_ptr)->getNumEnabledSlices(), 4);
    }
}

// check that a perfect config can be downbinned with half fbpa mode
TEST_F(FSChipDownbinAda, Test_downBin_ad102_32bit_downbin_from_perfect) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["ltc"] = {11, 11};
    skuconfig.fsSettings["fbp"] = {6, 6};
    
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 44);
    EXPECT_EQ(fsChip->getEnableCount(module_type_id::fbp), 6);
}

// check that a perfect config can be downbinned with fbp floorsweeping
TEST_F(FSChipDownbinAda, Test_downBin_ad102_fbp_fs_downbin_from_perfect) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["ltc"] = {10, 10};
    skuconfig.fsSettings["fbp"] = {5, 5};
    
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 40);
    EXPECT_EQ(fsChip->getEnableCount(module_type_id::fbp), 5);
}

// check that the fbp with a bad slice will be chosen
TEST_F(FSChipDownbinAda, Test_downBin_ad102_fbp_fs_downbin_from_bad_slice) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["ltc"] = {10, 10};
    skuconfig.fsSettings["fbp"] = {5, 5};

    fsChip->returnModuleList(module_type_id::fbp)[3]->returnModuleList(module_type_id::l2slice)[0]->setEnabled(false);
    
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 40);
    EXPECT_EQ(fsChip->getEnableCount(module_type_id::fbp), 5);
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[3]->getEnabled());
}

// check that the fbp with a bad ltc will be chosen
TEST_F(FSChipDownbinAda, Test_downBin_ad102_fbp_fs_downbin_from_bad_ltc) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["ltc"] = {10, 10};
    skuconfig.fsSettings["fbp"] = {5, 5};

    fsChip->returnModuleList(module_type_id::fbp)[4]->returnModuleList(module_type_id::ltc)[0]->setEnabled(false);
    
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 40);
    EXPECT_EQ(fsChip->getEnableCount(module_type_id::fbp), 5);
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[4]->getEnabled());
}

// check that the fbp with a bad fbio will be chosen
TEST_F(FSChipDownbinAda, Test_downBin_ad102_fbp_fs_downbin_from_bad_fbio) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["ltc"] = {10, 10};
    skuconfig.fsSettings["fbp"] = {5, 5};

    fsChip->returnModuleList(module_type_id::fbp)[5]->returnModuleList(module_type_id::fbio)[0]->setEnabled(false);
    
    EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << msg;
    EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << msg;
    EXPECT_EQ(fsChip->getNumEnabledSlices(), 40);
    EXPECT_EQ(fsChip->getEnableCount(module_type_id::fbp), 5);
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::fbp)[5]->getEnabled());
}


// ================================================================================================
// Randomized happy path tests
// ================================================================================================

class FSAdaRandomDownbinTest : public ::testing::Test {
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
        fslib::FbpMasks fbpSettings = {0};
        fslib::GpcMasks gpcSettings = {0};
};

constexpr int num_random_iter = 10;

struct sku_generator_cfg {
    std::function<uint32_t(const fslib::chipPOR_settings_t&)> max_dead_tpc_gen_fn;
    bool allow_dead_gpc = true;
    std::function<uint32_t(const fslib::chipPOR_settings_t&)> min_enabled_tpc_gen_fn; 
    int example_total_ltcs_dead = 2;
    bool allow_dead_fbp = false;
};

static FUSESKUConfig::SkuConfig makeExampleAdaSku(fslib::ADLit1Chip_t* chip, struct sku_generator_cfg sku_params) {
    FUSESKUConfig::SkuConfig skuconfig;
    fslib::Chip type = chip->getChipSettings().chip_type;
    const fslib::chipPOR_settings_t chip_cfg = chip->getChipSettings().chip_por_settings;
    if (type == fslib::Chip::AD103) {
        // Account for smaller GPC
        skuconfig.fsSettings["tpc"] = {chip_cfg.num_gpcs * chip_cfg.num_tpc_per_gpc - sku_params.max_dead_tpc_gen_fn(chip_cfg) - 2, // minEnableCount 
                                       chip_cfg.num_gpcs * chip_cfg.num_tpc_per_gpc - sku_params.max_dead_tpc_gen_fn(chip_cfg) - 2, // maxEnableCount
                                       3, // minEnableCountPerGroup
                                       UNKNOWN_ENABLE_COUNT, // enableCountPerGroup
                                       0,  //repairMinCount
                                       0,  //repairMaxCount
                                       {}, // mustEnableList
                                       {}, // mustDisableList
                                       UNKNOWN_ENABLE_COUNT, // maxUGPUImbalance
                                       {} // Skyline
                                       };
    } else {
        skuconfig.fsSettings["tpc"] = {chip_cfg.num_gpcs * chip_cfg.num_tpc_per_gpc - sku_params.max_dead_tpc_gen_fn(chip_cfg), // minEnableCount 
                                       chip_cfg.num_gpcs * chip_cfg.num_tpc_per_gpc - sku_params.max_dead_tpc_gen_fn(chip_cfg), // maxEnableCount
                                       sku_params.min_enabled_tpc_gen_fn(chip_cfg), // minEnableCountPerGroup
                                       UNKNOWN_ENABLE_COUNT, // enableCountPerGroup
                                       0,  // repairMinCount
                                       0,  // repairMaxCount
                                       {}, // mustEnableList
                                       {}, // mustDisableList
                                       UNKNOWN_ENABLE_COUNT, // maxUGPUImbalance
                                       {} // Skyline
                                       };
    }

    skuconfig.fsSettings["gpc"] = {chip_cfg.num_gpcs - (sku_params.allow_dead_gpc ? 1 : 0),
                                   chip_cfg.num_gpcs   
                                  };
    skuconfig.fsSettings["fbp"] = {chip_cfg.num_fbps - (sku_params.allow_dead_fbp ? 1 : 0),
                                   chip_cfg.num_fbps 
                                  };
    skuconfig.fsSettings["ltc"] = {chip_cfg.num_ltcs - sku_params.example_total_ltcs_dead, // minEnableCount
                                   chip_cfg.num_ltcs - sku_params.example_total_ltcs_dead, // maxEnableCount
                                   1,  // minEnableCountPerGroup
                                   UNKNOWN_ENABLE_COUNT // enableCountPerGroup
                                   };
    return skuconfig;
}

// Some parameters that lend well to randomized testing
const std::vector<struct sku_generator_cfg> sku_parameter_generators0{
    // FS 1 GPC and 1 TPC
    {[](const fslib::chipPOR_settings_t& chip_cfg){return chip_cfg.num_tpc_per_gpc + 1;}, true, 
     [](const fslib::chipPOR_settings_t& chip_cfg){return chip_cfg.num_tpc_per_gpc - 2;}, 2, false},
    // Don't FS a full GPC
    {[](const fslib::chipPOR_settings_t& chip_cfg){return chip_cfg.num_tpc_per_gpc - 1;}, false, 
     [](const fslib::chipPOR_settings_t& chip_cfg){return 1;}, 2, false}
};

// Generation parameters that will need edge case testing
const std::vector<struct sku_generator_cfg> sku_parameter_generators1{
    // FS 1 TPC from each GPC
    {[](const fslib::chipPOR_settings_t& chip_cfg){return chip_cfg.num_gpcs;}, false, 
     [](const fslib::chipPOR_settings_t& chip_cfg){return chip_cfg.num_tpc_per_gpc - 1;}, 2, false}
};


// Ensure perfect chip can be downbinned to approximate mainstream SKU
TEST_F(FSAdaRandomDownbinTest, Test_downBin_allchip_perfect) {
    // Floorsweep one random GPC for each chip
    for (auto &chip : chips_in_litter) {
        std::string chip_name = chip.second->getChipSettings().chip_por_settings.name;
        for (auto &sku_params : sku_parameter_generators0) {
            FUSESKUConfig::SkuConfig skuconfig = makeExampleAdaSku(chip.second, sku_params);
            std::unique_ptr<FSChip_t> fsChip = createChip(chip.first);
            if (chip.first == fslib::Chip::AD103) {
                gpcSettings.tpcPerGpcMasks[3] |= 0x30;
                gpcSettings.pesPerGpcMasks[3] |= 0x4;  
            }
            fsChip->applyGPCMasks(gpcSettings);
            fsChip->applyFBPMasks(fbpSettings);            
            is_valid = fslib::IsFloorsweepingValid(
                chip.first, gpcSettings, fbpSettings, msg, fslib::FSmode::PRODUCT);
            ASSERT_TRUE(is_valid) << "A perfect " << chip_name << " chip was considered invalid!"
                                    << " msg: " << msg;
            
            EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << chip_name << " " << msg;
            EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << chip_name << " " << msg;
            EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << chip_name << " " << msg;
            EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << chip_name << " " << msg;
            
            // Reset settings
            msg = "";
            fbpSettings = {0};    
            gpcSettings = {0};
        }
    }
}


// Ensure chip with 1 dead GPC can be downbinned to SKU 
TEST_F(FSAdaRandomDownbinTest, Test_downBin_allchip_random_floorsweep_1gpc) {
    // Floorsweep one random GPC for each chip
    for (auto &chip : chips_in_litter) {
        std::string chip_name = chip.second->getChipSettings().chip_por_settings.name;        
        for (auto &sku_params : sku_parameter_generators0) {
            FUSESKUConfig::SkuConfig skuconfig = makeExampleAdaSku(chip.second, sku_params);
            for (int i = 0; i < num_random_iter; i++) {
                std::unique_ptr<FSChip_t> fsChip = createChip(chip.first);
                if (chip.first == fslib::Chip::AD103) {
                    gpcSettings.tpcPerGpcMasks[3] |= 0x30;
                    gpcSettings.pesPerGpcMasks[3] |= 0x4;  
                }
                floorsweepRandomGPCs(gpcSettings, chip.second->getChipPORSettings(), (1));
                fsChip->applyGPCMasks(gpcSettings);
                fsChip->applyFBPMasks(fbpSettings);            
                is_valid = fslib::IsFloorsweepingValid(
                    chip.first, gpcSettings, fbpSettings, msg, fslib::FSmode::PRODUCT);
                ASSERT_TRUE(is_valid) << "A perfect " << chip_name << " chip was considered invalid!"
                                    << " msg: " << msg;
                
                EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg) == sku_params.allow_dead_gpc) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->downBin(skuconfig, msg) == sku_params.allow_dead_gpc) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg) == sku_params.allow_dead_gpc) << chip_name << " " << msg;
                // Even if requested downbin is invalid, we shouldn't ilwalidate the chip
                EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << chip_name << " " << msg;
                
                // Reset settings
                msg = "";
                fbpSettings = {0};    
                gpcSettings = {0};
            }
        }
    }
}

// Ensure chip with 1 dead TPC can be downbinned to approximate mainstream SKU
TEST_F(FSAdaRandomDownbinTest, Test_downBin_allchip_random_floorsweep_1tpc) {
    // Floorsweep one random GPC for each chip
    for (auto &chip : chips_in_litter) {
        std::string chip_name = chip.second->getChipSettings().chip_por_settings.name;
        for (auto &sku_params : sku_parameter_generators0) {
            FUSESKUConfig::SkuConfig skuconfig = makeExampleAdaSku(chip.second, sku_params);
            // Note: these tests take a long time (up to 10s per downbin)
            for (int i = 0; i < 1; i++) {
                std::unique_ptr<FSChip_t> fsChip = createChip(chip.first);
                if (chip.first == fslib::Chip::AD103) {
                    gpcSettings.tpcPerGpcMasks[3] |= 0x30;
                    gpcSettings.pesPerGpcMasks[3] |= 0x4;  
                }
                floorsweepRandomTPCs(gpcSettings, chip.second->getChipPORSettings(), 1, false);
                fsChip->applyGPCMasks(gpcSettings);
                fsChip->applyFBPMasks(fbpSettings);            
                is_valid = fslib::IsFloorsweepingValid(
                    chip.first, gpcSettings, fbpSettings, msg, fslib::FSmode::PRODUCT);
                ASSERT_TRUE(is_valid) << "A perfect " << chip_name << " chip was considered invalid!"
                                    << " msg: " << msg;
                
                EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << chip_name << " " << msg;
                
                // Reset settings
                msg = "";
                fbpSettings = {0};    
                gpcSettings = {0};
            }
        }
    }
}

// Ensure chip with generated number of TPCs floorswept can be downbinned to approximate mainstream SKU
TEST_F(FSAdaRandomDownbinTest, Test_downBin_allchip_random_floorsweep_maxTPCs) {
    GTEST_SKIP(); //TODO fix this so it works with true random
    // Floorsweep the max number of TPCs for each chip
    for (auto &chip : chips_in_litter) {
        std::string chip_name = chip.second->getChipSettings().chip_por_settings.name;
        for (auto &sku_params : sku_parameter_generators0) {
            FUSESKUConfig::SkuConfig skuconfig = makeExampleAdaSku(chip.second, sku_params);
            const fslib::chipPOR_settings_t chip_cfg = chip.second->getChipSettings().chip_por_settings;
            for (int i = 0; i < num_random_iter; i++) {
                std::unique_ptr<FSChip_t> fsChip = createChip(chip.first);
                if (chip.first == fslib::Chip::AD103) {
                    gpcSettings.tpcPerGpcMasks[3] |= 0x30;
                    gpcSettings.pesPerGpcMasks[3] |= 0x4;  
                    // We already "floorswept" 2 TPCs
                    floorsweepRandomTPCs(gpcSettings, chip.second->getChipPORSettings(), sku_params.max_dead_tpc_gen_fn(chip_cfg) - 2, false);
                } else {
                    // FS some random TPCs up to the limit
                    floorsweepRandomTPCs(gpcSettings, chip.second->getChipPORSettings(), sku_params.max_dead_tpc_gen_fn(chip_cfg), false);
                }
                fsChip->applyGPCMasks(gpcSettings);
                fsChip->applyFBPMasks(fbpSettings);            
                is_valid = fslib::IsFloorsweepingValid(
                    chip.first, gpcSettings, fbpSettings, msg, fslib::FSmode::PRODUCT);
                ASSERT_TRUE(is_valid) << "A perfect " << chip_name << " chip was considered invalid!"
                                    << " msg: " << msg;
                
                EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << chip_name << " " << msg;
                
                // Reset settings
                msg = "";
                fbpSettings = {0};    
                gpcSettings = {0};
            }
        }
    }
}

// Ensure chip with 3 fewer TPCs than needed floorswept can be downbinned to generated SKU
TEST_F(FSAdaRandomDownbinTest, Test_downBin_allchip_random_floorsweep_max_minus_3_TPCs) {
    GTEST_SKIP();  //TODO fix this so it works with true random
    // Floorsweep maxTPCs - 3 for each chip
    for (auto &chip : chips_in_litter) {
        std::string chip_name = chip.second->getChipSettings().chip_por_settings.name;
        for (auto &sku_params : sku_parameter_generators0) {
            FUSESKUConfig::SkuConfig skuconfig = makeExampleAdaSku(chip.second, sku_params);
            const fslib::chipPOR_settings_t chip_cfg = chip.second->getChipSettings().chip_por_settings;
            for (int i = 0; i < num_random_iter; i++) {
                std::unique_ptr<FSChip_t> fsChip = createChip(chip.first);
                if (chip.first == fslib::Chip::AD103) {
                    gpcSettings.tpcPerGpcMasks[3] |= 0x30;
                    gpcSettings.pesPerGpcMasks[3] |= 0x4;  
                }
                // FS some random TPCs up to the limit
                floorsweepRandomTPCs(gpcSettings, chip.second->getChipPORSettings(), (sku_params.max_dead_tpc_gen_fn(chip_cfg) - 3), false);
                fsChip->applyGPCMasks(gpcSettings);
                fsChip->applyFBPMasks(fbpSettings);            
                is_valid = fslib::IsFloorsweepingValid(
                    chip.first, gpcSettings, fbpSettings, msg, fslib::FSmode::PRODUCT);
                ASSERT_TRUE(is_valid) << "A perfect " << chip_name << " chip was considered invalid!"
                                    << " msg: " << msg;
                
                EXPECT_TRUE(fsChip->canFitSKU(skuconfig, msg)) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->downBin(skuconfig, msg)) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->isInSKU(skuconfig, msg)) << chip_name << " " << msg;
                EXPECT_TRUE(fsChip->isValid(FSmode::FUNC_VALID, msg)) << chip_name << " " << msg;
                
                // Reset settings
                msg = "";
                fbpSettings = {0};    
                gpcSettings = {0};
            }
        }
    }
}


}