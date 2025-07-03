/****************************************************************************\

Copyright (c) 2017 NVIDIA CORPORATION.  All Rights Reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto.  Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.

*****************************************************************************

Note well: I spent two days tuning this code, so please run the performance
tuning code in cu/cta_swizzle_test.cpp before checking in any changes to it.

Class CtaMap mimics SASS code that remaps HW's linear CTA rasterization
into something that reduces total accesses to A and B matrix data.  The
strategy is to rasterize across a specified column width of CTAs, then
move down to the next row.  Each column group must be a power of 2 wide
to keep SASS implementation, and the search space for chooseBestLog2GroupCols,
reasonable.  The column group size is automatically reduced by SASS near the
right edge of the grid.  The last column group can be 3 wide because 2 then 1
is inefficient and allowing 3 wasn't too bad.)

Traversal from one column group to the next optionally serpentines, in
which case always changes direction from top to bottom, to bottom to top,
or vice-versa, even when the width of a column group has been automatically
reduced.

But the real point of the class is to then compute an *optimal* starting
column width for a specified grid and CTA wave size, which results in
the minimal amount of data fetched from A and B for the entire grid.

Since a CTA isn't always square, the CTA tile size is taken into account
when computing how much data is fetched from A and B.  We compensate for
different data sizes for A and B data.  Since CUDNN's implicit GEMM
replicates A matrix data, we account for that too, which results in waves
that appear to be tallish and narrow compared to a GEMM wave shape.

A simple strategy that tries to choose a column width that minimizes A
and B data fetched for a single CTA wave is not sufficient.  When a
CTA wave wraps around from one column group to another, it fetches more
B data.  This can make "squarish" rasterization of a wave worse than no
remapping at all!  Instead, we exhaustively try all possible column
widths, sum how much data all CTA waves fetch, and choose the best.  Here
again, a simple strategy to stop increasing the column width once we've
passed the "best" width is not sufficient...we are not looking at a
parabola-ish function, but one that bounces up and down jerkily.  I hope
the code here is fast enough that this doesn't significantly affect
kernel launch time, as the payoffs in bandwidth reduction can be quite large.

\****************************************************************************/

#include <cstdlib>
#include <cstdio>
#include <bitset>
#include <cassert>
#include <algorithm>

// DIV can be 'H' (hardware); 'S' (software constant integer optimization),
// which is faster; or 'B' (both to check that they get the same answer)
#define DIV 'S'

class CtaSwizzle {
private:
    typedef unsigned long long uint64;


    // clz - "count leading zeros." In most modern architectures this
    // should be handled by a single instruction (e.g., clz or bsr), but C
    // offers no clean why to express it, so we must rely on
    // compiler-specific intrinsics.
    static inline unsigned clz(unsigned value) {
#if defined __GNUC__
        return __builtin_clz(value);
#elif defined _MSC_VER
        unsigned long first_zero = 0;
        if ( _BitScanReverse( &first_zero, value ) ) {
            return 31 - first_zero;
        }
        else {
            return 32;
        }
#else
#error "Unknown platform for clz"
#endif
    }


public:
    class Pos2 {
    public:
        Pos2(unsigned row, unsigned col) : row(row), col(col) {}

        unsigned row, col;

        Pos2 &operator +=(const Pos2 &rhs) {
            row += rhs.row;
            col += rhs.col;
            return *this;
        }
    }; // class Pos2


    // Batched GEMM and split-k-across-CTAs must know all 3 dimensions of the
    // grid and a CTA's position. The batch CTA dimension (a.k.a. grid.Z)
    // indicates which group of A, B, C matrices to use in batched mode, or
    // which group of k indices to use in split-k-across-CTAs.  In those cases,
    // we only share data among CTAs in the same "batch".

    class Pos3 {
    public:
        Pos3(unsigned row, unsigned col, unsigned batch)
            : row(row), col(col), batch(batch) {}

        unsigned row, col, batch;
    }; // class Pos


    // Pos3 + modified log2GroupCols + up/down direction in group.
    // We always serpentine between adjacent columns, no matter how the
    // right edge loops through halving groupCols.
    class  Pos3X: public Pos3 {
    public:
        Pos3X(Pos3 pos, unsigned groupCols, unsigned log2GroupCols,
              bool bottomToTop)
            : Pos3(pos), groupCols(groupCols), log2GroupCols(log2GroupCols),
              bottomToTop(bottomToTop) {}

        unsigned groupCols;
        unsigned log2GroupCols;
        bool     bottomToTop;
    }; // Pos3X


    // Convert an integer division into faster sequence of multiplication and
    // shift.  Guaranteed for all unsigned divisors except 0 (duh), and 1.
    // div by 1 returns the numerator, so we don't care if the parameters we
    // compute for it are wrong. See "Division by Invariant Integers using
    // Multiplication" by Torbjorn Granlund and Peter L. Montgomery at
    // https://gmplib.org/~tege/divcnst-pldi94.pdf
    class ConstDiv {
    public:
        ConstDiv(unsigned divisor = 1U) : divisor(divisor) {
            assert(divisor != 0 && "ConstDiv cannot divide by 0");
            ceilLog2 = 32 - clz(divisor - 1U);

            m = unsigned(((1ULL << (32+ceilLog2)) - (uint64(divisor) << 32ULL))
                         / divisor) + 1;
        } // ConstDiv()

        inline unsigned div(unsigned n) const {
            if (divisor == 1) return n;
            unsigned t1 = (uint64(m) * n) >> 32;  // I.e., IMUL.HI
            // 64-bit add, shift is faster than their 2-shift version
            unsigned quotient = (uint64(t1) + n) >> ceilLog2;
            return quotient;
        } // unsigned div()

    private:
        unsigned divisor, ceilLog2, m;
    }; // class ConstDiv


    // Max log2GroupCols is really 15, but we limit search to groupCols = 256.
    static const unsigned maxLog2GroupCols = 8;

    Pos3 gridDim;       // (row, col, batch) size of grid in CTAs
    unsigned ctasPerBatch;
    unsigned ctasPerGrid;
    ConstDiv constDivGridDimRow;
    ConstDiv constDivCtasPerBatch;
    Pos2 ctaTile;       // (row, col) size of CTA in elements
    // Below parameters are only used by implicit GEMMs.
    Pos2 filter;        // (row, col) size of implicit GEMM filter.
    Pos2 convStride;    // (row, col) size of implicit GEMM filter's stride.
    Pos2 outputSize;    // (row, col) size of implicit GEMM output.
    unsigned defaultAReplication;

    // Construct finction for explicit GEMMs.
    CtaSwizzle(
        const Pos3 &gridDim,
        const Pos2 &ctaTile)
      : CtaSwizzle(gridDim, ctaTile, Pos2(1U, 1U), Pos2(1U, 1U), Pos2(1U, 1U)) {}

    // Construct function for implicit GEMMs.
    CtaSwizzle(
        const Pos3 &gridDim,
        const Pos2 &ctaTile,
        const Pos2 &filter,
        const Pos2 &convStride,
        const Pos2 &outputSize) : gridDim(gridDim), ctaTile(ctaTile), filter(filter),
            convStride(convStride), outputSize(outputSize)
    {
#if 0
        printf("CtaSwizzle created with gridDim(%d, %d, %d), ctaTile(%d, %d)\n",
                gridDim.row, gridDim.col, gridDim.batch, ctaTile.row, ctaTile.col);
#endif
        ctasPerBatch = gridDim.row * gridDim.col;
        ctasPerGrid  = ctasPerBatch * gridDim.batch;

        // For convolutions, filter.row and filter.col can be > 1.  If the
        // input width of the activation is small enough, we'll replicate
        // data nearly filter.col / convStride.col times within a CTA as we
        // sweep the filter from left to right.  The activation data at the
        // bottommost row of the filter is then replicated by another factor
        // of filter.row, as we move the filter down each activation row and
        // sweep left to right again and again.  The cache should avoid
        // refetching this from the FB.  This reduces the effective "height"
        // of the CTA as seen by the FB by "nearly"
        // (filter.row / convStride.row) * (filter.col / convStride.col)

        // If the activation width is sufficiently large compared to the
        // CTA height (and it often is), then we only replicate
        // activations *within a CTA* as we create new A matrix rows by
        // sweeping the filter to the right, so replicate approximately
        // filter.col times.  By the time we reach the right edge of the
        // input data, and move the filter back to the left edge and down
        // one row, we're in another CTA below us.

        // Unfortunately, we do not know the # of CTAs per wave now.
        // So we'll just play it safe and use filter.col / ConvStride_W for
        // the degree of replication that is captured by the L1 and sometimes
        // L2 if L1 is not big enough. We'll check if there's more replication
        // later at chooseBestLog2GroupCols().

        // See BUG 200497373 for more information.

        defaultAReplication = std::max(1U, filter.col / convStride.col);
        constDivGridDimRow = ConstDiv(gridDim.row);
        if (gridDim.batch > 1) {
            constDivCtasPerBatch = ConstDiv(ctasPerBatch);
        }
    }
    // Map a 2D position that was column-major scanline rasterized to a
    // new 2D position that is row-major scanline rasterized within
    // column groups.  This is the same mapping that cta_swizzle.py generates.
    const Pos3X mapPos3ToSwizzled(
        const Pos3     pos,
              unsigned log2GroupCols,
              bool     serpentine) const
    {
        unsigned groupCols = (1 << log2GroupCols);
#ifndef NDEBUG
        if (groupCols > gridDim.col) {
            printf("groupCols=%d > gridDim.col=%d, which not only causes SASS to waste cycles reducing to a reasonable value, but also results in incorrect computation of up/down direction.  The client should ensure the stated condition.", groupCols, gridDim.col);
            assert(false);
        }
        if ((pos.row >= gridDim.row) || (pos.col >= gridDim.col)
                || (pos.batch >= gridDim.batch)) {
            printf("pos(%d, %d, %d) is outside of gridDim(%d, %d, %d)\n",
                pos.row, pos.col, pos.batch,
                gridDim.row, gridDim.col, gridDim.batch);
                assert(false);
        }
#endif
        unsigned mask = groupCols - 1;
        bool bottomToTop = false;
        unsigned swizRow, swizCol;
        for (;;) {
            // testCol = last CTA column in group
            unsigned testCol  = pos.col | mask;

            // Are all CTA's in column group inside right edge of grid?
            bool colInGroup  = testCol < gridDim.col;

            // bottomToTop ^= (pos.col & groupCols) != 0
            // For starting groupCols (which we know at least one group uses,
            // due to rude assertion above), this sets
            // bottomToTop = (pos.col / groupCols) % 2 == 1
            // That is, each odd groupCols goes bottom to top.
            // After that, it inverts bottomToTop for each reduced-size
            // groupCols that gets used by some group.  It's almost magic.
            bottomToTop = bottomToTop != ((pos.col & groupCols) != 0);
#if 0
            printf("testCol=%d, colInGroup=%d\n", testCol, colInGroup);
#endif
            // These are not yet used if we loop, but allows better scheduling
            unsigned colMod  = pos.col & mask;
            unsigned colBase = pos.col & ~mask;
#if 0
            printf("colMod=%d, colBase=%d\n", colMod, colBase);
#endif

            if (colInGroup) {
                // Linear CTA index within current group
                unsigned linearLocal = colMod * gridDim.row + pos.row;
                // swizRow = linearLocal / groupCols
                swizRow = linearLocal >> log2GroupCols;
                // swizCol = colBase + linearLocal % groupCols
                swizCol = colBase | (linearLocal & mask);
                break;
            }

            // If we reduce rightmost groupCols to 3 is the group inside?
            // colInGroup3 = group width == 4 && in rightmost group
            //               && gridDim.col mod 4 == 3
            bool colInGroup3 = (groupCols == 4) && (testCol == gridDim.col);

            if (colInGroup3) {
                // Linear CTA index within current group
                unsigned linearLocal = colMod * gridDim.row + pos.row;
                // swizRow = linearLocal / 3
                swizRow = (uint64(linearLocal) * 0x55555556) >> 32ULL;
                // swizCol = colBase + linearLocal % 3
                swizCol = colBase - swizRow * 3 + linearLocal;
                break;
            }

            // Next iteration.
            groupCols >>= 1;
            mask >>= 1;
            log2GroupCols -= 1;
        } // end for

#if 0
        printf("linearLocal=%d, colInGroup3=%d\n", linearLocal, colInGroup3);
#endif

        bottomToTop &= serpentine;
        if (bottomToTop) {
            swizRow = gridDim.row - 1 - swizRow;
        }
        return Pos3X(Pos3(swizRow, swizCol, pos.batch),
                    groupCols, log2GroupCols, bottomToTop);
    } // mapPos3ToSwizzled


    // Map column-major linearized CTA index to a swizzled 3D position
    // This is useful for mapping the first and last CTA in a wave.
    const Pos3X mapLinearToSwizzled(
        const unsigned index,
        const unsigned log2GroupCols,
        const bool     serpentine) const
    {
        // First map to a 3D position.  Rasterization proceeds row by row
        // down a column, then across column by column, then batch by batch.
        // (Remember, row = x and col = y.)
        unsigned batch    = 0;
        unsigned batchMod = index;
        if (index >= ctasPerBatch) {
            // Only needed for batched GEMM/split-k-across-CTAs
#if DIV == 'H'
            batch    = index / ctasPerBatch;
            batchMod = index % ctasPerBatch;
#elif DIV == 'S'
            batch    = constDivCtasPerBatch.div(index);
            batchMod = index - batch * ctasPerBatch;
#elif DIV == 'B'
            batch    = index / ctasPerBatch;
            batchMod = index % ctasPerBatch;
            unsigned batch2    = constDivCtasPerBatch.div(index);
            unsigned batchMod2 = index - batch * ctasPerBatch;
            if ((batch != batch2) || (batchMod != batchMod2)) {
                printf("Const div failed: batch = %d, batch2 = %d, batchMod = %d, batchMod2 = %d\n",
                    batch, batch2, batchMod, batchMod2);
                assert(false);
            }
#endif
        }
#if DIV == 'H'
        unsigned col = batchMod / gridDim.row;
        unsigned row = batchMod % gridDim.row;
#elif DIV == 'S'
        unsigned col = constDivGridDimRow.div(batchMod);
        unsigned row = batchMod - col * gridDim.row;
#elif DIV == 'B'
        unsigned col = batchMod / gridDim.row;
        unsigned row = batchMod % gridDim.row;
        unsigned col2 = constDivGridDimRow.div(batchMod);
        unsigned row2 = batchMod - col * gridDim.row;
        if ((col != col2) || (row != row2)) {
            printf("Const div failed: col = %d, col2 = %d, row = %d, row2 = %d\n",
                col, col2, row, row2);
            assert(false);
        }
#endif

        return mapPos3ToSwizzled(Pos3(row, col, batch), log2GroupCols,
            serpentine);
    }


    // Compute # of CTAs high in A and wide in B, fetched by CTAs in same batch
    Pos2 batchCtaFetches(
        const Pos3X &first,
        const Pos3X &last,
        const bool serpentine) const
    {
        if (first.batch != last.batch) {
            printf("first.batch=%d != last.batch=%d\n",
                first.batch, last.batch);
            assert(false);
        }
        unsigned firstMask = first.groupCols - 1;
        // First column in the current group
        unsigned firstBColBase = first.col & ~firstMask;

        // The last CTA in the wave is farther to the right, and so may be
        // in an automatically narrowed column group smaller than first.
        unsigned lastMask = last.groupCols - 1;
        unsigned lastBColBase = last.col & ~lastMask;

        // How many columns of B data?  From the above assertion, we know
        // that we'll always span an entire B column group with the wave,
        // unless we're in the "last" row of the column group.
        bool firstSpansColGroup = (first.bottomToTop && (first.row > 0))
            || (!first.bottomToTop && (first.row < (gridDim.row-1)));
        unsigned firstBCol = firstSpansColGroup ? firstBColBase : first.col;

        // Similar logic applies here.  We'll always span an entire B
        // column group, unless we're in the "first" row of the column.
        bool lastSpansColGroup =
               (last.bottomToTop && (last.row < (gridDim.row-1)))
            || (!last.bottomToTop && (last.row > 0));
        unsigned lastBCol = lastSpansColGroup ? (last.col | lastMask)
                                              : last.col;
        // Compensate for possible groupCols == 3
        if (lastBCol == gridDim.col)  lastBCol--;
        unsigned bCols = lastBCol - firstBCol + 1;

        // A is trickier.
        unsigned aRows;
        if (firstBColBase == lastBColBase) {
            // Same column group.  Span from one position to the other.
            aRows = std::abs(int(last.row) - int(first.row)) + 1;
        } else if ((firstBColBase + first.groupCols) == lastBColBase){
            // Adjacent column groups.
            if (serpentine) {
                // Use the larger of the span of the first CTA to the "last"
                // row in its column, and "first" row of the next column to
                // the last CTA.
#ifndef NDEBUG
                assert((first.bottomToTop != last.bottomToTop)
                     && "Somehow serpentine code got confused");
#endif
                if (first.bottomToTop) {
                    aRows = std::max(first.row, last.row) + 1;
                } else {
                    aRows = std::max(gridDim.row - first.row,
                                     gridDim.row - last.row);
                }
            } else {
                // Both columns groups going top to bottom, so sum of first
                // CTA to bottom of column and top of column to last CTA.
#ifndef NDEBUG
                assert(!first.bottomToTop && !last.bottomToTop
                     && "Somehow serpentine code got confused");
#endif
                aRows = std::min(gridDim.row,
                                 (gridDim.row - first.row + 1) + last.row);
            } // if serpentine ... else ...
        } else {
            // At least one complete column in middle of wave, so span all
            // of grid height.
            aRows = gridDim.row;
        } // if various A matrix cases

        return Pos2(aRows, bCols);
    } // batchCtaFetches


    // Compute # of CTAs high in A, and wide in B, fetched by all CTA waves
    // in the grid.  We don't share information between batches, so we must
    // compute this independently for each batch index.
    Pos2 gridCtaFetches(
        const unsigned ctasPerWave,
        const unsigned log2GroupCols,
        const bool     serpentine)
    {
#ifndef NDEBUG
        unsigned groupCols = 1 << log2GroupCols;
        if ((ctasPerWave <= groupCols) && (groupCols > 1)) {
            printf("It makes no sense to have ctasPerWave=%d <= groupCols=%d, as the whole point is to share *both* A and B data.  Further, this code depends upon this condition.", ctasPerWave, groupCols);
        }
#endif
        Pos2 total = Pos2(0, 0);
        for (unsigned firstLinear = 0;
             firstLinear < ctasPerGrid;
             firstLinear += ctasPerWave) {

            Pos3X first = mapLinearToSwizzled(firstLinear, log2GroupCols,
                serpentine);
            unsigned lastLinear =
                std::min(firstLinear+ctasPerWave, ctasPerGrid) - 1;
            Pos3X last = mapLinearToSwizzled(lastLinear, log2GroupCols,
                serpentine);

            Pos2 waveTotal = Pos2(0, 0);
            if (first.batch == last.batch) {
                waveTotal = batchCtaFetches(first, last, serpentine);
            } else {
                // Split work into 3 parts
                // From first to the last CTA in the first batch
                unsigned lastLinearInBatch =
                    first.batch * ctasPerBatch + ctasPerBatch - 1;
                Pos3X lastInBatch = mapLinearToSwizzled(lastLinearInBatch,
                    log2GroupCols, serpentine);
                waveTotal = batchCtaFetches(first, lastInBatch, serpentine);
                // Completely covered batches, if any, between first and last
                unsigned factor = last.batch - first.batch - 1;
                waveTotal += Pos2(factor * gridDim.row, factor * gridDim.col);
                // From the first CTA in the last batch to last
                unsigned firstLinearInBatch = last.batch * ctasPerBatch;
                Pos3X firstInBatch = mapLinearToSwizzled(firstLinearInBatch,
                    log2GroupCols, serpentine);
                waveTotal += batchCtaFetches(firstInBatch, last, serpentine);
            }

#if defined(DEBUG)
           printf("log2 = %d, ctas %3d..%3d, batch = %d..%d: A height = %d, B width = %d\n",
               log2GroupCols, firstLinear, lastLinear, first.batch, last.batch,
               waveTotal.row, waveTotal.col);
#endif
            total += waveTotal;
        } // end for firstCta

        return total;
    } // gridCtaFetches

    // Find the best value for log2GroupCols for this grid and CTA wave
    // size.  At least theoretically, serpentining is never worse then
    // always going top to bottom, so that's hardwired here.
    unsigned chooseBestLog2GroupCols(unsigned ctasPerWave) {
        if (ctasPerWave >= (gridDim.batch * ctasPerBatch - 1)) {
            // No possible way to help by swizzling ids.
            return 0;
        }
        unsigned bestLog2GroupCols = 0;
        unsigned bestFetches = ~0;
        unsigned wavesPerGrid = (ctasPerGrid + ctasPerWave / 2 ) / ctasPerWave;
        for (unsigned log2GroupCols = 0;
             log2GroupCols <= maxLog2GroupCols;
             log2GroupCols++) {
            unsigned groupCols = 1 << log2GroupCols;
            if (gridDim.col != 3 && groupCols > gridDim.col) break;
            // For grid width = 3, we will evaluate groupCols = 4 to see
            // if we can fetch less tiles
            // See https://jirasw.nvidia.com/browse/CFK-257
            if (gridDim.col == 3 && groupCols > 4) break;
            if (groupCols >= ctasPerWave) break;
#if 0
            // This is blindingly faster than the exhaustive code, but doesn't
            // account for a wave wrapping around to the next column group at
            // the bottom or top of the grid, and so can choose too wide.
            Pos2 ctaFetches((ctasPerWave + groupCols - 1) / groupCols,
                            groupCols);
            if (ctaFetches.row > gridDim.row) {
                // CTA wave occupies multiple col groups, can even be one more
                // col group than we compute here
                ctaFetches.col = (ctasPerWave + gridDim.row + 1) / gridDim.row;
                ctaFetches.row = gridDim.row;
            }
#else
            Pos2 ctaFetches = gridCtaFetches(ctasPerWave, log2GroupCols, true);
#endif
            unsigned aReplication = defaultAReplication;
            if (filter.col != 1 || filter.row != 1) {
                unsigned rowFetchedPerWave(ctaFetches.row * ctaTile.row / wavesPerGrid);
                if (outputSize.col < rowFetchedPerWave) {
                    aReplication *= std::max(1U, std::min(rowFetchedPerWave / outputSize.col, filter.row) / convStride.row);
                }
            }
            // Scale fetches up by CTA tile dimensions, and scale A fetches
            // down by degree of filter replication (for implicit GEMM).
            unsigned fetches = ctaFetches.row * ctaTile.row / aReplication
                              + ctaFetches.col * ctaTile.col;
#if defined(DEBUG)
            printf("A total = %d, B total = %d, scaled fetches = %d\n",
                   ctaFetches.row, ctaFetches.col, fetches);
#endif
            if (fetches < bestFetches) {
                bestFetches = fetches;
                bestLog2GroupCols = log2GroupCols;
            } else if (fetches > (bestFetches * 5 / 4)) {
                // Unlikely that we'll manage to get better from here
                break;
            }
        } // end for log2GroupCols;
        return bestLog2GroupCols;
    } // choseBestLog2GroupCols
}; // class CtaSwizzle

