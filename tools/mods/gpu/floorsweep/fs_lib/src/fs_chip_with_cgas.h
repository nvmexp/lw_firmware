#pragma once

#include "fs_chip_core.h"
#include "SkylineSpareSelector.h"
namespace fslib {

class ChipWithCGAs_t : public virtual FSChip_t {

    public:
	virtual bool skylineFits(const std::vector<uint32_t>& skyline, std::string& msg) const;
	module_index_list_t sortGPCsByTPCSkylineMargin(const std::vector<uint32_t>& skyline) const;
	virtual bool getTiePreferenceGPCFS(const const_flat_module_list_t& gpc_list, int32_t left, int32_t right) const = 0;
	module_index_list_t getPreferredTPCsToDisableForSkyline(const FUSESKUConfig::SkuConfig& sku) const;
	module_index_list_t getPreferredGPCsByTPCSkylineMarginFilter(const std::vector<uint32_t>& skyline) const;
	bool screenSkylineSparesImpl(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
	std::vector<uint32_t> getOptimalTPCDownBinForSparesWrap(const FUSESKUConfig::SkuConfig& potentialSKU, uint32_t repairMaxCount, std::string& msg) const;
	std::vector<uint32_t> getOptimalTPCDownBinForSparesWrap(const FUSESKUConfig::SkuConfig& potentialSKU, const std::vector<uint32_t>& skyline, const std::vector<uint32_t>& aliveTPCs, uint32_t repairMaxCount, std::string& msg) const;
	bool canFitSkylineGPC0Screen(const FUSESKUConfig::SkuConfig& potentialSKU, std::string& msg) const;
	bool pickSpareTPC(const FUSESKUConfig::SkuConfig& potentialSKU, int32_t& gpc_idx, int32_t& tpc_idx) const;
	void pickSpareTPCs(const FUSESKUConfig::SkuConfig& potentialSKU, uint32_t num_spares, fslib::GpcMasks& spare_tpc_masks);
};

}