#ifndef __FT_SASS_GEMM_LEVEL_HPP__
#define __FT_SASS_GEMM_LEVEL_HPP__

#include <array>
#include <cstdint>
#include <cstring>

#include "cask_core.h"
#include "ft_common_types.hpp"

#define SCI(X) static_cast<uint32_t>(X)

namespace cask {
namespace sass_gemm_kr {

class ft_sass_gemm_level {
   public:
    enum level_t {
        LEVEL_SPA = 0,
        LEVEL_VER,

        LEVEL_A_TYPE,
        LEVEL_B_TYPE,
        LEVEL_C_TYPE,
        LEVEL_ACLWM_TYPE,
        LEVEL_BIAS_TYPE,

        LEVEL_A_LAYOUT,
        LEVEL_B_LAYOUT,
        LEVEL_C_LAYOUT,

        LEVEL_SHAPE,
        LEVEL_SLICE,
        LEVEL_STAGE,

        LEVEL_TNS,
        LEVEL_N_RESIDUE,
        LEVEL_RELUBIAS,

        LEVEL_LDG_A,
        LEVEL_LDG_B,

        LEVEL_LEAF,
        _LEVEL_COUNT_
    };

    static const uint32_t LEVEL_SPA_COUNT = SCI(kr::spa_t::SPA_COUNT);
    static const uint32_t LEVEL_VER_COUNT = SCI(kr::version_t::VER_COUNT);

    static const uint32_t LEVEL_A_TYPE_COUNT     = SCI(kr::datatype_t::DATATYPE_COUNT);
    static const uint32_t LEVEL_B_TYPE_COUNT     = SCI(kr::datatype_t::DATATYPE_COUNT);
    static const uint32_t LEVEL_C_TYPE_COUNT     = SCI(kr::datatype_t::DATATYPE_COUNT);
    static const uint32_t LEVEL_ACLWM_TYPE_COUNT = SCI(kr::datatype_t::DATATYPE_COUNT);
    static const uint32_t LEVEL_BIAS_TYPE_COUNT  = SCI(kr::datatype_t::DATATYPE_COUNT);

    static const uint32_t LEVEL_A_LAYOUT_COUNT = SCI(layout_t::LAYOUT_COUNT);
    static const uint32_t LEVEL_B_LAYOUT_COUNT = SCI(layout_t::LAYOUT_COUNT);
    static const uint32_t LEVEL_C_LAYOUT_COUNT = SCI(layout_t::LAYOUT_COUNT);

    static const uint32_t LEVEL_SHAPE_COUNT = SCI(shape_t::SHAPE_COUNT);
    static const uint32_t LEVEL_SLICE_COUNT = SCI(slice_t::SLICE_COUNT);
    static const uint32_t LEVEL_STAGE_COUNT = SCI(stage_t::STAGE_COUNT);

    static const uint32_t LEVEL_TNS_COUNT       = SCI(kr::tensor_t::TENSOR_COUNT);
    static const uint32_t LEVEL_N_RESIDUE_COUNT = SCI(n_residue_t::N_RESIDUE_COUNT);
    static const uint32_t LEVEL_RELUBIAS_COUNT  = SCI(kr::relubias_support_t::RELUBIAS_COUNT);

    static const uint32_t LEVEL_LDG_A_COUNT = SCI(kr::ldga_t::LOG2_LDGA_COUNT);
    static const uint32_t LEVEL_LDG_B_COUNT = SCI(kr::ldgb_t::LOG2_LDGB_COUNT);
    static const uint32_t LEVEL_LEAF_COUNT  = SCI(kr::leaf_t::LEAF_COUNT);
    static const uint32_t LEVEL_COUNT       = SCI(_LEVEL_COUNT_);

    static constexpr uint32_t level_sizes[LEVEL_COUNT] = {
        LEVEL_SPA_COUNT,        LEVEL_VER_COUNT,

        LEVEL_A_TYPE_COUNT,     LEVEL_B_TYPE_COUNT,    LEVEL_C_TYPE_COUNT,
        LEVEL_ACLWM_TYPE_COUNT, LEVEL_BIAS_TYPE_COUNT,

        LEVEL_A_LAYOUT_COUNT,   LEVEL_B_LAYOUT_COUNT,  LEVEL_C_LAYOUT_COUNT,

        LEVEL_SHAPE_COUNT,      LEVEL_SLICE_COUNT,     LEVEL_STAGE_COUNT,

        LEVEL_TNS_COUNT,        LEVEL_N_RESIDUE_COUNT, LEVEL_RELUBIAS_COUNT,

        LEVEL_LDG_A_COUNT,      LEVEL_LDG_B_COUNT,     LEVEL_LEAF_COUNT,
    };

    static constexpr const char *level_names[LEVEL_COUNT] = {"SPA",
                                                             "VER",
                                                             "A_TYPE",
                                                             "B_TYPE",
                                                             "C_TYPE",
                                                             "ACLWM_TYPE",
                                                             "BIAS_TYPE",
                                                             "A_LAYOUT",
                                                             "B_LAYOUT",
                                                             "C_LAYOUT",
                                                             "SHAPE",
                                                             "SLICE",
                                                             "STAGE",
                                                             "TENSOR",
                                                             "N_RESIDUE",
                                                             "RELU",
                                                             "LDGA",
                                                             "LDGB",
                                                             "LEAF" /* unusued */};

    static constexpr const char *const *level_strings[LEVEL_COUNT] = {kr::spa_strings,
                                                                      kr::version_strings,
                                                                      kr::datatype_strings,
                                                                      kr::datatype_strings,
                                                                      kr::datatype_strings,
                                                                      kr::datatype_strings,
                                                                      kr::datatype_strings,

                                                                      layout_strings,
                                                                      layout_strings,
                                                                      layout_strings,
                                                                      shape_strings,
                                                                      slice_strings,
                                                                      stage_strings,
                                                                      kr::tensor_strings,
                                                                      n_residue_strings,
                                                                      kr::relubias_support_strings,
                                                                      kr::ldga_strings,
                                                                      kr::ldgb_strings,
                                                                      kr::leaf_strings /* unused */};

    static const uint32_t LEVEL_MAX_ENUM_COUNT = 16;

    struct level_selection_t {
        std::array<uint32_t, static_cast<size_t>(LEVEL_COUNT)> selectors;
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
        // all the interior types of sass_gemm_kernel_record_t -- TODO: somehow
        // avoid the copy & paste.
        kr::spa_t spa;
        kr::version_t ver;

        kr::datatype_t a;
        kr::datatype_t b;
        kr::datatype_t c;
        kr::datatype_t aclwm;
        kr::datatype_t bias;

        layout_t a_lay;
        layout_t b_lay;
        layout_t c_lay;

        shape_t shp;
        slice_t slc;
        stage_t stg;

        kr::tensor_t tns;
        n_residue_t n_res;
        kr::relubias_support_t rb;

        kr::ldga_t ldga;
        kr::ldgb_t ldgb;
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
        return (static_cast<uint32_t>(level) < static_cast<uint32_t>(_LEVEL_COUNT_));
    }
    static bool
    isLevelInterior(level_t level) {
        return (_LEVEL_COUNT_ != level) && (LEVEL_LEAF != level);
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

}  // sass_gemm_kr
}  // cask

#undef SCI

#endif
