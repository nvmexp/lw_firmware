/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010,2014-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   unit_cirbuffer.cpp
 * @brief  Unit tests for the cirlwlar buffer
 */

#include "unittest.h"
#include "core/include/xp.h"
#include "core/include/cirlwlarbuffer.h"

namespace
{
    class CirBufTest : public UnitTest
    {
    public:
        CirBufTest();
        virtual ~CirBufTest();
        virtual void Run();
    private:
        void SimplePushPop();
        void CheckInterators();
        void CheckResize();
        void CheckReserve();
        void CheckCopyAssignment();

        CirBuffer<UINT32> ReturnCirBuffer();
        void PrintBuffer(CirBuffer<UINT32> &MyBuffer);

        CirBuffer<UINT32> m_MyBuffer;
    };

    CirBufTest::CirBufTest()
     : UnitTest("CirBufferTest")
    {
    }

    CirBufTest::~CirBufTest()
    {
    }

    ADD_UNIT_TEST(CirBufTest);

    // Meat of the test
    void CirBufTest::Run()
    {
        SimplePushPop();
        CheckInterators();
        CheckResize();
        CheckReserve();
        CheckCopyAssignment();
    }

    // Subtest 1
    void CirBufTest::SimplePushPop()
    {
        Printf(Tee::PriLow, "----SimplePushPop: < capacity----\n");
        CirBuffer<UINT32> MyBuffer(5);
        // push in less than capacity, then pop_back
        for (UINT32 i = 0; i < 3; i++)
        {
            MyBuffer.push_back(i);
        }
        UINT32 Size = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, 3);

        // Check Front/Back:
        UINT32 Front = MyBuffer.front();
        UINT32 Back  = MyBuffer.back();
        UNIT_ASSERT_EQ(Front, 0);
        UNIT_ASSERT_EQ(Back, 2);
        Printf(Tee::PriLow, " pop_back Val = ");
        for (UINT32 i = 0; i < Size; i++)
        {
            UINT32 Val = MyBuffer.pop_back();
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, (Size - i - 1));
        }
        Size = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, 0);

        // push in less than capacity, then pop_front (moves m_EndIdx)
        for (UINT32 i = 0; i < 3; i++)
        {
            MyBuffer.push_back(i);
        }
        Size = MyBuffer.size();
        Printf(Tee::PriLow, "\n size = %d\n pop_front Val =", Size);
        for (UINT32 i = 0; i < Size; i++)
        {
            UINT32 Val = MyBuffer.pop_front();
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, i);
        }
        Size = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, 0);
        Printf(Tee::PriLow, "\nbegin=%ld, end=%ld\n",
               static_cast<unsigned long>(MyBuffer.GetBeginIdx()),
               static_cast<unsigned long>(MyBuffer.GetEndIdx()));

        // NOTE: At this point m_BeginIdx = 3 because of pop_front

        Printf(Tee::PriLow, "SimplePushPop: @ capacity.\n");
        // push in more than capacity (just overwrite), then pop_front
        for (UINT32 i = 0; i < 7; i++)
        {
            MyBuffer.push_back(i);
        }

        PrintBuffer(MyBuffer); // "3 4 5 6 1e 2b" . '1' is the bubble

        Size = MyBuffer.size();
        // validates get size for the case where End < Begin
        UNIT_ASSERT_EQ(Size, 5);
        Printf(Tee::PriLow, "Size = %d\n  pop_back Val =", Size);

        Size = MyBuffer.size();
        for (UINT32 i = 0; i < Size; i++)
        {
            // this covers pop_back() with m_EndIdx == 0
            UINT32 Val = MyBuffer.pop_back();
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, (6 - i));
        }

        Printf(Tee::PriLow, "\nbegin=%ld, end=%ld\n",
               static_cast<unsigned long>(MyBuffer.GetBeginIdx()),
               static_cast<unsigned long>(MyBuffer.GetEndIdx()));
        UINT32 BeginIdx = MyBuffer.GetBeginIdx();
        UINT32 EndIdx   = MyBuffer.GetEndIdx();
        UNIT_ASSERT_EQ(BeginIdx, 5);
        UNIT_ASSERT_EQ(EndIdx, 5);

        for (UINT32 i = 0; i < 7; i++)
        {
            MyBuffer.push_back(i);
        }

        PrintBuffer(MyBuffer); //"1e 2b 3 4 5 6" . '1' is the bubble

        Front = MyBuffer.front();
        Back  = MyBuffer.back();
        UNIT_ASSERT_EQ(Front, 2);
        UNIT_ASSERT_EQ(Back, 6);

        Size = MyBuffer.size();
        Printf(Tee::PriLow, "Size = %d\n  pop_front Val =", Size);
        BeginIdx = MyBuffer.GetBeginIdx();
        EndIdx   = MyBuffer.GetEndIdx();
        UNIT_ASSERT_EQ(BeginIdx, 1);
        UNIT_ASSERT_EQ(EndIdx, 0);
        for (UINT32 i = 0; i < Size; i++)
        {
            UINT32 Val = MyBuffer.pop_front();
            Printf(Tee::PriLow, " %d", Val);
        }
        Printf(Tee::PriLow, "\n");
        // we should be back at size 0 now:
        Size = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, 0);
        BeginIdx = MyBuffer.GetBeginIdx();
        EndIdx   = MyBuffer.GetEndIdx();
        UNIT_ASSERT_EQ(EndIdx, 0);
        UNIT_ASSERT_EQ(BeginIdx, 0);
    }

    // Subtest 2
    void CirBufTest::CheckInterators()
    {
        Printf(Tee::PriLow, "----Iterators:----\n");
        CirBuffer<UINT32> MyBuffer(5);
        // push in less than capacity, then pop_back
        for (UINT32 i = 0; i < 4; i++)
        {
            MyBuffer.push_back(i);
        }
        // use pop_front to shift the m_BeginIdx forward by 1
        MyBuffer.pop_front();
        UINT32 BeginIdx = MyBuffer.GetBeginIdx();
        UNIT_ASSERT_EQ(BeginIdx, 1);
        MyBuffer.push_back(4);
        MyBuffer.push_back(5);
        UINT32 EndIdx = MyBuffer.GetEndIdx();
        UNIT_ASSERT_EQ(EndIdx, 0);

        PrintBuffer(MyBuffer); //"0e 1b 2 3 4 5" . '0' is the bubble

        // iterate from start to end
        Printf(Tee::PriLow, "  f_iter val = ");
        CirBuffer<UINT32>::iterator iter = MyBuffer.begin();
        for (UINT32 IterIdx = 1; iter != MyBuffer.end(); iter++, IterIdx++)
        {
            UINT32 Val = *iter;
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, IterIdx);
        }
        //++iter; // causes assert

        size_t TailIdx = MyBuffer.GetEndIdx() - 1;
        if (0 == MyBuffer.GetEndIdx())
            TailIdx = MyBuffer.capacity();

        iter = CirBuffer<UINT32>::iterator(&MyBuffer, TailIdx);
        for (UINT32 IterIdx = 5; iter != MyBuffer.rend(); --iter, IterIdx--)
        {
            UINT32 Val = *iter;
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, IterIdx);
        }
        //--iter; // causes assert

        Printf(Tee::PriLow, "\n  r_iter val = ");
        CirBuffer<UINT32>::reverse_iterator riter = MyBuffer.rbegin();
        for (UINT32 IterIdx = 5; riter != MyBuffer.end(); riter++, IterIdx--)
        {
            UINT32 Val = *riter;
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, IterIdx);
        }
        Printf(Tee::PriLow, "\n");
        MyBuffer.push_back(6);
        MyBuffer.push_back(7);
        PrintBuffer(MyBuffer); //"6 7 2e 3b 4 5"

        // use ++c_iter
        Printf(Tee::PriLow, "  f_c_iter val = ");
        CirBuffer<UINT32>::const_iterator c_iter = MyBuffer.begin();
        for (UINT32 IterIdx = 3; c_iter != MyBuffer.end(); ++c_iter, IterIdx++)
        {
            UINT32 Val = *c_iter;
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, IterIdx);
        }
        // use ++r_c_interator
        Printf(Tee::PriLow, "\n  r_c_iter val = ");
        CirBuffer<UINT32>::const_reverse_iterator r_c_iter = MyBuffer.rbegin();
        for (UINT32 IterIdx = 7; r_c_iter != MyBuffer.rend(); ++r_c_iter, IterIdx--)
        {
            UINT32 Val = *r_c_iter;
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, IterIdx);
        }
        //++r_c_iter; // causes assert

        size_t HeadIdx = MyBuffer.GetBeginIdx();
        r_c_iter = CirBuffer<UINT32>::const_reverse_iterator(&MyBuffer, HeadIdx);
        for (UINT32 IterIdx = 3; r_c_iter != MyBuffer.end(); --r_c_iter, IterIdx++)
        {
            UINT32 Val = *r_c_iter;
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, IterIdx);
        }
        //--r_c_iter; // causes assert

        // cross assignment test - check inheritance:
        iter = MyBuffer.begin();
        c_iter = iter ;
        Printf(Tee::PriLow, "\lwal of c_iter = iter = %d\n", *iter);

        CirBuffer<UINT32>::const_iterator & citer2 = iter;
        Printf(Tee::PriLow, "val of & citer2 = iter = %d\n", *citer2);

        riter = MyBuffer.rbegin();
        r_c_iter = riter ;
        Printf(Tee::PriLow, "val of r_c_iter = riter = %d\n", *riter);

        // these would cause compile errors
/*
        CirBuffer<UINT32>::iterator & iter2 = c_iter;
        Printf(Tee::PriLow, "val of & iter2 = c_iter = %d\n", *iter2);

        c_iter = MyBuffer.begin();
        iter = c_iter;
        Printf(Tee::PriLow, "val of iter = c_iter = %d\n", *iter);

        r_c_iter = MyBuffer.rbegin();
        riter = r_c_iter;
        Printf(Tee::PriLow, "val of riter = r_c_iter = %d\n", *riter);
*/
        // test -- iterator with size < capacity
        MyBuffer.pop_front();
        PrintBuffer(MyBuffer); //"6 7 NAe NA 4b 5"
        HeadIdx = MyBuffer.GetBeginIdx();
        r_c_iter = CirBuffer<UINT32>::const_reverse_iterator(&MyBuffer, HeadIdx);
        for (UINT32 IterIdx = 4; r_c_iter != MyBuffer.end(); --r_c_iter, IterIdx++)
        {
            UINT32 Val = *r_c_iter;
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, IterIdx);
        }
        //--r_c_iter; // causes assert
        TailIdx = MyBuffer.GetEndIdx() - 1;
        if (0 == MyBuffer.GetEndIdx())
            TailIdx = MyBuffer.capacity();

        iter = CirBuffer<UINT32>::iterator(&MyBuffer, TailIdx);
        for (UINT32 IterIdx = 7; iter != MyBuffer.rend(); --iter, IterIdx--)
        {
            UINT32 Val = *iter;
            Printf(Tee::PriLow, " %d", Val);
            UNIT_ASSERT_EQ(Val, IterIdx);
        }
        //--iter;  // causes assert
        Printf(Tee::PriLow, "\n");
    }

    // Subtest 3
    void CirBufTest::CheckResize()
    {
        Printf(Tee::PriLow, "----CheckResize:----\n");
        CirBuffer<UINT32> MyBuffer(7);
        for (UINT32 i = 0; i < 4; i++)
        {
            MyBuffer.push_back(i);
        }
        PrintBuffer(MyBuffer);

        // size should remain the same
        MyBuffer.resize(7);
        UINT32 Size = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, 4);
        MyBuffer.resize(20);
        Size = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, 4);

        // downsize to 2
        Printf(Tee::PriLow, "  resize to 2\n");
        MyBuffer.resize(2);
        Size = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, 2);
        // Only the end index should move
        UINT32 EndIdx   = MyBuffer.GetEndIdx();
        UINT32 BeginIdx = MyBuffer.GetBeginIdx();
        UNIT_ASSERT_EQ(EndIdx, 2);
        UNIT_ASSERT_EQ(BeginIdx, 0);
        PrintBuffer(MyBuffer);

        // push things in again. Let's check out wrap around
        Printf(Tee::PriLow, "Now push more in to have a a wrap around\n");
        for (UINT32 i = 2; i < 8; i++)
        {
            MyBuffer.push_back(i);
        }
        // Should be: "0e 1b 2 3 4 5 6 7", 0 is the bubble
        PrintBuffer(MyBuffer);
        Printf(Tee::PriLow, "  resize to 4 elements\n");
        MyBuffer.resize(4);
        PrintBuffer(MyBuffer);
        Size = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, 4);
        EndIdx   = MyBuffer.GetEndIdx();
        UNIT_ASSERT_EQ(EndIdx, 5);  // 5 is the bubble

        Printf(Tee::PriLow, "  resize by adding 9, but to size of 8 - invalid, so no-op\n");
        MyBuffer.resize(8, 9);
        Size = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, 4);    // last resize should be no-op
        PrintBuffer(MyBuffer);

        // Try resize with adding 9 as padding
        Printf(Tee::PriLow, "  resize by adding 9 as padding\n");
        MyBuffer.resize(7, 9);
        PrintBuffer(MyBuffer);
        Size = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, 7);
        EndIdx   = MyBuffer.GetEndIdx();
        BeginIdx = MyBuffer.GetBeginIdx();
        // 0 is the bubble again, and  begin index doesn't change
        UNIT_ASSERT_EQ(EndIdx, 0);
        UNIT_ASSERT_EQ(BeginIdx, 1);
        // make sure the back is actualy 9 - the padded value
        UINT32 Back = MyBuffer.back();
        UNIT_ASSERT_EQ(Back, 9);
    }

    // Subtest 4
    void CirBufTest::CheckReserve()
    {
        Printf(Tee::PriLow, "----CheckReserve:----\n");
        CirBuffer<UINT32> MyBuffer(5);

        // capacity check. Size should not change
        UINT32 Capacity = MyBuffer.capacity();
        UINT32 Size     = MyBuffer.size();
        UNIT_ASSERT_EQ(Capacity, 5);
        UNIT_ASSERT_EQ(Size, 0);

        // make sure that capacity doesn't change if newSize is smaller
        MyBuffer.reserve(1);
        UNIT_ASSERT_EQ(Capacity, 5);

        // fill out the current cir buffer and have overlap so m_EndIdx < m_BeginIdx
        for (UINT32 i = 0; i < 8; i++)
        {
            MyBuffer.push_back(i);
        }
        PrintBuffer(MyBuffer); // 6 7 2e 3b 4 5
        Size = MyBuffer.size();

        MyBuffer.reserve(8);
        PrintBuffer(MyBuffer); // 3b 4 5 6 7 0e
        UINT32 NewSize = MyBuffer.size();
        UNIT_ASSERT_EQ(Size, NewSize);

        // check the values using iterator
        CirBuffer<UINT32>::const_iterator c_iter = MyBuffer.begin();
        for (UINT32 IterIdx = 3; c_iter != MyBuffer.end(); ++c_iter, IterIdx++)
        {
            UINT32 Val = *c_iter;
            UNIT_ASSERT_EQ(Val, IterIdx);
        }
    }

    void CirBufTest::CheckCopyAssignment()
    {
        Printf(Tee::PriLow, "----CheckCopyAssignment:----\n");
        m_MyBuffer.reserve(4);
        for (UINT32 i = 0; i < 4; i++)
        {
            m_MyBuffer.push_back(i);
        }
        PrintBuffer(m_MyBuffer);

        // assignment
        CirBuffer<UINT32> DstBuffer;
        DstBuffer = m_MyBuffer;
        UINT32 Capacity = DstBuffer.capacity();
        UINT32 Size     = DstBuffer.size();
        UNIT_ASSERT_EQ(Capacity, 4);
        UNIT_ASSERT_EQ(Size, 4);
        CirBuffer<UINT32>::const_iterator c_iter = DstBuffer.begin();
        for (UINT32 IterIdx = 0; c_iter != DstBuffer.end(); ++c_iter, IterIdx++)
        {
            UINT32 Val = *c_iter;
            UNIT_ASSERT_EQ(Val, IterIdx);
        }

        // self assignment
        DstBuffer = DstBuffer;
        c_iter = DstBuffer.begin();
        for (UINT32 IterIdx = 0; c_iter != DstBuffer.end(); ++c_iter, IterIdx++)
        {
            UINT32 Val = *c_iter;
            UNIT_ASSERT_EQ(Val, IterIdx);
        }

        m_MyBuffer.push_back(4);
        // test copy
        const CirBuffer<UINT32> &DstBuffer2 = ReturnCirBuffer();
        CirBuffer<UINT32>::const_iterator c_iter2 = DstBuffer2.begin();
        for (UINT32 IterIdx = 1; c_iter2 != DstBuffer2.end(); ++c_iter2, IterIdx++)
        {
            UINT32 Val = *c_iter2;
            UNIT_ASSERT_EQ(Val, IterIdx);
        }

        m_MyBuffer.push_back(5);
        CirBuffer<UINT32> DstBuffer3;
        DstBuffer3 = ReturnCirBuffer();
        CirBuffer<UINT32>::const_iterator c_iter3 = DstBuffer3.begin();
        for (UINT32 IterIdx = 2; c_iter3 != DstBuffer3.end(); ++c_iter3, IterIdx++)
        {
            UINT32 Val = *c_iter3;
            UNIT_ASSERT_EQ(Val, IterIdx);
        }
    }

    CirBuffer<UINT32> CirBufTest::ReturnCirBuffer()
    {
        // this tests CirBuffer(const CirBuffer& Src)
        return m_MyBuffer;
    }

    void CirBufTest::PrintBuffer(CirBuffer<UINT32> &MyBuffer)
    {
        UINT32 EndIdx   = MyBuffer.GetEndIdx();
        UINT32 BeginIdx = MyBuffer.GetBeginIdx();

        for (UINT32 i = 0; i <= MyBuffer.capacity(); i++)
        {
            if (BeginIdx > EndIdx)
            {
                if ((i < BeginIdx) && (i >= EndIdx))
                    Printf(Tee::PriLow, " NA");
                else
                    Printf(Tee::PriLow, " %d", MyBuffer.GetValAtIdx(i));
            }
            else
            {
                if ((i >= EndIdx) || (i < BeginIdx))
                    Printf(Tee::PriLow, " NA");
                else
                    Printf(Tee::PriLow, " %d", MyBuffer.GetValAtIdx(i));
            }

            if (i == EndIdx)
                Printf(Tee::PriLow, "_e");
            if (i == BeginIdx)
                Printf(Tee::PriLow, "_b");
        }

        Printf(Tee::PriLow, "\n");
    }
}
