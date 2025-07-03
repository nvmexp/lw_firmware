#ifndef __FT_NARY_NODE_HPP__
#define __FT_NARY_NODE_HPP__
#include <cstdio>
#include <vector>

#include "ft_kernel_record.hpp"

#define UNUSED(expr)  \
    do {              \
        (void)(expr); \
    } while (0)

namespace cask {
namespace ft {

template <typename T>
class naryNode {
    typedef typename T::level_t level_t;

   private:
    uint32_t nodeArraySz;
    naryNode<T> **nodeArray;
    naryNode<T> *nodeParent;

    level_t level;

    std::vector<cask::kernel_record_t> *pvec_pkr;

    // forcibly prevent usage
    naryNode(const naryNode &rhs) { UNUSED(rhs); }

   public:
    naryNode();
    ~naryNode();

    bool
    isEmpty() const;
    bool
    isLeaf() const;
    bool
    isLeafEmpty() const;

    Error
    createBranches(level_t nxt_lvl, uint32_t sz);
    naryNode<T> *
    getNodeBranch(uint32_t idx) const;
    bool
    hasBranchOrLeaf(uint32_t idx) const;

    Error
    setParent(naryNode<T> *q);
    naryNode<T> *
    getParent() const;

    Error
    setLevel(level_t lvl);
    level_t
    getLevel() const;

    uint32_t
    getSz() const;

    Error
    addKernelRecord(const cask::kernel_record_t &kr);
    // Useful for print.
    Error
    appendTo(std::vector<cask::kernel_record_t> &results) const;

    Error
    appendTo(kernel_record_t **lwr, const kernel_record_t *end) const;
};

}  // ft
}  // cask
#endif
