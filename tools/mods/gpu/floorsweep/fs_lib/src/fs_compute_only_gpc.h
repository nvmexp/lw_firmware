#pragma once

#include "fs_chip_core.h"

namespace fslib {

class ChipWithComputeOnlyGPCs : public virtual FSChip_t {

	public:
	virtual uint32_t getNumEnabledGfxTPCs() const = 0;
	bool hasEnoughGfxTPCs(const FUSESKUConfig::SkuConfig& sku, std::string& msg) const;

};

const static std::string min_gfx_tpcs_str{ "min_gfx_tpcs" };

}