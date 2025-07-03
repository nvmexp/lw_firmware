#pragma once

#include "fs_chip_core.h"
#include <memory>


namespace fslib {

// return a pointer to a chip based on the Chip enum passed in
std::unique_ptr<FSChip_t> createChip(Chip chip);
void createChipContainer(Chip chip, FSChipContainer_t& chip_container);

}  // namespace fslib
