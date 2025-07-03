/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   unit_tasker.cpp
 * @brief  Unit tests for the Tasker.
 */

#include "unittest.h"
#include "core/include/cpu.h"
#include "core/include/massert.h"

namespace
{
    struct MemCopyTest: public UnitTest
    {
        MemCopyTest() : UnitTest("MemCopy") { }
        virtual ~MemCopyTest() = default;
        void Run() override
        {
            // Copy zero bytes - must not crash
            Cpu::MemCopy(nullptr, nullptr, 0);

            // Copy one byte
            {
                UINT08 src  = 1;
                UINT08 dest = 2;
                Cpu::MemCopy(&dest, &src, sizeof(src));
                UNIT_ASSERT_EQ(src,  1);
                UNIT_ASSERT_EQ(dest, 1);
            }

            // Copy one UINT32
            {
                UINT32 src  = 0xDEADBEADU;
                UINT32 dest = 0xC0DECAFEU;
                Cpu::MemCopy(&dest, &src, sizeof(src));
                UNIT_ASSERT_EQ(src,  0xDEADBEADU);
                UNIT_ASSERT_EQ(dest, 0xDEADBEADU);
            }

            // Copy one UINT64
            {
                UINT64 src  = 0xDEADBEADF00DC0DAULL;
                UINT64 dest = 0xC0DECAFEEFACED0LWLL;
                Cpu::MemCopy(&dest, &src, sizeof(src));
                UNIT_ASSERT_EQ(src,  0xDEADBEADF00DC0DAULL);
                UNIT_ASSERT_EQ(dest, 0xDEADBEADF00DC0DAULL);
            }

            // Copy with different offsets
            for (UINT32 startOffs = 0; startOffs <= 8; startOffs++)
            {
                for (UINT32 endOffs = 0; endOffs <= 8; endOffs++)
                {
                    for (UINT32 size = 16; size <= 24; size += 8)
                    {
                        for (UINT32 shift = 0; shift <= 8; shift++)
                        {
                            vector<UINT08> src(size);
                            vector<UINT08> dest(size + shift);

                            UINT32 i;
                            for (i = 0; i < size; i++)
                            {
                                src[i] = static_cast<UINT08>(i);
                            }
                            for (i = 0; i < size + shift; i++)
                            {
                                dest[i] = static_cast<UINT08>(0xFFU - i);
                            }

                            MASSERT(size >= (startOffs + endOffs));
                            MASSERT(size - startOffs - endOffs <= src.size());
                            MASSERT(shift + size - startOffs - endOffs <= dest.size());
                            const UINT32 copySize    = size - startOffs - endOffs;
                            const UINT32 destOffs    = startOffs + shift;
                            const UINT32 destEndOffs = destOffs + copySize;
                            Cpu::MemCopy(&dest[destOffs],
                                         &src[startOffs],
                                         size - startOffs - endOffs);

                            for (i = 0; i < size; i++)
                            {
                                UNIT_ASSERT_EQ(src[i], static_cast<UINT08>(i));
                            }
                            for (i = 0; i < size + shift; i++)
                            {
                                if ((i < destOffs) || (i >= destEndOffs))
                                {
                                    const UINT08 expected = static_cast<UINT08>(
                                                                0xFFU - i);
                                    UNIT_ASSERT_EQ(dest[i], expected);
                                }
                                else
                                {
                                    const UINT08 expected = static_cast<UINT08>(
                                                                i - destOffs + startOffs);
                                    UNIT_ASSERT_EQ(dest[i], expected);
                                }
                            }
                        }
                    }
                }
            }
        }
    };

    ADD_UNIT_TEST(MemCopyTest);

    struct MemSetTest: public UnitTest
    {
        MemSetTest() : UnitTest("MemSet") { }
        virtual ~MemSetTest() = default;
        void Run() override
        {
            // Set zero bytes - must not crash
            Cpu::MemSet(nullptr, 0, 0);

            // Set one byte
            {
                UINT08 dest = 1;
                Cpu::MemSet(&dest, 2, sizeof(dest));
                UNIT_ASSERT_EQ(dest, 2);
            }

            // Set one UINT32
            {
                UINT32 dest = 0xCC00CC00U;
                Cpu::MemSet(&dest, 0xABU, sizeof(dest));
                UNIT_ASSERT_EQ(dest, 0xABABABABU);
            }

            // Set one UINT64
            {
                UINT64 dest = 0xCC00CC00CC00CC00ULL;
                Cpu::MemSet(&dest, 0xABU, sizeof(dest));
                UNIT_ASSERT_EQ(dest, 0xABABABABABABABABULL);
            }

            // Set with different offsets
            for (UINT32 startOffs = 0; startOffs <= 8; startOffs++)
            {
                for (UINT32 endOffs = 0; endOffs <= 8; endOffs++)
                {
                    for (UINT32 size = 16; size <= 24; size += 8)
                    {
                        vector<UINT08> dest(size, 0xCDU);

                        MASSERT(size >= startOffs + endOffs);

                        Cpu::MemSet(&dest[startOffs],
                                    0xABU,
                                    size - startOffs - endOffs);

                        for (UINT32 i = 0; i < size; i++)
                        {
                            if ((i < startOffs) || (i >= size - endOffs))
                            {
                                const UINT08 expected = 0xCDU;
                                UNIT_ASSERT_EQ(dest[i], expected);
                            }
                            else
                            {
                                const UINT08 expected = 0xABU;
                                UNIT_ASSERT_EQ(dest[i], expected);
                            }
                        }
                    }
                }
            }
        }
    };

    ADD_UNIT_TEST(MemSetTest);

} // anonymous namespace
