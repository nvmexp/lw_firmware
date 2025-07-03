#include <iostream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <array>
#include "fs_chip_factory.h"
#include "fs_chip_hopper.h"
#include "SkylineSpareSelector.h"
#include <assert.h>

namespace fslib {

//-----------------------------------------------------------------------------
// Include generated headers
//-----------------------------------------------------------------------------
#include "gh100_config.h"


//-----------------------------------------------------------------------------
// GHLit1GPC0_t
//-----------------------------------------------------------------------------

/**
 * @brief Check if GH100's GPC0 is valid. This contains specific rules for checking gh100's special case gpc0
 * 
 * @param fsmode degree of valid to check for 
 * @param msg if it's not valid. This will be set to a string that explains why
 * @return true if valid
 * @return false if not valid
 */
bool GHLit1GPC0_t::isValid(FSmode fsmode, std::string& msg) const {

    if (!getEnabled()){
        std::stringstream ss;
        ss << "GPC0 cannot be disabled!";
        msg = ss.str();
        return false;
    }

    // gfx only gpc
    if (!PESs[0]->getEnabled()){
        std::stringstream ss;
        ss << "The PES in GPC0 cannot be disabled!";
        msg = ss.str();
        return false;
    }

    if (!ROPs[0]->getEnabled()){
        std::stringstream ss;
        ss << "The ROP in GPC0 cannot be disabled!";
        msg = ss.str();
        return false;
    }

    if (!(TPCs[0]->getEnabled() || TPCs[1]->getEnabled() || TPCs[2]->getEnabled())){
        std::stringstream ss;
        ss << "The GFX TPCs in GPC0 cannot all be disabled!";
        msg = ss.str();
        return false;
    }
    
    return GHLit1GPC_t::isValid(fsmode, msg);
}

/**
 * @brief Call both the GHLit1GPC_t downbin and GPCWithGFX_t downbin function
 * 
 */
void GHLit1GPC0_t::funcDownBin() {
    GHLit1GPC_t::funcDownBin();
    GPCWithGFX_t::funcDownBin();
}

/**
 * @brief Call both the GHLit1GPC_t downbin and GPCWithGFX_t setup function
 * 
 * @param settings 
 */
void GHLit1GPC0_t::setup(const chipPOR_settings_t& settings) {
    GPCWithGFX_t::setup(settings);
    GHLit1GPC_t::setup(settings);
}

/**
 * @brief Get a mapping of module types to function pointers returning those types
 * In this case, combine GPCWithGFX_t with GHLit1GPC_t
 * 
 * @return const module_map_t& 
 */
const module_map_t& GHLit1GPC0_t::getModuleMapRef() const {
    auto makeMap = [this]() -> module_map_t {
        module_map_t gfx_module_map = GPCWithGFX_t::getModuleMapRef();
        module_map_t module_map = GHLit1GPC_t::getModuleMapRef();
        module_map.insert(gfx_module_map.begin(), gfx_module_map.end());
        return module_map;
    };
    const static module_map_t module_map = makeMap();
    return module_map;
}

const module_set_t& GHLit1GPC0_t::getSubModuleTypes() const {
    // const static to avoid construction every time, while also not requiring every instance to maintain its own copy
    const static module_set_t child_set = makeChildModuleSet();
    return child_set;
}

/**
 * @brief apply the mask to cpcs and gfx units
 * 
 * @param mask defined in fslib.h
 * @param index Which GPC this is this
 */
void GHLit1GPC0_t::applyMask(const fslib::GpcMasks& mask, uint32_t index){
    GPCWithGFX_t::applyMask(mask, index);
    GHLit1GPC_t::applyMask(mask, index);
}

/**
 * @brief Get the fslib.h masks for this GPC
 * 
 * @param mask defined in fslib.h
 * @param index Which GPC this is
 */
void GHLit1GPC0_t::getMask(fslib::GpcMasks& mask, uint32_t index) const {
    GPCWithGFX_t::getMask(mask, index);
    GHLit1GPC_t::getMask(mask, index);
}

/**
 * @brief Return a list of CPC idxs sorted based on enabled TPC count per CPC
 * Specialization for gpc0 because we want to prioritize cpcs 1 and 2 over 0
 * 
 * @return module_index_list_t 
 */
module_index_list_t GHLit1GPC0_t::getCPCIdxsByTPCCount() const {
    module_index_list_t cpc_idxs = {1,2,0};
    PESMapping_t mapping(static_cast<int32_t>(CPCs.size()), static_cast<int32_t>(TPCs.size()));
    sortMajorUnitByMinorCount(getBaseModuleList(CPCs), getBaseModuleList(TPCs), cpc_idxs, mapping);
    return cpc_idxs;
}

/**
 * @brief Balance out the TPCs per PES. But also prefer the compute only tpcs over gfx tpcs
 * 
 * @return module_index_list_t 
 */
module_index_list_t GHLit1GPC0_t::getPreferredDisableListTPC(const FUSESKUConfig::SkuConfig& potentialSKU) const {
    module_index_list_t list = GPCWithCPC_t::getPreferredDisableListTPC(potentialSKU);

    uint32_t min_gfx_tpcs = 1;
    auto min_tpcs_map_it = potentialSKU.misc_params.find(fslib::min_gfx_tpcs_str);
    if (min_tpcs_map_it != potentialSKU.misc_params.end()) {
        min_gfx_tpcs = min_tpcs_map_it->second;
    }

    
    for (uint32_t gfx_tpc : {0, 1, 2}) {
        auto list_it = std::find(list.begin(), list.end(), gfx_tpc);
        if (list_it != list.end()) {
            list.erase(list_it);

            // If we don't have gfx tpcs to spare, then don't offer them as options to floorsweep
            if (getNumEnabledRange(TPCs, 0, 2) > min_gfx_tpcs) {
                list.push_back(gfx_tpc);
            }
        }
    }

    return list;
}


/**
 * @brief Check if GH100's GPC is valid. This contains specific rules for checking gh100's GPCs
 * 
 * @param fsmode degree of valid to check for 
 * @param msg if it's not valid. This will be set to a string that explains why
 * @return true if valid
 * @return false if not valid
 */
bool GHLit1GPC_t::isValid(FSmode fsmode, std::string& msg) const {
    if (getEnabled()){
        if (!CPCs[0]->getEnabled() && !CPCs[1]->getEnabled()){
            std::stringstream ss;
            ss << "CPC0 and CPC1 cannot both be floorswept in " << getTypeStr() << instance_number << "!";
            msg = ss.str();
            return false;
        }

        if (fsmode == FSmode::PRODUCT){
            if (getNumEnabled(CPCs) < 2){
                std::stringstream ss;
                ss << "In product mode, GH100 needs to have at least 2 " << CPCs[0]->getTypeStr() << "s per " << getTypeStr();
                ss << ", but " << getTypeStr() << instance_number << " has only 1 enabled!";
                msg = ss.str();
                return false;
            }
        }
    }
    return GPCWithCPC_t::isValid(fsmode, msg);
}


//-----------------------------------------------------------------------------
// GHLit1Chip_t and GH100_t
//-----------------------------------------------------------------------------

/**
 * @brief Check the gh100 specific rules for FBP floorsweeping
 * 
 * @param fsmode degree of valid to check for 
 * @param msg if it's not valid. This will be set to a string that explains why
 * @return true if valid
 * @return false if not valid
 */
bool GHLit1FBP_t::isValid(FSmode fsmode, std::string& msg) const {

    // only 1 slice per FBP allowed to be disabled
    // unless the LTC is disabled
    if (ltcHasPartialSlicesEnabled(0) && ltcHasPartialSlicesEnabled(1)) {
        std::stringstream ss;
        ss << "FBP" << instance_number << " cannot have more than 1 slice floorswept!";
        msg = ss.str();
        return false;
    }

    // only 1 slice per LTC is allowed to be floorswept
    for (uint32_t i = 0; i < LTCs.size(); i++){
        if (!ltcIsValid(fsmode, i, msg)){
            return false;
        }
    }

    // FBIOs must be floorswept if the FBP is floorswept, and both must be enabled if the FBP is enabled
    if (getEnabled()){
        if (getNumEnabled(FBIOs) != FBIOs.size()) {
            std::stringstream ss;
            ss << "FBIOs in FBP" << instance_number << " cannot be floorswept while the FBP is enabled!";
            msg = ss.str();
            return false;
        }
    } else {
        if (getNumEnabled(FBIOs) != 0) {
            std::stringstream ss;
            ss << "FBIOs in FBP" << instance_number << " cannot be enabled if the FBP is floorswept!";
            msg = ss.str();
            return false;
        }
    }

    return HBMFBP_t::isValid(fsmode, msg);
}

/**
 * @brief Downbin this ghlit1 FBP to make it functionally valid
 * 
 */
void GHLit1FBP_t::funcDownBin() {
    if (ltcHasPartialSlicesEnabled(0) && ltcHasPartialSlicesEnabled(1)){
        setEnabled(false);
    }

    if (getEnabled()){
        if (getNumEnabled(FBIOs) != FBIOs.size()) {
            setEnabled(false);
        }
    }

    HBMFBP_t::funcDownBin();
}

/**
 * @brief Check the gh100 specific rules for LTC floorsweeping
 * 
 * @param fsmode degree of valid to check for 
 * @param ltc_idx which ltc are we checking?
 * @param msg if it's not valid. This will be set to a string that explains why
 * @return true if valid
 * @return false if not valid
 */
bool GHLit1FBP_t::ltcIsValid(FSmode fsmode, uint32_t ltc_idx, std::string& msg) const {
    // only 1 slice per LTC is allowed to be floorswept
    int slices_enabled_ltc = getNumEnabledSlicesPerLTC(ltc_idx);
    if (slices_enabled_ltc == 1 || slices_enabled_ltc == 2){
        std::stringstream ss;
        ss << "LTC" << ltc_idx << " in FBP" << instance_number << " cannot have more than 1 slice floorswept!";
        msg = ss.str();
        return false;
    }
    return FBP_t::ltcIsValid(fsmode, ltc_idx, msg);
}

/**
 * @brief Return a new GPC instance that is appropriate for this GPC type
 * 
 * @return std::unique_ptr<GPC_t> 
 */
void GHLit1Chip_t::getGPCType(GPCHandle_t& handler) const {
    makeModule(handler, GHLit1GPC_t());
}

/**
 * @brief Return a new FBP instance that is appropriate for this GPC type
 * 
 * @return std::unique_ptr<FBP_t> 
 */
void GHLit1Chip_t::getFBPType(FBPHandle_t& handler) const {
    makeModule(handler, GHLit1FBP_t());
}

/**
 * @brief This function calls the base class setup function, but then replaces the GPC0 with a GHLit1GPC0_t instance
 * 
 */
void GH100_t::instanceSubModules() {
    // Set up like normal
    GPUWithuGPU_t::instanceSubModules();
    
    // but replace gpc0 with a gfx gpc
    makeModule(GPCs[0], GHLit1GPC0_t());
    GPCs[0]->setup(getChipPORSettings());
}


/**
 * @brief hopper amap supports specific slice counts
 * 
 * @param fsmode 
 * @param msg 
 * @return true 
 * @return false 
 */
bool GH100_t::enabledSliceCountIsValid(FSmode fsmode, std::string& msg) const {

    // AMAP only supports these slice counts
    int num_enabled_slices = getNumEnabledSlices();
    if (!(num_enabled_slices == 96
       || num_enabled_slices == 94
       || num_enabled_slices == 92
       || num_enabled_slices % 8 == 0)){
        std::stringstream ss;
        ss << "Invalid number of enabled slices: " << num_enabled_slices << "!";
        msg = ss.str();
        return false;
    }
    return true;
}

/**
 * @brief hopper amap supports specific slice counts, so if l2slice, then check that
 * 
 * @param fsmode 
 * @param msg 
 * @return true 
 * @return false 
 */
bool GH100_t::enableCountIsValid(module_type_id submodule_type, FSmode fsmode, std::string& msg) const {
    switch(submodule_type){
        // if slice, then check if slice counts are valid
        case module_type_id::l2slice: return enabledSliceCountIsValid(fsmode, msg);
        // if other, than use the parent class function
        default: return GPUWithuGPU_t::enableCountIsValid(submodule_type, fsmode, msg);
    }
}

/**
 * @brief Prefer HBM E, then B, then don't care
 * 
 * @param module_type 
 * @return module_index_list_t 
 */
module_index_list_t GH100_t::getFSTiePreference(module_type_id module_type) const {
    switch(module_type){
        case module_type_id::fbp: return {6, 11, 1, 4};
        case module_type_id::ltc: return {12, 13, 22, 23, 2, 3, 8, 9};

        default: return {};
    }
}

/**
 * @brief Return a mapping of FBPs to their HBM partners
 * 
 * @param fbp_idx 
 * @return uint32_t 
 */
static uint32_t getHBMPair(uint32_t fbp_idx){
                                                    //0, 1, 2, 3, 4, 5, 6,  7, 8,  9, 10, 11
    const static std::array<uint32_t, 12> hbm_mapping{2, 4, 0, 5, 1, 3, 11, 9, 10, 7,  8, 6};

    return hbm_mapping[fbp_idx];
}


/**
 * @brief Sort the list of FBPs based on whether there are already FBPs floorswept
 * 
 * @param fbp_list 
 */
void GH100_t::preferWholes(module_index_list_t& fbp_list) const {
    module_index_list_t priority_list;
    for(uint32_t i = 0; i < fbp_list.size(); i++){
        uint32_t fbp_idx = fbp_list[i];

        if(!FBPs[getHBMPair(fbp_idx)]->getEnabled()){
            //put every FBP with a disabled HBM site partner at the front of the preferred disable lis
            priority_list.push_back(fbp_idx);
        }
    }

    // pull out the prioritized ids
    for (uint32_t priority_fbp_idx : priority_list){
        fbp_list.erase(std::remove(fbp_list.begin(), fbp_list.end(), priority_fbp_idx), fbp_list.end());
    }

    // add them to the front
    for(uint32_t i = 0; i < priority_list.size(); i++){
        fbp_list.insert(fbp_list.begin() + i, priority_list[i]);
    }
}

void GH100_t::preferWholeSitesSubFBP(const_flat_module_list_t& node_list, const std::vector<module_type_id>& priority) const {
    // if we need to floorsweep more LTCs, we want to floorsweep an FBP that is already partially floorswept if possible
    sortListByEnableCountAscending(node_list, priority);
}

void GH100_t::sortPreferredMidLevelModules(const_flat_module_list_t& node_list, const std::vector<module_type_id>& priority) const {
    if (node_list.at(0)->getTypeEnum() == module_type_id::fbp && priority.back() == module_type_id::ltc){
        preferWholeSitesSubFBP(node_list, priority);
    } else {
        GPUWithuGPU_t::sortPreferredMidLevelModules(node_list, priority);
    }
}

/**
 * @brief Prefer whole to half sites. If a half site is already floorswept, make sure we recommend FSing the other half, rather than something new
 * Also, do not recommend floorsweeping GPC0
 * 
 * @param module_type 
 * @return module_index_list_t 
 */
module_index_list_t GH100_t::getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const {
    module_index_list_t disable_list = GPUWithuGPU_t::getPreferredDisableList(module_type, potentialSKU);
    switch(module_type){
        case module_type_id::gpc: {
            // remove gpc 0 from the list, since it cannot be floorswept
            disable_list.erase(std::remove(disable_list.begin(), disable_list.end(), 0), disable_list.end());
            break;
        }
        case module_type_id::tpc:
        {
            // If no skyline was found or tpcs aren't specified in the sku
            const auto& fsSettings_it = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc));
            if (fsSettings_it == potentialSKU.fsSettings.end() || fsSettings_it->second.skyline.empty()) {
                return GPUWithuGPU_t::getPreferredDisableList(module_type_id::tpc, potentialSKU);
            }
            return getPreferredTPCsToDisableForSkyline(potentialSKU);
        }
        case module_type_id::fbp: {
            preferWholes(disable_list);
            break;
        }
        default: {
            break;
        }
    }
    return disable_list;
}

/**
 * @brief Return the number of HBMs enabled in this config
 * 
 * @return uint32_t 
 */
uint32_t GH100_t::getHBMEnableCount() const {
    uint32_t hbms_enabled_doubled = 0;
    for (uint32_t i = 0; i < FBPs.size(); i++){
        // if either FBP in the HBM is enabled, then the HBM is needed
        if (FBPs[i]->getEnabled() || FBPs[getHBMPair(i)]->getEnabled()){
            hbms_enabled_doubled += 1;
        }
    }
    return hbms_enabled_doubled / 2;
}

/**
 * @brief Choose the best config.
 * Prefer whole sites over half sites. 
 * Prefer half sites over quarter sites.
 * 
 * @param candidate 
 * @return int 
 */
int GH100_t::compareConfig(const FSChip_t* candidate) const {
    // prefer whole sites
    // if one config has all whole sites, but the other has half or quarter sites, prefer the one with whole sites
    // this can easily be checked by preferring che config with fewest FBPs

    const GH100_t* gh100_candidate_ptr = dynamic_cast<const GH100_t*>(candidate);
    assert(gh100_candidate_ptr != nullptr);

    uint32_t this_num_hbms = getHBMEnableCount();
    uint32_t candidate_num_hbms = gh100_candidate_ptr->getHBMEnableCount();

    if (this_num_hbms > candidate_num_hbms){
        return 1;
    }

    if (this_num_hbms < candidate_num_hbms){
        return -1;
    }

    // if they are equal, then pick the config with the fewest FBPs
    uint32_t this_num_fbps = getTotalEnableCount(module_type_id::fbp);
    uint32_t candidate_num_fbps = candidate->getTotalEnableCount(module_type_id::fbp);

    if (this_num_fbps > candidate_num_fbps){
        return 1;
    }

    if (this_num_fbps < candidate_num_fbps){
        return -1;
    }
    
    // if equal then call the base class version
    return FSChip_t::compareConfig(candidate);
}

/**
 * @brief Apply known downbinning heuristics for GH100 here
 * 
 * @param sku 
 */
void GH100_t::applyDownBinHeuristic(const FUSESKUConfig::SkuConfig& sku) {
    funcDownBin();
    // in a perfect memory system config
    // a todo here would be to make this more flexible so this function can work on non-perfect configs
    if (
        (getTotalEnableCount(module_type_id::ltc) == getModuleCount(module_type_id::ltc)) && 
        (getTotalEnableCount(module_type_id::l2slice) == getModuleCount(module_type_id::l2slice)) && 
        (getTotalEnableCount(module_type_id::fbp) == getModuleCount(module_type_id::fbp))
        ) {
        
        int32_t ltcs_to_disable = getTotalEnableCount(module_type_id::ltc) - getMaxAllowed(module_type_id::ltc, sku);

        // pick B or E if we need to FS 2 FBPs
        if (ltcs_to_disable == 4) {
            // if HBM B or E is must enable, and we need to floorsweep some ltcs, pick ones from B or E
            if (getMinAllowed(module_type_id::fbp, sku) == 10){
                if (!isMustEnable(sku, module_type_id::fbp, 6) && !isMustEnable(sku, module_type_id::fbp, 11)){
                    returnModuleList(module_type_id::fbp)[6]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[11]->setEnabled(false);
                } else if (!isMustEnable(sku, module_type_id::fbp, 1) && !isMustEnable(sku, module_type_id::fbp, 4)){
                    returnModuleList(module_type_id::fbp)[1]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[4]->setEnabled(false);
                }
                funcDownBin();
            }
        }

        // pick BE, AC, DF if need to FS 4 FBPs
        if (ltcs_to_disable == 8) {
            // if HBM AC or DF is must enable, and we need to floorsweep some ltcs, pick ones from AC or DF
            if (getMinAllowed(module_type_id::fbp, sku) == 8) {
                bool AIsNotMustEnable = !isMustEnable(sku, module_type_id::fbp, 0) && !isMustEnable(sku, module_type_id::fbp, 2);
                bool BIsNotMustEnable = !isMustEnable(sku, module_type_id::fbp, 1) && !isMustEnable(sku, module_type_id::fbp, 4);
                bool CIsNotMustEnable = !isMustEnable(sku, module_type_id::fbp, 3) && !isMustEnable(sku, module_type_id::fbp, 5);
                bool DIsNotMustEnable = !isMustEnable(sku, module_type_id::fbp, 7) && !isMustEnable(sku, module_type_id::fbp, 9);
                bool EIsNotMustEnable = !isMustEnable(sku, module_type_id::fbp, 6) && !isMustEnable(sku, module_type_id::fbp, 11);
                bool FIsNotMustEnable = !isMustEnable(sku, module_type_id::fbp, 8) && !isMustEnable(sku, module_type_id::fbp, 10);
                if (BIsNotMustEnable && EIsNotMustEnable) {
                    returnModuleList(module_type_id::fbp)[1]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[4]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[6]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[11]->setEnabled(false);
                } else if (AIsNotMustEnable && CIsNotMustEnable) {
                    returnModuleList(module_type_id::fbp)[0]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[2]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[3]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[5]->setEnabled(false);
                } else if (DIsNotMustEnable && FIsNotMustEnable) {
                    returnModuleList(module_type_id::fbp)[7]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[9]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[8]->setEnabled(false);
                    returnModuleList(module_type_id::fbp)[10]->setEnabled(false);
                }
                funcDownBin();
            }
        }
    }
}

/**
 * @brief Return a const ref to the uGPU GPC mapping
 * 
 * @return const std::vector<uint32_t>& 
 */
const std::vector<uint32_t>& GH100_t::getUGPUGPCMap() const {
    const static std::vector<uint32_t> ugpu_gpc_map_s = {0, 0, 1, 1, 1, 1, 0, 0};
    return ugpu_gpc_map_s;
}

/**
 * @brief Get the number of enabled gfx tpcs
 * 
 * @return uint32_t 
 */
uint32_t GH100_t::getNumEnabledGfxTPCs() const {
    // gfx tpcs are tpcs 0..2 in gpc0
    return getNumEnabledRange(GPCs[0]->TPCs, 0, 2);
}

/**
 * @brief Override the chip's downbin function. This is so that we can optimize for spare selection
 * 
 * @param potentialSKU 
 * @param msg 
 * @return true 
 * @return false 
 */
bool GH100_t::downBin(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) {

    if (!skuIsPossible(potentialSKU, msg)) {
        return false;
    }

    applyMustDisableLists(potentialSKU);
    funcDownBin();
    disableNonCompliantInstances(potentialSKU, msg);
    applyDownBinHeuristic(potentialSKU);


    bool canFitSkuNoSpares = GPUWithuGPU_t::canFitSKU(potentialSKU, msg);

    const auto& tpc_settings_it = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc));

    std::unique_ptr<FSChip_t> test_clone;
    bool test_downbin_success = false;
    
    if (tpc_settings_it != potentialSKU.fsSettings.end()) {
        const auto& tpc_settings = tpc_settings_it->second;

        if (canFitSkuNoSpares) {

            // First try a config with the max, then back off if not found
            for (int32_t repair_count_attempt = tpc_settings.repairMaxCount; repair_count_attempt > 0; repair_count_attempt--) {

                // create a temp new sku that allows more tpcs
                FUSESKUConfig::SkuConfig newSku(potentialSKU);
                auto& temp_tpc_settings = newSku.fsSettings.find(idToStr(module_type_id::tpc))->second;
                temp_tpc_settings.maxEnableCount = tpc_settings.maxEnableCount + repair_count_attempt;
                temp_tpc_settings.minEnableCount = tpc_settings.minEnableCount + repair_count_attempt;
                temp_tpc_settings.repairMaxCount = 0;

                // find the optimal logical tpc config for repair
                std::string spares_msg;
                temp_tpc_settings.skyline = getOptimalTPCDownBinForSparesWrap(potentialSKU, repair_count_attempt, spares_msg);

                // If the spare selection failed, then don't bother searching for configs that can fit
                if (temp_tpc_settings.skyline.size() == 0) {
                    continue;
                }

                // check if this config can fit that optimal logical tpc config
                test_clone = clone();
                std::string test_clone_msg;
                test_downbin_success = test_clone->downBin(newSku, test_clone_msg);
                if (test_downbin_success) {
                    // now pick spares and disable them
                    GH100_t* gh100_ptr = dynamic_cast<GH100_t*>(test_clone.get());
                    assert(gh100_ptr != nullptr);
                    *this = *gh100_ptr;

                    pickSpareTPCs(potentialSKU, repair_count_attempt, spare_gpc_mask);
                    return true;
                }
            }
        } else {
            return false;
        }
    }

    // If we couldn't pick spares, or if tpcs aren't in the sku, then call the function without consideration for spares
    test_clone = canFitSKUSearch(potentialSKU, msg);
    if (test_clone != nullptr) {
        GH100_t* gh100_ptr = dynamic_cast<GH100_t*>(test_clone.get());
        assert(gh100_ptr != nullptr);
        *this = *gh100_ptr;
    }

    return test_clone != nullptr;
}


/**
 * @brief GH100 specialization for canFitSKUSearch. Checks the skyline as well as the generic stuff
 * 
 * @param potentialSKU 
 * @param msg 
 * @return std::unique_ptr<FSChip_t> 
 */
std::unique_ptr<FSChip_t> GH100_t::canFitSKUSearch(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {
    // check the skyline stuff
    const auto& fsSettings_it = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc));
    // if tpcs aren't specified in the sku config, then skyline isn't a concern
    if (fsSettings_it != potentialSKU.fsSettings.end()) {
        // if skyline was specified
        if (!fsSettings_it->second.skyline.empty()) {
            if (!skylineFits(fsSettings_it->second.skyline, msg)){
                return nullptr;
            }

            if (!canFitSkylineGPC0Screen(potentialSKU, msg)) {
                return nullptr;
            }
        }
    }

    //check min gfx tpcs
    if (!hasEnoughGfxTPCs(potentialSKU, msg)){
        return nullptr;
    }

    // check the other generic stuff
    return GPUWithuGPU_t::canFitSKUSearch(potentialSKU, msg);
}

/**
 * @brief GH100 specialization for isInSKU. Checks the skyline in addition to the generic stuff.
 * 
 * @param potentialSKU 
 * @param msg 
 * @return true 
 * @return false 
 */
bool GH100_t::isInSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {
    // check the skyline stuff
    const auto& fsSettings_it = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc));
    // if tpcs aren't specified in the sku config, then skyline isn't a concern
    if (fsSettings_it != potentialSKU.fsSettings.end()) {
        // if skyline was specified
        if (!fsSettings_it->second.skyline.empty()) {
            if (!skylineFits(fsSettings_it->second.skyline, msg)){
                return false;
            }
        }
    }

    //check min gfx tpcs
    if (!hasEnoughGfxTPCs(potentialSKU, msg)){
        return false;
    }

    return GPUWithuGPU_t::isInSKU(potentialSKU, msg);
}

/**
 * @brief GH100 override for skuIsPossible. Just checks if the skyline is possible.
 * 
 * @param potentialSKU 
 * @param msg 
 * @return true 
 * @return false 
 */
bool GH100_t::skuIsPossible(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const {

    std::unique_ptr<FSChip_t> fresh_chip = createChip(getChipEnum());
    const GH100_t* gh100_chip = dynamic_cast<const GH100_t*>(fresh_chip.get());
    assert(gh100_chip != nullptr);
    fresh_chip->applyMustDisableLists(potentialSKU);
    fresh_chip->funcDownBin();
    
    const auto tpc_settings_it = potentialSKU.fsSettings.find(idToStr(module_type_id::tpc));
    if (tpc_settings_it != potentialSKU.fsSettings.end()) {
        // if skyline was specified
        if (!tpc_settings_it->second.skyline.empty()) {
            if (!gh100_chip->skylineFits(tpc_settings_it->second.skyline, msg)){
                return false;
            }

            uint32_t skyline_tpcs = std::accumulate(tpc_settings_it->second.skyline.begin(), tpc_settings_it->second.skyline.end(), 0);
            if (getMaxAllowed(module_type_id::tpc, potentialSKU) < skyline_tpcs){
                std::stringstream ss;
                ss << "This sku is not possible because the number of skyline TPCs exceeds the maximum allowed TPCs!";
                msg = ss.str();
                return false;
            }

            // if the skyline is smaller than the minimum number of GPCs allowed, then there will be at least 1 GPC that is all singletons
            // There need to be enough singletons for this GPC to be valid
            if (tpc_settings_it->second.skyline.size() < getMinAllowed(module_type_id::gpc, potentialSKU)) {
                uint32_t max_tpc_count = tpc_settings_it->second.maxEnableCount;
                uint32_t num_singletons = max_tpc_count - skyline_tpcs;
                uint32_t min_tpcs_per_gpc = tpc_settings_it->second.getMinGroupCount();
                uint32_t gpcs_with_only_singletons = getMinAllowed(module_type_id::gpc, potentialSKU) - static_cast<int>(tpc_settings_it->second.skyline.size());
                if (num_singletons < min_tpcs_per_gpc * gpcs_with_only_singletons){
                    std::stringstream ss;
                    ss << "This sku is not possible because the number of singletons (" << num_singletons << ") is less than the min tpc per gpc count! ";
                    ss << "The GPC does not meet the sku requirement, but it is not allowed to be floorswept because of min gpcs = ";
                    ss << getMinAllowed(module_type_id::gpc, potentialSKU);
                    msg = ss.str();
                    return false;
                }
            }
        }
    }

    return GPUWithuGPU_t::skuIsPossible(potentialSKU, msg);
}

/**
 * @brief Pick the ideal GPC to floorsweep out of the two indexes into the list
 * 
 * @param gpc_list a list of gpcs
 * @param left 
 * @param right 
 * @return true 
 * @return false 
 */
bool GH100_t::getTiePreferenceGPCFS(const const_flat_module_list_t& gpc_list, int32_t left, int32_t right) const {
    uint32_t left_physical_id = gpc_list[left]->instance_number;
    uint32_t right_physical_id = gpc_list[right]->instance_number;


    uint32_t tpcs_in_ugpu_of_left = getTPCCountPerUGPU(getUGPUGPCMap()[left_physical_id]);
    uint32_t tpcs_in_ugpu_of_right = getTPCCountPerUGPU(getUGPUGPCMap()[right_physical_id]);

    // if the ugpus are balanced, or if both GPCs are in the same uGPU, pick the GPC with more total TPCs
    if (tpcs_in_ugpu_of_left == tpcs_in_ugpu_of_right) {

        uint32_t left_enabled_tpcs = gpc_list[left]->getEnableCount(module_type_id::tpc);
        uint32_t right_enabled_tpcs = gpc_list[right]->getEnableCount(module_type_id::tpc);

        // if total TPC counts are equal, then pick the one with the lower physical ID
        if (left_enabled_tpcs == right_enabled_tpcs) {
            return left_physical_id < right_physical_id;
        }

        //pick the GPC with more total TPCs
        return left_enabled_tpcs > right_enabled_tpcs;
    }

    // return true if the tpc count in the ugpu of the left param is greater than that of the right param
    return tpcs_in_ugpu_of_left > tpcs_in_ugpu_of_right;
}

}  // namespace fslib
