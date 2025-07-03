#ifndef SAFE_INCLUDE_GUARD_SHARED_TYPES_H
#define SAFE_INCLUDE_GUARD_SHARED_TYPES_H

// enable doxygen:
//! @file

#include "cask/safe/preprocess.h"

namespace cask_safe {

enum struct PreferentialBool : uint8_t {
    Auto,     //!< Allow CASK to determine whether to enable the feature.
    Disabled, //!< Disable use of the feature.
    Enabled,  //!< The feature must be enabled. If the kernel does not support
              //!< linked feature, the flow will return an @ref Error at some point.
};

} // cask_safe

#endif // SAFE_INCLUDE_GUARD_SHARED_TYPES_H
