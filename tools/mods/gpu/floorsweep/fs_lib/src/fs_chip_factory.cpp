#include "fs_chip_factory.h"

#include "fs_chip_hopper.h"
#include "fs_chip_ada.h"

namespace fslib {

//-----------------------------------------------------------------------------
// MISC FUNCTIONS
//-----------------------------------------------------------------------------

/**
 * @brief Helper factory function for createChip()
 * 
 * @param chip 
 * @return std::unique_ptr<FSChip_t> 
 */
static std::unique_ptr<FSChip_t> createChipHelper(Chip chip){
    switch(chip) {
        case Chip::GH100: return std::make_unique<GH100_t>();
        case Chip::AD102: return std::make_unique<AD102_t>();
        case Chip::AD103: return std::make_unique<AD103_t>();
        case Chip::AD104: return std::make_unique<AD104_t>();
        case Chip::AD106: return std::make_unique<AD106_t>();
        case Chip::AD107: return std::make_unique<AD107_t>();
        default: {
            return nullptr;
        }
    }
}

/**
 * @brief This factory function creates a chip instance of the correct class
 * based on the enum. Returns nullptr if the chip is not supported
 * @param chip 
 * @return std::unique_ptr<FSChip_t> 
 */
std::unique_ptr<FSChip_t> createChip(Chip chip){
    std::unique_ptr<FSChip_t> chip_ptr = createChipHelper(chip);
    if (chip_ptr == nullptr){
        return nullptr;
    }
    chip_ptr->setup();
    return chip_ptr;
}

/**
 * @brief Initialize a chip container object with the specified chip type
 * 
 * @param chip 
 * @param chip_container 
 */
void createChipContainer(Chip chip, FSChipContainer_t& chip_container){
    switch(chip) {
        case Chip::GH100: chip_container.createModule<GH100_t>(); break;
        case Chip::AD102: chip_container.createModule<AD102_t>(); break;
        case Chip::AD103: chip_container.createModule<AD103_t>(); break;
        case Chip::AD104: chip_container.createModule<AD104_t>(); break;
        case Chip::AD106: chip_container.createModule<AD106_t>(); break;
        case Chip::AD107: chip_container.createModule<AD107_t>(); break;
        default: {
            chip_container.reset();
            return;
        }
    }
    chip_container->setup();
}

}  // namespace fslib
