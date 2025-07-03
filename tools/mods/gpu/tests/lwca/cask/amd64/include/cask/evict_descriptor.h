#ifndef INCLUDE_GUARD_CASK_EVICTION_DESCRIPTOR_H
#define INCLUDE_GUARD_CASK_EVICTION_DESCRIPTOR_H
// enable doxygen:
//! @file
#include <stdint.h>


namespace cask {

#define MEM_DESC_CLEAR_FIELD(desc, mask) ((desc) &= ~(mask))
#define MEM_DESC_SET_FIELD(desc, mask, shift, val) ((desc) |= ((uint64_t(val) << shift) & (mask)))
#define MEM_DESC_ENCODE_FIELD(desc, mask, shift, val) do{MEM_DESC_CLEAR_FIELD(desc, mask); MEM_DESC_SET_FIELD(desc, mask, shift, val);}while(0)
#define MEM_DESC_DECODE_FIELD(desc, mask, shift) (((desc)&(mask))>>shift)

#define MEM_DESC_L2SECT_MASK     0x4000000000000000ul
#define MEM_DESC_L2SECT_SHIFT    62

#define MEM_DESC_L1ILW_MASK      0x2000000000000000ul
#define MEM_DESC_L1ILW_SHIFT     61

#define MEM_DESC_L2MODE_MASK     0x1800000000000000ul
#define MEM_DESC_L2MODE_SHIFT    59

#define MEM_DESC_L2COP_ON_MASK   0x0600000000000000ul
#define MEM_DESC_L2COP_ON_SHIFT  57

#define MEM_DESC_L2COP_OFF_MASK  0x0100000000000000ul
#define MEM_DESC_L2COP_OFF_SHIFT 56

/// \brief Base. should not use directly, use sub class intead
class MemDescriptor
{
public:
    enum L2DescriptorMode {
        kDescImplicit = 0,
        kDescInterleaved = 2,   // Interleaved mode:  A HW hash aims at distributing the use of the "on"-class
                                //                    (passing address test) uniformly across the VA space.
        kDescBlockType = 3,     // Block-type mode:  Ignoring the upper bits, addresses within [block_start << block_size,
                                //                   (block_start+block_count) << block_size) are accessed with the On-L2-cop.
    };

    enum L2CopOn {
        // Encodings for L2 cache op to used with a memory op if the address test for cache op passes.
        kL2CopOnEvictNormal = 0,
        kL2CopOnEvictFirst = 1,
        kL2CopOnEvictLast = 2,
        kL2CopOnEvictNormalDemote = 3
    };

    enum L2CopOff {
        // Encodings for L2 cache op to used with a memory op if the address test for cache op fails.
        kL2CopOffEvictNormal = 0,
        kL2CopOffEvictFirst = 1,
    };

    ~MemDescriptor()
    {}

    /// \brief get 64bit descriptor
    inline uint64_t getDescriptor() {
        return desc_;
    }

    /// \brief get low 32bit of the descriptor
    inline uint32_t getDescriptorLsw() {
        return static_cast<uint32_t>(desc_ & 0xffffffff);
    }

    /// \brief get high 32bit of the descriptor
    inline uint32_t getDescriptorMsw() {
        return static_cast<uint32_t>(desc_ >> 32);
    }

    /// \brief L2 Cache hint that applies unconditionally and independently of whether the L2 descriptor is used.
    ///        This hint causes L2 to promote requests to 256B requests to DRAM,
    ///        and the global sector promotion state is ignored.
    inline void setL2SectProm(bool b) {
        if (b) {
            MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_L2SECT_MASK, MEM_DESC_L2SECT_SHIFT, 1);
        }
        else {
            MEM_DESC_CLEAR_FIELD(desc_, MEM_DESC_L2SECT_MASK);
        }
    }

    /// \brief  L1 Cache hint that applies unconditionally and independently of whether the L2 descriptor is used.
    ///        This hint causes L1 to ilwalidate and avoid allocating a tag for this access.
    ///        Applying this hint transforms LDGSTS.ACCESS into LDGSTS.BYPASS if the access
    ///        size is 128, but only ilwalidates L1 for other .ACCESS sizes.  It is ignored for
    ///        other instructions.
    inline void setL1IlwDontAlloc(bool b) {
        if (b) {
            MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_L1ILW_MASK, MEM_DESC_L1ILW_SHIFT, 1);
        }
        else {
            MEM_DESC_CLEAR_FIELD(desc_, MEM_DESC_L1ILW_MASK);
        }
    }

    /// \brief  Encodings for L2 cache op to used with a memory op if the address test for cache op passes.
    inline void setL2CopOn(L2CopOn policy) {
        MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_L2COP_ON_MASK, MEM_DESC_L2COP_ON_SHIFT, policy);
    }

    /// \brief  Encodings for L2 cache op to used with a memory op if the address test for cache op fails.
    inline void setL2CopOff(L2CopOff policy) {
        MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_L2COP_OFF_MASK, MEM_DESC_L2COP_OFF_SHIFT, policy);
    }
protected:

    MemDescriptor() :
        desc_(0)
    {}

    inline void setL2DescMode(L2DescriptorMode mode) {
        MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_L2MODE_MASK, MEM_DESC_L2MODE_SHIFT, mode);
    }

protected:
    uint64_t desc_;
};

#define MEM_DESC_IMPLI_FRACTION_MASK        0x00f0000000000000ul
#define MEM_DESC_IMPLI_FRACTION_SHIFT       52

#define MEM_DESC_IMPLI_PAGE_CNT_MASK        0x000fffe000000000ul
#define MEM_DESC_IMPLI_PAGE_CNT_SHIFT       37

#define MEM_DESC_IMPLI_START_ADDRESS_MASK   0x0000001ffffffffful
#define MEM_DESC_IMPLI_START_ADDRESS_SHIFT  0

class MemImplicitDescriptor : public MemDescriptor
{
public:
    MemImplicitDescriptor(L2CopOn policyOn, L2CopOff policyOff) : MemDescriptor()
    {
        setL2DescMode(kDescImplicit);
        setL2CopOn(policyOn);
        setL2CopOff(policyOff);
    }

    ~MemImplicitDescriptor()
    {}

    /// \brief This field specifies the numerator N, for the desired fraction (N/16)
    ///        of memory accesses to pass address test within aperture.
    inline void setFraction(uint32_t fraction) {
        MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_IMPLI_FRACTION_MASK, MEM_DESC_IMPLI_FRACTION_SHIFT, fraction);
    }

    /// \brief The size of aperture in 4K pages (up to 128MB)
    inline void setPageCount(uint32_t pageCount) {
        MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_IMPLI_PAGE_CNT_MASK, MEM_DESC_IMPLI_PAGE_CNT_SHIFT, pageCount);
    }

    /// \brief 4K Page address (lower 12 bits dropped)
    ///        for the start of the aperture defined by implicit descriptor
    inline void setStartAddr(uint32_t addr) {
        MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_IMPLI_START_ADDRESS_MASK, MEM_DESC_IMPLI_START_ADDRESS_SHIFT, addr);
    }
};

#define MEM_DESC_EXPLI_FRACTION_MASK        0x00f0000000000000ul
#define MEM_DESC_EXPLI_FRACTION_SHIFT       52

class MemExpliInterleavedDesc : public MemDescriptor
{
public:
    MemExpliInterleavedDesc(L2CopOn policyOn, L2CopOff policyOff, uint32_t fraction) : MemDescriptor()
    {
        setL2DescMode(kDescInterleaved);
        setL2CopOn(policyOn);
        setL2CopOff(policyOff);
        setFraction(fraction);
    }

    ~MemExpliInterleavedDesc()
    {}

    /// \brief  In interleaved mode, this field specifies the numerator N,
    ///         for the desired fraction (N/16) of memory accesses to pass address test.
    inline void setFraction(uint32_t fraction) {
        MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_EXPLI_FRACTION_MASK, MEM_DESC_EXPLI_FRACTION_SHIFT, fraction);
    }

    // Default value for eviction normal
    const static uint32_t DEFAULT_HI  = 0x10000000ul;
    const static uint32_t DEFAULT_LOW = 0;
};


#define MEM_DESC_EXPLI_BLOCK_SIZE_MASK        0x00f0000000000000ul
#define MEM_DESC_EXPLI_BLOCK_SIZE_SHIFT       52

#define MEM_DESC_EXPLI_BLOCK_START_MASK       0x0008f00000000000ul
#define MEM_DESC_EXPLI_BLOCK_START_SHIFT      44

#define MEM_DESC_EXPLI_BLOCK_COUNT_MASK       0x00000fe000000000ul
#define MEM_DESC_EXPLI_BLOCK_COUNT_SHIFT      37

class MemExpliBlockTypeDesc : public MemDescriptor
{
public:
    enum BlockSize {        //  defines the block size as  (1 << (12+static_cast(blockSize))
        kBlocksize4K = 0,
        kBlocksize8K = 1,
        kBlocksize16K = 2,
        kBlocksize32K = 3,
        kBlocksize64K = 4,
        kBlocksize128K = 5,
        kBlocksize256K = 6,
        kBlocksize512K = 7,
        kBlocksize1M = 8,
        kBlocksize2M = 9,
        kBlocksize4M = 10,
        kBlocksize8M = 11,
        kBlocksize16M = 12,
        kBlocksize32M = 13,
    };

    MemExpliBlockTypeDesc(L2CopOn policyOn, L2CopOff policyOff) : MemDescriptor()
    {
        setL2DescMode(kDescBlockType);
        setL2CopOn(policyOn);
        setL2CopOff(policyOff);
    }

    ~MemExpliBlockTypeDesc()
    {}

    inline void setBlockSize(BlockSize size) {
        MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_EXPLI_BLOCK_SIZE_MASK, MEM_DESC_EXPLI_BLOCK_SIZE_SHIFT, size);
    }

    inline void setBlockStart(uint32_t start) {
        MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_EXPLI_BLOCK_START_MASK, MEM_DESC_EXPLI_BLOCK_START_SHIFT, start);
    }

    inline void setBlockCount(uint32_t cnt) {
        MEM_DESC_ENCODE_FIELD(desc_, MEM_DESC_EXPLI_BLOCK_COUNT_MASK, MEM_DESC_EXPLI_BLOCK_COUNT_SHIFT, cnt);
    }
};



}


#endif