#pragma once

#include "fs_chip_core.h"
#include "fs_chip_with_cgas.h"
#include "fs_compute_only_gpc.h"
#ifdef _MSC_VER
#pragma warning(disable : 4250) // disable warning for "inherits via dominance"
#endif

namespace fslib {

class GHLit1FBP_t : public HBMFBP_t {
public:
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual bool ltcIsValid(FSmode fsmode, uint32_t ltc_idx, std::string& msg) const;
    virtual void funcDownBin();
};

class GHLit1GPC_t : public GPCWithCPC_t {
public:
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
};

// GH100 GPC. Specific rules can be defined in the isValid function
class GHLit1GPC0_t : public GHLit1GPC_t, public GPCWithGFX_t {
    virtual bool isValid(FSmode fsmode, std::string& msg) const;
    virtual void funcDownBin();
    virtual void setup(const chipPOR_settings_t& settings);
    virtual void applyMask(const fslib::GpcMasks& mask, uint32_t index);
    virtual void getMask(fslib::GpcMasks& mask, uint32_t index) const;
    virtual const module_map_t& getModuleMapRef() const;
    virtual const module_set_t& getSubModuleTypes() const;
    virtual module_index_list_t getCPCIdxsByTPCCount() const;
    virtual module_index_list_t getPreferredDisableListTPC(const FUSESKUConfig::SkuConfig& potentialSKU) const;
};

class GHLit1Chip_t : public GPUWithuGPU_t, public ChipWithCGAs_t, public ChipWithComputeOnlyGPCs {
public:
    virtual void getGPCType(GPCHandle_t& handle) const;
    virtual void getFBPType(FBPHandle_t& handle) const;
};

class GH100_t : public GHLit1Chip_t {
   public:
    virtual const chip_settings_t& getChipSettings() const;
    bool enabledSliceCountIsValid(FSmode fsmode, std::string& msg) const;
    virtual void instanceSubModules();
    virtual module_index_list_t getFSTiePreference(module_type_id module_type) const;
    virtual module_index_list_t getPreferredDisableList(module_type_id module_type, const FUSESKUConfig::SkuConfig& potentialSKU) const;
    void preferWholes(module_index_list_t& fbp_list) const;
    void sortPreferredMidLevelModules(const_flat_module_list_t& node_list, const std::vector<module_type_id>& priority) const;
    void preferWholeSitesSubFBP(const_flat_module_list_t& node_list, const std::vector<module_type_id>& priority) const;
    virtual bool enableCountIsValid(module_type_id submodule_type, FSmode fsmode, std::string& msg) const;
    virtual int compareConfig(const FSChip_t* candidate) const;
    uint32_t getHBMEnableCount() const;
    virtual void applyDownBinHeuristic(const FUSESKUConfig::SkuConfig& potentialSKU);
    virtual const std::vector<uint32_t>& getUGPUGPCMap() const;
    virtual std::unique_ptr<FSChip_t> canFitSKUSearch(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual bool downBin(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg);
    virtual bool isInSKU(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual bool skuIsPossible(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
    virtual bool getTiePreferenceGPCFS(const const_flat_module_list_t& gpc_list, int32_t left, int32_t right) const;
    virtual uint32_t getNumEnabledGfxTPCs() const;
};

}  // namespace fslib



