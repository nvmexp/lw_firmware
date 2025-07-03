#ifndef __FT_KERNEL_RECORD_HPP__
#define __FT_KERNEL_RECORD_HPP__
#include <cassert>
#define FT_ASSERT assert
#define FT_RETURN_ERROR_IF_FALSE(C)        \
    do {                                   \
        if (!(C)) {                        \
            return Error::FUNCTIONAL_TREE; \
        }                                  \
    } while (0)

#define FT_RETURN_IF_FALSE(C) \
    do {                      \
        if (!(C)) {           \
            return;           \
        }                     \
    } while (0)

#define FT_RETURN_FALSE_IF_FALSE(C) \
    do {                            \
        if (!(C)) {                 \
            return false;           \
        }                           \
    } while (0)

#include "ft_common_types.hpp"
#include "ft_gemmcolw1x1_kernel_record.hpp"
#include "ft_sass_gemm_kernel_record.hpp"
#include "ft_sass_kernel_record.hpp"
#include "ft_xmma_kernel_record.hpp"
#include "ft_first_layer_kernel_record.hpp"
#include "ft_lwtlass_kernel_record.hpp"

namespace cask {
struct kernel_record_t {
    record_type_t tag;
    bool valid;
    union {
        sasskr::sass_kernel_record_t skr;
        xmmakr::xmma_kernel_record_t xkr;
        flkr::fl_kernel_record_t fkr;
        lwtkr::lwtlass_kernel_record_t ckr;
        sass_gemm_kr::sass_gemm_kernel_record_t sgkr;
    } val;
};

const char *
get_kernel_record_name(const kernel_record_t *kr);
void
print_kernel_record(const kernel_record_t *kr);
void
print_spa(kr::spa_t v);
void
print_datatype(kr::datatype_t v);
void
print_version(kr::version_t v);
void
print_tensor(kr::tensor_t v);
void
print_relubias(kr::relubias_support_t v);
void
print_ldga(kr::ldga_t v);
void
print_ldbg(kr::ldgb_t v);
#if defined(FT_AMPERE_TREE_LEVELS)
void
print_abi(kr::abi_ver_t v);
#endif
void
print_sass_kernel_record(const cask::kernel_record_t *kr);
void
print_xmma_kernel_record(const cask::kernel_record_t *kr);
void
print_first_layer_kernel_record(const cask::kernel_record_t *kr);
void
print_lwtlass_kernel_record(const cask::kernel_record_t *kr);
void
print_sass_gemm_kernel_record(const cask::kernel_record_t *kr);
}  // namespace cask
#endif
