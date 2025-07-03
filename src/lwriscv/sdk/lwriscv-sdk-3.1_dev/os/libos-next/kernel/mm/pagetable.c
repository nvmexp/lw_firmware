#include "kernel.h"
#include "pagetable.h"
#include "memorypool.h"
#include "diagnostics.h"
#include <lwmisc.h>
#include "libos.h"
#include "mm/memorypool.h"
#include "mm/objectpool.h"
#include "mm/identity.h"
#include "mm/softmmu.h"
#include "pin.h"

/*
Task.state.radix
 |       'Global'
 |->|----------------|              'Lower'                      
    |  Global PDE 0  | -------> -------------------                'Leaf'
    |----------------|          |  Lower PDE 0     |  ------->  -----------------
    |     . . .      |          |------------------|            |     PTE 0     |
    ------------------          | Virtual MPU PTE  |            |---------------|
                                |------------------|            |   . . .       |
                                |    . . .         |            -----------------
                                --------------------

There are no null pointers in the Global PDE.
For example, An empty Global PDE 0 entry points to zeroMiddle which
is a valid pagedirectory. This removes a branch in softmmu.s

The Lower Page Directory contains either a pointer to a leaf pagetable
or a 2mb virtual mpu entry.  

These pagetables are designed as a frontend for the LWRISCV 1.1 and 2.0
MPUs.  These MPU's are linear scan and support arbitrary sized memory
entries.  This behavior can be accessed by using 2mb Virtual MPU PTEs.
During a map, the kernel will allocate a VirtualMpuEntry for the desired 
contiguous mapping and ensure that all 2mb pages covered by the region
point to that virtual mpu entry. Whenever memory is accessed in that region, 
the kernel will load the MPU data contained in the descriptor. 
For additional performance, the descriptor may specify which 
MPU entry # should be used as lower entry numbers have a quicker L1 TLB miss cost.

These pagetables also support LWRISCV 2.1 hashed MPU format.  In this operating
mode we have the additional restriction that the kernel must slice contiguous
mappings larger than 2mb into 2mb chunks.    

It's important to note that 2mb virtual mpu entries are fundamentally more powerful
than 2mb pages.  For instance, you can map a LIBOS_CONFIG_PAGETABLE_ENTRIESkb contiguous region of memory
at an arbitrary address with a single virtual mpu entry.  This owes to the fact
that the virtual mpu entry contains the mapping VA, PA, and size to load into the
MPU.

It's also important to note that on Turing/Ampere a single virtual mpu entry
can be arbitrarily large. The first time we touch any page in that region, we'll
hit the virtual mpu entry PTE, and load the appropriate MPU entry.  Until that entry is
evicted it will be able to translate any address in that region without another
page fault.
   
Future note: This pagetable will contain blackwell page table entries in
             place of region descriptors.  These are strictly traditional
             power of two pages.


*/


/**
 * @brief PageTableLeaf structure
 * 
 * Page size is LIBOS_CONFIG_PAGESIZE (leaf):
 * 
 *      63                                  7-9      6    5   4   3   2    
 *     ___________________________________________________________________________
 *     | 0 | PA                       |  Reserved |  UC | S | X | W | R | 0  | 0  | 
 *     ----------------------------------------------------------------------------
 * 
 *     Kernel will print Performance warnings on pre-hopper if it detects
 *     4kb page usage.  It can be avoided by by ensuring your linker
 *     scripts place your .text and .data sections at 2mb aligned addresses.    
 */
typedef struct
{
    LwU64 entries[LIBOS_CONFIG_PAGESIZE / sizeof(LwU64)];
} PageTableLeaf;

/**
 * @brief PageTableMiddle structure
 * 
 * Page size is LIBOS_CONFIG_PAGESIZE_MEDIUM
 * 
 * 
 *         63                          10      7-9      6    5   4   3   2    
 *         ___________________________________________________________________________
 *         | 0 | PA of Descriptor         | Reserved |  UC | S | X | W | R | 0  | 0  | 
 *         ----------------------------------------------------------------------------
 *         
 *         Descriptor = Offset from start of LIBOS_CONFIG_SOFTTLB_MAPPING_BASE
 * 
 *         @note: This physical address is an offset from the start of FBGPA
 *             and is 53 bits in width.  
 * 
 *         The descriptor is a small data structure containg the data to load into MPU
 *         virtualAddress
 *         size
 *         physicalAddress
 *         attributes                          
 *         desiredMpuIndex
 * 
 *         If the desiredMpuIndex is zero, the page fault handler will pick a random entry.
 * 
 *     page directory entry:
 * 
 *         63                           0
 *         --------------------------------
 *         | 1 |  Kernel VA of pagetable  |
 *         -------------------------------- 
*/
typedef struct PageTableMiddle
{
    LwS64 entries[LIBOS_CONFIG_PAGESIZE / sizeof(LwU64)]; 
} PageTableMiddle;

#define LIBOS_PAGETABLE_PTE_ACCESS_MASK               0x3ff
#define LIBOS_PAGETABLE_PTE_VIRTUAL_MPU_ADDRESS_SHIFT 10

PageTableMiddle * zeroMiddle;

__attribute__((aligned(LIBOS_CONFIG_PAGESIZE))) PageTableGlobal   kernelGlobalPagetable;

/**
 * @brief Allocates a global PD
 * 
 * @note Global PDE's are not allowed to contain null entries.
 *       As a result they point to a reserved middle PD called "zeroMiddle".
 *       This PD has no valid pages.
 * 
 * @return PageTableGlobal* 
 */
PageTableGlobal * KernelPagetableAllocateGlobal()
{
    LwU64 pa;
    if (LibosOk != KernelMemoryPoolAllocatePage(KernelPoolMemorySet, LIBOS_CONFIG_PAGESIZE_LOG2, &pa))
        return 0;

    PageTableGlobal * page = (PageTableGlobal *)KernelPhysicalIdentityMap(pa);

    for (LwU32 i = 0; i < sizeof(page->entries)/sizeof(*page->entries); i++)
        page->entries[i] = zeroMiddle;                
    
    // There are zero "filled" PDEs in this page
    PageState * state = KernelPageStateByAddress(KernelPhysicalIdentityReverseMap(page), 0);
    state->pagetable.filledEntries = 0;        

    return page;
}

/**
 * @brief Release the memory associated with a Global PD
 * 
 * @param global 
 */
void KernelPagetableDestroy(PageTableGlobal * global)
{
   KernelMemoryPoolRelease(KernelPhysicalIdentityReverseMap(global), LIBOS_CONFIG_PAGESIZE);
}

/**
 * @brief Allocated a middle PD (one level below global)
 * 
 * Middle PDE's may either point to a Lower PD, a VirtualMpuEntry, or contain 0.
 * 
 * @return PageTableMiddle* 
 */
PageTableMiddle * KernelPagetableAllocateMiddle()
{
    LwU64 pa;
    if (LibosOk != KernelMemoryPoolAllocatePage(KernelPoolMemorySet, LIBOS_CONFIG_PAGESIZE_LOG2, &pa))
        return 0;

    PageTableMiddle * page = (PageTableMiddle *)KernelPhysicalIdentityMap(pa);

    memset(page, 0, sizeof(*page));
    
    // There are zero "filled" PDEs in this page
    PageState * state = KernelPageStateByAddress(KernelPhysicalIdentityReverseMap(page), 0);
    state->pagetable.filledEntries = 0;        

    return page;
}

/**
 * @brief Allocate a leaf PD
 * 
 * @return PageTableLeaf* 
 */
PageTableLeaf * KernelPagetableAllocateLeaf()
{
    LwU64 pa;
    if (LibosOk != KernelMemoryPoolAllocatePage(KernelPoolMemorySet, LIBOS_CONFIG_PAGESIZE_LOG2, &pa))
        return 0;

    PageTableLeaf * page = (PageTableLeaf *)KernelPhysicalIdentityMap(pa);

    memset(page, 0, sizeof(*page));
    
    // There are zero "filled" PDEs in this page
    PageState * state = KernelPageStateByAddress(KernelPhysicalIdentityReverseMap(page), 0);
    state->pagetable.filledEntries = 0;        

    return page;
}

/**
 * @brief Initialize the pagetable subsystem
 * 
 */
void KernelInitPagetable()
{   
    //  Create a zero middle page
    zeroMiddle = KernelPagetableAllocateMiddle();
    KernelPanilwnless(zeroMiddle != 0);      

    // Initialize the global page table for the kernel
    for (LwU32 i = 0; i < sizeof(kernelGlobalPagetable.entries)/sizeof(*kernelGlobalPagetable.entries); i++)
        kernelGlobalPagetable.entries[i] = zeroMiddle;   

    KernelAssert(LIBOS_CONFIG_PAGESIZE_LOG2 >= LIBOS_PAGETABLE_PTE_VIRTUAL_MPU_ADDRESS_SHIFT);

#if LIBOS_CONFIG_MPU_HASHED
    // The SoftTLB identity map is at entry 0
    LwU64 softIndex = KernelBootstrapHash1pb(LIBOS_CONFIG_SOFTTLB_MAPPING_BASE);
    KernelPanilwnless(softIndex == LIBOS_CONFIG_MPU_IDENTITY);

    LwU64 textIndex = KernelBootstrapHashMedium(LIBOS_CONFIG_KERNEL_TEXT_BASE);
    KernelPanilwnless(textIndex == LIBOS_CONFIG_MPU_TEXT);

    // The text and data sections are 64-bit bits apart
    LwU64 dataIndex = KernelBootstrapHashMedium(LIBOS_CONFIG_KERNEL_DATA_BASE);
    KernelPanilwnless(dataIndex == LIBOS_CONFIG_MPU_DATA);
#endif

    KernelPoolConstruct(sizeof(VirtualMpuEntry), LIBOS_POOL_VIRTUAL_MPU_ENTRY, 0);
}

/**
 * @brief Validates that a virtualAddress is addressable with the current
 *        3 level page table format.
 * 
 * @param address 
 * @return LwBool 
 */
LwBool KernelPagetableValidAddress(LwU64 virtualAddress)
{
    const LwU64 topBits = 64 - LIBOS_CONFIG_VIRTUAL_ADDRESS_SIZE;

    // Replicate bit (LIBOS_CONFIG_VIRTUAL_ADDRESS_SIZE-1) in bit positions LIBOS_CONFIG_VIRTUAL_ADDRESS_SIZE..63
    // If the address still matches, then it's valid.
    return (((LwS64)(virtualAddress << topBits)) >> topBits) == (LwS64)virtualAddress;
}

/**
 * @brief Finds the page table mapping containing the virtual address.
 *        Fills out the PageMapping structure with a copy of the data from the PTE.
 * 
 *        The output mapping structure holds page reference counts and thus you 
 *        must call KernelPagingMappingRelease when done.
 */
LibosStatus KernelPageMappingPin(PageTableGlobal * table, LwU64 virtualAddress, /* OUT */ PageMapping * mapping)
{
    if (!KernelPagetableValidAddress(virtualAddress))
        return LW_FALSE;

    // Ensure the address is within bounds
    const LwU64 topIndex = (virtualAddress >> LIBOS_CONFIG_PAGESIZE_HUGE_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1);

    PageTableMiddle * m = table->entries[topIndex];
    LwS64 lv = m->entries[(virtualAddress >> LIBOS_CONFIG_PAGESIZE_MEDIUM_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1)];
    
    // Is a 2mb virtual mpu entry 
    if (lv >= 0)
    {
        if (!lv)
            return LW_FALSE;
            
        LwU64 physicalAddressOfDescriptor = lv >> LIBOS_PAGETABLE_PTE_VIRTUAL_MPU_ADDRESS_SHIFT;

        // Pull access from pagetable entry
        mapping->access = lv & LIBOS_PAGETABLE_PTE_ACCESS_MASK;

        // Find the virtual mpu descriptor
        mapping->virtualMpu = (VirtualMpuEntry *)KernelPhysicalIdentityMap(physicalAddressOfDescriptor);

        // Copy mapping from the virtual MPU descriptor
        mapping->virtualAddress = mapping->virtualMpu->virtualAddress;
        mapping->size = mapping->virtualMpu->size;
        mapping->physicalAddress = mapping->virtualMpu->physicalAddress;
    }
    else // lv < 0 : This is a normal page directory
    {
        // Look up the PTE entry
        PageTableLeaf *l = (PageTableLeaf *)lv; 
        LwU64 entry = l->entries[(virtualAddress >> LIBOS_CONFIG_PAGESIZE_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1)];
        if (!entry) 
            return LW_FALSE;
        
        mapping->access = entry & LIBOS_PAGETABLE_PTE_ACCESS_MASK;
        mapping->virtualAddress = virtualAddress &~ (LIBOS_CONFIG_PAGESIZE-1);
        mapping->virtualMpu = 0;
        mapping->physicalAddress = entry &~ (LIBOS_CONFIG_PAGESIZE-1);
        mapping->size = LIBOS_CONFIG_PAGESIZE;
    }

    // Pin the pages if they're in the numa code
    KernelMemoryPoolAcquire(mapping->physicalAddress, mapping->size);

    return LibosOk;
}

/**
 * @brief Releases any reference counts held by a PageMapping structure.
 */
void KernelPageMappingRelease(PageMapping * pageMapping)
{
    KernelMemoryPoolRelease(pageMapping->physicalAddress, pageMapping->size);
}

/**
 * @brief Maps a single page into the pagetable
 * 
 * @param table 
 * @param virtualAddress 
 * @param physicalAddress 
 * @param size 
 * @param access 
 * @return LibosStatus 
 */
LibosStatus KernelPagetableMapPage(PageTableGlobal * table, LwU64 virtualAddress, LwU64 physicalAddress, LwU64 size, LwU64 access)
{
    KernelPanilwnless(size == LIBOS_CONFIG_PAGESIZE);

    if (table == &kernelGlobalPagetable)
        access |= LibosMemoryKernelPrivate;

    // @todo: Should check the entire range
    if (!KernelPagetableValidAddress(virtualAddress))
        return LibosErrorArgument;

    // Ensure the address is within bounds
    const LwU64 topIndex = (virtualAddress >> LIBOS_CONFIG_PAGESIZE_HUGE_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1);

    if (table->entries[topIndex] == zeroMiddle)
    {
        PageTableMiddle * page = KernelPagetableAllocateMiddle();
        if (!page)
            return LibosErrorOutOfMemory;
        table->entries[topIndex] = page;
        /* We don't mantain a count on the root pagetable page */
    }

    PageTableMiddle * midTable = table->entries[topIndex];
    const LwU64 midIndex = (virtualAddress >> LIBOS_CONFIG_PAGESIZE_MEDIUM_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1);

    if (midTable->entries[midIndex] == 0)
    {
        PageTableLeaf * page = KernelPagetableAllocateLeaf();
        if (!page)
            return LibosErrorOutOfMemory;
        midTable->entries[midIndex] = (LwS64)page;
        PageState * state = KernelPageStateByAddress(KernelPhysicalIdentityReverseMap(midTable), 0);
        state->pagetable.filledEntries++;
    }

    PageTableLeaf * leafTable = (PageTableLeaf *)midTable->entries[midIndex];
    LwU64 leafIndex = (virtualAddress >> LIBOS_CONFIG_PAGESIZE_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1);

    if (leafTable->entries[leafIndex]) {
       // There was already a PTE at this location, release the reference count on the page it pointed to
       KernelMemoryPoolRelease(leafTable->entries[leafIndex] &~ 1023, LIBOS_CONFIG_PAGESIZE);
    }
    else {
        // This slot is transition from empty to filled, increase the filled count for this pagetable
        PageState * state = KernelPageStateByAddress(KernelPhysicalIdentityReverseMap(leafTable), 0);
        state->pagetable.filledEntries++;        
    }

    leafTable->entries[leafIndex] = physicalAddress | access;

    // Increment the new page reference count
    KernelMemoryPoolAcquire(physicalAddress, LIBOS_CONFIG_PAGESIZE);

    return LibosOk;
}

/**
 * @brief Clone a the pagetable contents for a user-space task into kernel.
 *        This function ensures all pages are fully accessible.
 *        
 *        This is solely for KernelMapUser.
 * 
 *        Design opens:  We're transitioning from page reference counts to pointers to 
 *                       mapping objects. 
 *  
 * @param table 
 * @param virtualAddress 
 * @param source 
 * @param virtualAddressSource 
 * @param size 
 * @return LibosStatus 
 */
LibosStatus KernelPagetablePinAndMap(PageTableGlobal * table, LwU64 virtualAddress, PageTableGlobal * source, LwU64 virtualAddressSource, LwU64 size)
{
    KernelAssert(KernelRangeDoesntWrap(virtualAddress, size) &&
                 KernelRangeDoesntWrap(virtualAddressSource, size) &&
                 KernelAddressPageAligned(virtualAddress) &&
                 KernelAddressPageAligned(virtualAddressSource) &&
                 KernelAddressPageAligned(size));

    KernelTrace("Pagetable clone: table=%p va=%llx va-src=%llx size=%llx\n", table, virtualAddress, virtualAddressSource, size);

    // Addresses must be aligned. 
    KernelAssert(((virtualAddress) & (LIBOS_CONFIG_PAGESIZE-1)) == 0);
    KernelAssert(((virtualAddressSource) & (LIBOS_CONFIG_PAGESIZE-1)) == 0);

    // @note: Size doesn't need to be page aligned
    for  (LwU64 i = 0; i < size; )
    {
        // Translate the virtual address to a page or virtualMpu Mapping
        PageMapping mapping;
        if (KernelPageMappingPin(source, virtualAddressSource + i, &mapping) != LibosOk)
            goto unmapAndReturn;

        if (mapping.virtualMpu)
        {
            // Create a new virtual mpu entry
            // Slice the entry we do have to match the mapping region
            // Populate each 2mb entry covered by that virtual MPU entry
            // advance i

            KernelPanilwnless(0 /* NYI */);
        }
        else
        {
            // Call contiguous map call
            LibosStatus status = KernelPagetableMapPage(table, 
                                                        virtualAddress + i, 
                                                        mapping.physicalAddress + (virtualAddressSource + i - mapping.virtualAddress),
                                                        mapping.size, 
                                                        mapping.access);
            if (status != LibosOk)
                goto unmapAndReturn;

            i += mapping.size;
        }

        KernelPageMappingRelease(&mapping);
    }
    
    // Ilwalidate the SW TLB for this address range
    // @todo: This API requires page aligned virtualAddress and size
    //        Ensure that's in the contract for this function.
    KernelSoftmmuFlushPages(virtualAddress, size);

    return LibosOk;

unmapAndReturn:
    KernelPagetableClearRange(table, virtualAddress, size);
    return LibosErrorFailed;
}

/**
 * @brief Clears all PTE's in the address range.  Does not ilwalidate the TLB.
 * 
 * @param table 
 * @param virtualAddress 
 * @param size 
 * @return LibosStatus 
 */
LibosStatus KernelPagetableClearRange(PageTableGlobal * table, LwU64 virtualAddress, LwU64 size)
{
    LwU64 end = virtualAddress + size;
    KernelPanilwnless(KernelPagetableValidAddress(virtualAddress));

    while (virtualAddress < end)
    {
        // Ensure the address is within bounds
        const LwU64 topIndex = (virtualAddress >> LIBOS_CONFIG_PAGESIZE_HUGE_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1);

        PageTableMiddle * m = table->entries[topIndex];
        if (m == zeroMiddle) {
            virtualAddress += LIBOS_CONFIG_PAGESIZE_HUGE;
            continue;
        }

        PageTableLeaf *l = (PageTableLeaf *)m->entries[(virtualAddress >> LIBOS_CONFIG_PAGESIZE_MEDIUM_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1)];
        if (l == 0) {
            virtualAddress += LIBOS_CONFIG_PAGESIZE_MEDIUM;
            continue;
        }

        LwU64 entry = l->entries[(virtualAddress >> LIBOS_CONFIG_PAGESIZE_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1)];
        l->entries[(virtualAddress >> LIBOS_CONFIG_PAGESIZE_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1)] = 0;
        
        // 4kb page
        if (entry) 
        {
            // Release the page pointed to by the entry
            KernelMemoryPoolRelease(entry &~ (LIBOS_CONFIG_PAGESIZE-1), LIBOS_CONFIG_PAGESIZE);

            // Release empty page table pages
            PageState * leafState = KernelPageStateByAddress(KernelPhysicalIdentityReverseMap(l), 0);
            if (!--leafState->pagetable.filledEntries)
            {
                KernelMemoryPoolRelease(KernelPhysicalIdentityReverseMap(l), LIBOS_CONFIG_PAGESIZE);

                m->entries[(virtualAddress >> LIBOS_CONFIG_PAGESIZE_MEDIUM_LOG2) & (LIBOS_CONFIG_PAGETABLE_ENTRIES-1)] = 0;
                PageState * midState = KernelPageStateByAddress(KernelPhysicalIdentityReverseMap(m), 0);
                if (!--midState->pagetable.filledEntries) {
                    KernelMemoryPoolRelease(KernelPhysicalIdentityReverseMap(m), LIBOS_CONFIG_PAGESIZE);
                    table->entries[topIndex] = 0;
                }
            }
        }       

        virtualAddress += LIBOS_CONFIG_PAGESIZE;
    }

    return LibosOk;
}
