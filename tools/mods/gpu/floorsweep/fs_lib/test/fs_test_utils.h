#pragma once

#include <gtest/gtest.h>

#include <iostream>
#include <fstream>
#include <string>
#include <iterator>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cassert>
#include <random>
#include <vector>

#include "fs_lib.h"
#include "fs_chip_core.h"

namespace fslib {
class FSGenericTest : public ::testing::Test {
    protected:
        void SetUp() override {
            msg = "";
        }

        bool is_valid = false;
        std::string msg;
        fslib::FbpMasks fbConfig0 = {0};
        fslib::GpcMasks gpcConfig0 = {0};

};


bool hasProtectedCompute(fslib::GpcMasks &masks);
uint32_t nBits(uint32_t n);
uint32_t maxTPCEnableCount(const GpcMasks& masks, uint32_t num_nodes, uint32_t num_units);
bool getGPCFloorswept(fslib::GpcMasks &masks, uint32_t index);
uint32_t numGPCsEnabled(fslib::GpcMasks &masks, const fslib::chipPOR_settings_t &settings);
uint32_t numGenerilwnitEnabled(uint32_t mask, uint32_t num_units);
uint32_t minTPCEnableCount(const GpcMasks& masks, uint32_t num_nodes, uint32_t num_units);
void floorsweepRandomGPCs(fslib::GpcMasks &masks, const fslib::chipPOR_settings_t &settings, uint32_t num_gpcs_to_fs);
void floorsweepRandomPESs(fslib::GpcMasks &masks, const fslib::chipPOR_settings_t &settings, uint32_t num_pes_to_fs, bool same_gpc);
void floorsweepRandomTPCs(fslib::GpcMasks &masks, const fslib::chipPOR_settings_t &settings, uint32_t num_tpcs_to_fs, bool same_gpc);
bool getFBPFloorswept(fslib::FbpMasks &masks, uint32_t index);
uint32_t numFBPsEnabled(fslib::FbpMasks &masks, const fslib::chipPOR_settings_t &settings);
void floorsweepRandomFBPs(fslib::FbpMasks &masks, const fslib::chipPOR_settings_t &settings, uint32_t num_fbps_to_fs);
std::vector<uint32_t> selectNRandom(uint32_t max_idx, uint32_t num_to_pick);

std::ostream& operator<<(std::ostream& os, const FbpMasks& masks);
std::ostream& operator<<(std::ostream& os, const GpcMasks& masks);


template<class array_type>
void init_array(array_type& array, const std::vector<uint32_t>& input){
    for (uint32_t i = 0; i < input.size(); i++){
        array[i] = input[i];
    }
}

template <class array_type>
void disableNRandom(array_type& masks, uint32_t num_to_disable, uint32_t node_count, uint32_t unit_count) {
    for (uint32_t flat_idx : selectNRandom(node_count * unit_count, num_to_disable)) {
        uint32_t node_idx = flat_idx / unit_count;
        uint32_t unit_idx = flat_idx % unit_count;
        //std::cout << flat_idx << " " << node_idx << " " << unit_idx << std::endl;
        masks[node_idx] |= 1 << unit_idx;
    }
}

}