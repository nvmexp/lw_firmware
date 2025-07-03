/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * ----------------------------------------------------------------------
 * Copyright © 2005-2014 Rich Felker, et al.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * ----------------------------------------------------------------------
 */

#include "lwtypes.h"
#include "lwport/lwport.h"
#include "utils/lwassert.h"

#include <stddef.h> // size_t

struct chunk {
    size_t psize;
    size_t csize;
    struct chunk *next; // Malloc returns a pointer here
    struct chunk *prev;
};

/*
If allocated
----------------------------------------------
| psize | csize |     User data               |
----------------------------------------------

If free
----------------------------------------------
| psize | csize |  next | prev | Undef        |
----------------------------------------------
*/

struct bin {
    struct chunk *head;
    struct chunk *tail;
};

void __bin_chunk(struct chunk *self);

#define BIN_COUNT 64
#define LAST_BIN (BIN_COUNT -1)

#define SIZE_ALIGN (4*sizeof(size_t))
#define SIZE_MASK (-SIZE_ALIGN)
#define CHUNK_HEADER_OVERHEAD (2*sizeof(size_t))

#define C_INUSE_ILWERSE_MASK (-2)
#define CHUNK_SIZE(c) ((c)->csize & C_INUSE_ILWERSE_MASK)
#define CHUNK_PSIZE(c) ((c)->psize & C_INUSE_ILWERSE_MASK)
#define PREV_CHUNK(c) ((struct chunk *)((char *)(c) - CHUNK_PSIZE(c)))
#define NEXT_CHUNK(c) ((struct chunk *)((char *)(c) + CHUNK_SIZE(c)))
#define MEM_TO_CHUNK(p) (struct chunk *)((char *)(p) - CHUNK_HEADER_OVERHEAD)
#define CHUNK_TO_MEM(c) (void *)((char *)(c) + CHUNK_HEADER_OVERHEAD)
#define BIN_TO_CHUNK(i) (MEM_TO_CHUNK(&mal.bins[i].head))

#define C_INUSE  ((size_t)1)

#define IS_FREE(c) !((c)->csize & (C_INUSE))

#if defined(__GNUC__) && defined(__PIC__)
#define inline inline __attribute__((always_inline))
#endif

static struct {
   LwU64 binmap;
   struct bin bins[BIN_COUNT];
} mal; /* todo: extract into an argument to allow user storage in tp */

#define CHECK_CHUNK_OVERRUN(current) check_chunk_overrun(current, __FUNCTION__, __LINE__)
static void check_chunk_overrun(struct chunk *current, const char *from_func, const int from_line)
{
    struct chunk *next = NEXT_CHUNK(current);

    if (next == current)
        return;

    if (next->psize == current->csize)
        return;

    LW_PRINTF(LEVEL_ERROR, "from %s:%d: Chunk mismatch\n", from_func, from_line);
    LW_PRINTF(LEVEL_ERROR, "   current = 0x%p, size = %lld\n", current, current->csize);
    LW_PRINTF(LEVEL_ERROR, "   next = 0x%p, psize = %lld\n", next, next->psize);
    /* Crash on corrupted footer (likely from buffer overflow) */
    LW_ASSERT(0);
}

void printMallocChunkOverruns(void)
{
    // print 10 max entries
    LwU32 max_prints = 10;
    LwU32 num_prints = 0;
    LW_PRINTF(LEVEL_ERROR, "Checking for malloc chunk mismatch...\n");

    for (LwU32 bin = 0; bin < BIN_COUNT; bin++)
    {
        for (struct chunk * current = mal.bins[bin].head, * end = BIN_TO_CHUNK(bin);
            current != end;
            current = current->next)
        {
            struct chunk *next = NEXT_CHUNK(current);
            if (next == current)
                continue;

            if (next->psize == current->csize)
                continue;

            LW_PRINTF(LEVEL_ERROR, "Malloc chunk mismatch for bin %d\n", bin);
            LW_PRINTF(LEVEL_ERROR, "  current = 0x%p, size = %lld\n", current, current->csize);
            LW_PRINTF(LEVEL_ERROR, "  next = 0x%p, psize = %lld\n", next, next->psize);

            if (++num_prints == max_prints)
            {
                LW_PRINTF(LEVEL_ERROR, "  Printing no more entries\n");
                return;
            }
        }
    }
}

// @todo: Extract libbitops.c
__attribute__((no_sanitize_address)) static int first_set(LwU64 x)
{
    // RISC-V doesn't yet have bit operations
    // Use a perfect hash to map 2^k -> k

    static const char debruijn64[BIN_COUNT] = {
        0, 1, 2, 53, 3, 7, 54, 27, 4, 38, 41, 8, 34, 55, 48, 28,
        62, 5, 39, 46, 44, 42, 22, 9, 24, 35, 59, 56, 49, 18, 29, 11,
        63, 52, 6, 26, 37, 40, 33, 47, 61, 45, 43, 21, 23, 58, 17, 10,
        51, 25, 36, 32, 60, 20, 57, 16, 50, 31, 19, 15, 30, 14, 13, 12
    };
    return debruijn64[(x&-x) * 0x022fdd63cc95386dull >> 58];
}

static const unsigned char bin_tab[60] = {
                32,33,34,35,36,36,37,37,38,38,39,39,
    40,40,40,40,41,41,41,41,42,42,42,42,43,43,43,43,
    44,44,44,44,44,44,44,44,45,45,45,45,45,45,45,45,
    46,46,46,46,46,46,46,46,47,47,47,47,47,47,47,47,
};

__attribute__((no_sanitize_address)) static int bin_index(size_t x)
{
    x = x / SIZE_ALIGN - 1;
    if (x <= 32) return x;
    if (x < 512) return bin_tab[x / 8 - 4];
    if (x > 0x1c00) return LAST_BIN;
    return bin_tab[x / 128 - 4] + 16;
}

// Find next largest bin
__attribute__((no_sanitize_address)) static int bin_index_up(size_t x)
{
    x = x / SIZE_ALIGN - 1;
    if (x <= 32) return x;
    x--;
    if (x < 512) return bin_tab[x / 8 - 4] + 1;
    if (x > 0x1c00) return LAST_BIN;
    return bin_tab[x / 128 - 4] + 17;
}

__attribute__((no_sanitize_address)) static int adjust_size(size_t *n)
{
    size_t oldSize = *n;
    size_t newSize = oldSize + (CHUNK_HEADER_OVERHEAD + SIZE_ALIGN - 1);

    // Check for overflow
    if (newSize < oldSize)
        return -1;

    *n = newSize & SIZE_MASK;
    return 0;
}

__attribute__((no_sanitize_address)) static void unbin(struct chunk *c, int i)
{
    CHECK_CHUNK_OVERRUN(c);
    if (c->prev == c->next)
        mal.binmap &= ~(1ULL << i);
    c->prev->next = c->next;
    c->next->prev = c->prev;
    c->csize |= C_INUSE;
    NEXT_CHUNK(c)->psize |= C_INUSE;
}

__attribute__((no_sanitize_address)) static int alloc_fwd(struct chunk *c)
{
    int i;
    size_t k;
    if (!((k = c->csize) & C_INUSE)) {
        i = bin_index(k);
        unbin(c, i);
        return 1;
    }
    return 0;
}

__attribute__((no_sanitize_address))static int alloc_rev(struct chunk *c)
{
    int i;
    size_t k;
    if (!((k = c->psize) & C_INUSE)) {
        i = bin_index(k);
        unbin(PREV_CHUNK(c), i);
        return 1;
    }
    return 0;
}

__attribute__((no_sanitize_address)) static void trim(struct chunk *self, size_t n)
{
    size_t n1 = CHUNK_SIZE(self);
    struct chunk *next, *split;

    if (n >= n1 - sizeof(struct chunk)) return;

    CHECK_CHUNK_OVERRUN(self);
    next = NEXT_CHUNK(self);
    split = (struct chunk *)((char *)self + n);

    split->psize = n | C_INUSE;
    split->csize = (n1 - n) | C_INUSE;
    next->psize = (n1 - n) | C_INUSE;
    self->csize = n | C_INUSE;

    CHECK_CHUNK_OVERRUN(self);

    __bin_chunk(split);
}

//
// MUSLC was originally designed to use mmap for large allocations
// Instead: We use a linear search the largest bin (containing only chunks larger than ~256kb)
//
// @todo: Allow secondary allocator
__attribute__((no_sanitize_address)) void * slow_malloc(size_t n)
{
    for (struct chunk * c = mal.bins[LAST_BIN].head, * ce = BIN_TO_CHUNK(LAST_BIN); c != ce; c = c->next)
    {
        CHECK_CHUNK_OVERRUN(c);
        if (c->csize >= n) {
            unbin(c, LAST_BIN);
            trim(c, n);
            return CHUNK_TO_MEM(c);
        }
    }
    return 0;
}

// @todo: 
__attribute__((no_sanitize_address)) void *malloc(size_t n)
{
    struct chunk *c;
    int i, j;

    // Round size up to account for header (overflow safe)
    if (adjust_size(&n) < 0)
        return 0;

    // Find next largest bin (needed for mask computation of sufficiently large bins)
    i = bin_index_up(n);

    // The last bin is a linked list and requires a slow search through
    if (i >= LAST_BIN)
        return slow_malloc(n);

    // No bins of sufficient size have (any) entries?! Punch out.
    LwU64 mask = mal.binmap & -(1ULL << i);
    if (!mask)
        return 0;

    // Find the smallest non-empty bin that suffices
    j = first_set(mask);
    c = mal.bins[j].head;

    // Assert that the bin isn't empty
    LW_ASSERT (c != BIN_TO_CHUNK(j));

    // Remove the memory from the bin
    unbin(c, j);

    // Free the memory that wasn't required
    trim(c, n);

    return CHUNK_TO_MEM(c);
}

__attribute__((no_sanitize_address)) void free(void *p)
{
    struct chunk *self = MEM_TO_CHUNK(p);

#if LIBOS_SANITIZE_ADDRESS
    __msan_poison(self, (LwU8*)NEXT_CHUNK(self) - (LwU8*)self);
#endif

    LW_ASSERT(!IS_FREE(self));
    __bin_chunk(self);
}


__attribute__((no_sanitize_address)) void __bin_chunk(struct chunk *self)
{
    struct chunk *next = NEXT_CHUNK(self);
    size_t final_size, new_size, size;
    int i;

    final_size = new_size = CHUNK_SIZE(self);

    CHECK_CHUNK_OVERRUN(self);

    for (;;) {
        if (self->psize & next->csize & C_INUSE) {
            self->csize = final_size | C_INUSE;
            next->psize = final_size | C_INUSE;
            i = bin_index(final_size);
            if (self->psize & next->csize & C_INUSE)
                break;
        }

        // Reverse coalesce
        if (alloc_rev(self)) {
            self = PREV_CHUNK(self);
            size = CHUNK_SIZE(self);
            final_size += size;
        }

        // Forward coalesce
        if (alloc_fwd(next)) {
            size = CHUNK_SIZE(next);
            final_size += size;
            next = NEXT_CHUNK(next);
        }
    }

    if (!(mal.binmap & 1ULL << i))
        mal.binmap |= 1ULL << i;

    self->csize = final_size;
    next->psize = final_size;

    self->next = BIN_TO_CHUNK(i);
    self->prev = mal.bins[i].tail;
    self->next->prev = self;
    self->prev->next = self;
}


__attribute__((no_sanitize_address)) void malloc_donate(char *start, char *end)
{
#if LIBOS_SANITIZE_ADDRESS
    __msan_poison(start, end-start);
#endif

    size_t align_start_up = (SIZE_ALIGN - 1) & (-(LwUPtr)start - CHUNK_HEADER_OVERHEAD);
    size_t align_end_down = (SIZE_ALIGN - 1) & (LwUPtr)end;

    /* Getting past this condition ensures that the padding for alignment
     * and header overhead will not overflow and will leave a nonzero
     * multiple of SIZE_ALIGN bytes between start and end. */
    if ((size_t)(end - start) <= CHUNK_HEADER_OVERHEAD + align_start_up + align_end_down)
        return;
    start += align_start_up + CHUNK_HEADER_OVERHEAD;
    end -= align_end_down;

    struct chunk *c = MEM_TO_CHUNK(start), *n = MEM_TO_CHUNK(end);
    c->psize = n->csize = C_INUSE;
    c->csize = n->psize = C_INUSE | (end - start);
    __bin_chunk(c);
}

__attribute__((no_sanitize_address)) void malloc_initialize(void)
{
    for (int i = 0; i < BIN_COUNT; i++)
        mal.bins[i].head = mal.bins[i].tail = BIN_TO_CHUNK(i);
}
