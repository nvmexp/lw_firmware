
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include "fs_compute_only_gpc.h"
namespace fslib {
/**
 * @brief if this chip has enough gfx tpcs to satisfy the sku, return true,
 * if not, return false and set msg to explain why not
 *
 * @param sku
 * @param msg
 * @return true
 * @return false
 */
bool ChipWithComputeOnlyGPCs::hasEnoughGfxTPCs(const FUSESKUConfig::SkuConfig& sku, std::string& msg) const {
    const auto gfx_tpcs_it = sku.misc_params.find(min_gfx_tpcs_str);
    if (gfx_tpcs_it != sku.misc_params.end()) {
        int32_t min_gfx_tpcs = gfx_tpcs_it->second;
        if (getNumEnabledGfxTPCs() < static_cast<uint32_t>(min_gfx_tpcs)) {
            std::stringstream ss;
            ss << "Not enough enabled GFX TPCs to fit the sku! ";
            ss << "This chip has " << getNumEnabledGfxTPCs() << ", but the sku requires ";
            ss << min_gfx_tpcs << "!";
            msg = ss.str();
            return false;
        }
    }
    return true;
}

}