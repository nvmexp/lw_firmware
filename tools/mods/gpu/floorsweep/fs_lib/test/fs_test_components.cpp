#include <gtest/gtest.h>

#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <algorithm>
#include "fs_lib.h"
#include "fs_chip_core.h"
#include "fs_chip_ada.h"
#include "fs_chip_hopper.h"
#include "fs_chip_factory.h"
#include "fs_test_utils.h"
#include <iomanip>
#include <sstream>
#include <cassert>

#ifdef _MSC_VER
#pragma warning(disable : 4267)  // disable warnings about gcc pragmas and size_t to uint32_t colwersion
#endif

namespace fslib {

class FSChipComponent : public ::testing::Test {
    protected:
        void SetUp() override {
        }
};

static chipPOR_settings_t getSettingsForFBPTest() {
    chipPOR_settings_t settings;
    settings.num_ltcs = 2;
    settings.num_fbps = 1;
    settings.num_fbpas = 1;
    return settings;
}

static chipPOR_settings_t getSettingsForGPCWithCPCTest() {
    chipPOR_settings_t settings;
    settings.num_pes_per_gpc = 3;
    settings.num_tpc_per_gpc = 9;
    return settings;
}


TEST_F(FSChipComponent, Test_strToId) {
    EXPECT_EQ(strToId("gpc"), module_type_id::gpc);
    EXPECT_EQ(strToId("tpc"), module_type_id::tpc);
    EXPECT_EQ(strToId("pes"), module_type_id::pes);
    EXPECT_EQ(strToId("rop"), module_type_id::rop);
    EXPECT_EQ(strToId("ltc"), module_type_id::ltc);
}

TEST_F(FSChipComponent, Test_idToStr) {
    EXPECT_EQ(idToStr(module_type_id::gpc), "gpc");
    EXPECT_EQ(idToStr(module_type_id::rop), "rop");
    EXPECT_EQ(idToStr(module_type_id::tpc), "tpc");
    EXPECT_EQ(idToStr(module_type_id::pes), "pes");
    EXPECT_EQ(idToStr(module_type_id::ltc), "ltc");
}

TEST_F(FSChipComponent, Test_TPCPESMap) {
    {
        PESMapping_t tpc_pes_map(3, 9);

        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(0), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(1), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(2), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(3), 1);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(4), 1);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(5), 1);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(6), 2);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(7), 2);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(8), 2);

        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(0), (PESMapping_t::tpc_list_t{0, 1, 2}));
        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(1), (PESMapping_t::tpc_list_t{3, 4, 5}));
        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(2), (PESMapping_t::tpc_list_t{6, 7, 8}));
    }
    {
        PESMapping_t tpc_pes_map(3, 6);

        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(0), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(1), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(2), 1);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(3), 1);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(4), 2);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(5), 2);

        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(0), (PESMapping_t::tpc_list_t{0, 1}));
        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(1), (PESMapping_t::tpc_list_t{2, 3}));
        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(2), (PESMapping_t::tpc_list_t{4, 5}));
    }
    {
        PESMapping_t tpc_pes_map(2, 5);

        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(0), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(1), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(2), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(3), 1);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(4), 1);

        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(0), (PESMapping_t::tpc_list_t{0, 1, 2}));
        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(1), (PESMapping_t::tpc_list_t{3, 4}));
    }
    {
        PESMapping_t tpc_pes_map(3, 8);

        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(0), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(1), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(2), 0);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(3), 1);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(4), 1);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(5), 1);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(6), 2);
        EXPECT_EQ(tpc_pes_map.getTPCtoPESMapping(7), 2);

        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(0), (PESMapping_t::tpc_list_t{0, 1, 2}));
        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(1), (PESMapping_t::tpc_list_t{3, 4, 5}));
        EXPECT_EQ(tpc_pes_map.getPEStoTPCMappingVector(2), (PESMapping_t::tpc_list_t{6, 7}));
    }
}

TEST_F(FSChipComponent, Test_sortMajorUnitByMinorCount_1) {
    DDRFBP_t ddrfbp;
    const DDRFBP_t& const_ddrfbp = ddrfbp;

    ddrfbp.setup(getSettingsForFBPTest());

    module_index_list_t ltcs_by_l2slice = sortMajorUnitByMinorCount(
        const_ddrfbp.returnModuleList(module_type_id::ltc), const_ddrfbp.returnModuleList(module_type_id::l2slice));

    EXPECT_EQ(ltcs_by_l2slice, (module_index_list_t{0,1}));
}

TEST_F(FSChipComponent, Test_sortMajorUnitByMinorCount_2) {
    DDRFBP_t ddrfbp;
    const DDRFBP_t& const_ddrfbp = ddrfbp;
    ddrfbp.setup(getSettingsForFBPTest());

    ddrfbp.L2Slices[0]->setEnabled(false);

    module_index_list_t ltcs_by_l2slice = sortMajorUnitByMinorCount(
        const_ddrfbp.returnModuleList(module_type_id::ltc), const_ddrfbp.returnModuleList(module_type_id::l2slice));

    EXPECT_EQ(ltcs_by_l2slice, (module_index_list_t{1,0}));
}

TEST_F(FSChipComponent, Test_sortMajorUnitByMinorCount_3) {
    GPCWithCPC_t gpcwithcpc;
    const GPCWithCPC_t& const_gpcwithcpc = gpcwithcpc;
    gpcwithcpc.setup(getSettingsForGPCWithCPCTest());

    gpcwithcpc.TPCs[0]->setEnabled(false);

    module_index_list_t cpcs_by_tpc = sortMajorUnitByMinorCount(
        const_gpcwithcpc.returnModuleList(module_type_id::cpc), const_gpcwithcpc.returnModuleList(module_type_id::tpc));

    EXPECT_EQ(cpcs_by_tpc, (module_index_list_t{1,2,0}));
}

TEST_F(FSChipComponent, Test_sortMajorUnitByMinorCount_4) {
    GPCWithCPC_t gpcwithcpc;
    const GPCWithCPC_t& const_gpcwithcpc = gpcwithcpc;
    gpcwithcpc.setup(getSettingsForGPCWithCPCTest());

    gpcwithcpc.TPCs[0]->setEnabled(false);
    gpcwithcpc.TPCs[1]->setEnabled(false);
    gpcwithcpc.TPCs[4]->setEnabled(false);

    module_index_list_t cpcs_by_tpc = sortMajorUnitByMinorCount(
        const_gpcwithcpc.returnModuleList(module_type_id::cpc), const_gpcwithcpc.returnModuleList(module_type_id::tpc));

    EXPECT_EQ(cpcs_by_tpc, (module_index_list_t{2,1,0}));
}

TEST_F(FSChipComponent, Test_getModuleList1) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0xcc;
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    EXPECT_EQ(fsChip->returnModuleList(module_type_id::gpc).size(), 8);
}

TEST_F(FSChipComponent, Test_getModuleList2) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0xcc;
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    EXPECT_EQ(fsChip->returnModuleList(module_type_id::gpc)[0]->returnModuleList(module_type_id::tpc).size(), 9);
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[2]->getEnabled());
}

TEST_F(FSChipComponent, Test_getModuleCount) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    EXPECT_EQ(fsChip->returnModuleList(module_type_id::gpc)[0]->getModuleCount(module_type_id::tpc), 9);
    EXPECT_EQ(fsChip->getModuleCount(module_type_id::tpc), 72);
}


TEST_F(FSChipComponent, Test_getFlatList) {
    //------------------------------------------------------------------------------
    // 
    //------------------------------------------------------------------------------
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0xcc;
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    EXPECT_EQ(fsChip->getFlatList({module_type_id::gpc}).size(), 8);
    EXPECT_EQ(fsChip->getFlatList({module_type_id::gpc, module_type_id::tpc}).size(), 72);
    EXPECT_EQ(fsChip->getFlatList({module_type_id::fbp, module_type_id::ltc}).size(), 24);
    EXPECT_EQ(fsChip->getFlatList({module_type_id::fbp, module_type_id::l2slice}).size(), 96);
    flat_module_list_t flat_tpc_list = fsChip->getFlatList({module_type_id::gpc, module_type_id::tpc});
    EXPECT_TRUE(flat_tpc_list[10]->getEnabled());
    EXPECT_FALSE(flat_tpc_list[11]->getEnabled());
    EXPECT_FALSE(flat_tpc_list[12]->getEnabled());
    EXPECT_TRUE(flat_tpc_list[13]->getEnabled());
}

TEST_F(FSChipComponent, Test_getParentModuleName) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    EXPECT_EQ(fsChip->findParentModuleTypeId(module_type_id::tpc), module_type_id::gpc);
    EXPECT_EQ(fsChip->findParentModuleTypeId(module_type_id::gpc), module_type_id::chip);
    EXPECT_EQ(fsChip->findParentModuleTypeId(module_type_id::chip), module_type_id::invalid);
}

TEST_F(FSChipComponent, Test_getFlatModuleList) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    
    flat_module_list_t chip_gpc_flat_list = fsChip->getFlatModuleList(module_type_id::gpc);
    EXPECT_EQ(chip_gpc_flat_list.size(), 8);

    flat_module_list_t chip_tpc_flat_list = fsChip->getFlatModuleList(module_type_id::tpc);
    EXPECT_EQ(chip_tpc_flat_list.size(), 72);

    flat_module_list_t gpc_tpc_flat_list = static_cast<ParentFSModule_t*>(chip_gpc_flat_list[0])->getFlatModuleList(module_type_id::tpc);
    EXPECT_EQ(gpc_tpc_flat_list.size(), 9);
}

TEST_F(FSChipComponent, Test_getModuleFlatIdx) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->getFlatModuleList(module_type_id::tpc)[4]->setEnabled(false);
    fsChip->getFlatModuleList(module_type_id::tpc)[9]->setEnabled(false);
    fsChip->getFlatModuleList(module_type_id::tpc)[20]->setEnabled(false);
    fsChip->getFlatModuleList(module_type_id::tpc)[35]->setEnabled(false);
    fsChip->getFlatModuleList(module_type_id::tpc)[71]->setEnabled(false);
    fsChip->getFlatModuleList(module_type_id::tpc)[60]->setEnabled(false);

    for(uint32_t i = 0; i < fsChip->getFlatModuleList(module_type_id::tpc).size(); i++){
        EXPECT_EQ(fsChip->getFlatModuleList(module_type_id::tpc)[i], fsChip->getModuleFlatIdx(module_type_id::tpc, i)) << i;
    }

}

TEST_F(FSChipComponent, Test_skuIsPossible) {
    std::string msg;
    {   // basic case. valid
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {32, 32, 4, UNKNOWN_ENABLE_COUNT, 0, 0, {}, {9,10}};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_TRUE(skuconfig.skuIsPossible(msg)) << msg;
    }
    {   // min greater than max invalid
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {33, 32, 4};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(skuconfig.skuIsPossible(msg));
    }
    {   // mustDisable list cannot overlap with the mustEnable list
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {32, 32, 4, UNKNOWN_ENABLE_COUNT, 0, 0, {3, 5, 7, 10}, {5}};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(skuconfig.skuIsPossible(msg));
    }
    {   // minEnablePerGroup cannot be more than maxEnableCount
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {4, 4, 5};
        skuconfig.fsSettings["gpc"] = {8, 8};
        EXPECT_FALSE(skuconfig.skuIsPossible(msg));
    }
}

TEST_F(FSChipComponent, Test_clone) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0xcc;
    gpcSettings.pesPerGpcMasks[0] = 0x0;

    GpcMasks spareGpcSettings = { 0 };
    spareGpcSettings.tpcPerGpcMasks[1] = 0x4;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->spare_gpc_mask = spareGpcSettings;

    std::unique_ptr<FSChip_t> cloned_chip = fsChip->clone();

    EXPECT_TRUE(cloned_chip->GPCs[1]->TPCs[0]->getEnabled());
    EXPECT_TRUE(cloned_chip->GPCs[1]->TPCs[1]->getEnabled());
    EXPECT_FALSE(cloned_chip->GPCs[1]->TPCs[2]->getEnabled());
    EXPECT_FALSE(cloned_chip->GPCs[1]->TPCs[3]->getEnabled());
    EXPECT_TRUE(cloned_chip->GPCs[1]->TPCs[4]->getEnabled());
    EXPECT_TRUE(cloned_chip->GPCs[1]->TPCs[5]->getEnabled());
    EXPECT_FALSE(cloned_chip->GPCs[1]->TPCs[6]->getEnabled());
    EXPECT_FALSE(cloned_chip->GPCs[1]->TPCs[7]->getEnabled());
    EXPECT_TRUE(fsChip->GPCs[0]->returnModuleList(module_type_id::pes)[0]->getEnabled());
    EXPECT_TRUE(cloned_chip->GPCs[0]->returnModuleList(module_type_id::pes)[0]->getEnabled());

    cloned_chip->GPCs[1]->TPCs[0]->setEnabled(false);
    EXPECT_FALSE(cloned_chip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());
    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[1]->returnModuleList(module_type_id::tpc)[0]->getEnabled());

    EXPECT_EQ(cloned_chip->spare_gpc_mask, spareGpcSettings);
}


TEST_F(FSChipComponent, Test_equals1) {
    std::unique_ptr<FSChip_t> fsChip1 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        gpcSettings.tpcPerGpcMasks[1] = 0xcc;
        fsChip1->applyGPCMasks(gpcSettings);
        fsChip1->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip2 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        gpcSettings.tpcPerGpcMasks[1] = 0xcc;
        fsChip2->applyGPCMasks(gpcSettings);
        fsChip2->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip3 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        gpcSettings.tpcPerGpcMasks[1] = 0x33;
        fsChip3->applyGPCMasks(gpcSettings);
        fsChip3->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip4 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = { 0 };
        GpcMasks gpcSettings = { 0 };
        GpcMasks spareGpcSettings = { 0 };
        gpcSettings.tpcPerGpcMasks[1] = 0x33;
        spareGpcSettings.tpcPerGpcMasks[1] = 0x8;
        fsChip3->spare_gpc_mask = spareGpcSettings;
        fsChip3->applyGPCMasks(gpcSettings);
        fsChip3->applyFBPMasks(fbpSettings);
    }


    EXPECT_TRUE (*fsChip1 == *fsChip2);
    EXPECT_TRUE (*fsChip2 == *fsChip1);
    EXPECT_FALSE(*fsChip1 == *fsChip3);
    EXPECT_FALSE(*fsChip3 == *fsChip1);
    EXPECT_FALSE(*fsChip3 == *fsChip2);
    EXPECT_FALSE(*fsChip2 == *fsChip3);
    EXPECT_FALSE(*fsChip4 == *fsChip3);
}

TEST_F(FSChipComponent, Test_equalsPES) {
    // make sure mismatching PESs aren't equal
    std::unique_ptr<FSChip_t> fsChip1 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        gpcSettings.pesPerGpcMasks[0] = 0x1;
        fsChip1->applyGPCMasks(gpcSettings);
        fsChip1->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip2 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        gpcSettings.pesPerGpcMasks[0] = 0x0;
        fsChip2->applyGPCMasks(gpcSettings);
        fsChip2->applyFBPMasks(fbpSettings);
    }
    EXPECT_FALSE(*fsChip1 == *fsChip2);
}

TEST_F(FSChipComponent, Test_equalsROP) {
    // make sure mismatching ROPs aren't equal
    std::unique_ptr<FSChip_t> fsChip1 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        gpcSettings.ropPerGpcMasks[0] = 0x1;
        fsChip1->applyGPCMasks(gpcSettings);
        fsChip1->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip2 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        gpcSettings.ropPerGpcMasks[0] = 0x0;
        fsChip2->applyGPCMasks(gpcSettings);
        fsChip2->applyFBPMasks(fbpSettings);
    }
    EXPECT_FALSE(*fsChip1 == *fsChip2);
}

TEST_F(FSChipComponent, Test_equalsFBIO) {
    // make sure mismatching FBIOs aren't equal
    std::unique_ptr<FSChip_t> fsChip1 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        fbpSettings.fbioPerFbpMasks[0] = 0x1;
        fsChip1->applyGPCMasks(gpcSettings);
        fsChip1->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip2 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        fbpSettings.fbioPerFbpMasks[0] = 0x0;
        fsChip2->applyGPCMasks(gpcSettings);
        fsChip2->applyFBPMasks(fbpSettings);
    }
    EXPECT_FALSE(*fsChip1 == *fsChip2);
}

TEST_F(FSChipComponent, Test_equalsLTC) {
    // make sure mismatching LTCs aren't equal
    std::unique_ptr<FSChip_t> fsChip1 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        fbpSettings.ltcPerFbpMasks[0] = 0x1;
        fsChip1->applyGPCMasks(gpcSettings);
        fsChip1->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip2 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        fbpSettings.ltcPerFbpMasks[0] = 0x0;
        fsChip2->applyGPCMasks(gpcSettings);
        fsChip2->applyFBPMasks(fbpSettings);
    }
    EXPECT_FALSE(*fsChip1 == *fsChip2);
}

TEST_F(FSChipComponent, Test_equalsSlice) {
    // make sure mismatching LTCs aren't equal
    std::unique_ptr<FSChip_t> fsChip1 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        fbpSettings.l2SlicePerFbpMasks[0] = 0x1;
        fsChip1->applyGPCMasks(gpcSettings);
        fsChip1->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip2 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        fbpSettings.l2SlicePerFbpMasks[0] = 0x0;
        fsChip2->applyGPCMasks(gpcSettings);
        fsChip2->applyFBPMasks(fbpSettings);
    }
    EXPECT_FALSE(*fsChip1 == *fsChip2);
}

TEST_F(FSChipComponent, Test_merge) {
    std::unique_ptr<FSChip_t> fsChip1 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        gpcSettings.tpcPerGpcMasks[1] = 0xcc;
        fsChip1->applyGPCMasks(gpcSettings);
        fsChip1->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip2 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        gpcSettings.tpcPerGpcMasks[2] = 0xcc;
        fsChip2->applyGPCMasks(gpcSettings);
        fsChip2->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip3 = createChip(fslib::Chip::GH100);
    {
        FbpMasks fbpSettings = {0};
        GpcMasks gpcSettings = {0};
        gpcSettings.tpcPerGpcMasks[1] = 0xcc;
        gpcSettings.tpcPerGpcMasks[2] = 0xcc;
        fsChip3->applyGPCMasks(gpcSettings);
        fsChip3->applyFBPMasks(fbpSettings);
    }

    std::unique_ptr<FSChip_t> fsChip4 = fsChip1->clone();
    fsChip4->merge(*fsChip2);

    EXPECT_FALSE(*fsChip1 == *fsChip2);
    EXPECT_FALSE(*fsChip1 == *fsChip3);
    EXPECT_FALSE(*fsChip3 == *fsChip2);

    EXPECT_FALSE(*fsChip4 == *fsChip2);
    EXPECT_FALSE(*fsChip4 == *fsChip1);

    EXPECT_TRUE(*fsChip3 == *fsChip4);
}

TEST_F(FSChipComponent, Test_get_masks_apply_masks) {

    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0x13;
    gpcSettings.pesPerGpcMasks[1] = 0x1;
    gpcSettings.ropPerGpcMasks[1] = 0x1;
    gpcSettings.gpcMask = 0x4;

    fbpSettings.fbpMask = 0x2;
    fbpSettings.fbioPerFbpMasks[1] = 0x1;
    fbpSettings.ltcPerFbpMasks[0] = 0x1;
    fbpSettings.halfFbpaPerFbpMasks[0] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    EXPECT_TRUE(fsChip->returnModuleList(module_type_id::gpc)[0]->getEnabled());
    EXPECT_FALSE(fsChip->returnModuleList(module_type_id::gpc)[2]->getEnabled());


    FbpMasks fbpSettingsOut = {0};
    GpcMasks gpcSettingsOut = {0};

    fsChip->getGPCMasks(gpcSettingsOut);
    fsChip->getFBPMasks(fbpSettingsOut);

    EXPECT_EQ(fbpSettings, fbpSettingsOut);
    EXPECT_EQ(gpcSettings, gpcSettingsOut);
}

TEST_F(FSChipComponent, Test_getEnableCount) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0x13;
    gpcSettings.pesPerGpcMasks[1] = 0x1;
    gpcSettings.ropPerGpcMasks[1] = 0x1;
    gpcSettings.gpcMask = 0x4;

    fbpSettings.fbpMask = 0x2;
    fbpSettings.fbioPerFbpMasks[1] = 0x1;
    fbpSettings.ltcPerFbpMasks[0] = 0x1;
    fbpSettings.halfFbpaPerFbpMasks[0] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    EXPECT_EQ(fsChip->getEnableCount(module_type_id::gpc), 7);
    EXPECT_EQ(fsChip->returnModuleList(module_type_id::gpc)[1]->getEnableCount(module_type_id::tpc), 6);
}

TEST_F(FSChipComponent, Test_getCPCIdxsByTPCCount_1) {
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0x100;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    EXPECT_EQ((module_index_list_t{0,1,2}), dynamic_cast<GPCWithCPC_t*>(fsChip->returnModuleList(module_type_id::gpc)[1])->getCPCIdxsByTPCCount());
}

TEST_F(FSChipComponent, Test_getCPCIdxsByTPCCount_2) {
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    EXPECT_EQ((module_index_list_t{1,2,0}), dynamic_cast<GPCWithCPC_t*>(fsChip->returnModuleList(module_type_id::gpc)[1])->getCPCIdxsByTPCCount());
}

TEST_F(FSChipComponent, Test_getCPCIdxsByTPCCount_3) {
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0x8;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    EXPECT_EQ((module_index_list_t{0,2,1}), dynamic_cast<GPCWithCPC_t*>(fsChip->returnModuleList(module_type_id::gpc)[1])->getCPCIdxsByTPCCount());
}

TEST_F(FSChipComponent, Test_getSortedListByEnableCount) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[0] = 0x13;
    gpcSettings.tpcPerGpcMasks[1] = 0x1;
    gpcSettings.tpcPerGpcMasks[2] = 0x100;
    gpcSettings.tpcPerGpcMasks[3] = 0x1f0;
    gpcSettings.tpcPerGpcMasks[4] = 0x123;
    gpcSettings.tpcPerGpcMasks[5] = 0x67;
    gpcSettings.tpcPerGpcMasks[7] = 0x70;

    fbpSettings.ltcPerFbpMasks[4] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    EXPECT_EQ((module_index_list_t{3, 5, 4, 0, 7, 1, 2, 6}), fsChip->getSortedListByEnableCount(module_type_id::gpc, module_type_id::tpc));
    EXPECT_EQ((module_index_list_t{1, 4, 0, 2, 3, 5, 6, 7, 8, 9, 10, 11}), fsChip->getSortedListByEnableCount(module_type_id::fbp, module_type_id::ltc));
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_1) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[0] = 0x13;
    gpcSettings.tpcPerGpcMasks[1] = 0x1;
    gpcSettings.tpcPerGpcMasks[2] = 0x100;
    gpcSettings.tpcPerGpcMasks[3] = 0x1f0;
    gpcSettings.tpcPerGpcMasks[4] = 0x123;
    gpcSettings.tpcPerGpcMasks[5] = 0x67;
    gpcSettings.tpcPerGpcMasks[6] = 0x0;
    gpcSettings.tpcPerGpcMasks[7] = 0x70;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    EXPECT_EQ((module_index_list_t{3, 5, 4, 7, 1, 2, 6}), fsChip->getPreferredDisableList(module_type_id::gpc));
    EXPECT_EQ((module_index_list_t{6, 11, 1, 4, 0, 2, 3, 5, 7, 8, 9, 10}), fsChip->getPreferredDisableList(module_type_id::fbp));


    // first pick the gpc with the fewest TPCs (gpc6), then pick a tpc from there
    EXPECT_EQ(fsChip->getPreferredDisableList(module_type_id::tpc)[0], 54);
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_2) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    init_array(gpcSettings.tpcPerGpcMasks, {0x49, 0x49, 0x49, 0x49, 0x49, 0x49, 0x11, 0x49}); //gpc6 has fewest tpc defects

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    // first pick the gpc with the fewest TPCs (gpc6), then pick a tpc from there
    EXPECT_EQ(fsChip->getPreferredDisableList(module_type_id::tpc)[0], 60);
}

// don't recommend to floorsweep already disabled modules
TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_3) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[0] = 0x13;
    gpcSettings.tpcPerGpcMasks[1] = 0x1;
    gpcSettings.tpcPerGpcMasks[2] = 0x100;
    gpcSettings.tpcPerGpcMasks[4] = 0x123;
    gpcSettings.tpcPerGpcMasks[5] = 0x67;
    gpcSettings.tpcPerGpcMasks[7] = 0x70;
    gpcSettings.gpcMask = 0x8;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    EXPECT_EQ((module_index_list_t{5, 4, 7, 1, 2, 6}), fsChip->getPreferredDisableList(module_type_id::gpc));
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_4) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};
    fbpSettings.ltcPerFbpMasks[0] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    EXPECT_EQ((module_index_list_t{0, 3, 6, 11, 1, 4, 2, 5, 7, 8, 9, 10}), fsChip->getPreferredDisableList(module_type_id::fbp));
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_within_gpc1) {
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0x10;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();
    
    module_index_list_t order{0,6,1,3,2,8,7,5};
    // TPC getPreferredDisable optimized to return only optimal TPC
    for (uint32_t i = 0; i < fsChip->returnModuleList(module_type_id::gpc)[1]->getEnableCount(module_type_id::tpc); i++) {
        EXPECT_EQ(order[i], fsChip->returnModuleList(module_type_id::gpc)[1]->getPreferredDisableList(module_type_id::tpc)[0]);
        static_cast<GPC_t*>(fsChip->returnModuleList(module_type_id::gpc)[1])->TPCs[order[i]]->setEnabled(false);
    }
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_within_gpc2) {
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    module_index_list_t order{3,6,1,4,5,7,8,2};
    // TPC getPreferredDisable optimized to return only optimal TPC
    for (uint32_t i = 0; i < fsChip->returnModuleList(module_type_id::gpc)[1]->getEnableCount(module_type_id::tpc); i++) {
        EXPECT_EQ(order[i], fsChip->returnModuleList(module_type_id::gpc)[1]->getPreferredDisableList(module_type_id::tpc)[0]);
        static_cast<GPC_t*>(fsChip->returnModuleList(module_type_id::gpc)[1])->TPCs[order[i]]->setEnabled(false);
    }
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_within_gpc3) {
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0x100;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    module_index_list_t order{0,3,1,4,6,2,5,7};
    // TPC getPreferredDisable optimized to return only optimal TPC
    for (uint32_t i = 0; i < fsChip->returnModuleList(module_type_id::gpc)[1]->getEnableCount(module_type_id::tpc); i++) {
        EXPECT_EQ(order[i], fsChip->returnModuleList(module_type_id::gpc)[1]->getPreferredDisableList(module_type_id::tpc)[0]);
        static_cast<GPC_t*>(fsChip->returnModuleList(module_type_id::gpc)[1])->TPCs[order[i]]->setEnabled(false);
    }
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_within_gpc4) {
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0x13;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    module_index_list_t order{6,3,7,2,5,8};
    // TPC getPreferredDisable optimized to return only optimal TPC
    for (uint32_t i = 0; i < fsChip->returnModuleList(module_type_id::gpc)[1]->getEnableCount(module_type_id::tpc); i++) {
        EXPECT_EQ(order[i], fsChip->returnModuleList(module_type_id::gpc)[1]->getPreferredDisableList(module_type_id::tpc)[0]);
        static_cast<GPC_t*>(fsChip->returnModuleList(module_type_id::gpc)[1])->TPCs[order[i]]->setEnabled(false);
    }
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_within_gpc5) {
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[1] = 0x19;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    module_index_list_t order{6,1,7,2,5,8};
    // TPC getPreferredDisable optimized to return only optimal TPC
    for (uint32_t i = 0; i < fsChip->returnModuleList(module_type_id::gpc)[1]->getEnableCount(module_type_id::tpc); i++) {
        EXPECT_EQ(order[i], fsChip->returnModuleList(module_type_id::gpc)[1]->getPreferredDisableList(module_type_id::tpc)[0]);
        static_cast<GPC_t*>(fsChip->returnModuleList(module_type_id::gpc)[1])->TPCs[order[i]]->setEnabled(false);
    }
}

TEST_F(FSChipComponent, Test_sortGPCsByTPCSkylineMargin_gh100_1) {
    GpcMasks gpcSettings = {0};
    init_array(gpcSettings.tpcPerGpcMasks, {0x1, 0x1, 0xf, 0x7, 0x3, 0xf, 0x0, 0x1f});

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    const GH100_t* gh100_ptr = dynamic_cast<const GH100_t*>(fsChip.get());
    assert(gh100_ptr != nullptr);

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    std::vector<uint32_t> skyline = {3,3,3,3,3,3,3,3};
    EXPECT_EQ(gh100_ptr->sortGPCsByTPCSkylineMargin(skyline), (module_index_list_t{6,0,1,4,3,2,5,7}));
}

//Similar to the last one, but this one should have ties broken by ugpu imbalance
TEST_F(FSChipComponent, Test_sortGPCsByTPCSkylineMargin_gh100_imbalance) {
    GpcMasks gpcSettings = {0};

    // ugpu0 has 11 dead tpcs
    // ugpu1 has 9 dead tpcs
    init_array(gpcSettings.tpcPerGpcMasks, {0x1, 0xf, 0x1, 0x3, 0x3, 0xf, 0x0, 0x3f});

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    const GH100_t* gh100_ptr = dynamic_cast<const GH100_t*>(fsChip.get());
    assert(gh100_ptr != nullptr);

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    std::vector<uint32_t> skyline = {3,3,3,3,3,3,3,3};
    EXPECT_EQ(gh100_ptr->getuGPUImbalance(), -2);
    // 2 comes before 0 because ugpu1 has more good TPCs.
    // 5 comes before 1 and 7 because ugpu1 has more good TPCs
    EXPECT_EQ(gh100_ptr->sortGPCsByTPCSkylineMargin(skyline), (module_index_list_t{6,2,0,3,4,5,1,7}));
}

//Similar to the last one, but with a less square skyline
TEST_F(FSChipComponent, Test_sortGPCsByTPCSkylineMargin_gh100_3) {
    GpcMasks gpcSettings = {0};

    // ugpu0 has 1 dead tpcs
    // ugpu1 has 3 dead tpcs
    init_array(gpcSettings.tpcPerGpcMasks, {0x2, 0x0, 0x4, 0x0, 0x3, 0x0, 0x0, 0x0});

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    const GH100_t* gh100_ptr = dynamic_cast<const GH100_t*>(fsChip.get());
    assert(gh100_ptr != nullptr);

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    std::vector<uint32_t> skyline =   {6,6,7,8,8,9,9,9};
    // logical ordering of tpc counts: 7/8/8/9/9/9/9/9
    // physical ids in logical order:  4,0,2,1,3,5,6,7

    EXPECT_EQ(gh100_ptr->sortGPCsByTPCSkylineMargin(skyline), (module_index_list_t{0,1,3,2,4,6,7,5}));
}

// this skyline can fit
TEST_F(FSChipComponent, Test_skylineFits_gh100_1) {
    GpcMasks gpcSettings = {0};
    init_array(gpcSettings.tpcPerGpcMasks, {0x2, 0x0, 0x4, 0x0, 0x3, 0x0, 0x0, 0x0});

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    const GH100_t* gh100_ptr = dynamic_cast<const GH100_t*>(fsChip.get());
    assert(gh100_ptr != nullptr);

    std::string msg;

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    std::vector<uint32_t> skyline = {6,6,7,8,8,9,9,9};
    EXPECT_TRUE(gh100_ptr->skylineFits(skyline, msg)) << msg;
}

// Test a skyline that has fewer than 8 GPCs
TEST_F(FSChipComponent, Test_skylineFits_gh100_2) {
    GpcMasks gpcSettings = {0};
    init_array(gpcSettings.tpcPerGpcMasks, {0x2, 0x0, 0x4, 0xf, 0x3, 0x0, 0x0, 0x0});

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    const GH100_t* gh100_ptr = dynamic_cast<const GH100_t*>(fsChip.get());
    assert(gh100_ptr != nullptr);

    std::string msg;

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    std::vector<uint32_t> skyline = {6,6,7,8,8,9,9};
    EXPECT_TRUE(gh100_ptr->skylineFits(skyline, msg)) << msg;
}

// This skyline cannot fit, there are too many GPCs with 2 or more bad TPCs
TEST_F(FSChipComponent, Test_skylineFits_gh100_3) {
    GpcMasks gpcSettings = {0};
    init_array(gpcSettings.tpcPerGpcMasks, {0x21, 0x0, 0x4, 0x0, 0x3, 0x0, 0x70, 0x3});

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    const GH100_t* gh100_ptr = dynamic_cast<const GH100_t*>(fsChip.get());
    assert(gh100_ptr != nullptr);

    std::string msg;

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    std::vector<uint32_t> skyline = {6,6,7,8,8,9,9,9};
    EXPECT_FALSE(gh100_ptr->skylineFits(skyline, msg)) << msg;
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_within_gpc0_1) {
    GpcMasks gpcSettings = {0};
    gpcSettings.tpcPerGpcMasks[0] = 0x10;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->funcDownBin();

    // EXPECT_EQ((module_index_list_t{6,7,8,0,1,2,3,5}), fsChip->returnModuleList(module_type_id::gpc)[0]->getPreferredDisableList(module_type_id::tpc));
    
    module_index_list_t order{6,3,7,5,8,0,1,2};
    // TPC getPreferredDisable optimized to return only optimal TPC
    for (uint32_t i = 0; i < fsChip->returnModuleList(module_type_id::gpc)[0]->getEnableCount(module_type_id::tpc); i++) {
        EXPECT_EQ(order[i], fsChip->returnModuleList(module_type_id::gpc)[0]->getPreferredDisableList(module_type_id::tpc)[0]);
        static_cast<GPC_t*>(fsChip->returnModuleList(module_type_id::gpc)[0])->TPCs[order[i]]->setEnabled(false);
    }
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_fbp_0) {
    FbpMasks fbpSettings = {0};
    fbpSettings.l2SlicePerFbpMasks[8] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    EXPECT_EQ(7, fsChip->getPreferredDisableList(module_type_id::fbp)[0]);
    EXPECT_EQ(8, fsChip->getPreferredDisableList(module_type_id::fbp)[1]);
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_fbp_1) {
    FbpMasks fbpSettings = {0};
    fbpSettings.l2SlicePerFbpMasks[1] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    EXPECT_EQ(1, fsChip->getPreferredDisableList(module_type_id::fbp)[0]);
    EXPECT_EQ(4, fsChip->getPreferredDisableList(module_type_id::fbp)[1]);
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_fbp_2) {
    FbpMasks fbpSettings = {0};
    fbpSettings.ltcPerFbpMasks[9] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    EXPECT_EQ( 9, fsChip->getPreferredDisableList(module_type_id::fbp)[0]);
    EXPECT_EQ(10, fsChip->getPreferredDisableList(module_type_id::fbp)[1]);
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_fbp_3) {
    FbpMasks fbpSettings = {0};
    fbpSettings.ltcPerFbpMasks[5] = 0x1;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    EXPECT_EQ(2, fsChip->getPreferredDisableList(module_type_id::fbp)[0]);
    EXPECT_EQ(5, fsChip->getPreferredDisableList(module_type_id::fbp)[1]);
}

TEST_F(FSChipComponent, Test_getPreferredDisableList_slice_gh100_nofs) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->funcDownBin();

    EXPECT_EQ(6 * 8, fsChip->getPreferredDisableList(module_type_id::l2slice)[0]);
}


//prefer wholes to halves
TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_fbp_prefer_wholes_1) {
    FbpMasks fbpSettings = {0};
    fbpSettings.fbpMask = 0x80; //fbp 7 already dead //which means 8 will be too

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    EXPECT_EQ( 9, fsChip->getPreferredDisableList(module_type_id::fbp)[0]);
    EXPECT_EQ(10, fsChip->getPreferredDisableList(module_type_id::fbp)[1]);
}

//prefer wholes to halves
TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_fbp_prefer_wholes_2) {
    FbpMasks fbpSettings = {0};
    fbpSettings.fbpMask = 0x8; //fbp 3 already dead //which means 0 will be too

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    EXPECT_EQ(2, fsChip->getPreferredDisableList(module_type_id::fbp)[0]);
    EXPECT_EQ(5, fsChip->getPreferredDisableList(module_type_id::fbp)[1]);
}

//prefer site E
TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_fbp_prefer_E) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    EXPECT_EQ(6, fsChip->getPreferredDisableList(module_type_id::fbp)[0]);
    EXPECT_EQ(11, fsChip->getPreferredDisableList(module_type_id::fbp)[1]);
}

// If half of site E is already FSed, the next LTC should also be in site E
TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_ltc_prefer_E_1) {
    FbpMasks fbpSettings = {0};
    fbpSettings.ltcPerFbpMasks[6] = 0x1;
    fbpSettings.ltcPerFbpMasks[11] = 0x2;
    fbpSettings.l2SlicePerFbpMasks[6] = 0xf;
    fbpSettings.l2SlicePerFbpMasks[11] = 0xf0;

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);

    EXPECT_EQ(13, fsChip->getPreferredDisableList(module_type_id::ltc)[0]);
}

//prefer LTCs in site E
TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_ltc_prefer_E_2) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    EXPECT_EQ(12, fsChip->getPreferredDisableList(module_type_id::ltc)[0]);
    fsChip->getFlatModuleList(module_type_id::ltc)[12]->setEnabled(false);
    fsChip->funcDownBin();

    EXPECT_EQ(13, fsChip->getPreferredDisableList(module_type_id::ltc)[0]);
}

#if 0 //todo
//prefer LTCs that already have dead slices
TEST_F(FSChipComponent, Test_getPreferredDisableList_gh100_ltc_prefer_bad_slices) {
    FbpMasks fbpSettings = {0};
    fbpSettings.l2SlicePerFbpMasks[7] = 0x20;
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    fsChip->applyFBPMasks(fbpSettings);
    fsChip->funcDownBin();

    //EXPECT_EQ(15, fsChip->getPreferredDisableList(module_type_id::ltc)[0]); //TODO prefer LTCs with bad slices
}
#endif

//prefer LTCs that already have dead slices
TEST_F(FSChipComponent, Test_getPreferredDisableListMustEnable_gh100_1) {
    {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["ltc"] = {20, 20, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {12, 23}, {}};
        skuconfig.fsSettings["fbp"] = {10, 10};

        EXPECT_EQ(fsChip->getPreferredDisableListMustEnable(module_type_id::fbp, skuconfig)[0], 1);
    }
    {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["ltc"] = {20, 20};
        skuconfig.fsSettings["fbp"] = {10, 12, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {6,11}, {}};

        EXPECT_EQ(fsChip->getPreferredDisableListMustEnable(module_type_id::fbp, skuconfig)[0], 1);
    }
}

TEST_F(FSChipComponent, Test_findAllSubUnitTypes_GPC) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    EXPECT_EQ((module_set_t{module_type_id::tpc, module_type_id::cpc, module_type_id::pes, module_type_id::rop}), 
    fsChip->returnModuleList(module_type_id::gpc)[0]->findAllSubUnitTypes());

    EXPECT_EQ((module_set_t{
        module_type_id::tpc, module_type_id::cpc, 
        module_type_id::pes, module_type_id::rop,
        module_type_id::gpc, module_type_id::fbp,
        module_type_id::ltc, module_type_id::l2slice,
        module_type_id::fbio
        }), 
        fsChip->findAllSubUnitTypes()
    );
}

TEST_F(FSChipComponent, Test_makeHierarchyMap) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    hierarchy_map_t expected;
    expected[module_type_id::tpc] = {module_type_id::gpc, module_type_id::tpc};
    expected[module_type_id::cpc] = {module_type_id::gpc, module_type_id::cpc};
    expected[module_type_id::rop] = {module_type_id::gpc, module_type_id::rop};
    expected[module_type_id::pes] = {module_type_id::gpc, module_type_id::pes};
    expected[module_type_id::gpc] = {module_type_id::gpc};

    expected[module_type_id::fbp]      = {module_type_id::fbp};
    expected[module_type_id::fbio]     = {module_type_id::fbp, module_type_id::fbio};
    expected[module_type_id::ltc]      = {module_type_id::fbp, module_type_id::ltc};
    expected[module_type_id::l2slice]  = {module_type_id::fbp, module_type_id::l2slice};

    EXPECT_EQ(expected, fsChip->makeHierarchyMap());
}

// try getting the hierarchy within a GPC
TEST_F(FSChipComponent, Test_makeHierarchyMap_GPC) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    hierarchy_map_t expected;
    expected[module_type_id::tpc] = {module_type_id::tpc};
    expected[module_type_id::cpc] = {module_type_id::cpc};
    expected[module_type_id::rop] = {module_type_id::rop};
    expected[module_type_id::pes] = {module_type_id::pes};
    EXPECT_EQ(expected, static_cast<ParentFSModule_t*>(fsChip->returnModuleList(module_type_id::gpc)[0])->makeHierarchyMap());
}

TEST_F(FSChipComponent, Test_findHierarchy) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    hierarchy_list_t expected = {module_type_id::gpc, module_type_id::tpc};
    EXPECT_EQ(expected, fsChip->findHierarchy(module_type_id::tpc));
}

TEST_F(FSChipComponent, Test_findAllSubUnitTypes) {
    FbpMasks fbpSettings = {0};
    GpcMasks gpcSettings = {0};

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    EXPECT_EQ(fsChip->findAllSubUnitTypes(), 
    (module_set_t{module_type_id::rop, module_type_id::pes, module_type_id::tpc, module_type_id::cpc,
                              module_type_id::gpc, module_type_id::fbp, module_type_id::ltc, module_type_id::l2slice,
                              module_type_id::fbio}));

    EXPECT_EQ(fsChip->returnModuleList(module_type_id::gpc)[0]->findAllSubUnitTypes(), 
    (module_set_t{module_type_id::rop, module_type_id::pes, module_type_id::tpc, module_type_id::cpc}));
}

TEST_F(FSChipComponent, Test_getMinGroupCount) {
    {
        //minEnablePerGroup is set, but enableCountPerGroup is not set
        FUSESKUConfig::FsConfig fsconfig{62, 62, 5};
        EXPECT_EQ(fsconfig.getMinGroupCount(), 5);
    }
    {
        //minEnablePerGroup is set, but enableCountPerGroup is not set
        FUSESKUConfig::FsConfig fsconfig{62, 62, UNKNOWN_ENABLE_COUNT, 4};
        EXPECT_EQ(fsconfig.getMinGroupCount(), 4);
    }
    {
        //minEnablePerGroup is set, but enableCountPerGroup is not set
        FUSESKUConfig::FsConfig fsconfig{62, 62};
        EXPECT_EQ(fsconfig.getMinGroupCount(), 1);
    }
}

TEST_F(FSChipComponent, Test_getMaxAllowed) {

    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);

    FUSESKUConfig::SkuConfig skuconfig;
    skuconfig.fsSettings["tpc"] = {61, 61, 6};
    skuconfig.fsSettings["gpc"] = {7, 8};

    EXPECT_EQ(fsChip->getMaxAllowed(module_type_id::fbp, skuconfig), 12);

}

TEST_F(FSChipComponent, Test_isInMustEnableList) {
    {
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["gpc"] = {7, 7};
        EXPECT_FALSE(isInMustEnableList(skuconfig, module_type_id::gpc, 8));
    }
    {
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["gpc"] = {7, 7, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {2,3,4}, {}};
        EXPECT_TRUE(isInMustEnableList(skuconfig, module_type_id::gpc, 4));
    }
    {
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["gpc"] = {7, 7, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {2,3,4}, {}};
        EXPECT_FALSE(isInMustEnableList(skuconfig, module_type_id::fbp, 4));
    }
}

// test that a "must enable" submodule causes the parent module to be must enable also
TEST_F(FSChipComponent, Test_isMustEnable_1) {
    {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["tpc"] = {1, 72, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {9}, {}};

        EXPECT_TRUE(fsChip->isMustEnable(skuconfig, module_type_id::gpc, 1));
        EXPECT_FALSE(fsChip->isMustEnable(skuconfig, module_type_id::gpc, 0));
    }
    {
        std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
        FUSESKUConfig::SkuConfig skuconfig;
        skuconfig.fsSettings["ltc"] = {1, 24, UNKNOWN_ENABLE_COUNT, UNKNOWN_ENABLE_COUNT, 0, 0, {12,13,22,23}, {}};

        EXPECT_TRUE(fsChip->isMustEnable(skuconfig, module_type_id::fbp, 6));
        EXPECT_TRUE(fsChip->isMustEnable(skuconfig, module_type_id::fbp, 11));
    }
}

TEST_F(FSChipComponent, Test_apply3SliceProduct_1) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    ADLit1Chip_t* adlit1_chip_ptr = static_cast<ADLit1Chip_t*>(fsChip.get());

    std::string msg;
    mask32_t first_mask;
    adlit1_chip_ptr->applySliceProduct(mask32_t(0x81), 0);

    EXPECT_TRUE(adlit1_chip_ptr->isValid(FSmode::PRODUCT, msg)) << msg;

    EXPECT_EQ(adlit1_chip_ptr->getNumEnabledSlices(), 36);
    EXPECT_EQ(static_cast<const FBP_t*>(adlit1_chip_ptr->returnModuleList(module_type_id::fbp)[0])->getL2Mask(), 0x81);
    EXPECT_EQ(static_cast<const FBP_t*>(adlit1_chip_ptr->returnModuleList(module_type_id::fbp)[1])->getL2Mask(), 0x42);
}

TEST_F(FSChipComponent, Test_apply3SliceProduct_2) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    ADLit1Chip_t* adlit1_chip_ptr = static_cast<ADLit1Chip_t*>(fsChip.get());

    std::string msg;
    mask32_t first_mask;
    adlit1_chip_ptr->applySliceProduct(mask32_t(0x42), 0);

    EXPECT_TRUE(adlit1_chip_ptr->isValid(FSmode::PRODUCT, msg)) << msg;

    EXPECT_EQ(adlit1_chip_ptr->getNumEnabledSlices(), 36);
    EXPECT_EQ(static_cast<const FBP_t*>(adlit1_chip_ptr->returnModuleList(module_type_id::fbp)[0])->getL2Mask(), 0x42);
    EXPECT_EQ(static_cast<const FBP_t*>(adlit1_chip_ptr->returnModuleList(module_type_id::fbp)[1])->getL2Mask(), 0x24);
}

TEST_F(FSChipComponent, Test_canDo3SliceProduct) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::AD102);
    ADLit1Chip_t* adlit1_chip_ptr = static_cast<ADLit1Chip_t*>(fsChip.get());

    std::string msg;
    mask32_t first_mask;
    EXPECT_TRUE(adlit1_chip_ptr->canDoSliceProduct(3, msg, first_mask)) << msg;
}

TEST_F(FSChipComponent, Test_DDRFBP_funcDownBin) {
    DDRFBP_t ddrfbp;
    ddrfbp.setup(getSettingsForFBPTest());

    ddrfbp.setEnabled(false);
    std::string msg;
    EXPECT_FALSE(ddrfbp.isValid(FSmode::FUNC_VALID, msg));

    ddrfbp.funcDownBin();

    EXPECT_TRUE(ddrfbp.isValid(FSmode::FUNC_VALID, msg)) << msg;
    EXPECT_FALSE(ddrfbp.HalfFBPAs[0]->getEnabled());
}

TEST_F(FSChipComponent, Test_DDRFBP_getSliceStartMask2) {
    DDRFBP_t ddrfbp;
    ddrfbp.setup(getSettingsForFBPTest());
    EXPECT_EQ(ddrfbp.getSliceStartMask(2), mask32_t(0x3c));
}

TEST_F(FSChipComponent, Test_DDRFBP_nextL2ProdMask1) {
    DDRFBP_t ddrfbp;
    ddrfbp.setup(getSettingsForFBPTest());

    std::string msg;
    mask32_t mask1(0x81);
    mask32_t mask2;
    ddrfbp.nextL2ProdMask(msg, mask1, mask2);

    EXPECT_EQ(mask2, mask32_t(0x42));
}

TEST_F(FSChipComponent, Test_DDRFBP_nextL2ProdMask2) {
    DDRFBP_t ddrfbp;
    ddrfbp.setup(getSettingsForFBPTest());

    std::string msg;
    mask32_t mask1(0xc3);
    mask32_t mask2;
    ddrfbp.nextL2ProdMask(msg, mask1, mask2);

    EXPECT_EQ(mask2, mask32_t(0xc3));
}

TEST_F(FSChipComponent, Test_DDRFBP_nextL2ProdMask3) {
    DDRFBP_t ddrfbp;
    ddrfbp.setup(getSettingsForFBPTest());

    std::string msg;
    mask32_t mask1(0x3c);
    mask32_t mask2;
    EXPECT_TRUE(ddrfbp.nextL2ProdMask(msg, mask1, mask2));

    EXPECT_EQ(mask2, mask32_t(0x3c));
}

TEST_F(FSChipComponent, Test_containerCreate) {
    FSChipContainer_t container;
    container.createModule<AD102_t>();
    
    EXPECT_EQ(container->getTypeEnum(), module_type_id::chip);
}

TEST_F(FSChipComponent, Test_createChipContainer) {
    FSChipContainer_t container;
    createChipContainer(Chip::AD102, container);
    
    EXPECT_EQ(container->getTypeEnum(), module_type_id::chip);

    FSChipContainer_t container2(Chip::AD102);
}

#ifdef USEBASEVECTORVIEW
TEST_F(FSChipComponent, Test_BaseVectorView) {

    PrimaryModuleList_t<L2SliceHandle_t, 8> slice_list;

    slice_list.resize(7);
    for (auto& L2Slice : slice_list){
        makeModule(L2Slice, L2Slice_t());
    }
    slice_list[3]->setEnabled(false);
    const PrimaryModuleList_t<L2SliceHandle_t, 8>& const_ref = slice_list;
    ConstFSModuleVectorView adapter(const_ref);

    EXPECT_EQ(adapter.size(), 7);

    EXPECT_FALSE(adapter[3]->getEnabled());
    EXPECT_TRUE(adapter[2]->getEnabled());

    std::vector<const FSModule_t*> module_list;
    for (const FSModule_t* ptr : adapter) {
        module_list.push_back(ptr);
    }

    EXPECT_FALSE(module_list.at(3)->getEnabled());
    EXPECT_TRUE(module_list.at(2)->getEnabled());

    FSModuleVectorView nonconst = adapter.removeConst();
    nonconst[6]->setEnabled(false);
    EXPECT_FALSE(module_list.at(6)->getEnabled());
}
#endif

TEST_F(FSChipComponent, Test_getLogicalTPCCounts_1) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    GpcMasks gpcSettings = { 0 };
    init_array(gpcSettings.tpcPerGpcMasks, { 0x1f0, 0x9, 0x0, 0x5, 0x0, 0x0, 0x1, 0x0 });
    fsChip->applyGPCMasks(gpcSettings);
    EXPECT_EQ(fsChip->getLogicalTPCCounts(), (std::vector<uint32_t>{4, 7, 7, 8, 9, 9, 9, 9}));
}

TEST_F(FSChipComponent, Test_getLogicalTPCCounts_2) {
    std::unique_ptr<FSChip_t> fsChip = createChip(fslib::Chip::GH100);
    GpcMasks gpcSettings = { 0 };
    init_array(gpcSettings.tpcPerGpcMasks, { 0x123, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef, 0x1ff });
    fsChip->applyGPCMasks(gpcSettings);
    EXPECT_EQ(fsChip->getLogicalTPCCounts(), (std::vector<uint32_t>{0, 2, 4, 4, 4, 5, 6, 6}));
}

}