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

#ifndef _CG_COALESCED_REDUCE_H_
#define _CG_COALESCED_REDUCE_H_

#include "info.h"
#include "helpers.h"
#include "cooperative_groups.h"
#include "partitioning.h"
#include "coalesced_scan.h"

_CG_BEGIN_NAMESPACE

namespace details {

template <typename TyVal, typename TyOp>
_CG_QUALIFIER auto coalesced_reduce_to_one(const coalesced_group& group, TyVal&& val, TyOp&& op) -> decltype(op(val, val)) {
    if (group.size() == 32) {
        auto out = val;
        for (int offset = group.size() >> 1; offset > 0; offset >>= 1) {
            out = op(out, group.shfl_up(out, offset));
        }
        return out;
    }
    else {
        auto scan_result =
            inclusive_scan_non_contiguous(group, _CG_STL_NAMESPACE::forward<TyVal>(val), _CG_STL_NAMESPACE::forward<TyOp>(op));
        return scan_result;
    }
}

template <typename TyVal, typename TyOp>
_CG_QUALIFIER auto coalesced_reduce(const coalesced_group& group, TyVal&& val, TyOp&& op) -> decltype(op(val, val)) {
    auto out = coalesced_reduce_to_one(group, _CG_STL_NAMESPACE::forward<TyVal>(val), _CG_STL_NAMESPACE::forward<TyOp>(op));
    if (group.size() == 32) {
        return group.shfl(out, 31);
    }
    else {
        unsigned int group_mask = _coalesced_group_data_access::get_mask(group);
        unsigned int last_thread_id = 31 - __clz(group_mask);
        return details::tile::shuffle_dispatch<TyVal>::shfl(
            _CG_STL_NAMESPACE::forward<TyVal>(out), group_mask, last_thread_id, 32);
    }
}

template <typename TyVal, typename TyOp, unsigned int TySize, typename ParentT>
_CG_QUALIFIER auto coalesced_reduce(const __single_warp_thread_block_tile<TySize, ParentT>& group, 
                                    TyVal&& val,
                                    TyOp&& op) -> decltype(op(val, val)) {
    auto out = val;
    for (int mask = TySize >> 1; mask > 0; mask >>= 1) {
        out = op(out, group.shfl_xor(out, mask));
    }

    return out;
}

} // details

_CG_END_NAMESPACE

#endif // _CG_COALESCED_REDUCE_H_
