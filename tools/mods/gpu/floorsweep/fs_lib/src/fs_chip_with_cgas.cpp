#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include "fs_chip_with_cgas.h"
#include <numeric>
#include <algorithm>
#include <assert.h>

namespace fslib {

/**
 * @brief Check if the skyline can fit inside this config
 *
 * @param skyline
 * @param msg
 * @return true
 * @return false
 */
bool ChipWithCGAs_t::skylineFits(const std::vector<uint32_t>& skyline, std::string& msg) const {
    //the skyline is guaranteed to be sorted

    // sort the gpcs by tpc count
    const_flat_module_list_t gpc_list = viewToList(returnModuleList(module_type_id::gpc));
    const static std::vector<module_type_id> tpcs_only = { module_type_id::tpc };
    sortListByEnableCountAscending(gpc_list, tpcs_only);

    if (skyline.size() > GPCs.size()) {
        std::stringstream ss;
        ss << "Not enough enabled GPCs to fit the skyline! ";
        ss << "The skyline requires " << skyline.size() << " GPCs, but only ";
        ss << getEnableCount(module_type_id::gpc) << " GPCs are enabled!";
        msg = ss.str();
        return false;
    }

    // start_idx can be unsigned because we can't get here if skyline.size() > GPCs.size()
    uint32_t start_idx = static_cast<int32_t>(GPCs.size()) - static_cast<int32_t>(skyline.size());

    for (uint32_t i = start_idx; i < gpc_list.size(); i++) {
        const FSModule_t* gpc = gpc_list[i];

        uint32_t skyline_idx = i - start_idx;

        // if the skyline requires more tpcs than there are available, then it doesn't match
        if (skyline[skyline_idx] > gpc->getEnableCount(module_type_id::tpc)) {
            std::stringstream ss;
            ss << "The skyline can't fit! ";
            ss << "Logical GPC" << i << " (physical " << std::dec << gpc->instance_number << ") would need at least " << skyline[skyline_idx];
            ss << " enabled TPCs! ";
            ss << "This chip's skyline is: ";

            for (const FSModule_t* print_gpc : gpc_list) {
                ss << print_gpc->getEnableCount(module_type_id::tpc) << "/";
            }
            ss.seekp(-1, std::ios_base::end);
            ss << ", skyline was: ";
            for (uint32_t tpc_count : skyline) {
                ss << tpc_count << "/";
            }
            ss.seekp(-1, std::ios_base::end);
            ss << "!";


            msg = ss.str();
            return false;
        }
    }

    return true;
}

/**
 * @brief Return a list of flat tpc indexes which are the best candidates for floorsweeping with respect to skyline
 * Not every tpc is included. Only one per GPC. This is to improve the runtime.
 *
 * @param potentialSKU
 * @return module_index_list_t
 */
module_index_list_t ChipWithCGAs_t::getPreferredTPCsToDisableForSkyline(const FUSESKUConfig::SkuConfig& potentialSKU) const {

    const auto& fsSettings_it = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc));
    const auto& skyline = fsSettings_it->second.skyline;


    //Get a list of GPCs to floorsweep a TPC out of
    module_index_list_t gpcs_to_downgrade = getPreferredGPCsByTPCSkylineMarginFilter(skyline);
    //module_index_list_t gpcs_to_downgrade = sortGPCsByTPCSkylineMargin(skyline);

    module_index_list_t flat_tpcs_to_downgrade;

    // pick all the available TPCs from each GPC in order
    uint32_t tpc_per_gpc_count = static_cast<uint32_t>(GPCs[0]->TPCs.size());
    for (uint32_t gpc_idx : gpcs_to_downgrade) {
        for (uint32_t tpc_idx : GPCs[gpc_idx]->getPreferredDisableList(module_type_id::tpc, potentialSKU)) {
            uint32_t flat_tpc_idx = gpc_idx * tpc_per_gpc_count + tpc_idx;
            flat_tpcs_to_downgrade.push_back(flat_tpc_idx);
        }
    }

    return flat_tpcs_to_downgrade;
}

/**
 * @brief Get a list of GPCs in preferred order to try removing TPCs from. Floorswept GPCs will not be included, nor will GPCs
 * that only have barely enough TPCs to fit the skyline.
 *
 * @param skyline
 * @return module_index_list_t
 */
module_index_list_t ChipWithCGAs_t::getPreferredGPCsByTPCSkylineMarginFilter(const std::vector<uint32_t>& skyline) const {
    // For each GPC, clone the chip and try reducing the TPC count, then check if the skyline can fit.
    // If the skyline can't fit, then don't offer that GPC IDX as an option to floorsweep TPCs from.
    // This is necesary to check for GPCs in which the skyline TPC margin is zero. It may still be possible to floorsweep
    // a TPC from one of these GPCs if there are other GPCs that can replace them in the skyline. The easiest way to implement
    // the algorithm was to clone the chip, and try disabling a TPC in that GPC, and then check if the skyline still fits. If so,
    // then that GPC is elligible for downbinning.
    // Also don't offer disabled GPCs

    module_index_list_t gpc_idxs = sortGPCsByTPCSkylineMargin(skyline);

    module_index_list_t allowed_gpcs;
    for (uint32_t gpc_idx : gpc_idxs) {
        if (GPCs[gpc_idx]->getEnabled()) {
            std::unique_ptr<FSChip_t> clone_chip = clone();
            const ChipWithCGAs_t* skyline_chip_ptr = dynamic_cast<const ChipWithCGAs_t*>(clone_chip.get());
            assert(skyline_chip_ptr != nullptr);

            clone_chip->GPCs[gpc_idx]->killAnyTPC();

            std::string msg;
            if (skyline_chip_ptr->skylineFits(skyline, msg)) {
                allowed_gpcs.push_back(gpc_idx);
            }
        }
    }
    return allowed_gpcs;
}

/**
 * @brief Return a list of gpc indexes, which have been sorted based on the number of tpcs available for
 * floorsweeping but still within the skyline restriction.
 *
 * @param skyline
 * @return module_index_list_t
 */
module_index_list_t ChipWithCGAs_t::sortGPCsByTPCSkylineMargin(const std::vector<uint32_t>& skyline) const {

    // Algorithm strategy:
    // sort the GPCs in order of ascending TPC counts
    // Then align the GPCs to the skyline
    // Then pick the GPC with the most TPCs that exceeds the skyline amount and pick a TPC in there
    // If there is a Tie, pick a GPC that would result in less uGPU imbalance
    // If there is still a tie, pick the GPC with the most enabled TPCs
    // If there is still a tie, pick the GPC with the lower physical ID

    const_flat_module_list_t gpc_list = viewToList(returnModuleList(module_type_id::gpc));
    const static std::vector<module_type_id> tpcs_only = { module_type_id::tpc };
    sortListByEnableCountAscending(gpc_list, tpcs_only);

    auto sortComparison = [&gpc_list, &skyline, this](int32_t left, int32_t right) -> bool {

        int32_t skip_amt = static_cast<int32_t>(GPCs.size()) - static_cast<int32_t>(skyline.size());

        if (skip_amt < 0) {
            throw FSLibException("This skyline can't fit!");
        }

        if (left < skip_amt || right < skip_amt) {
            return left > right;
        }

        uint32_t left_enabled_tpcs = gpc_list[left]->getEnableCount(module_type_id::tpc);
        uint32_t right_enabled_tpcs = gpc_list[right]->getEnableCount(module_type_id::tpc);

        int32_t left_margin = left_enabled_tpcs - skyline[left - skip_amt];
        int32_t right_margin = right_enabled_tpcs - skyline[right - skip_amt];

        // choose based on uGPU imbalance
        if (left_margin == right_margin) {
            return getTiePreferenceGPCFS(gpc_list, left, right);
        }

        // return true if the tpc margin is greater on the left side
        return left_margin > right_margin;
    };

    std::vector<uint32_t> tpc_counts_logical;
    for (const auto gpc : gpc_list) {
        tpc_counts_logical.push_back(gpc->getEnableCount(module_type_id::tpc));
    }

    // logical GPC IDs
    module_index_list_t logical_gpc_idx_list;
    logical_gpc_idx_list.resize(gpc_list.size());
    std::iota(logical_gpc_idx_list.begin(), logical_gpc_idx_list.end(), 0);
    std::stable_sort(logical_gpc_idx_list.begin(), logical_gpc_idx_list.end(), sortComparison);

    module_index_list_t physical_gpc_idx_list;
    physical_gpc_idx_list.reserve(gpc_list.size());
    for (uint32_t logical_gpc : logical_gpc_idx_list) {
        physical_gpc_idx_list.push_back(gpc_list[logical_gpc]->instance_number);
    }
    return physical_gpc_idx_list;
}


/**
 * @brief Wrapper for calling the spare tpc analysis program
 *
 * @param potentialSKU
 * @param repairMaxCount
 * @return std::vector<uint32_t>
 */
std::vector<uint32_t> ChipWithCGAs_t::getOptimalTPCDownBinForSparesWrap(const FUSESKUConfig::SkuConfig& potentialSKU, const std::vector<uint32_t>& skyline, const std::vector<uint32_t>& aliveTPCs, uint32_t repairMaxCount, std::string& msg) const {
    const auto& tpc_settings = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc))->second;

    int singletonsRequired = tpc_settings.maxEnableCount - std::accumulate(skyline.begin(), skyline.end(), 0);

    if (singletonsRequired < 0) {
        std::stringstream ss;
        ss << "error in spares callwlation: skyline is too large for num total tpcs";
        ss << ". There are " << tpc_settings.maxEnableCount << " total tpcs, and the skyline is ";
        for (uint32_t vgpc : skyline) {
            ss << vgpc << "/";
        }
        ss.seekp(-1, std::ios_base::end);
        msg = ss.str();
        return {};
    }

    std::vector<uint32_t> best_logical_tpc_config_for_repair;
    int tpcs_per_gpc = static_cast<int>(GPCs[0]->TPCs.size());

    int min_tpc_per_gpc = (tpc_settings.minEnablePerGroup != UNKNOWN_ENABLE_COUNT) ? tpc_settings.minEnablePerGroup : 0;

    int result = getOptimalTPCDownBinForSpares(skyline, aliveTPCs, singletonsRequired, repairMaxCount, tpcs_per_gpc, min_tpc_per_gpc, best_logical_tpc_config_for_repair, msg);
    if (result < 0) {
        return {};
    }
    return best_logical_tpc_config_for_repair;
}

bool ChipWithCGAs_t::screenSkylineSparesImpl(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {
    const auto& tpc_settings = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc))->second;

    std::vector<uint32_t> skyline = tpc_settings.skyline;

    std::vector<uint32_t> aliveTPCs = getLogicalTPCCounts();

    // Put zeros in front of the skyline if needed
    while (aliveTPCs.size() > skyline.size()) {
        skyline.insert(skyline.begin(), 0);
    }

    std::string spares_msg;
    if (getOptimalTPCDownBinForSparesWrap(potentialSKU, skyline, aliveTPCs, 0, spares_msg).size() == 0) {
        std::stringstream ss;
        ss << "failed to fit skyline: " << spares_msg;
        msg = ss.str();
        return false;
    }
    return true;
}

std::vector<uint32_t> ChipWithCGAs_t::getOptimalTPCDownBinForSparesWrap(const FUSESKUConfig::SkuConfig& potentialSKU, uint32_t repairMaxCount, std::string& msg) const {
    const auto& tpc_settings = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc))->second;

    std::vector<uint32_t> skyline = tpc_settings.skyline;

    // Put zeros in front of the skyline if needed
    if (skyline.size() < getMaxAllowed(module_type_id::gpc, potentialSKU)) {
        skyline.resize(getMaxAllowed(module_type_id::gpc, potentialSKU), 0);
    }
    std::sort(skyline.begin(), skyline.end());

    return getOptimalTPCDownBinForSparesWrap(potentialSKU, skyline, getLogicalTPCCounts(), repairMaxCount, msg);
}

// remove gpc0 from the equation by reducing the total tpc count by the number of tpcs gpc0 can have
// and reduce the gpc count of the sku by 1
static void removeGPCFromSkuEquation(FUSESKUConfig::SkuConfig& sku, uint32_t tpcs_in_gpc_to_remove, uint32_t max_gpcs_in_chip) {

    FUSESKUConfig::FsConfig& tpc_settings = sku.fsSettings.find(idToStr(module_type_id::tpc))->second;
    tpc_settings.minEnableCount -= tpcs_in_gpc_to_remove;
    tpc_settings.maxEnableCount -= tpcs_in_gpc_to_remove;

    auto gpc_settings_it = sku.fsSettings.find(idToStr(module_type_id::gpc));
    // If GPC isn't specified in the sku, then set it to have a min of 1, and max of N-1 where N is the max possible in the chip
    if (gpc_settings_it == sku.fsSettings.end()) {
        sku.fsSettings[idToStr(module_type_id::gpc)] = { 1u, static_cast<uint32_t>(max_gpcs_in_chip - 1) };
    }
    else {
        gpc_settings_it->second.minEnableCount -= 1;
        gpc_settings_it->second.maxEnableCount -= 1;

        // If it's 0, set it to 1
        if (gpc_settings_it->second.minEnableCount == 0) {
            gpc_settings_it->second.minEnableCount = 1;
        }
    }
}

// Remove a gpc from a skyline and logical GPC list
static void removeGPCFromAliveTPCsAndSkyline(uint32_t tpcs_in_gpc_to_remove, uint32_t vgpc_size, std::vector<uint32_t>& aliveTPCs, std::vector<uint32_t>& skyline) {

    // remove the vgpc from the skyline
    if (vgpc_size != 0) { // Don't try to remove 0s, since there won't be any there yet
        skyline.erase(std::find(skyline.begin(), skyline.end(), vgpc_size));
    }

    // Remove GPC0 from the logical config
    aliveTPCs.erase(std::find(aliveTPCs.begin(), aliveTPCs.end(), tpcs_in_gpc_to_remove));

    // Remove 0s from the aliveTPCs list since they are redundant here
    while (aliveTPCs[0] == 0) {
        // take advantage of the guarantee that aliveTPCs is already sorted
        aliveTPCs.erase(aliveTPCs.begin());
    }

    // Insert 0s at the beginning of the skyline to match the length of aliveTPCs
    while (aliveTPCs.size() > skyline.size()) {
        skyline.insert(skyline.begin(), 0);
    }
}

/**
 * @brief A screening function to rule out most bad configs more quickly to avoid having to do the big relwrsive search
 * This is only helpful if there is a zero in the skyline. If there is not a zero in the skyline, this check is not useful
 * The idea is that the main relwrsive search has a very hard time with configs that require floorsweeping gpc0. It will try
 * every possibility before returning false.
 * @param potentialSKU
 * @param msg
 * @return true if the config passes the screen. It doesn't mean the config is possible, just that it wasn't ruled out
 * @return false if this config cannot fit the sku. No point doing further checks.
 */
bool ChipWithCGAs_t::canFitSkylineGPC0Screen(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {
    // This is only helpful if there is a zero in the skyline. If there is not a zero in the skyline, this check is not useful
    // This first check is to see if the skyline can fit while gpc0 is all singletons
    // To do this, we can leverage the getOptimalTPCDownBinForSpares, since it runs much more quickly.


    // This is not setup to handle cases where GPC FS is required, but it can handle cases where GPC FS is possible
    uint32_t num_enabled_gpcs = getNumEnabled(GPCs);
    auto gpc_settings_it = potentialSKU.fsSettings.find(idToStr(module_type_id::gpc));
    if (gpc_settings_it != potentialSKU.fsSettings.end()) {
        // If the maxEnableCount for gpc is lower than num enabled, then we can't handle this, so return true
        if (gpc_settings_it->second.maxEnableCount < num_enabled_gpcs) {
            return true;
        }
    }

    const FUSESKUConfig::FsConfig& tpc_settings = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc))->second;
    uint32_t min_tpcs_per_gpc = tpc_settings.getMinGroupCount();
    uint32_t tpcs_in_gpc0 = GPCs[0]->getEnableCount(module_type_id::tpc);

    // Only check this step if there is a possibility of a gpc with only singletons
    // This means that the maximum allowed GPC count has to be greater than the number of GPCs Specified in the skyline
    if (tpc_settings.skyline.size() < getMaxAllowed(module_type_id::gpc, potentialSKU)) {

        // Try this using every possible tpc count that gpc0 is allowed to have
        for (uint32_t tpcs_gpc0_can_have = min_tpcs_per_gpc; tpcs_gpc0_can_have <= tpcs_in_gpc0; tpcs_gpc0_can_have++) {

            // The strategy is to try removing gpc0 from the equation, and see if the skyline still fits.
            // The sku needs to have its total tpc count reduced by the amount that gpc0 contributes
            // We need to try every possible singleton count that gpc0 could have
            // The skyline needs to have its 0 removed

            FUSESKUConfig::SkuConfig skuCopy = potentialSKU;
            removeGPCFromSkuEquation(skuCopy, tpcs_gpc0_can_have, static_cast<uint32_t>(GPCs.size()));

            // remove the vgpc from the skyline and remove GPC0 from the logical config
            auto& skyline_ref = skuCopy.fsSettings.find(idToStr(module_type_id::tpc))->second.skyline;
            std::vector<uint32_t> aliveTPCs = getLogicalTPCCounts();
            removeGPCFromAliveTPCsAndSkyline(tpcs_in_gpc0, 0, aliveTPCs, skyline_ref);

            // If we can fit with this configuration, then that means that it is possible for gpc0 to have all singletons
            // (and thus does not need to be floorswept for the config to fit the sku)
            // Stop and return true
            const auto& skyline = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc))->second.skyline;
            std::string spares_msg;
            if (getOptimalTPCDownBinForSparesWrap(skuCopy, skyline, aliveTPCs, 0, spares_msg).size() != 0) {
                return true;
            }
        }
    }

    // Next try removing non-zero gpcs from the skyline (picking only those which gpc0 could possibly satisfy)
    // This check is to see if gpc0 can be part of the skyline

    // Try this for every possible tpc count that gpc0 can have
    // (This means every vgpc in the skyline that is less than the tpc count in gpc0
    for (uint32_t vgpc_size : tpc_settings.skyline) {
        for (uint32_t tpcs_gpc0_can_have = vgpc_size; tpcs_gpc0_can_have <= tpcs_in_gpc0; tpcs_gpc0_can_have++) {
            FUSESKUConfig::SkuConfig skuCopy = potentialSKU;
            removeGPCFromSkuEquation(skuCopy, tpcs_gpc0_can_have, static_cast<uint32_t>(GPCs.size()));

            // remove the vgpc from the skyline and remove GPC0 from the logical config
            auto& skyline_ref = skuCopy.fsSettings.find(idToStr(module_type_id::tpc))->second.skyline;
            std::vector<uint32_t> aliveTPCs = getLogicalTPCCounts();
            removeGPCFromAliveTPCsAndSkyline(tpcs_in_gpc0, vgpc_size, aliveTPCs, skyline_ref);

            // If we can fit with this configuration, then this means that gpc0 can be part of the skyline,
            // and therefore does need to be floorswept in order to fit the sku
            std::string spares_msg;
            if (getOptimalTPCDownBinForSparesWrap(skuCopy, skyline_ref, aliveTPCs, 0, spares_msg).size() != 0) {
                return true;
            }
        }
    }

    if (!screenSkylineSparesImpl(potentialSKU, msg)) {
        return false;
    }

    // If nothing can work, then return false
    msg = "This config cannot work while GPC zero is not floorswept, but GPC zero cannot be floorswept!";
    return false;
}

/**
 * @brief Pick num_spares enabled TPCs and disable them as spares
 *
 * @param potentialSKU
 * @param num_spares
 * @param spare_tpc_masks
 */
void ChipWithCGAs_t::pickSpareTPCs(const FUSESKUConfig::SkuConfig& potentialSKU, uint32_t num_spares, fslib::GpcMasks& spare_tpc_masks) {
    for (uint32_t i = 0; i < num_spares; i++) {
        int32_t spare_tpc_idx = -1;
        int32_t spare_gpc_idx = -1;
        bool success = pickSpareTPC(potentialSKU, spare_gpc_idx, spare_tpc_idx);
        if (success) {
            GPCs[spare_gpc_idx]->TPCs[spare_tpc_idx]->setEnabled(false);
            spare_tpc_masks.tpcPerGpcMasks[spare_gpc_idx] |= (1 << spare_tpc_idx);
        }
    }
}

/**
 * @brief Identify an enabled TPC to turn into a spare
 *
 * @param potentialSKU
 * @param spare_gpc_idx
 * @param spare_tpc_idx
 * @return true
 * @return false
 */
bool ChipWithCGAs_t::pickSpareTPC(const FUSESKUConfig::SkuConfig& potentialSKU, int32_t& spare_gpc_idx, int32_t& spare_tpc_idx) const {

    // a TPC in the mustEnableList or mustDisableList cannot be a spare
    // a non-singleton TPC cannot be a spare.
    // otherwise, all TPCs are equal in terms of repairability

    // list of GPCs with singletons
    std::vector<uint32_t> alive_tpcs = getLogicalTPCCounts();

    const auto& tpc_settings = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc))->second;
    std::vector<uint32_t> skyline = tpc_settings.skyline;

    if (skyline.size() < GPCs.size()) {
        skyline.resize(GPCs.size(), 0);
    }
    std::sort(skyline.begin(), skyline.end());

    std::set<uint32_t> tpc_counts_that_can_become_spares;
    for (uint32_t i = 0; i < skyline.size(); i++) {
        if (alive_tpcs[i] > skyline[i]) {
            tpc_counts_that_can_become_spares.insert(alive_tpcs[i]);
        }
    }

    std::vector<uint32_t> physical_gpcs_that_can_have_spares;

    // Pick a GPC to put the spare in
    // start with GPC counts that have the most TPCs
    for (auto phys_gpc_it = tpc_counts_that_can_become_spares.rbegin(); phys_gpc_it != tpc_counts_that_can_become_spares.rend(); phys_gpc_it++) {
        for (uint32_t phys_gpc = 0; phys_gpc < GPCs.size(); phys_gpc++) {
            if (getNumEnabled(GPCs[phys_gpc]->TPCs) == *phys_gpc_it) {
                physical_gpcs_that_can_have_spares.push_back(phys_gpc);
            }
        }
    }

    // Now pick a TPC from a GPC on the list
    // It can't be a mustEnable
    for (uint32_t gpc_idx : physical_gpcs_that_can_have_spares) {
        for (uint32_t tpc_idx : GPCs[gpc_idx]->getPreferredDisableList(module_type_id::tpc, potentialSKU)) {
            if (GPCs[gpc_idx]->TPCs[tpc_idx]->getEnabled()) {
                uint32_t flat_idx = gpc_idx * static_cast<uint32_t>(GPCs[gpc_idx]->TPCs.size()) + tpc_idx;
                // Also check if disabling this TPC would make the config invalid. if so, then pick something else
                // this check avoids the possibility of having a GPC or CPC with only spare TPCs and disabled TPCs
                std::unique_ptr<FSChip_t> test_clone = clone();
                test_clone->GPCs[gpc_idx]->TPCs[tpc_idx]->setEnabled(false);
                std::string dummymsg;
                if (test_clone->isValid(FSmode::FUNC_VALID, dummymsg)) {
                    if (!isMustEnable(potentialSKU, module_type_id::tpc, flat_idx)) {
                        spare_gpc_idx = gpc_idx;
                        spare_tpc_idx = tpc_idx;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

}
