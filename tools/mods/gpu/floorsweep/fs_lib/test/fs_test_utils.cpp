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
#include <numeric>

#include "fs_lib.h"
#include "fs_chip_core.h"
#include "fs_test_utils.h"

namespace fslib {


/**
 * @brief Checks chip_type to see if we have TPCs/GPCs that must be enabled.  
 * Enables those compute units in must_enable, and disables those compute units in lwrrent_masks 
 * so randomized functions don't touch these units.
 * 
 * @param masks The current set of enabled compute units
 * @param must_enable Empty set of GpcMasks. AND final masks after randomized FS with binary ilwerse of must_enable.
 * @param settings configuration of the current chip
 * @return true This chip has compute units that must stay enabled.  Make sure to re-enable the must_enable units
 * @return false This chip does not have protected compute units.  It is safe to ignore must_enable.
 */
bool hasProtectedCompute(fslib::GpcMasks &lwrrent_masks, fslib::GpcMasks &must_enable, const fslib::chipPOR_settings_t &settings) {
    if (settings.name == "gh100") {
        // GH100 GPC0, GPC0CPC0, GPC0ROP, and GCP0PES must not have defects
        // For now, just ensure that no GFX TPC gets disabled randomly
        // GFX TPCs should be tested as a directed edge case.
        must_enable.gpcMask = 0x1;
        must_enable.pesPerGpcMasks[0] = 0x1;
        must_enable.ropPerGpcMasks[0] = 0x1;
        must_enable.cpcPerGpcMasks[0] = 0x1;
        must_enable.tpcPerGpcMasks[0] = 0x7;
        lwrrent_masks.gpcMask |= must_enable.gpcMask;
        lwrrent_masks.pesPerGpcMasks[0] |= must_enable.pesPerGpcMasks[0];
        lwrrent_masks.ropPerGpcMasks[0] |= must_enable.ropPerGpcMasks[0];
        lwrrent_masks.cpcPerGpcMasks[0] |= must_enable.cpcPerGpcMasks[0];
        lwrrent_masks.tpcPerGpcMasks[0] |= must_enable.tpcPerGpcMasks[0];
        return true;
    } else {
        return false;
    }
}


/**
 * @brief Generate a bitmask with N conselwtive bits set
 * 
 * @param n Number of bits to set (starting at LSB)
 * @return uint32_t Integer with least significant `n` bits set
 */
uint32_t nBits(uint32_t n) {
    int ret = 0;
    for (uint32_t i = 0; i < n; i++) {
        ret |= (1 << i);
    }
    return ret;
}


// ------------------------------------------------------------------------------------
// GPC functions
// ------------------------------------------------------------------------------------

/**
 * @brief Returns the floorswept status of a GPC in a GpcMasks struct
 * 
 * @param masks Struct containing compute unit floorsweeping status
 * @param index Physical index of GPC to query
 * @return true This GPC is floorswept
 * @return false This GPC is enabled
 */
bool getGPCFloorswept(fslib::GpcMasks &masks, uint32_t index) {
    return (masks.gpcMask >> index) & 0x1;
}

/**
 * @brief Returns the number of GPCs not floorswept
 * 
 * @param masks Struct containing compute unit floorsweeping status
 * @param settings Information about the chip (contains number of GPCs)
 * @return uint32_t The number of GPCs not floorswept
 */
uint32_t numGPCsEnabled(fslib::GpcMasks &masks, const fslib::chipPOR_settings_t &settings) {
    uint32_t num_enabled = 0;
    for (uint32_t i = 0; i < settings.num_gpcs; i++) {
        if (!getGPCFloorswept(masks, i)) {
            num_enabled++;
        }
    }
    return num_enabled;
}

/**
 * @brief Generic function to see how many units are enabled in a mask
 * 
 * @param masks Struct containing unit floorsweeping status
 * @param num_units Number of possible units in chip
 * @return uint32_t Number of enabled units
 */
uint32_t numGenerilwnitEnabled(uint32_t mask, uint32_t num_units) {
    uint32_t num_enabled = 0;
    for (uint32_t i = 0; i < num_units; i++) {
        if (!((mask >> i) & 0x1)) {
            num_enabled++;
        }
    }
    return num_enabled;    
}

uint32_t minTPCEnableCount(const GpcMasks& masks, uint32_t num_nodes, uint32_t num_units) {
    uint32_t max = 0;
    for (uint32_t i = 0; i < 32; i++) {
        uint32_t check_max = numGenerilwnitEnabled(masks.tpcPerGpcMasks[i], num_units);
        if (check_max > max) {
            max = check_max;
        }
    }
    return max;
}

/**
 * @brief Take a `GpcMasks` struct and floorsweep a number of GPCs randomly
 *
 * Floorsweeps `num_gpcs_to_fs` GPCs and their sub-units.  Does not floorsweep already floorswept units 
 * 
 * @param masks Struct containing compute unit floorsweeping status
 * @param settings Information about the chip 
 * @param num_gpcs_to_fs Number of GPCs to floorsweep
 */
void floorsweepRandomGPCs(fslib::GpcMasks &masks, const fslib::chipPOR_settings_t &settings, uint32_t num_gpcs_to_fs) {
    EXPECT_LE(num_gpcs_to_fs, settings.num_gpcs) << "Test ERROR: trying to floorsweep " << num_gpcs_to_fs
                                                 << " GPCs, chip has " << settings.num_gpcs << " GPCs total!";
    EXPECT_LE(num_gpcs_to_fs, numGPCsEnabled(masks, settings)) 
                                                << "Test ERROR: trying to floorsweep " << num_gpcs_to_fs
                                                << " GPCs, chip has " << numGPCsEnabled(masks, settings)
                                                << " GPCs still enabled!";

    fslib::GpcMasks protected_masks{};
    bool protected_compute = hasProtectedCompute(masks, protected_masks, settings);
    static std::default_random_engine generator;
    std::uniform_int_distribution<uint32_t> s_distribution(0, settings.num_gpcs - 1);
    for (uint32_t i = 0; i < num_gpcs_to_fs; i++) {
        uint32_t to_fs = s_distribution(generator);
        // Ensure GPC is not already floorswept
        while (getGPCFloorswept(masks, to_fs)) {
            to_fs = s_distribution(generator);
        }
        // Floorsweep GPC and all sub-units
        masks.gpcMask |= 0x1 << to_fs;
        masks.pesPerGpcMasks[to_fs] = nBits(settings.num_pes_per_gpc);
        masks.tpcPerGpcMasks[to_fs] = nBits(settings.num_tpc_per_gpc);
        masks.ropPerGpcMasks[to_fs] = nBits(settings.num_rop_per_gpc);
        // This should be safe to do even for non-CPC products. We could make a function that does this separately
        masks.cpcPerGpcMasks[to_fs] = nBits(settings.num_pes_per_gpc);
    }

    // Re-enable any protected compute units
    if (protected_compute) {
        masks.gpcMask &= ~protected_masks.gpcMask;
        for (uint32_t idx = 0; idx < settings.num_gpcs; idx++) {
            masks.pesPerGpcMasks[idx] &= ~protected_masks.pesPerGpcMasks[idx];
            masks.ropPerGpcMasks[idx] &= ~protected_masks.ropPerGpcMasks[idx];
            masks.cpcPerGpcMasks[idx] &= ~protected_masks.cpcPerGpcMasks[idx];
            masks.tpcPerGpcMasks[idx] &= ~protected_masks.tpcPerGpcMasks[idx];
        }
    }
}

/**
 * @brief Take a `GpcMasks` struct and floorsweep a number of PESs randomly
 * 
 * @param masks Struct containing compute unit floorsweeping status
 * @param settings Information about the chip 
 * @param num_pes_to_fs The number of PESs to floorsweep
 * @param same_gpc Floorsweep PESs from the same GPC or from random GPCs
 */
void floorsweepRandomPESs(fslib::GpcMasks &masks, const fslib::chipPOR_settings_t &settings, uint32_t num_pes_to_fs, bool same_gpc) {
    static std::default_random_engine s_gpc_generator;
    std::uniform_int_distribution<uint32_t> gpc_distribution(0, settings.num_gpcs - 1);
    bool gpc_found = false;

    fslib::GpcMasks protected_masks{};
    bool protected_compute = hasProtectedCompute(masks, protected_masks, settings);

    if (same_gpc) {
        EXPECT_LE(num_pes_to_fs, settings.num_pes_per_gpc) << "Test ERROR: trying to floorsweep " << num_pes_to_fs
                                                           << " PESs in one GPC, chip has " 
                                                           << settings.num_pes_per_gpc << " PESs per GPC total!";
        uint32_t gpcs_checked = 0x0;
        while (gpcs_checked !=nBits(settings.num_gpcs)) {
            uint32_t gpc_to_fs = gpc_distribution(s_gpc_generator);
            gpcs_checked |= 0x1 << gpc_to_fs;
            if (!getGPCFloorswept(masks, gpc_to_fs) && 
                    num_pes_to_fs <= numGenerilwnitEnabled(masks.pesPerGpcMasks[gpc_to_fs], settings.num_pes_per_gpc)) {
                if (num_pes_to_fs == numGenerilwnitEnabled(masks.pesPerGpcMasks[gpc_to_fs], settings.num_pes_per_gpc)) {
                    // Disable GPC as well
                    masks.gpcMask |= 0x1 << gpc_to_fs;
                    masks.pesPerGpcMasks[gpc_to_fs] = nBits(settings.num_pes_per_gpc);
                    masks.tpcPerGpcMasks[gpc_to_fs] = nBits(settings.num_tpc_per_gpc);
                    masks.ropPerGpcMasks[gpc_to_fs] = nBits(settings.num_rop_per_gpc);
                    // This should be safe to do even for non-CPC products. We could make a function that does this separately
                    masks.cpcPerGpcMasks[gpc_to_fs] = nBits(settings.num_pes_per_gpc);
                } else {
                    static std::default_random_engine s_generator_pes;
                    std::uniform_int_distribution<uint32_t> distribution_pes(0, settings.num_pes_per_gpc - 1);
                    for (uint32_t i = 0; i < num_pes_to_fs; i++) {
                        uint32_t pes_to_fs = distribution_pes(s_generator_pes);
                        while((masks.pesPerGpcMasks[gpc_to_fs] >> pes_to_fs) & 0x1) {
                            pes_to_fs = distribution_pes(s_generator_pes);
                        }
                        masks.pesPerGpcMasks[gpc_to_fs] |= 1 << pes_to_fs;
                        // Kill CPC/TPCs
                        masks.cpcPerGpcMasks[gpc_to_fs] |= 1 << pes_to_fs;
                        fslib::PESMapping_t mappings(settings.num_pes_per_gpc, settings.num_tpc_per_gpc);
                        for (int j : mappings.getPEStoTPCMapping(pes_to_fs)) {
                            masks.tpcPerGpcMasks[gpc_to_fs] |= 1 << j;
                        }
                    }
                }
                gpc_found = true;
                break;

            }
        }
        EXPECT_TRUE(gpc_found) << "Test ERROR: no active GPCs had " << num_pes_to_fs << " PESs active!";
    } else {
        uint32_t num_pes_active = 0;
        for (uint32_t gpc = 0; gpc < settings.num_gpcs; gpc++) {
            num_pes_active += numGenerilwnitEnabled(masks.pesPerGpcMasks[gpc], settings.num_pes_per_gpc);
        }
        EXPECT_LE(num_pes_to_fs, num_pes_active) << "Test ERROR: trying to floorsweep " << num_pes_to_fs
                                                 << " PESs in a chip, chip has " 
                                                 << num_pes_active << " PESs active!";
        for (uint32_t i = 0; i < num_pes_to_fs; i++) {
            floorsweepRandomPESs(masks, settings, 1, true);
        }
    }

    // Re-enable any protected compute units
    if (protected_compute) {
        masks.gpcMask &= ~protected_masks.gpcMask;
        for (uint32_t idx = 0; idx < settings.num_gpcs; idx++) {
            masks.pesPerGpcMasks[idx] &= ~protected_masks.pesPerGpcMasks[idx];
            masks.ropPerGpcMasks[idx] &= ~protected_masks.ropPerGpcMasks[idx];
            masks.cpcPerGpcMasks[idx] &= ~protected_masks.cpcPerGpcMasks[idx];
            masks.tpcPerGpcMasks[idx] &= ~protected_masks.tpcPerGpcMasks[idx];
        }
    }
}


/**
 * @brief Take a `GpcMasks` struct and floorsweep a number of TPCs randomly
 * 
 * @param masks Struct containing compute unit floorsweeping status
 * @param settings Information about the chip 
 * @param num_tpcs_to_fs The number of TPCs to floorsweep
 * @param same_gpc Floorsweep TPCs from the same GPC or from random GPCs
 */
void floorsweepRandomTPCs(fslib::GpcMasks &masks, const fslib::chipPOR_settings_t &settings, uint32_t num_tpcs_to_fs, bool same_gpc) {
    static std::default_random_engine s_generator_gpc;
    std::uniform_int_distribution<uint32_t> distribution_gpc(0, settings.num_gpcs - 1);
    bool gpc_found = false;
    
    fslib::GpcMasks protected_masks{};
    bool protected_compute = hasProtectedCompute(masks, protected_masks, settings);

    if (same_gpc) {
        EXPECT_LE(num_tpcs_to_fs, settings.num_tpc_per_gpc) << "Test ERROR: trying to floorsweep " << num_tpcs_to_fs
                                                           << " TPCs in one GPC, chip has " 
                                                           << settings.num_tpc_per_gpc << " TPCs per GPC total!";
        uint32_t gpcs_checked = 0x0;
        while (gpcs_checked !=nBits(settings.num_gpcs)) {
            uint32_t gpc_to_fs = distribution_gpc(s_generator_gpc);
            gpcs_checked |= 0x1 << gpc_to_fs;
            if (!getGPCFloorswept(masks, gpc_to_fs) && 
                    num_tpcs_to_fs <= numGenerilwnitEnabled(masks.tpcPerGpcMasks[gpc_to_fs], settings.num_tpc_per_gpc)) {
                if (num_tpcs_to_fs == numGenerilwnitEnabled(masks.tpcPerGpcMasks[gpc_to_fs], settings.num_tpc_per_gpc)) {
                    // Disable GPC as well
                    masks.gpcMask |= 0x1 << gpc_to_fs;
                    masks.pesPerGpcMasks[gpc_to_fs] = nBits(settings.num_pes_per_gpc);
                    masks.tpcPerGpcMasks[gpc_to_fs] = nBits(settings.num_tpc_per_gpc);
                    masks.ropPerGpcMasks[gpc_to_fs] = nBits(settings.num_rop_per_gpc);
                    // This should be safe to do even for non-CPC chips. We could make a function that does this separately
                    masks.cpcPerGpcMasks[gpc_to_fs] = nBits(settings.num_pes_per_gpc);
                } else {
                    static std::default_random_engine s_generator_tpc;
                    std::uniform_int_distribution<uint32_t> distribution_tpc(0, settings.num_tpc_per_gpc - 1);
                    for (uint32_t i = 0; i < num_tpcs_to_fs; i++) {
                        uint32_t tpc_to_fs = distribution_tpc(s_generator_tpc);
                        while((masks.tpcPerGpcMasks[gpc_to_fs] >> tpc_to_fs) & 0x1) {
                            tpc_to_fs = distribution_tpc(s_generator_tpc);
                        }
                        masks.tpcPerGpcMasks[gpc_to_fs] |= 1 << tpc_to_fs;
                    
                        // Check if we need to floorsweep PES/CPC:
                        fslib::PESMapping_t mappings(settings.num_pes_per_gpc, settings.num_tpc_per_gpc);
                        int pes_id = mappings.getTPCtoPESMapping(tpc_to_fs);
                        bool all_tpc_fs = true;
                        for (int j : mappings.getPEStoTPCMapping(pes_id)) {
                            all_tpc_fs &= 1 & (masks.tpcPerGpcMasks[gpc_to_fs] >> j);
                        }
                        if (all_tpc_fs) {
                            masks.pesPerGpcMasks[gpc_to_fs] |= 1 << pes_id;
                            masks.cpcPerGpcMasks[gpc_to_fs] |= 1 << pes_id;
                        }
                    }
                }
                gpc_found = true;
                break;
            }
        }
        EXPECT_TRUE(gpc_found) << "Test ERROR: no active GPCs had " << num_tpcs_to_fs << " TPCs active!";
    } else {
        uint32_t num_tpcs_active = 0;
        for (uint32_t gpc = 0; gpc < settings.num_gpcs; gpc++) {
            num_tpcs_active += numGenerilwnitEnabled(masks.tpcPerGpcMasks[gpc], settings.num_tpc_per_gpc);
        }
        EXPECT_LE(num_tpcs_to_fs, num_tpcs_active) << "Test ERROR: trying to floorsweep " << num_tpcs_to_fs
                                                   << " TPCs in a chip, chip has " 
                                                   << num_tpcs_active << " TPCs active!";
        for (uint32_t i = 0; i < num_tpcs_to_fs; i++) {
            floorsweepRandomTPCs(masks, settings, 1, true);
        }
    }

    // Re-enable any protected compute units
    if (protected_compute) {
        masks.gpcMask &= ~protected_masks.gpcMask;
        for (uint32_t idx = 0; idx < settings.num_gpcs; idx++) {
            masks.pesPerGpcMasks[idx] &= ~protected_masks.pesPerGpcMasks[idx];
            masks.ropPerGpcMasks[idx] &= ~protected_masks.ropPerGpcMasks[idx];
            masks.cpcPerGpcMasks[idx] &= ~protected_masks.cpcPerGpcMasks[idx];
            masks.tpcPerGpcMasks[idx] &= ~protected_masks.tpcPerGpcMasks[idx];
        }
    }
}

std::vector<uint32_t> selectNRandom(uint32_t max_idx, uint32_t num_to_pick) {
    static std::default_random_engine generator;
    std::vector<uint32_t> indexes;
    indexes.resize(max_idx);
    std::iota(indexes.begin(), indexes.end(), 0);
    std::shuffle(indexes.begin(), indexes.end(), generator);
    indexes.resize(num_to_pick);
    return indexes;
}

// ------------------------------------------------------------------------------------
// FBP functions
// ------------------------------------------------------------------------------------

/**
 * @brief Returns the floorswept status of a FBP in a FbpMasks struct
 * 
 * @param masks Struct containing compute unit floorsweeping status
 * @param index Physical index of FBP to query
 * @return true This FBP is floorswept
 * @return false This FBP is enabled
 */
bool getFBPFloorswept(fslib::FbpMasks &masks, uint32_t index) {
    return (masks.fbpMask >> index) & 0x1;
}

/**
 * @brief Returns the number of FBPs not floorswept
 * 
 * @param masks Struct containing compute unit floorsweeping status
 * @param settings Information about the chip (contains number of FBPs)
 * @return uint32_t The number of FBPs not floorswept
 */
uint32_t numFBPsEnabled(fslib::FbpMasks &masks, const fslib::chipPOR_settings_t &settings) {
    uint32_t num_enabled = 0;
    for (uint32_t i = 0; i < settings.num_fbps; i++) {
        if (!getFBPFloorswept(masks, i)) {
            num_enabled++;
        }
    }
    return num_enabled;
}

/**
 * @brief Take a `FbpMasks` struct and floorsweep a number of FBPs randomly
 *
 * Floorsweeps `num_fbps_to_fs` FBPs and their sub-units.  Does not floorsweep already floorswept units 
 * 
 * @param masks Struct containing compute unit floorsweeping status
 * @param settings Information about the chip 
 * @param num_fbps_to_fs Number of FBPs to floorsweep
 */
void floorsweepRandomFBPs(fslib::FbpMasks &masks, const fslib::chipPOR_settings_t &settings, uint32_t num_fbps_to_fs) {
    EXPECT_LE(num_fbps_to_fs, settings.num_fbps) << "Test ERROR: trying to floorsweep " << num_fbps_to_fs
                                                 << " FBPs, chip has " << settings.num_fbps << " FBPs total!";
    EXPECT_LE(num_fbps_to_fs, numFBPsEnabled(masks, settings)) 
                                                << "Test ERROR: trying to floorsweep " << num_fbps_to_fs
                                                << " FBPs, chip has " << numFBPsEnabled(masks, settings) 
                                                << " FBPs still enabled!";
    static std::default_random_engine s_generator;
    std::uniform_int_distribution<uint32_t> distribution(0, settings.num_fbps - 1);
    for (uint32_t i = 0; i < num_fbps_to_fs; i++) {
        uint32_t to_fs = distribution(s_generator);
        // Ensure FBP is not already floorswept
        while (getFBPFloorswept(masks, to_fs)) {
            to_fs = distribution(s_generator);
        }
        // Floorsweep FBP and all sub-units
        masks.fbpMask |= 0x1 << to_fs;
        masks.fbioPerFbpMasks[to_fs] = nBits(settings.num_fbpas / settings.num_fbps);
        masks.fbpaPerFbpMasks[to_fs] = nBits(settings.num_fbpas / settings.num_fbps);
        masks.halfFbpaPerFbpMasks[to_fs] = nBits(2 * settings.num_fbpas / settings.num_fbps);
        masks.ltcPerFbpMasks[to_fs] = nBits(settings.num_ltcs / settings.num_fbps);
        // Would be nice to have this not hard-coded
        masks.l2SlicePerFbpMasks[to_fs] = nBits(4 * settings.num_ltcs / settings.num_fbps);
    }
}


}