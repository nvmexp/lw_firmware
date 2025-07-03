 /* Copyright 1993-2016 LWPU Corporation.  All rights reserved.
  *
  * NOTICE TO LICENSEE:
  *
  * The source code and/or documentation ("Licensed Deliverables") are
  * subject to LWPU intellectual property rights under U.S. and
  * international Copyright laws.
  *
  * The Licensed Deliverables contained herein are PROPRIETARY and
  * CONFIDENTIAL to LWPU and are being provided under the terms and
  * conditions of a form of LWPU software license agreement by and
  * between LWPU and Licensee ("License Agreement") or electronically
  * accepted by Licensee.  Notwithstanding any terms or conditions to
  * the contrary in the License Agreement, reproduction or disclosure
  * of the Licensed Deliverables to any third party without the express
  * written consent of LWPU is prohibited.
  *
  * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
  * LICENSE AGREEMENT, LWPU MAKES NO REPRESENTATION ABOUT THE
  * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  THEY ARE
  * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.
  * LWPU DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED
  * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY,
  * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
  * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
  * LICENSE AGREEMENT, IN NO EVENT SHALL LWPU BE LIABLE FOR ANY
  * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
  * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
  * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
  * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
  * OF THESE LICENSED DELIVERABLES.
  *
  * U.S. Government End Users.  These Licensed Deliverables are a
  * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT
  * 1995), consisting of "commercial computer software" and "commercial
  * computer software documentation" as such terms are used in 48
  * C.F.R. 12.212 (SEPT 1995) and are provided to the U.S. Government
  * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and
  * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
  * U.S. Government End Users acquire the Licensed Deliverables with
  * only those rights set forth herein.
  *
  * Any use of the Licensed Deliverables in individual and commercial
  * software must include, in the user documentation and internal
  * comments to the code, the above Disclaimer and U.S. Government End
  * Users Notice.
  */

#ifndef _CG_DRIVER_API_H
#define _CG_DRIVER_API_H

#include "info.h"

_CG_BEGIN_NAMESPACE

namespace details {
    template <unsigned int RegId>
    _CG_QUALIFIER unsigned int load_elw_reg() {
        // Abort by default
        _CG_ABORT();
        return 0;
    }

    template <unsigned int HiReg, unsigned int LoReg>
    _CG_QUALIFIER unsigned long long load_elw_reg64() {
        unsigned long long registerLo = load_elw_reg<LoReg>();
        unsigned long long registerHi = load_elw_reg<HiReg>();

        return (registerHi << 32) | registerLo;
    }

// inline PTX for accessing registers requires an immediate for the special reg
# define LOAD_ELWREG(NUMBER) \
    template <> _CG_QUALIFIER unsigned int load_elw_reg<NUMBER>() { \
        unsigned int r; \
        asm ("mov.u32 %0, %%elwreg" #NUMBER ";" : "=r"(r)); \
        return r; \
    }

    // Instantiate loaders for registers used
    LOAD_ELWREG(0);
    LOAD_ELWREG(1);
    LOAD_ELWREG(2);
# undef LOAD_ELWREG

    struct grid_workspace {
        unsigned int wsSize;
        unsigned int barrier;
    };

    _CG_QUALIFIER grid_workspace* get_grid_workspace() {
        unsigned long long gridWsAbiAddress = load_elw_reg64<1, 2>();
        // Interpret the address from elwreg 1 and 2 as the driver's grid workspace
        return (reinterpret_cast<grid_workspace*>(gridWsAbiAddress));
    }
}
_CG_END_NAMESPACE

#endif // _CG_DRIVER_API_H
