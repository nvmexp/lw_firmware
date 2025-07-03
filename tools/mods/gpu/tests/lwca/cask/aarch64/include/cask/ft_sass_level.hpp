#ifndef __FT_SASS_LEVEL_HPP__
#define __FT_SASS_LEVEL_HPP__

#include <array>
#include <cstdint>
#include <cstring>

#include "cask_core.h"
#include "ft_common_types.hpp"
#include "ft_common_colwolution_types.hpp"

#define SCI(X) static_cast<uint32_t>(X)

namespace cask {
namespace sasskr {

class ft_sass_level {
   public:
    enum level_t {
        LEVEL_SPA = 0,
        LEVEL_COLW,
        LEVEL_GEMM,
        LEVEL_VER,

        LEVEL_INP_TYPE,
#if defined(FT_AMPERE_TREE_LEVELS)
        LEVEL_INP_AM_TYPE,
#endif
        LEVEL_FLT_TYPE,
        LEVEL_OUT_TYPE,
        LEVEL_ACLWM_TYPE,
        LEVEL_BIAS_TYPE,

        LEVEL_INP_FMT,
        LEVEL_FLT_FMT,
        LEVEL_OUT_FMT,

        LEVEL_INP_VECT_DIM,
        LEVEL_FLT_VECT_DIM,
        LEVEL_OUT_VECT_DIM,

        LEVEL_INP_VECT_SCALARS,
        LEVEL_FLT_VECT_SCALARS,
        LEVEL_OUT_VECT_SCALARS,

        LEVEL_TNS,

        LEVEL_STRD_B,
        LEVEL_IDXING,
        LEVEL_SPLITK,

        LEVEL_BETA,

        LEVEL_EDGE,
        LEVEL_PAD,
        LEVEL_DIL,
        LEVEL_STRD,

        LEVEL_FILT_SZ,
        LEVEL_FILT_K_RES,
        LEVEL_INP_C_RES,

        LEVEL_GROUP,
        LEVEL_RELUBIAS,

        LEVEL_LDG_A,
        LEVEL_LDG_B,
        LEVEL_KBLOCK,

        LEVEL_DYNAMIC_RESIZE,
        LEVEL_BATCH,
#if defined(FT_AMPERE_TREE_LEVELS)
        LEVEL_ABI,
#endif

        LEVEL_SBUF,
        LEVEL_SLICE,
        LEVEL_STAGES,
        LEVEL_ND,

        LEVEL_LEAF /* always needed */,
        _LEVEL_COUNT_ /* always needed */
    };

    static const uint32_t LEVEL_SPA_COUNT  = SCI(kr::SPA_COUNT);
    static const uint32_t LEVEL_COLW_COUNT = SCI(krcc::COLW_COUNT);
    static const uint32_t LEVEL_GEMM_COUNT = SCI(GEMM_COUNT);
    static const uint32_t LEVEL_VER_COUNT  = SCI(kr::VER_COUNT);

    static const uint32_t LEVEL_INP_TYPE_COUNT = SCI(kr::DATATYPE_COUNT);
#if defined(FT_AMPERE_TREE_LEVELS)
    static const uint32_t LEVEL_INP_AM_TYPE_COUNT = SCI(kr::DATATYPE_COUNT);
#endif
    static const uint32_t LEVEL_FLT_TYPE_COUNT   = SCI(kr::DATATYPE_COUNT);
    static const uint32_t LEVEL_OUT_TYPE_COUNT   = SCI(kr::DATATYPE_COUNT);
    static const uint32_t LEVEL_ACLWM_TYPE_COUNT = SCI(kr::DATATYPE_COUNT);
    static const uint32_t LEVEL_BIAS_TYPE_COUNT  = SCI(kr::DATATYPE_COUNT);

    static const uint32_t LEVEL_INP_FMT_COUNT = SCI(krcc::LAYOUT_COUNT);
    static const uint32_t LEVEL_FLT_FMT_COUNT = SCI(krcc::LAYOUT_COUNT);
    static const uint32_t LEVEL_OUT_FMT_COUNT = SCI(krcc::LAYOUT_COUNT);

    static const uint32_t LEVEL_INP_VECT_DIM_COUNT = SCI(krcc::VECT_COUNT);
    static const uint32_t LEVEL_FLT_VECT_DIM_COUNT = SCI(krcc::VECT_COUNT);
    static const uint32_t LEVEL_OUT_VECT_DIM_COUNT = SCI(krcc::VECT_COUNT);

    static const uint32_t LEVEL_INP_VECT_SCALARS_COUNT = SCI(krcc::VECT_SCALARS_COUNT);
    static const uint32_t LEVEL_FLT_VECT_SCALARS_COUNT = SCI(krcc::VECT_SCALARS_COUNT);
    static const uint32_t LEVEL_OUT_VECT_SCALARS_COUNT = SCI(krcc::VECT_SCALARS_COUNT);

    static const uint32_t LEVEL_TNS_COUNT = SCI(kr::TENSOR_COUNT);

    static const uint32_t LEVEL_STRD_B_COUNT = SCI(STRIDED_B_COUNT);
    static const uint32_t LEVEL_IDXING_COUNT = SCI(krcc::INDEXING_COUNT);
    static const uint32_t LEVEL_SPLITK_COUNT = SCI(SPLITK_COUNT);

    static const uint32_t LEVEL_BETA_COUNT = SCI(BETA_COUNT);

    static const uint32_t LEVEL_EDGE_COUNT = SCI(EDGE_COUNT);
    static const uint32_t LEVEL_PAD_COUNT  = SCI(krcc::PAD_COUNT);
    static const uint32_t LEVEL_DIL_COUNT  = SCI(DILATION_COUNT);
    static const uint32_t LEVEL_STRD_COUNT = SCI(krcc::STRIDE_COUNT);

    static const uint32_t LEVEL_FILT_SZ_COUNT    = SCI(krcc::FILT_SZ_COUNT);
    static const uint32_t LEVEL_FILT_K_RES_COUNT = SCI(krcc::FILTER_K_COUNT);
    static const uint32_t LEVEL_INP_C_RES_COUNT  = SCI(krcc::INPUT_C_COUNT);

    static const uint32_t LEVEL_GROUP_COUNT    = SCI(krcc::GROUP_SUPPORT_COUNT);
    static const uint32_t LEVEL_RELUBIAS_COUNT = SCI(kr::RELUBIAS_COUNT);

    static const uint32_t LEVEL_LDG_A_COUNT  = SCI(kr::LOG2_LDGA_COUNT);
    static const uint32_t LEVEL_LDG_B_COUNT  = SCI(kr::LOG2_LDGB_COUNT);
    static const uint32_t LEVEL_KBLOCK_COUNT = SCI(krcc::KBLOCK_COUNT);

    static const uint32_t LEVEL_DYNAMIC_RESIZE_COUNT = SCI(DYNAMIC_RESIZE_COUNT);
    static const uint32_t LEVEL_BATCH_COUNT          = SCI(BATCH_COUNT);
#if defined(FT_AMPERE_TREE_LEVELS)
    static const uint32_t LEVEL_ABI_COUNT = SCI(kr::ABI_COUNT);
#endif

    static const uint32_t LEVEL_SBUF_COUNT   = SCI(BUF_COUNT);
    static const uint32_t LEVEL_SLICE_COUNT  = SCI(SLICED_COUNT);
    static const uint32_t LEVEL_STAGES_COUNT = SCI(krcc::STAGES_COUNT);
    static const uint32_t LEVEL_ND_COUNT     = SCI(ND_COUNT);

    static const uint32_t LEVEL_LEAF_COUNT = SCI(kr::LEAF_COUNT);
    static const uint32_t LEVEL_COUNT      = SCI(_LEVEL_COUNT_);

    static const uint32_t level_sizes[LEVEL_COUNT];
    static const char *level_names[LEVEL_COUNT];
    static const char *const *level_strings[LEVEL_COUNT];

    static const uint32_t LEVEL_MAX_ENUM_COUNT = 16;

    struct level_selection_t {
        uint32_t selectors[static_cast<size_t>(LEVEL_COUNT)];
    };

    struct level_vector_t {
        level_selection_t sel;
        level_t level;
    };

    struct search_t {
        uint32_t selectors[static_cast<size_t>(LEVEL_COUNT)][static_cast<size_t>(LEVEL_MAX_ENUM_COUNT)];
        uint32_t selector_count[static_cast<size_t>(LEVEL_COUNT)];
        bool valid;
    };

    union branch_s {
        // all the interior types of sass_kernel_record_t -- TODO: somehow avoid the
        // copy & paste.
        kr::spa_t spa;
        krcc::colw_t colw;
        gemm_t gemm;
        kr::version_t ver;

        kr::datatype_t inp;
#if defined(FT_AMPERE_TREE_LEVELS)
        kr::datatype_t inp_am;
#endif
        kr::datatype_t flt;
        kr::datatype_t out;
        kr::datatype_t aclwm;
        kr::datatype_t bias;

        krcc::layout_t inp_fmt;
        krcc::layout_t flt_fmt;
        krcc::layout_t out_fmt;

        krcc::vect_t inp_vect;
        krcc::vect_t flt_vect;
        krcc::vect_t out_vect;

        krcc::vect_scalars_t inp_vect_scalars;
        krcc::vect_scalars_t flt_vect_scalars;
        krcc::vect_scalars_t out_vect_scalars;

        kr::tensor_t tns;

        strided_b_t strd_b;
        krcc::idxing_t idxing;
        split_k_t splitk;

        beta_t beta;

        edge_t edge;
        krcc::pad_t pad;
        dilation_t dil;
        krcc::stride_t strd;

        krcc::filter_sz_t filt_sz;
        krcc::filter_k_residue_t filt_k_res;
        krcc::input_c_residue_t inp_c_res;

        krcc::group_support_t gp;
        kr::relubias_support_t rb;

        kr::ldga_t ldga;
        kr::ldgb_t ldgb;
        krcc::kblock_t kb;

        dynamic_resize_t dr;
        batch_n_residue_t bn;
#if defined(FT_AMPERE_TREE_LEVELS)
        kr::abi_ver_t abi;
#endif
        sbuf_t buf;
        sliced_t slc;
        krcc::stages_t stage;
        nd_support_t nd;
    };

    struct branch_t {
        level_t tag;  // controls a switch statement
        branch_s val;
    };

    static Error
    nextLevel(level_t lwr, level_t *next);
    static Error
    prevLevel(level_t lwr, level_t *prev);

    static level_t
    levelBegin() {
        return static_cast<level_t>(0);
    }
    static bool
    isLevelValid(level_t level) {
        return ((static_cast<uint32_t>(level) < static_cast<uint32_t>(_LEVEL_COUNT_)));
    }
    static bool
    isLevelInterior(level_t level) {
        return ((_LEVEL_COUNT_ != level) && (LEVEL_LEAF != level));
    }

    static Error
    initLevelVector(const kernel_record_t *p, level_vector_t *lv);
    static Error
    branchToIdx(const branch_t *b, uint32_t *bidx);
    static Error
    idxToBranch(branch_t *bch, uint32_t bidx);

    static void
    printSearch(const search_t *sch);
};

}  // sasskr
}  // cask

#undef SCI
#endif
