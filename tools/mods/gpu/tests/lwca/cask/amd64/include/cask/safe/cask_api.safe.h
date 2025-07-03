#ifndef SAFE_INCLUDE_GUARD_CASK_API_SAFE_H
#define SAFE_INCLUDE_GUARD_CASK_API_SAFE_H

#include "cask/safe/preprocess.h"

#include "cask/safe/platform.h"
#include "cask/safe/run_info.h"
#include "cask/safe/cask_runner.h"
#include "cask/safe/convolution_runner.h"
#include "cask/safe/deconvolution_runner.h"
#include "cask/safe/pooling_runner.h"
#include "cask/safe/softmax_runner.h"

namespace cask_safe {

void InitCaskSafe() noexcept;

}

#endif // SAFE_INCLUDE_GUARD_CASK_API_SAFE_H
