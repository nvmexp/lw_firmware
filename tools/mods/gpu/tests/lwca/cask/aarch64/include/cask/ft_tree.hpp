#ifndef __FT_TREE_HPP__
#define __FT_TREE_HPP__

#include <cstring>
#include <vector>

#include "ft_nary_node.hpp"

#define CASK_UNUSED(expr)  \
    do {              \
        (void)(expr); \
    } while (0)

namespace cask {
namespace ft {

// level , (cask::Colwolution, cask::ColwShader, cask::ColwShaderList) or (cask::ColwolutionWgrad,
// cask::ColwWgradShader, cask::ColwWgradShaderList)

template <typename T, typename U, typename V, typename W>
class subTree {
    typedef typename T::level_t level_t;
    typedef typename T::level_selection_t level_selection_t;
    typedef typename T::branch_t branch_t;
    typedef typename T::search_t search_t;

   private:
    naryNode<T> *root;

    size_t num_insertions;

    bool
    isFullyExplored(naryNode<T> **_q,
                    uint32_t (&sel_lwr_idx)[T::LEVEL_COUNT],
                    level_t *_level_lwr,
                    const search_t *sch);

    template <size_t N>
    bool
    isLastSelectorIdx(const search_t *sch, uint32_t (&sel_lwr_idx)[N], level_t lvl);

    struct print_t {
        bool terminals[T::LEVEL_COUNT];
    };

    void
    printTreeRelwrsive(level_t level, uint32_t branch_idx, naryNode<T> *q, print_t *pp) const;

    // forcibly disable
    subTree(const subTree &rhs) { CASK_UNUSED(rhs); };

   public:
    subTree();
    ~subTree();

    size_t
    getNumInsertions() const;

    Error
    insert(const cask::kernel_record_t &p);

    Error
    search(const search_t *sch, const U &comp, kernel_record_t **lwr, const kernel_record_t *end);

    Error
    functionalSearch(const search_t *sch, const U &comp, kernel_record_t **lwr, const kernel_record_t *end);

    void
    printTree() const;
};

}  // ft
}  // cask

#endif
