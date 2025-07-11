 /* Copyright 1993-2021 LWPU Corporation.  All rights reserved.
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

#ifndef _COOPERATIVE_GROUPS_HELPERS_H_
# define _COOPERATIVE_GROUPS_HELPERS_H_

#include "info.h"
#include "sync.h"

_CG_BEGIN_NAMESPACE

namespace details {
#ifdef _CG_CPP11_FEATURES
    template <typename Ty> struct _is_float_or_half          : public _CG_STL_NAMESPACE::is_floating_point<Ty> {};
# ifdef _CG_HAS_FP16_COLLECTIVE
    template <>            struct _is_float_or_half<__half>  : public _CG_STL_NAMESPACE::true_type {};
    template <>            struct _is_float_or_half<__half2> : public _CG_STL_NAMESPACE::true_type {};
# endif
    template <typename Ty>
    using  is_float_or_half = _is_float_or_half<typename _CG_STL_NAMESPACE::remove_cv<Ty>::type>;

    // Non-STL utility templates 
    template <typename Ty>
    using remove_qual = typename _CG_STL_NAMESPACE::remove_cv<typename _CG_STL_NAMESPACE::remove_reference<Ty>::type>::type;

    template <typename TyLhs, typename TyRhs>
    using is_op_type_same = _CG_STL_NAMESPACE::is_same<remove_qual<TyLhs>, remove_qual<TyRhs>
    >;
#endif

    template <typename TyTrunc>
    _CG_STATIC_QUALIFIER TyTrunc vec3_to_linear(dim3 index, dim3 nIndex) {
        return ((TyTrunc)index.z * nIndex.y * nIndex.x) +
               ((TyTrunc)index.y * nIndex.x) +
                (TyTrunc)index.x;
    }

    namespace cta {

        _CG_STATIC_QUALIFIER void sync()
        {
            __barrier_sync(0);
        }

        _CG_STATIC_QUALIFIER unsigned int num_threads()
        {
            return static_cast<unsigned int>(blockDim.x * blockDim.y * blockDim.z);
        }

        _CG_STATIC_QUALIFIER unsigned int thread_rank()
        {
            return vec3_to_linear<unsigned int>(threadIdx, blockDim);
        }

        _CG_STATIC_QUALIFIER dim3 group_index()
        {
            return dim3(blockIdx.x, blockIdx.y, blockIdx.z);
        }

        _CG_STATIC_QUALIFIER dim3 thread_index()
        {
            return dim3(threadIdx.x, threadIdx.y, threadIdx.z);
        }

        _CG_STATIC_QUALIFIER dim3 dim_threads()
        {
            return dim3(blockDim.x, blockDim.y, blockDim.z);
        }

        // Legacy aliases
        _CG_STATIC_QUALIFIER unsigned int size()
        {
            return num_threads();
        }

        _CG_STATIC_QUALIFIER dim3 block_dim()
        {
            return dim_threads();
        }

    };

    class _coalesced_group_data_access {
    public:
        // Retrieve mask of coalesced groups
        template <typename TyGroup>
        _CG_STATIC_QUALIFIER unsigned int get_mask(const TyGroup &group) {
            return group.get_mask();
        }

        // Retrieve mask of tiles
        template <template <typename, typename> typename TyGroup, typename Sz, typename Parent>
        _CG_STATIC_QUALIFIER unsigned int get_mask(const TyGroup<Sz, Parent> &group) {
            return group.build_maks();
        }

        template <typename TyGroup>
        _CG_STATIC_QUALIFIER TyGroup construct_from_mask(unsigned int mask) {
            return TyGroup(mask);
        }

        template <typename TyGroup>
        _CG_STATIC_QUALIFIER void modify_meta_group(TyGroup &group, unsigned int mgRank, unsigned int mgSize) {
            group._data.coalesced.metaGroupRank = mgRank;
            group._data.coalesced.metaGroupSize = mgSize;
        }
    };

    namespace tile {
        template <unsigned int TileCount, unsigned int TileMask, unsigned int LaneMask, unsigned int ShiftCount>
        struct _tile_helpers{
            _CG_STATIC_CONST_DECL unsigned int tileCount = TileCount;
            _CG_STATIC_CONST_DECL unsigned int tileMask = TileMask;
            _CG_STATIC_CONST_DECL unsigned int laneMask = LaneMask;
            _CG_STATIC_CONST_DECL unsigned int shiftCount = ShiftCount;
        };

        template <unsigned int> struct tile_helpers;
        template <> struct tile_helpers<32> : public _tile_helpers<1,  0xFFFFFFFF, 0x1F, 5> {};
        template <> struct tile_helpers<16> : public _tile_helpers<2,  0x0000FFFF, 0x0F, 4> {};
        template <> struct tile_helpers<8>  : public _tile_helpers<4,  0x000000FF, 0x07, 3> {};
        template <> struct tile_helpers<4>  : public _tile_helpers<8,  0x0000000F, 0x03, 2> {};
        template <> struct tile_helpers<2>  : public _tile_helpers<16, 0x00000003, 0x01, 1> {};
        template <> struct tile_helpers<1>  : public _tile_helpers<32, 0x00000001, 0x00, 0> {};

#ifdef _CG_CPP11_FEATURES
        namespace shfl {
            /***********************************************************************************
             * Relwrsively Sliced Shuffle
             *  Purpose:
             *      Slices an input type a number of times into integral types so that shuffles
             *      are well defined
             *  Expectations:
             *      This object *should not* be used from a reinterpret_cast pointer unless
             *      some alignment guarantees can be met. Use a memcpy to guarantee that loads
             *      from the integral types stored within are aligned and correct.
             **********************************************************************************/
            template <unsigned int count, bool intSized = (count <= sizeof(int))>
            struct relwrsive_sliced_shuffle_helper;

            template <unsigned int count>
            struct relwrsive_sliced_shuffle_helper<count, true> {
                int val;

                template <typename TyFn>
                _CG_QUALIFIER void ilwoke_shuffle(const TyFn &shfl) {
                    val = shfl(val);
                }
            };

            template <unsigned int count>
            struct relwrsive_sliced_shuffle_helper<count, false> {
                int val;
                relwrsive_sliced_shuffle_helper<count - sizeof(int)> next;

                template <typename TyFn>
                _CG_QUALIFIER void ilwoke_shuffle(const TyFn &shfl) {
                    val = shfl(val);
                    next.ilwoke_shuffle(shfl);
                }
            };
        }

        struct _memory_shuffle {
            template <typename TyElem, typename TyShflFn>
            _CG_STATIC_QUALIFIER TyElem _shfl_internal(TyElem elem, const TyShflFn& fn) {
                static_assert(sizeof(TyElem) > 0, "in memory shuffle is not yet implemented");
                return TyElem{};
            }

            template <typename TyElem, typename TyRet = remove_qual<TyElem>>
            _CG_STATIC_QUALIFIER TyRet shfl(TyElem&& elem, unsigned int gMask, unsigned int srcRank, unsigned int threads) {
                auto shfl = [=](int val) -> int {
                    return 0;
                };

                return _shfl_internal<TyRet>(_CG_STL_NAMESPACE::forward<TyElem>(elem), shfl);
            }

            template <typename TyElem, typename TyRet = remove_qual<TyElem>>
            _CG_STATIC_QUALIFIER TyRet shfl_down(TyElem&& elem, unsigned int gMask, unsigned int delta, unsigned int threads) {
                auto shfl = [=](int val) -> int {
                    return 0;
                };

                return _shfl_internal<TyRet>(_CG_STL_NAMESPACE::forward<TyElem>(elem), shfl);
            }

            template <typename TyElem, typename TyRet = remove_qual<TyElem>>
            _CG_STATIC_QUALIFIER TyRet shfl_up(TyElem&& elem, unsigned int gMask, unsigned int delta, unsigned int threads) {
                auto shfl = [=](int val) -> int {
                    return 0;
                };

                return _shfl_internal<TyRet>(_CG_STL_NAMESPACE::forward<TyElem>(elem), shfl);
            }

            template <typename TyElem, typename TyRet = remove_qual<TyElem>>
            _CG_STATIC_QUALIFIER TyRet shfl_xor(TyElem&& elem, unsigned int gMask, unsigned int lMask, unsigned int threads) {
                auto shfl = [=](int val) -> int {
                    return 0;
                };

                return _shfl_internal<TyRet>(_CG_STL_NAMESPACE::forward<TyElem>(elem), shfl);
            }
        };

        /***********************************************************************************
         * Intrinsic Device Function Shuffle
         *  Purpose:
         *      Uses a shuffle helper that has characteristics best suited for moving
         *      elements between threads
         *  Expectations:
         *      Object given will be forced into an l-value type so that it can be used
         *      with a helper structure that reinterprets the data into intrinsic compatible
         *      types
         *  Notes:
         *      !! TyRet is required so that objects are returned by value and not as
         *      dangling references depending on the value category of the passed object
         **********************************************************************************/
        struct _intrinsic_compat_shuffle {
            template <unsigned int count>
            using shfl_helper = shfl::relwrsive_sliced_shuffle_helper<count>;

            template <typename TyElem, typename TyShflFn>
            _CG_STATIC_QUALIFIER TyElem _shfl_internal(TyElem elem, const TyShflFn& fn) {
                static_assert(__is_trivially_copyable(TyElem), "Type is not compatible with device shuffle");
                shfl_helper<sizeof(TyElem)> helper;
                memcpy(&helper, &elem, sizeof(TyElem));
                helper.ilwoke_shuffle(fn);
                memcpy(&elem, &helper, sizeof(TyElem));
                return elem;
            }

            template <typename TyElem, typename TyRet = remove_qual<TyElem>>
            _CG_STATIC_QUALIFIER TyRet shfl(TyElem&& elem, unsigned int gMask, unsigned int srcRank, unsigned int threads) {
                auto shfl = [=](int val) -> int {
                    return __shfl_sync(gMask, val, srcRank, threads);
                };

                return _shfl_internal<TyRet>(_CG_STL_NAMESPACE::forward<TyElem>(elem), shfl);
            }

            template <typename TyElem, typename TyRet = remove_qual<TyElem>>
            _CG_STATIC_QUALIFIER TyRet shfl_down(TyElem&& elem, unsigned int gMask, unsigned int delta, unsigned int threads) {
                auto shfl = [=](int val) -> int {
                    return __shfl_down_sync(gMask, val, delta, threads);
                };

                return _shfl_internal<TyRet>(_CG_STL_NAMESPACE::forward<TyElem>(elem), shfl);
            }

            template <typename TyElem, typename TyRet = remove_qual<TyElem>>
            _CG_STATIC_QUALIFIER TyRet shfl_up(TyElem&& elem, unsigned int gMask, unsigned int delta, unsigned int threads) {
                auto shfl = [=](int val) -> int {
                    return __shfl_up_sync(gMask, val, delta, threads);
                };

                return _shfl_internal<TyRet>(_CG_STL_NAMESPACE::forward<TyElem>(elem), shfl);
            }

            template <typename TyElem, typename TyRet = remove_qual<TyElem>>
            _CG_STATIC_QUALIFIER TyRet shfl_xor(TyElem&& elem, unsigned int gMask, unsigned int lMask, unsigned int threads) {
                auto shfl = [=](int val) -> int {
                    return __shfl_xor_sync(gMask, val, lMask, threads);
                };

                return _shfl_internal<TyRet>(_CG_STL_NAMESPACE::forward<TyElem>(elem), shfl);
            }
        };

        struct _native_shuffle {
            template <typename TyElem>
            _CG_STATIC_QUALIFIER TyElem shfl(
                    TyElem elem, unsigned int gMask, unsigned int srcRank, unsigned int threads) {
                return static_cast<TyElem>(__shfl_sync(gMask, elem, srcRank, threads));
            }

            template <typename TyElem>
            _CG_STATIC_QUALIFIER TyElem shfl_down(
                    TyElem elem, unsigned int gMask, unsigned int delta, unsigned int threads) {
                return static_cast<TyElem>(__shfl_down_sync(gMask, elem, delta, threads));
            }

            template <typename TyElem>
            _CG_STATIC_QUALIFIER TyElem shfl_up(
                    TyElem elem, unsigned int gMask, unsigned int delta, unsigned int threads) {
                return static_cast<TyElem>(__shfl_up_sync(gMask, elem, delta, threads));
            }

            template <typename TyElem>
            _CG_STATIC_QUALIFIER TyElem shfl_xor(
                    TyElem elem, unsigned int gMask, unsigned int lMask, unsigned int threads) {
                return static_cast<TyElem>(__shfl_xor_sync(gMask, elem, lMask, threads));
            }
        };

        // Almost all arithmetic types are supported by native shuffle
        // Vector types are the exception
        template <typename TyElem>
        using use_native_shuffle = _CG_STL_NAMESPACE::integral_constant<
            bool,
            _CG_STL_NAMESPACE::is_integral<
                remove_qual<TyElem>>::value ||
            details::is_float_or_half<
                remove_qual<TyElem>>::value
        >;

        constexpr unsigned long long _MemoryShuffleLwtoff = 32;

        template <typename TyElem,
                  bool IsNative = use_native_shuffle<TyElem>::value,
                  bool InMem = (sizeof(TyElem) > _MemoryShuffleLwtoff)>
        struct shuffle_dispatch;

        template <typename TyElem>
        struct shuffle_dispatch<TyElem, true, false> :  public _native_shuffle {};

        template <typename TyElem>
        struct shuffle_dispatch<TyElem, false, false> : public _intrinsic_compat_shuffle {};

        template <typename TyElem>
        struct shuffle_dispatch<TyElem, false, true> :  public _memory_shuffle {};

#endif //_CG_CPP11_FEATURES
    };

    namespace multi_grid {
        struct multi_grid_functions;
    };

    namespace grid {
        _CG_STATIC_QUALIFIER void sync(unsigned int *bar) {
            unsigned int expected = gridDim.x * gridDim.y * gridDim.z;

            details::sync_grids(expected, bar);
        }

        _CG_STATIC_QUALIFIER unsigned long long num_blocks()
        {
            // grid.y * grid.z -> [max(65535) * max(65535)] fits within 4b, promote after multiplication
            // grid.x * (grid.y * grid.z) -> [max(2^31-1) * max(65535 * 65535)]  exceeds 4b, promote before multiplication
            return (unsigned long long)gridDim.x * (gridDim.y * gridDim.z);
        }

        _CG_STATIC_QUALIFIER unsigned long long num_threads()
        {
            return num_blocks() * cta::num_threads();
        }

        _CG_STATIC_QUALIFIER unsigned long long block_rank()
        {
            return vec3_to_linear<unsigned long long>(blockIdx, gridDim);
        }

        _CG_STATIC_QUALIFIER unsigned long long thread_rank()
        {
            return block_rank() * cta::num_threads() + cta::thread_rank();
        }

        _CG_STATIC_QUALIFIER dim3 dim_blocks()
        {
            return dim3(gridDim.x, gridDim.y, gridDim.z);
        }

        _CG_STATIC_QUALIFIER dim3 block_index()
        {
            return dim3(blockIdx.x, blockIdx.y, blockIdx.z);
        }


# if defined(_CG_HAS_CLUSTER_GROUP)
        _CG_STATIC_QUALIFIER dim3 dim_clusters() {
            unsigned int x = 0, y = 0, z = 0;
            asm("mov.u32 %0, %%nclusterid.x;" : "=r"(x));
            asm("mov.u32 %0, %%nclusterid.y;" : "=r"(y));
            asm("mov.u32 %0, %%nclusterid.z;" : "=r"(z));
            return dim3(x, y, z);
        }

        _CG_STATIC_QUALIFIER unsigned long long num_clusters() {
            const dim3 dimClusters = dim_clusters();
            return dimClusters.x * dimClusters.y * dimClusters.z;
        }

        _CG_STATIC_QUALIFIER dim3 cluster_index() {
            unsigned int x = 0, y = 0, z = 0;
            asm("mov.u32 %0, %%clusterid.x;" : "=r"(x));
            asm("mov.u32 %0, %%clusterid.y;" : "=r"(y));
            asm("mov.u32 %0, %%clusterid.z;" : "=r"(z));
            return dim3(x, y, z);
        }

        _CG_STATIC_QUALIFIER unsigned long long cluster_rank() {
            return vec3_to_linear<unsigned long long>(cluster_index(), dim_clusters());
        }
# endif


        // Legacy aliases
        _CG_STATIC_QUALIFIER unsigned long long size()
        {
            return num_threads();
        }

        _CG_STATIC_QUALIFIER dim3 grid_dim()
        {
            return dim_blocks();
        }
    };


#if defined(_CG_HAS_MULTI_GRID_GROUP)

    namespace multi_grid {
        _CG_STATIC_QUALIFIER unsigned long long get_intrinsic_handle()
        {
            return (lwdaCGGetIntrinsicHandle(lwdaCGScopeMultiGrid));
        }

        _CG_STATIC_QUALIFIER void sync(const unsigned long long handle)
        {
            lwdaError_t err = lwdaCGSynchronize(handle, 0);
        }

        _CG_STATIC_QUALIFIER unsigned int size(const unsigned long long handle)
        {
            unsigned int numThreads = 0;
            lwdaCGGetSize(&numThreads, NULL, handle);
            return numThreads;
        }

        _CG_STATIC_QUALIFIER unsigned int thread_rank(const unsigned long long handle)
        {
            unsigned int threadRank = 0;
            lwdaCGGetRank(&threadRank, NULL, handle);
            return threadRank;
        }

        _CG_STATIC_QUALIFIER unsigned int grid_rank(const unsigned long long handle)
        {
            unsigned int gridRank = 0;
            lwdaCGGetRank(NULL, &gridRank, handle);
            return gridRank;
        }

        _CG_STATIC_QUALIFIER unsigned int num_grids(const unsigned long long handle)
        {
            unsigned int numGrids = 0;
            lwdaCGGetSize(NULL, &numGrids, handle);
            return numGrids;
        }

# ifdef _CG_CPP11_FEATURES
        struct multi_grid_functions {
            decltype(multi_grid::get_intrinsic_handle) *get_intrinsic_handle;
            decltype(multi_grid::sync) *sync;
            decltype(multi_grid::size) *size;
            decltype(multi_grid::thread_rank) *thread_rank;
            decltype(multi_grid::grid_rank) *grid_rank;
            decltype(multi_grid::num_grids) *num_grids;
        };

        template <typename = void>
        _CG_STATIC_QUALIFIER const multi_grid_functions* load_grid_intrinsics() {
            __constant__ static const multi_grid_functions mgf {
                &multi_grid::get_intrinsic_handle,
                &multi_grid::sync,
                &multi_grid::size,
                &multi_grid::thread_rank,
                &multi_grid::grid_rank,
                &multi_grid::num_grids
            };

            return &mgf;
        }
# endif
    };
#endif


# if defined(_CG_HAS_CLUSTER_GROUP)
    namespace cluster {

        static __noinline__ __device__ bool isReal()
        {
            return __clusterDimIsSpecified();
        }

        _CG_STATIC_QUALIFIER void barrier_arrive()
        {
            __cluster_barrier_arrive();
        }

        _CG_STATIC_QUALIFIER void barrier_wait()
        {
            __cluster_barrier_wait();
        }

        _CG_STATIC_QUALIFIER void sync()
        {
            barrier_arrive();
            barrier_wait();
        }

        _CG_STATIC_QUALIFIER unsigned int query_shared_rank(const void *addr)
        {
            // FIXME: Hack while waiting for PTX implementation
            return static_cast<unsigned int>(((reinterpret_cast<unsigned long long>(addr)) >> 24) & 255ULL);
            //int rank;
            //asm volatile("getctarank.shared.clustershared.u64  %0, %1;" : "=r"(rank) : "l"(reinterpret_cast<unsigned long long>(addr)) :);
            //return static_cast<unsigned int>(rank);
        }

        template <typename T>
        _CG_STATIC_QUALIFIER T* map_shared_rank(T *addr, int rank)
        {
            unsigned long long result;
            asm volatile("setctarank.shared.clustershared.u64 %0, %1, %2;" : "=l"(result) : "l"(reinterpret_cast<unsigned long long>(addr)), "r"(rank) :);
            return reinterpret_cast<T*>(result);
        }

        _CG_STATIC_QUALIFIER dim3 block_index()
        {
            return __clusterRelativeBlockIdx();
        }

        _CG_STATIC_QUALIFIER unsigned int block_rank()
        {
            return __clusterRelativeBlockRank();
        }

        _CG_STATIC_QUALIFIER unsigned int thread_rank()
        {
            return block_rank() * cta::num_threads() + cta::thread_rank();
        }

        _CG_STATIC_QUALIFIER dim3 dim_blocks()
        {
            return __clusterDim();
        }

        _CG_STATIC_QUALIFIER unsigned int num_blocks()
        {
            return __clusterSizeInBlocks();
        }

        _CG_STATIC_QUALIFIER dim3 dim_threads()
        {
            const dim3 dimBlocks = dim_blocks();
            const unsigned int x = dimBlocks.x * blockDim.x;
            const unsigned int y = dimBlocks.y * blockDim.y;
            const unsigned int z = dimBlocks.z * blockDim.z;
            return dim3(x, y, z);
        }

        _CG_STATIC_QUALIFIER unsigned int num_threads()
        {
            return num_blocks() * cta::num_threads();
        }

    };
# endif


    _CG_STATIC_QUALIFIER unsigned int laneid()
    {
        unsigned int laneid;
        asm ("mov.u32 %0, %%laneid;" : "=r"(laneid));
        return laneid;
    }

    _CG_STATIC_QUALIFIER unsigned int lanemask32_eq()
    {
        unsigned int lanemask32_eq;
        asm ("mov.u32 %0, %%lanemask_eq;" : "=r"(lanemask32_eq));
        return (lanemask32_eq);
    }

    _CG_STATIC_QUALIFIER unsigned int lanemask32_lt()
    {
        unsigned int lanemask32_lt;
        asm ("mov.u32 %0, %%lanemask_lt;" : "=r"(lanemask32_lt));
        return (lanemask32_lt);
    }

    _CG_STATIC_QUALIFIER void abort()
    {
        _CG_ABORT();
    }

    template <typename Ty>
    _CG_QUALIFIER void assert_if_not_arithmetic() {
#ifdef _CG_CPP11_FEATURES
        static_assert(
            _CG_STL_NAMESPACE::is_integral<Ty>::value ||
            details::is_float_or_half<Ty>::value,
            "Error: Ty is neither integer or float"
        );
#endif
    }

#if defined(_CG_CPP11_FEATURES) && defined(_CG_ABI_EXPERIMENTAL)
    template <unsigned int numWarps>
    struct copy_channel {
        char* channel_ptr;
        barrier_t* sync_location;
        size_t channel_size;

        // One warp sending to all other warps, it has to wait for all other warps.
        struct send_many_to_many {
            _CG_STATIC_CONST_DECL wait_for_warps_kind wait_kind = wait_for_all_other_warps;
            _CG_STATIC_QUALIFIER void post_iter_release(unsigned int thread_idx, barrier_t* sync_location) {
                __syncwarp(0xFFFFFFFF);
                details::sync_warps_release(sync_location, thread_idx == 0, cta::thread_rank(), numWarps);
            }
        };

        // One warp is receiving and all other warps are sending to that warp, they have to wait for that one warp.
        struct send_many_to_one {
            _CG_STATIC_CONST_DECL wait_for_warps_kind wait_kind = wait_for_specific_warp;
            _CG_STATIC_QUALIFIER void post_iter_release(unsigned int thread_idx, barrier_t* sync_location) {
                // Wait for all warps to finish and let the last warp release all threads.
                if (details::sync_warps_last_releases(sync_location, cta::thread_rank(), numWarps)) {
                    details::sync_warps_release(sync_location, thread_idx == 0, cta::thread_rank(), numWarps);
                }
            }
        };

        template <unsigned int ThreadCnt, size_t ValSize, typename SendDetails>
        _CG_QUALIFIER void _send_value_internal(char* val_ptr, unsigned int thread_idx, unsigned int warp_id) {
            size_t thread_offset = thread_idx * sizeof(int);

            for (size_t i = 0; i < ValSize; i += channel_size) {
                size_t bytes_left = ValSize - i;
                size_t copy_chunk = min(bytes_left, channel_size);

                details::sync_warps_wait_for_warps<SendDetails::wait_kind>(warp_id, sync_location, cta::thread_rank(), numWarps);
                #pragma unroll 1
                for (size_t j = thread_offset; j < copy_chunk ; j += sizeof(int) * ThreadCnt) {
                    size_t my_bytes_left = copy_chunk - j;
                    memcpy(channel_ptr + j, val_ptr + i + j, min(my_bytes_left, sizeof(int)));
                }
                SendDetails::post_iter_release(thread_idx, sync_location);
            }
        }


        template <typename TyVal, unsigned int ThreadCnt, typename SendDetails>
        _CG_QUALIFIER void send_value(TyVal& val, unsigned int thread_idx, unsigned int warp_id) {
            _send_value_internal<ThreadCnt, sizeof(TyVal), SendDetails>(reinterpret_cast<char*>(&val), thread_idx, warp_id);
        }

        template <size_t ValSize>
        _CG_QUALIFIER void _receive_value_internal(char* val_ptr, bool warp_master, bool active) {
            for (size_t i = 0; i < ValSize; i += channel_size) {
                size_t bytes_left = ValSize - i;
                details::sync_warps_wait_for_release(sync_location, warp_master, cta::thread_rank(), numWarps);
                if (active) {
                    memcpy(val_ptr + i, channel_ptr, min(bytes_left, channel_size));
                }
            }
        }

        template <typename TyVal>
        _CG_QUALIFIER void receive_value(TyVal& val, bool warp_master, bool active = true) {
            _receive_value_internal<sizeof(TyVal)>(reinterpret_cast<char*>(&val), warp_master, active);
        }
    };

    _CG_STATIC_QUALIFIER constexpr unsigned int log2(unsigned int x) {
        return x == 1 ? 0 : 1 + log2(x / 2);
    }
#endif //_CG_CPP11_FEATURES

}; // !Namespace internal

_CG_END_NAMESPACE

#endif /* !_COOPERATIVE_GROUPS_HELPERS_H_ */
