/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "unittest.h"
#include "core/include/lwrm_handles.h"
#include <set>

namespace
{
    class RMHandleTest: public UnitTest
    {
        public:
            RMHandleTest();
            virtual void Run();

        private:
            void TestForeignHandles();
            void TestGenHandles();
            void TestMixForeignAndGenHandles();
            void TestReuse();
            void TestFreeWithChildren();

            static constexpr UINT32 s_NumHandles = 20;
    };
    ADD_UNIT_TEST(RMHandleTest);

    RMHandleTest::RMHandleTest() :
        UnitTest("RMHandleTest")
    {
    }

    void RMHandleTest::Run()
    {
        TestForeignHandles();
        TestGenHandles();
        TestMixForeignAndGenHandles();
        TestReuse();
        TestFreeWithChildren();
    }

    void RMHandleTest::TestForeignHandles()
    {
        Utility::HandleStorage handles;

        constexpr UINT32 parent = 0U; // null object

        const auto GenHandle = [](UINT32 i) -> UINT32
        {
            return (1U << 31) | i;
        };

        // Generate handles
        for (UINT32 i = 0; i < s_NumHandles; i++)
        {
            const auto handle = GenHandle(i);
            UNIT_ASSERT_EQ(handles.GeneratedHandle(handle), false);
            handles.RegisterForeignHandle(handle, parent);
        }

        UNIT_ASSERT_EQ(handles.NumHandles(), s_NumHandles);
        UNIT_ASSERT_EQ(handles.Empty(),      false);

        // Walk handles with iterator, ensure all are visited
        UINT32 found = 0;
        UINT32 total = 0;
        for (auto h: handles)
        {
            ++found;
            UNIT_ASSERT_EQ(h.parent, parent);
            const UINT32 idx = h.handle & 0xFFU;
            total += idx;
            UNIT_ASSERT_LT(idx, s_NumHandles);
            UNIT_ASSERT_EQ(h.handle & ~0xFFU, (1U << 31));
        }
        UNIT_ASSERT_EQ(found, s_NumHandles);
        UNIT_ASSERT_EQ(total, ((s_NumHandles - 1U) * s_NumHandles) >> 1);

        // Free handles
        for (UINT32 i = 0; i < s_NumHandles; i++)
        {
            const auto handle = GenHandle(i);
            UNIT_ASSERT_EQ(handles.GetParent(handle), parent);
            handles.Free(handle);
        }

        UNIT_ASSERT_EQ(handles.NumHandles(), 0U);
        UNIT_ASSERT_EQ(handles.Empty(),      true);
    }

    void RMHandleTest::TestGenHandles()
    {
        Utility::HandleStorage handles;

        set<UINT32> existingHandles;

        constexpr UINT32 parent = 0U; // null object

        // Generate handles
        for (UINT32 i = 0; i < s_NumHandles; i++)
        {
            const auto handle = handles.GenHandle(handles.UpperBits, parent);
            UNIT_ASSERT_EQ(handles.GeneratedHandle(handle), true);

            const bool alreadyExists = existingHandles.find(handle) != existingHandles.end();
            UNIT_ASSERT_EQ(alreadyExists, false);

            existingHandles.insert(handle);
        }

        UNIT_ASSERT_EQ(handles.NumHandles(), s_NumHandles);
        UNIT_ASSERT_EQ(handles.Empty(),      false);

        // Walk handles with iterator, ensure all are visited
        UINT32 found = 0;
        set<UINT32> noDupes = existingHandles;
        for (auto h: handles)
        {
            ++found;
            UNIT_ASSERT_EQ(h.parent, parent);

            const bool alreadyExists = noDupes.find(h.handle) != noDupes.end();
            UNIT_ASSERT_EQ(alreadyExists, true);

            noDupes.erase(h.handle);
        }
        UNIT_ASSERT_EQ(found, s_NumHandles);

        // Free handles
        for (auto handle: existingHandles)
        {
            handles.Free(handle);
        }

        UNIT_ASSERT_EQ(handles.NumHandles(), 0U);
        UNIT_ASSERT_EQ(handles.Empty(),      true);
    }

    void RMHandleTest::TestMixForeignAndGenHandles()
    {
        Utility::HandleStorage handles;

        set<UINT32> existingHandles;

        constexpr UINT32 parent = 0U; // null object

        const auto GenHandle = [](UINT32 i) -> UINT32
        {
            return (1U << 31) | i;
        };

        // Generate handles
        for (UINT32 i = 0; i < s_NumHandles; i++)
        {
            const auto fhandle = GenHandle(i);
            UNIT_ASSERT_EQ(handles.GeneratedHandle(fhandle), false);
            handles.RegisterForeignHandle(fhandle, parent);

            const auto ghandle = handles.GenHandle(handles.UpperBits, parent);
            UNIT_ASSERT_EQ(handles.GeneratedHandle(ghandle), true);

            const bool alreadyExists = existingHandles.find(ghandle) != existingHandles.end();
            UNIT_ASSERT_EQ(alreadyExists, false);

            existingHandles.insert(ghandle);
        }

        UNIT_ASSERT_EQ(handles.NumHandles(), s_NumHandles * 2U);
        UNIT_ASSERT_EQ(handles.Empty(),      false);

        // Walk handles with iterator, ensure all are visited
        UINT32 found = 0;
        UINT32 total = 0;
        set<UINT32> noDupes = existingHandles;
        for (auto h: handles)
        {
            ++found;
            UNIT_ASSERT_EQ(h.parent, parent);

            if (handles.GeneratedHandle(h.handle))
            {
                const bool alreadyExists = noDupes.find(h.handle) != noDupes.end();
                UNIT_ASSERT_EQ(alreadyExists, true);

                noDupes.erase(h.handle);
            }
            else
            {
                const UINT32 idx = h.handle & 0xFFU;
                total += idx;
                UNIT_ASSERT_LT(idx, s_NumHandles);
                UNIT_ASSERT_EQ(h.handle & ~0xFFU, (1U << 31));
            }
        }
        UNIT_ASSERT_EQ(found, s_NumHandles * 2U);
        UNIT_ASSERT_EQ(total, ((s_NumHandles - 1U) * s_NumHandles) >> 1);

        // Free handles
        for (UINT32 i = 0; i < s_NumHandles; i++)
        {
            const auto handle = GenHandle(i);
            UNIT_ASSERT_EQ(handles.GetParent(handle), parent);
            handles.Free(handle);
        }
        for (auto h: existingHandles)
        {
            handles.Free(h);
        }

        UNIT_ASSERT_EQ(handles.NumHandles(), 0U);
        UNIT_ASSERT_EQ(handles.Empty(),      true);
    }

    void RMHandleTest::TestReuse()
    {
        Utility::HandleStorage handles;

        set<UINT32> existingHandles;

        constexpr UINT32 parent = 0U; // null object

        // Generate handles
        for (UINT32 i = 0; i < s_NumHandles; i++)
        {
            const auto handle = handles.GenHandle(handles.UpperBits, parent);
            UNIT_ASSERT_EQ(handles.GeneratedHandle(handle), true);

            const bool alreadyExists = existingHandles.find(handle) != existingHandles.end();
            UNIT_ASSERT_EQ(alreadyExists, false);

            existingHandles.insert(handle);
        }

        UNIT_ASSERT_EQ(handles.NumHandles(), s_NumHandles);
        UNIT_ASSERT_EQ(handles.Empty(),      false);

        // Erase every other handle
        set<UINT32> origHandles = existingHandles;
        // Erase first, so don't erase last.  The last one has the highest value,
        // so it keeps the vector size.
        bool erase = true;
        for (auto h: handles)
        {
            if (erase)
            {
                handles.Free(h.handle);
                existingHandles.erase(h.handle);
            }

            erase = ! erase;
        }

        UNIT_ASSERT_EQ(handles.NumHandles(), s_NumHandles >> 1);
        UNIT_ASSERT_EQ(handles.Empty(),      false);

        // Generate again, should obtain the same handles as before
        for (UINT32 i = 0; i < (s_NumHandles >> 1); i++)
        {
            const auto handle = handles.GenHandle(handles.UpperBits, parent);
            UNIT_ASSERT_EQ(handles.GeneratedHandle(handle), true);

            const bool alreadyExists = existingHandles.find(handle) != existingHandles.end();
            UNIT_ASSERT_EQ(alreadyExists, false);

            existingHandles.insert(handle);

            const bool exitedBefore = origHandles.find(handle) != origHandles.end();
            UNIT_ASSERT_EQ(exitedBefore, true);
        }

        UNIT_ASSERT_EQ(handles.NumHandles(), s_NumHandles);
        UNIT_ASSERT_EQ(handles.Empty(),      false);

        // Free handles
        for (auto handle: existingHandles)
        {
            handles.Free(handle);
        }

        UNIT_ASSERT_EQ(handles.NumHandles(), 0U);
        UNIT_ASSERT_EQ(handles.Empty(),      true);
    }

    void RMHandleTest::TestFreeWithChildren()
    {
        Utility::HandleStorage handles;

        UINT32 parent[2];

        const auto GenHandle = [](UINT32 i) -> UINT32
        {
            return (1U << 31) | i;
        };

        constexpr UINT32 nullobj    = 0U;

        // Run the test twice, once delete parent[0] first, the second time delete parent[1] first
        for (UINT32 order = 0; order < 2; order++)
        {
            set<UINT32> existingHandles[2];

            // Generate parents
            for (UINT32 i = 0; i < 2; i++)
            {
                parent[i] = handles.GenHandle(handles.UpperBits, nullobj);
                UNIT_ASSERT_EQ(handles.GeneratedHandle(parent[i]), true);

                if (i == 1)
                {
                    UNIT_ASSERT_NEQ(parent[0], parent[1]);
                }
            }

            // Generate handles
            for (UINT32 i = 0; i < s_NumHandles; i++)
            {
                const auto idx     = i & 1;
                const auto phandle = parent[idx];

                const auto fhandle = GenHandle(i);
                UNIT_ASSERT_EQ(handles.GeneratedHandle(fhandle), false);
                handles.RegisterForeignHandle(fhandle, phandle);

                existingHandles[idx].insert(fhandle);

                const auto ghandle = handles.GenHandle(handles.UpperBits, phandle);
                UNIT_ASSERT_EQ(handles.GeneratedHandle(ghandle), true);

                const bool alreadyExistsG = existingHandles[idx].find(ghandle)
                                            != existingHandles[idx].end();
                UNIT_ASSERT_EQ(alreadyExistsG, false);

                existingHandles[idx].insert(ghandle);
            }

            UNIT_ASSERT_EQ(handles.NumHandles(), s_NumHandles * 2U + 2U);
            UNIT_ASSERT_EQ(handles.Empty(),      false);

            // Free first parent with children
            handles.FreeWithChildren(parent[order]);

            UNIT_ASSERT_EQ(handles.NumHandles(), s_NumHandles + 1U);
            UNIT_ASSERT_EQ(handles.Empty(),      false);

            UNIT_ASSERT_EQ(handles.GetParent(parent[order]), handles.Invalid);
            UNIT_ASSERT_EQ(handles.GetParent(parent[order ^ 1]), nullobj);

            for (auto h: existingHandles[order])
            {
                UNIT_ASSERT_EQ(handles.GetParent(h), handles.Invalid);
            }
            for (auto h: existingHandles[order ^ 1])
            {
                const auto phandle = handles.GetParent(h);
                UNIT_ASSERT_EQ(phandle, parent[order ^ 1]);
            }

            // Free second parent with children
            handles.FreeWithChildren(parent[order ^ 1]);

            UNIT_ASSERT_EQ(handles.NumHandles(), 0U);
            UNIT_ASSERT_EQ(handles.Empty(),      true);
            for (auto h: existingHandles[order ^ 1])
            {
                UNIT_ASSERT_EQ(handles.GetParent(h), handles.Invalid);
            }
        }
    }
}
