#ifndef FS_LIB_H
#define FS_LIB_H

#define PRINTF snprintf

#include <string>
#include <fstream>
#include <vector>
#include <map>

#define DEBUGMSG(...)

// MODS now supports half-fbpa masks, so enabling it
#define LW_MODS_SUPPORTS_HALFFBPA_MASK 1

// Keep this disabled until MODS GA104 support is ready
// Enabling since GA104 fmodel is clean
#define LW_MODS_SUPPORTS_GA104_MASKS 1

namespace fslib {

enum class Chip {
    GP102,
    GP104,
    GP106,
    GP107,
    GP108,  // Pascal
    GV100,  // Volta
    TU102,
    TU104,
    TU106,
    TU116,
    TU117,  // Turing
    GA100, 
    GA101,
    GA102,  // GA10x family
    GA103,
    GA104,
    GA106,
    GA107,  // Ampere
    GA10B,  // CheetAh
    GH100,  // Hopper
    GH202,  
    AD102,  // Ada
    AD103,
    AD104,
    AD106,
    AD107,
    GB100,  // Blackwell
    G000    // Fmodel only
};

const std::map<std::string, Chip>& getStrToChipMap();
const std::map<Chip, std::string>& getChipToStrMap();
const std::string& chipToStr(Chip chip);
Chip strToChip(const std::string& chip_str);
std::ostream& operator<<(std::ostream& os, const Chip& chip);

enum class FSmode { PRODUCT, FUNC_VALID, IGNORE_FSLIB };

struct FbpMasks {
    unsigned int fbpMask;
    unsigned int fbioPerFbpMasks[32];
    unsigned int fbpaPerFbpMasks[32];
    unsigned int halfFbpaPerFbpMasks[32];
    unsigned int ltcPerFbpMasks[32];
    unsigned int l2SlicePerFbpMasks[32];
};

struct GpcMasks {
    unsigned int gpcMask;
    unsigned int pesPerGpcMasks[32];
    unsigned int tpcPerGpcMasks[32];
    unsigned int ropPerGpcMasks[32];
    unsigned int cpcPerGpcMasks[32];
};
bool operator==(const FbpMasks& L, const FbpMasks& R);
bool operator==(const GpcMasks& L, const GpcMasks& R);

//-----------------------------------------------------------------------------
// SKU definition from mods
//-----------------------------------------------------------------------------
const uint32_t UNKNOWN_ENABLE_COUNT = 0xFFFFFFFF;
namespace FUSESKUConfig {
struct FsConfig
{
    uint32_t minEnableCount = 0;
    uint32_t maxEnableCount = 0;
    uint32_t minEnablePerGroup = UNKNOWN_ENABLE_COUNT;
    uint32_t enableCountPerGroup = UNKNOWN_ENABLE_COUNT;
    uint32_t repairMinCount = 0;
    uint32_t repairMaxCount = 0;
    std::vector<uint32_t> mustEnableList;
    std::vector<uint32_t> mustDisableList;

    bool fsConfigIsPossible(std::string& msg) const;
    uint32_t getMinGroupCount() const;

    uint32_t maxUGPUImbalance = UNKNOWN_ENABLE_COUNT;
    std::vector<uint32_t> skyline;
};

struct SkuConfig
{
    uint32_t skuId = 0;           // chip SKU id, unique to the fuse file
    std::string SkuName;          // chip SKU's name
    std::map<std::string, FsConfig> fsSettings;
    std::map<std::string, int32_t> misc_params;

    bool skuIsPossible(std::string& msg) const;
};

}

/*
bool IsFloorsweepingValid(Chip whichChip, FbpMasks fbpSettings,
                          std::string& msg, FSmode whichMode);
bool IsFloorsweepingValid(Chip whichChip, GpcMasks gpcSettings,
                          std::string& msg, FSmode whichMode);
*/
bool IsFloorsweepingValid(Chip whichChip, const GpcMasks& gpcSettings,
                          const FbpMasks& fbpSettings, std::string& msg,
                          FSmode whichMode);
bool IsFloorsweepingValid(Chip whichChip, const GpcMasks& gpcSettings,
                          const FbpMasks& fbpSettings, std::string& msg,
                          FSmode whichMode, bool printUgpu, std::ofstream &outFile);

bool IsInSKU(Chip whichChip, const GpcMasks& gpcSettings,
                          const FbpMasks& fbpSettings, 
                          const FUSESKUConfig::SkuConfig& potentialSKU,
                          std::string& msg);

bool CanFitSKU(Chip whichChip, const GpcMasks& gpcSettings,
                          const FbpMasks& fbpSettings, 
                          const FUSESKUConfig::SkuConfig& potentialSKU,
                          std::string& msg);

bool FuncDownBin(Chip whichChip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings, 
                 GpcMasks& additionalGPCSettings, FbpMasks& additionalFBPSettings, std::string msg);

bool DownBin(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings, 
               const FUSESKUConfig::SkuConfig& sku, 
               GpcMasks& additionalGPCSettings, FbpMasks& additionalFBPSettings, std::string& msg);

bool DownBin(Chip chip, const GpcMasks& gpcSettings, const FbpMasks& fbpSettings,
               const FUSESKUConfig::SkuConfig& sku,
               GpcMasks& additionalGPCSettings, FbpMasks& additionalFBPSettings,
               GpcMasks& spareGPCsettings, FbpMasks& spareFBPSettings, std::string& msg);

}  // namespace fslib
#endif
