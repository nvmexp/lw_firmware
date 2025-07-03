/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*
 * @file dmemovl-test.c
 *
 * @details    This file contains all the unit tests for the dmemovl malloc functions
 *
 */

/* ------------------------ System Includes --------------------------------- */
#include "test-macros.h"
#include "regmock.h"
#include <ut/ut.h>
#include "ut/ut_suite.h"
#include "ut/ut_case.h"
#include "ut/ut_assert.h"

/* ------------------------ Application includes ---------------------------- */
#include "dmemovl.h"
#include "lwrtos.h"
#include "lwos_ovl.h"
#include "lwos_ovl_priv.h"

/* ------------------------- Defines ---------------------------------------- */
/* ------------------------- Types Definitions ------------------------------ */
/* ------------------------- External Definitions --------------------------- */
extern LwUPtr _dmem_ovl_start_address[];
extern LwU16  _dmem_ovl_size_lwrrent[];
extern LwU16  _dmem_ovl_size_max[];
extern UPROC_STATIC LwBool LwosBHeapAllocationsBlocked;

/* ------------------------ Mocked Function Prototypes ---------------------- */

/* ------------------------ Unit-Under-Test --------------------------------- */

/* ------------------------ Local Variables --------------------------------- */

/* -------------------------------------------------------------------------- */

UT_SUITE_DEFINE(DMEMOVL,
    UT_SUITE_SET_COMPONENT("LWOS Command Management")
    UT_SUITE_SET_DESCRIPTION("Unit tests for the dmemovl functions")
    UT_SUITE_SET_OWNER("ashenfield")
    UT_SUITE_SETUP_HOOK(NULL)
    UT_SUITE_TEARDOWN_HOOK(NULL))


UT_CASE_DEFINE(DMEMOVL, lwosMalloc,
    UT_CASE_SET_DESCRIPTION("test the lwosMalloc function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMEMOVL_lwosMalloc]"))

/*!
 * @brief      Test the lwosMalloc function
 *
 * @details    The Unit Malloc lwosMalloc test performs two allocations of
 * various sizes and ensures that the returned pointers are sufficiently
 * far apart from each other. The address of the second pointer must not
 * be closer to the first pointer than the size of the first allocation.
 * This ensures that a sufficiently-sized block has been allocated and that
 * The allocater is not returning pointers which trample previously allocated
 * blocks.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(DMEMOVL, lwosMalloc)
{
    LwU32 s1;
    LwU32 s2;

    for (s1 = 1;  s1 <= 32; s1 <<= 1)
    {
        for (s2 = 1; s2 <= 32; s2 <<= 1)
        {
            void *pAddr1 = lwosMalloc_IMPL(OVL_INDEX_OS_HEAP, s1);
            void *pAddr2 = lwosMalloc_IMPL(OVL_INDEX_OS_HEAP, s2);
            LwU32 uAddr1 = (LwU32)pAddr1;
            LwU32 uAddr2 = (LwU32)pAddr2;

            UT_ASSERT_NOT_NULL(pAddr1);
            UT_ASSERT_NOT_NULL(pAddr2);

            //Confirm that the returned addresses are sufficiently far apart
            UT_ASSERT_NOT_IN_RANGE_UINT(uAddr2, uAddr1, uAddr1 + s1 - 1);
            UT_ASSERT_NOT_IN_RANGE_UINT(uAddr1, uAddr2, uAddr2 + s2 - 1);
        }
    }
}


UT_CASE_DEFINE(DMEMOVL, lwosMalloc_hugeSize,
    UT_CASE_SET_DESCRIPTION("test the lwosMalloc function with a huge size")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMEMOVL_lwosMalloc_hugeSize]"))

/*!
 * @brief      Test the lwosMalloc function with a huge allocation
 *
 * @details    Call lwosMalloc requesting an unreasonably large allocation
 * which will not be able to fit in the heap. The function should detect
 * this and return NULL.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(DMEMOVL, lwosMalloc_hugeSize)
{
    //overflow the size variable
    void *pAddr1 = lwosMalloc_IMPL(OVL_INDEX_OS_HEAP, 0x12345678);
    UT_ASSERT_NULL(pAddr1);

    //don't overflow the size variable, but exceed the max allowable size for heap
    void *pAddr2 = lwosMalloc_IMPL(OVL_INDEX_OS_HEAP, ((LwU32) _dmem_ovl_size_max[OVL_INDEX_OS_HEAP]) + 1);
    UT_ASSERT_NULL(pAddr1);
}

UT_CASE_DEFINE(DMEMOVL, lwosMalloc_zero,
    UT_CASE_SET_DESCRIPTION("test the lwosMalloc function with an argument of 0")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMEMOVL_lwosMalloc_zero]"))

/*!
 * @brief      Test the lwosMalloc function with a zero-sized allocation request
 *
 * @details    Call lwosMalloc requesting a zero-sized allocation.
 * The function should detect this and return NULL.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(DMEMOVL, lwosMalloc_zero)
{
    void *pAddr1 = lwosMalloc_IMPL(OVL_INDEX_OS_HEAP, 0);
    UT_ASSERT_NULL(pAddr1);
}

UT_CASE_DEFINE(DMEMOVL, lwosMallocAligned,
    UT_CASE_SET_DESCRIPTION("test the lwosMallocAligned function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMEMOVL_lwosMallocAligned]"))

/*!
 * @brief      Test the lwosMallocAligned function
 *
 * @details    Call lwosMallocAligned requesting various sizes and alignments
 * Ensure that the returned pointer matches the requested alignment.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(DMEMOVL, lwosMallocAligned)
{
    LwU32 align, size;

    for (align = 1;  align <= 64; align <<= 1)
    {
        for (size = 1;  size <= 32; size <<= 1)
        {
            void *pAddr = lwosMallocAligned_IMPL(OVL_INDEX_OS_HEAP, size, align);
            LwU32 uAddr = (LwU32)pAddr;
            UT_ASSERT_NOT_NULL(pAddr);
            UT_ASSERT_EQUAL_UINT((uAddr & (align-1)), 0); 
        }
    }
}

UT_CASE_DEFINE(DMEMOVL, lwosMallocAligned_hugeAlignment,
    UT_CASE_SET_DESCRIPTION("test the lwosMallocAligned function with a huge alignment")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMEMOVL_lwosMallocAligned_hugeAlignment]"))

/*!
 * @brief      Test the lwosMallocAligned function with a huge alignment
 *
 * @details    Call lwosMallocAligned requesting a huge alignment which
 * cannot be fulfilled by our heap. Ensure that the method returns NULL
 * to indicate that the allocation could not be completed.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(DMEMOVL, lwosMallocAligned_hugeAlignment)
{
    void *pAddr1, *pAddr2;

    // We are strarting with an empty heap (_dmem_ovl_size_lwrrent = 0)
    // so the large alignment won't cause a forward shift of _dmem_ovl_size_lwrrent
    pAddr1 = lwosMallocAligned_IMPL(OVL_INDEX_OS_HEAP, 0x8, 0x10);
    UT_ASSERT_NOT_NULL(pAddr1);

    // Now the heap is no longer empty, so the huge alignment will cause a forward shift
    // of _dmem_ovl_size_lwrrent, and the malloc should catch the overflow and return NULL
    pAddr2 = lwosMallocAligned_IMPL(OVL_INDEX_OS_HEAP, 0x8, 0x8000);
    UT_ASSERT_NULL(pAddr2);
}

UT_CASE_DEFINE(DMEMOVL, lwosCalloc,
    UT_CASE_SET_DESCRIPTION("test the lwosCalloc function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMEMOVL_lwosCalloc]"))

/*!
 * @brief      Test the lwosCalloc function
 *
 * @details    Write some nonzero data into the heap. Call the
 * lwosCalloc method to allocate a block and ensure that the data
 * we have previous written has been zeroed out. lwosCalloc wraps
 * lwosCallocAligned.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(DMEMOVL, lwosCalloc)
{
    const LwU32 size = 4;

    //put some garbage in the address we are about to allocate
    LwU32 *pHeap = (LwU32 *)_dmem_ovl_start_address[OVL_INDEX_OS_HEAP];
    *pHeap = 0xDEADBEEF;

    void *pAddr = lwosCalloc_IMPL(OVL_INDEX_OS_HEAP, size);

    //confirm that we got the same address that we just polluted
    UT_ASSERT_EQUAL_PTR(pAddr, ((void *)pHeap));

    //It should now be cleaned
    UT_ASSERT_EQUAL_UINT(*pHeap, 0);

    // huge allocation should fail
    pAddr = lwosCalloc_IMPL(OVL_INDEX_OS_HEAP, 0xFFFFFFFF);
    UT_ASSERT_NULL(pAddr);
}

UT_CASE_DEFINE(DMEMOVL, lwosMalloc_postInit,
    UT_CASE_SET_DESCRIPTION("test the lwosMalloc post-init")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMEMOVL_lwosMalloc_postInit]"))

/*!
 * @brief      Call lwosMalloc after init has completed
 *
 * @details    Call lwosHeapAllocationsBlock() to indicate that
 * Init has finished and further allocations are not allowed. Then
 * call lwosMalloc and ensure that it detects the error and halts the system.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(DMEMOVL, lwosMalloc_postInit)
{
    LwU32 initalHaltCount = UTF_INTRINSICS_MOCK_GET_HALT_COUNT();
    lwosHeapAllocationsBlock_IMPL();
    void *pAddr = lwosMalloc_IMPL(OVL_INDEX_OS_HEAP, 1);

    //reset the blocked flag for future tests
    LwosBHeapAllocationsBlocked = LW_FALSE;
    UT_ASSERT_EQUAL_UINT(initalHaltCount + 1, UTF_INTRINSICS_MOCK_GET_HALT_COUNT());
}

UT_CASE_DEFINE(DMEMOVL, lwosMallocResident,
    UT_CASE_SET_DESCRIPTION("test the lwosMallocResident function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMEMOVL_lwosMallocResident]"))

/*!
 * @brief      Test lwosMallocResident
 *
 * @details    Call lwosMallocResident() and ensure that it successfully allocates
 *             a block of memory and returns a non-NULL pointer.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(DMEMOVL, lwosMallocResident)
{
    void *pAddr1 = lwosMallocResident_IMPL(0x10);

    UT_ASSERT_NOT_NULL(pAddr1);
}

UT_CASE_DEFINE(DMEMOVL, lwosCallocResident,
    UT_CASE_SET_DESCRIPTION("test the lwosCallocResident function")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMEMOVL_lwosCallocResident]"))

/*!
 * @brief      Test the lwosCallocResident function
 *
 * @details    Write some nonzero data into the heap. Call the
 * lwosCallocResident method to allocate a block and ensure that the data
 * we have previous written has been zeroed out.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(DMEMOVL, lwosCallocResident)
{
    const LwU32 size = 4;

    //put some garbage in the address we are about to allocate
    LwU32 *pHeap = (LwU32 *)_dmem_ovl_start_address[OVL_INDEX_OS_HEAP];
    *pHeap = 0xDEADBEEF;

    void *pAddr = lwosCallocResident_IMPL(size);

    //confirm that we got the same address that we just polluted
    UT_ASSERT_EQUAL_PTR(pAddr, ((void *)pHeap));

    //It should now be cleaned
    UT_ASSERT_EQUAL_UINT(*pHeap, 0);
}

UT_CASE_DEFINE(DMEMOVL, lwosOsHeapGetNextFreeByte,
    UT_CASE_SET_DESCRIPTION("test the lwosOsHeapGetNextFreeByte and lwosOsHeapGetFreeHeap functions")
    UT_CASE_SET_TYPE(REQUIREMENTS)
    UT_CASE_LINK_REQS("TODO")
    UT_CASE_LINK_SPECS("SWVS-FUNC-NAME-REF[DMEMOVL_lwosOsHeapGetNextFreeByte]"))

/*!
 * @brief      Test the lwosOsHeapGetNextFreeByte and lwosOsHeapGetFreeHeap functions
 *
 * @details    Call lwosOsHeapGetNextFreeByte() and lwosOsHeapGetFreeHeap() and ensure
 * that the latter returns the entire size of the heap. After this, call lwosCallocResident_IMPL
 * to allocate a block of memory and then call lwosOsHeapGetNextFreeByte() and
 * lwosOsHeapGetFreeHeap() again, ensuring that the values have changed as we would expect
 * after the allocation.
 * 
 * Set up _dmem_ovl_start_address and _dmem_ovl_size_lwrrent in such a way that their sum would
 * overflow. Ensure the the function halts.
 *
 * No external unit dependencies are used.
 */
UT_CASE_RUN(DMEMOVL, lwosOsHeapGetNextFreeByte)
{
    const LwU32 size = 0x10;
    LwUPtr dmem_ovl_start_address_original = _dmem_ovl_start_address[OVL_INDEX_OS_HEAP]; 
    LwU16 dmem_ovl_size_lwrrent = _dmem_ovl_size_lwrrent[OVL_INDEX_OS_HEAP];

    LwUPtr ptr = lwosOsHeapGetNextFreeByte();
    LwU32 freeHeap = lwosOsHeapGetFreeHeap();
    UT_ASSERT_EQUAL_UINT(freeHeap, (LwU32) _dmem_ovl_size_max[OVL_INDEX_OS_HEAP]);
    void *pAddr = lwosCallocResident_IMPL(size);

    UT_ASSERT_EQUAL_UINT(ptr, (LwU32) pAddr);

    ptr = lwosOsHeapGetNextFreeByte();
    UT_ASSERT_EQUAL_UINT(ptr, ((LwU32) pAddr) + size);
    freeHeap = lwosOsHeapGetFreeHeap();
    UT_ASSERT_EQUAL_UINT(freeHeap, ((LwU32) _dmem_ovl_size_max[OVL_INDEX_OS_HEAP]) - size);
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 0);

    _dmem_ovl_start_address[OVL_INDEX_OS_HEAP] = 0xffffffff; 
    _dmem_ovl_size_lwrrent[OVL_INDEX_OS_HEAP] = 0x4;
    ptr = lwosOsHeapGetNextFreeByte();
    _dmem_ovl_start_address[OVL_INDEX_OS_HEAP] = dmem_ovl_start_address_original; 
    _dmem_ovl_size_lwrrent[OVL_INDEX_OS_HEAP] = dmem_ovl_size_lwrrent;
    UT_ASSERT_EQUAL_UINT(UTF_INTRINSICS_MOCK_GET_HALT_COUNT(), 1);
}
