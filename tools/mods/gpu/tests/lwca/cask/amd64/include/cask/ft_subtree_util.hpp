#ifndef __FT_SUBTREE_UTIL_HPP__
#define __FT_SUBTREE_UTIL_HPP__

#include <cassert>
namespace cask {
namespace ft {

template <typename T, size_t N>
Error
error_if_not_all_val(T (&arr)[N], T val) {
    for (size_t i = 0; i < N; ++i) {
        if (val != arr[i]) {
            return Error::FUNCTIONAL_TREE;
        }
    }
    return Error::OK;
}

template <typename T, size_t N>
void
print_iarr(const char* tag, T (&arr)[N]) {
    printf("%s=[", tag);
    for (size_t i = 0; i < N; ++i) {
        printf("%d", arr[i]);
        if (N - 1 != i) {
            printf(",");
        } else {
            printf("]\n");
        }
    }
}
}  // ft
}  // cask
#endif
