/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011,2016-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   unittest.h
 * @brief  Header file for the UnitTest class.
 */

#ifndef INCLUDED_UNITTEST_H
#define INCLUDED_UNITTEST_H

#include "core/include/rc.h"
#include <set>
#include <string>
#include <vector>

#define UNIT_ASSERT_EQ(a, b) \
    UnitTest::UnitAssertCmp(__LINE__, (a) == (b), "==", "!=", #a, #b, UnitTest::ToString(a), UnitTest::ToString(b))
#define UNIT_ASSERT_NEQ(a, b) \
    UnitTest::UnitAssertCmp(__LINE__, (a) != (b), "!=", "==", #a, #b, UnitTest::ToString(a), UnitTest::ToString(b))
#define UNIT_ASSERT_LT(a, b) \
    UnitTest::UnitAssertCmp(__LINE__, (a) <  (b), "<",  ">=", #a, #b, UnitTest::ToString(a), UnitTest::ToString(b))
#define UNIT_ASSERT_GT(a, b) \
    UnitTest::UnitAssertCmp(__LINE__, (a) >  (b), ">",  "<=", #a, #b, UnitTest::ToString(a), UnitTest::ToString(b))
#define UNIT_ASSERT_LE(a, b) \
    UnitTest::UnitAssertCmp(__LINE__, (a) <= (b), "<=", ">",  #a, #b, UnitTest::ToString(a), UnitTest::ToString(b))
#define UNIT_ASSERT_GE(a, b) \
    UnitTest::UnitAssertCmp(__LINE__, (a) >= (b), ">=", "<",  #a, #b, UnitTest::ToString(a), UnitTest::ToString(b))
#define UNIT_ASSERT_RC(f)                 \
    do                                    \
    {                                     \
        if (OK != (rc = (f)))             \
        {                                 \
            UNIT_ASSERT_EQ(rc, RC::OK);   \
            return;                       \
        }                                 \
    } while (0)
#define ADD_UNIT_TEST(classname) \
    UnitTest::TestCreator<classname> s_Create##classname

class RC;

class UnitTest
{
public:
    class TestCreatorBase
    {
    public:
        TestCreatorBase();
        virtual ~TestCreatorBase();
        virtual UnitTest* Create() const = 0;
        static TestCreatorBase* GetFirst() { return s_pFirst; }
        TestCreatorBase* GetNext() const { return m_pNext; }

    private:
        // non-copyable
        TestCreatorBase(const TestCreatorBase&);
        TestCreatorBase& operator=(const TestCreatorBase&);

        static TestCreatorBase* s_pFirst;
        static TestCreatorBase* s_pLast;
        TestCreatorBase* m_pNext;
    };

    template<typename T>
    class TestCreator: public TestCreatorBase
    {
    public:
        virtual ~TestCreator() { }
        virtual UnitTest* Create() const { return new T; }
    };

    virtual ~UnitTest() { }
    RC RunTest();
    const string &GetName() const { return m_Name; }
    int GetFailCount() const { return m_FailCount; }
    static RC RunTestSuite(const vector<string> &testList,
                           const vector<string> &skipList);
    static RC RunAllTests();
    static string ToString(bool);
    static string ToString(int);
    static string ToString(unsigned);
    static string ToString(UINT64);
    static string ToString(FLOAT32);
    static string ToString(FLOAT64);
    static string ToString(const string& s) { return s; }
    static string ToString(const char* s) { return s; }
    static string ToString(RC rc) { return rc.Message(); }
    template<typename T> static string ToString(const set<T> &value)
    {
        string str = "[";
        const char *sep = "";
        for (typename set<T>::const_iterator iter = value.begin();
             iter != value.end(); ++iter)
        {
            str += sep + ToString(*iter);
            sep = ", ";
        }
        str += "]";
        return str;
    }

protected:
    explicit UnitTest(const char* name);
    void UnitAssertCmp
    (
        int line,
        bool equal,
        const char* op,
        const char* negOp,
        const char* a,
        const char* b,
        const string& valA,
        const string& valB
    );

private:
    virtual void Run() = 0;

    string m_Name;
    void* m_Mutex;
    int m_FailCount;
};

#endif // !INCLUDED_UNITTEST_H
