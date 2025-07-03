#include "fs_lib.h"
#include <stdio.h>
#include <iostream>
#include <string>
#include "fs_chip.h"
#include "fs_chip_factory.h"

namespace fslib {

/**
 * @brief The old implementation of IsFloorsweepingValid, still used on older chips (pre-gh100)
 * 
 * @param chip The chip to be checked
 * @param gpcSettings Masks of GPCs/compute units
 * @param fbpSettings Masks of FPBs/memory units
 * @param[out] msg An error message describing why a configuration is invalid
 * @param mode The mode to use: PRODUCT, FUNC_VALID, IGNORE_FSLIB
 * @return true This configuration is valid
 * @return false This configuration is invalid
 */
bool IsFloorsweepingValidLegacyImpl(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
                          std::string& msg, FSmode mode) {
    FsChip fsChip(chip, mode);
    return fsChip.IsFsValid(gpcSettings, fbpSettings, msg);
}

/**
 * @brief The old implementation of IsFloorsweepingValid, still used on older chips (pre-gh100) 
 * 
 * @param chip The chip to be checked
 * @param gpcSettings Masks of GPCs/compute units
 * @param fbpSettings Masks of FPBs/memory units
 * @param[out] msg An error message describing why a configuration is invalid
 * @param mode The mode to use: PRODUCT, FUNC_VALID, IGNORE_FSLIB
 * @param printUgpu Print the uGPU active status
 * @param outFile The file to output the uGPU active status to
 * @return true This configuration is valid
 * @return false This configuration is invalid 
 */
bool IsFloorsweepingValidLegacyImpl(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
                          std::string& msg, FSmode mode, bool printUgpu, std::ofstream &outFile) {
    FsChip fsChip(chip, mode);
    bool result = true;
    result = fsChip.IsFsValid(gpcSettings, fbpSettings, msg);
    if (result && printUgpu) {
        if (outFile.is_open()) {
            outFile << fsChip.CheckBothUgpuActive(fbpSettings, gpcSettings) << ",";
        }
    }
    return result;
}

/**
 * @brief The current implementation of IsFloorsweepingValid (GH100 and newer)
 * 
 * @param chip The chip to be checked
 * @param gpcSettings Masks of GPCs/compute units
 * @param fbpSettings Masks of FPBs/memory units
 * @param[out] msg An error message describing why a configuration is invalid
 * @param mode The mode to use: PRODUCT, FUNC_VALID, IGNORE_FSLIB
 * @return true This configuration is valid
 * @return false This configuration is invalid
 */
bool IsFloorsweepingValidImpl(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
                          std::string& msg, FSmode mode) {
    std::unique_ptr<FSChip_t> fsChip = createChip(chip);
    if (fsChip == nullptr){
        msg = "invalid chip!";
        return true;
    }
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    return fsChip->isValid(mode, msg);

}


/**
 * @brief The current implementation of IsFloorsweepingValid (GH100 and newer) 
 * 
 * @param chip The chip to be checked
 * @param gpcSettings Masks of GPCs/compute units
 * @param fbpSettings Masks of FPBs/memory units
 * @param[out] msg An error message describing why a configuration is invalid
 * @param mode The mode to use: PRODUCT, FUNC_VALID, IGNORE_FSLIB
 * @param printUgpu Print the uGPU active status
 * @param outFile The file to output the uGPU active status to
 * @return true This configuration is valid
 * @return false This configuration is invalid 
 */
bool IsFloorsweepingValidImpl(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
                          std::string& msg, FSmode mode, bool printUgpu, std::ofstream &outFile) {
    FsChip fsChip(chip, mode);
    bool result = true;
    result = fsChip.IsFsValid(gpcSettings, fbpSettings, msg);
    if (result && printUgpu) {
        if (outFile.is_open()) {
            outFile << fsChip.CheckBothUgpuActive(fbpSettings, gpcSettings) << ",";
        }
    }
    return result;
}

/**
 * @brief Function that checks if a provided floorsweeping configuration is valid for a specified chip 
 * 
 * @param chip The chip to be checked
 * @param gpcSettings Masks of GPCs/compute units
 * @param fbpSettings Masks of FPBs/memory units
 * @param[out] msg An error message describing why a configuration is invalid
 * @param mode The mode to use: PRODUCT, FUNC_VALID, IGNORE_FSLIB
 * @return true This configuration is valid
 * @return false This configuration is invalid
 */
bool IsFloorsweepingValid(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
                          std::string& msg, FSmode mode) {

    if (mode == FSmode::IGNORE_FSLIB) {
        return true;
    }

    // use the different code bases depending on which chip
    switch(chip){
        case Chip::GH100: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode);
        case Chip::AD102: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode);
        case Chip::AD103: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode);
        case Chip::AD104: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode);
        case Chip::AD106: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode);
        case Chip::AD107: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode);
        default: return IsFloorsweepingValidLegacyImpl(chip, gpcSettings, fbpSettings, msg, mode);
    }
    //return IsFloorsweepingValidLegacyImpl(chip, gpcSettings, fbpSettings, msg, mode);
}

/**
 * @brief Function that checks if a provided floorsweeping configuration is valid for a specified chip  
 * 
 * @param chip The chip to be checked
 * @param gpcSettings Masks of GPCs/compute units
 * @param fbpSettings Masks of FPBs/memory units
 * @param[out] msg An error message describing why a configuration is invalid
 * @param mode The mode to use: PRODUCT, FUNC_VALID, IGNORE_FSLIB
 * @param printUgpu Print the uGPU active status
 * @param outFile The file to output the uGPU active status to
 * @return true This configuration is valid
 * @return false This configuration is invalid 
 */
bool IsFloorsweepingValid(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
                          std::string& msg, FSmode mode, bool printUgpu, std::ofstream &outFile) {

    // use the different code bases depending on which chip
    switch(chip){
        // case Chip::GH100: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode, printUgpu, outFile);
        case Chip::AD102: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode, printUgpu, outFile);
        case Chip::AD103: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode, printUgpu, outFile);
        case Chip::AD104: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode, printUgpu, outFile);
        case Chip::AD106: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode, printUgpu, outFile);
        case Chip::AD107: return IsFloorsweepingValidImpl(chip, gpcSettings, fbpSettings, msg, mode, printUgpu, outFile);
        default: return IsFloorsweepingValidLegacyImpl(chip, gpcSettings, fbpSettings, msg, mode, printUgpu, outFile);
    }
}

bool IsInSKU(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings, 
             const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) {
    
    std::unique_ptr<FSChip_t> fsChip = createChip(chip);
    if (fsChip == nullptr){
        msg = "invalid chip!";
        return false;
    }
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);
    
    if (!fsChip->skuIsPossible(potentialSKU, msg)){
        return false;
    }
    
    return fsChip->isInSKU(potentialSKU, msg);
}

bool CanFitSKU(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings, 
               const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) {
    
    std::unique_ptr<FSChip_t> fsChip = createChip(chip);
    if (fsChip == nullptr){
        msg = "invalid chip!";
        return false;
    }
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    if (!fsChip->skuIsPossible(potentialSKU, msg)){
        return false;
    }

    return fsChip->canFitSKU(potentialSKU, msg);
}

bool FuncDownBin(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings, 
                 GpcMasks& additionalGPCSettings, FbpMasks& additionalFBPSettings, std::string msg) {
    
    std::unique_ptr<FSChip_t> fsChip = createChip(chip);

    if (fsChip == nullptr){
        msg = "invalid chip!";
        return false;
    }
    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    fsChip->funcDownBin();

    if (!fsChip->isValid(FSmode::FUNC_VALID, msg)){
        return false;
    }

    fsChip->getGPCMasks(additionalGPCSettings);
    fsChip->getFBPMasks(additionalFBPSettings);

    return true;
}

bool DownBin(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
    const FUSESKUConfig::SkuConfig& sku,
    GpcMasks& additionalGPCSettings, FbpMasks& additionalFBPSettings, std::string& msg) {
    GpcMasks dummySpareGPCSettings;
    FbpMasks dummySpareFBPSettings;
    return DownBin(chip, gpcSettings, fbpSettings, sku, additionalGPCSettings, additionalFBPSettings,
        dummySpareGPCSettings, dummySpareFBPSettings, msg
    );
}

bool DownBin(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
    const FUSESKUConfig::SkuConfig& sku,
    GpcMasks& additionalGPCSettings, FbpMasks& additionalFBPSettings,
    GpcMasks& spareGPCsettings, FbpMasks& spareFBPSettings, std::string& msg) {
    
    std::unique_ptr<FSChip_t> fsChip = createChip(chip);

    if (fsChip == nullptr){
        msg = "invalid chip!";
        return false;
    }

    if (!fsChip->skuIsPossible(sku, msg)){
        return false;
    }

    fsChip->applyGPCMasks(gpcSettings);
    fsChip->applyFBPMasks(fbpSettings);

    fsChip->downBin(sku, msg);

    if (!fsChip->isValid(FSmode::FUNC_VALID, msg)){
        return false;
    }

    fsChip->getGPCMasks(additionalGPCSettings);
    fsChip->getFBPMasks(additionalFBPSettings);

    spareGPCsettings = fsChip->spare_gpc_mask;

    return true;
}



//-----------------------------------------------------------------------------
// Chip ID to string mapping functions
//-----------------------------------------------------------------------------

/**
 * @brief Return a map of str to chip enum
 *
 * @return std::map<std::string, module_type_id>
 */
static std::map<std::string, Chip> makeStrToChipMap() {
    std::map<std::string, Chip> str_to_id_map;
    str_to_id_map["gp102"] = Chip::GP102;
    str_to_id_map["gp104"] = Chip::GP104;
    str_to_id_map["ga106"] = Chip::GP106;
    str_to_id_map["gp107"] = Chip::GP107;
    str_to_id_map["gp108"] = Chip::GP108;
    str_to_id_map["gv100"] = Chip::GV100;
    str_to_id_map["tu102"] = Chip::TU102;
    str_to_id_map["tu104"] = Chip::TU104;
    str_to_id_map["tu106"] = Chip::TU106;
    str_to_id_map["tu116"] = Chip::TU116;
    str_to_id_map["tu117"] = Chip::TU117;
    str_to_id_map["ga100"] = Chip::GA100;
    str_to_id_map["ga102"] = Chip::GA102;
    str_to_id_map["ga103"] = Chip::GA103;
    str_to_id_map["ga104"] = Chip::GA104;
    str_to_id_map["ga106"] = Chip::GA106;
    str_to_id_map["ga107"] = Chip::GA107;
    str_to_id_map["gh100"] = Chip::GH100;
    str_to_id_map["ad102"] = Chip::AD102;
    str_to_id_map["ad103"] = Chip::AD103;
    str_to_id_map["ad104"] = Chip::AD104;
    str_to_id_map["ad106"] = Chip::AD106;
    str_to_id_map["ad107"] = Chip::AD107;
    return str_to_id_map;
}

/**
 * @brief Get a const reference to the Str To Chip Map object
 *
 * @return const std::map<std::string, Chip>&
 */
const std::map<std::string, Chip>& getStrToChipMap() {
    static std::map<std::string, Chip> str_to_id_map = makeStrToChipMap();
    return str_to_id_map;
}

/**
 * @brief create a map of chip to string. ilwerts the map created by makeChipStrMap
 *
 * @return std::map<Chip, std::string>
 */
static std::map<Chip, std::string> makeChipToStrMap() {
    std::map<Chip, std::string> id_to_str_map;
    for (const auto& kv : getStrToChipMap()) {
        // Check to make sure there wasn't a typo in the map above. There should be no duplicates
        if (id_to_str_map.find(kv.second) != id_to_str_map.end()) { throw FSLibException("found duplicate entry in makegetStrToIdMap()!"); }

        // ilwert the map iteratively
        id_to_str_map[kv.second] = kv.first;
    }
    return id_to_str_map;
}

/**
 * @brief Get a const reference to the chip To Str Map object
 *
 * @return const std::map<module_type_id, std::string>&
 */
const std::map<Chip, std::string>& getChipToStrMap() {
    static std::map<Chip, std::string> id_to_str_map = makeChipToStrMap();
    return id_to_str_map;
}

/**
 * @brief Map instance name to chip
 *
 * @param module_id
 * @return const std::string&
 */
const std::string& chipToStr(Chip chip) {
    const auto& map_it = getChipToStrMap().find(chip);
    return map_it->second;
}

/**
 * @brief map str instance name to enum
 *
 * @param module_str
 * @return module_type_id
 */
Chip strToChip(const std::string& chip_str) {
    const auto& map_it = getStrToChipMap().find(chip_str);
    return map_it->second;
}

std::ostream& operator<<(std::ostream& os, const Chip& chip) {
    os << chipToStr(chip);
    return os;
}


}  // namespace fslib
