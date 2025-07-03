#ifndef __FT_XMMA_LEVEL_HPP__
#define __FT_XMMA_LEVEL_HPP__

#if __cplusplus >= 201103L
#include <array>
#endif

#include <stdint.h>
#include <cstring>

#include "cask_core.h"
#include "ft_common_types.hpp"
#include "ft_common_colwolution_types.hpp"

#define SCI(X) static_cast<uint32_t>(X)

namespace cask {
namespace xmmakr {

class ft_xmma_level {
   public:
    enum level_t {
        LEVEL_SPA = 0,
        LEVEL_COLW,

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

        LEVEL_IDXING,
        LEVEL_INTERLEAVED,
        LEVEL_CTA_SPLITK,

        LEVEL_FILT_K_RES,
        LEVEL_INP_C_RES,

        LEVEL_KBLOCK,

        LEVEL_GROUP,
        LEVEL_RELUBIAS,
        LEVEL_STAGES,

#if defined(FT_AMPERE_TREE_LEVELS)
        LEVEL_ABI,
#endif

        LEVEL_LEAF /* always needed */,
        _LEVEL_COUNT_ /* always needed */
    };

    static const uint32_t LEVEL_SPA_COUNT  = SCI(kr::SPA_COUNT);
    static const uint32_t LEVEL_COLW_COUNT = SCI(krcc::COLW_COUNT);

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

    static const uint32_t LEVEL_IDXING_COUNT      = SCI(krcc::INDEXING_COUNT);
    static const uint32_t LEVEL_INTERLEAVED_COUNT = SCI(INTERLEAVED_COUNT);
    static const uint32_t LEVEL_CTA_SPLITK_COUNT  = SCI(CTA_SPLITK_COUNT);

    static const uint32_t LEVEL_FILT_K_RES_COUNT = SCI(krcc::FILTER_K_COUNT);
    static const uint32_t LEVEL_INP_C_RES_COUNT  = SCI(krcc::INPUT_C_COUNT);

    static const uint32_t LEVEL_KBLOCK_COUNT = SCI(krcc::KBLOCK_COUNT);

    static const uint32_t LEVEL_GROUP_COUNT    = SCI(krcc::GROUP_SUPPORT_COUNT);
    static const uint32_t LEVEL_RELUBIAS_COUNT = SCI(kr::RELUBIAS_COUNT);
    static const uint32_t LEVEL_STAGES_COUNT   = SCI(krcc::STAGES_COUNT);

#if defined(FT_AMPERE_TREE_LEVELS)
    static const uint32_t LEVEL_ABI_COUNT = SCI(kr::ABI_COUNT);
#endif

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
        kr::spa_t spa;
        krcc::colw_t colw;

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

        krcc::idxing_t idxing;
        interleaved_t ilv;
        cta_splitk_t csk;

        krcc::filter_k_residue_t filt_k_res;
        krcc::input_c_residue_t inp_c_res;
        krcc::kblock_t kb;

        krcc::group_support_t gp;
        kr::relubias_support_t rb;
        krcc::stages_t stage;

#if defined(FT_AMPERE_TREE_LEVELS)
        kr::abi_ver_t abi;
#endif
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

}  // xmmakr
}  // cask

#endif
