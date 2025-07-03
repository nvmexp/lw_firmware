/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "unittest.h"
#include "inc/bytestream.h"

namespace
{
    class ByteStreamTest: public UnitTest
    {
        public:
            ByteStreamTest() : UnitTest("ByteStream") { }

            virtual ~ByteStreamTest() { }

            virtual void Run()
            {
                TestExtraction();
                TestPB();
            }

            void TestExtraction()
            {
                {
                    ByteStream bs(vector<UINT08>{
                        0xDE, 0xC0,               // UINT16  = 0xC0DE
                        42, 0, 0, 0,              // INT32   = 42
                        0, 0, 0x80, 0x3F,         // FLOAT32 = 1
                        0x41, 0x42, 0,            // string  = "AB"
                        0, 0, 0, 0, 0, 0, 0, 0x40 // FLOAT64 = 2
                        });

                    // Extract individual values
                    {
                        RC rc;
                        UINT32 index = 0;

                        {
                            UINT16 a = 0;

                            UNIT_ASSERT_RC(bs.Unpack(&index, &a));
                            UNIT_ASSERT_EQ(a, 0xC0DE);
                        }

                        {
                            INT32   b = 0;
                            FLOAT32 c = 0;

                            UNIT_ASSERT_RC(bs.Unpack(&index, &b, &c));
                            UNIT_ASSERT_EQ(b, 42);
                            UNIT_ASSERT_EQ(c, 1.0f);
                        }

                        string  d;
                        FLOAT64 e = 0;

                        UNIT_ASSERT_RC(bs.Unpack(&index, &d, &e));
                        UNIT_ASSERT_EQ(d, "AB");
                        UNIT_ASSERT_EQ(e, 2.0);
                    }

                    // Extract all values at the same time
                    {
                        UINT16  a = 0;
                        INT32   b = 0;
                        FLOAT32 c = 0;
                        string  d;
                        FLOAT64 e = 0;

                        RC rc;
                        UNIT_ASSERT_RC(bs.UnpackAll(&a, &b, &c, &d, &e));
                        UNIT_ASSERT_EQ(a, 0xC0DE);
                        UNIT_ASSERT_EQ(b, 42);
                        UNIT_ASSERT_EQ(c, 1.0f);
                        UNIT_ASSERT_EQ(d, "AB");
                        UNIT_ASSERT_EQ(e, 2.0);
                    }

                    // Extract overlapping the end
                    {
                        UINT32 index = bs.size() - 2;
                        UINT32 value = 0xDDDDDDDDU;
                        const RC rc = bs.Unpack(&index, &value);
                        UNIT_ASSERT_NEQ(rc, RC::OK);
                        UNIT_ASSERT_EQ(value, 0xDDDDDDDDU);
                    }

                    // Extract from the end
                    {
                        UINT32 index = bs.size();
                        UINT32 value = 0xDDDDDDDDU;
                        const RC rc = bs.Unpack(&index, &value);
                        UNIT_ASSERT_NEQ(rc, RC::OK);
                        UNIT_ASSERT_EQ(value, 0xDDDDDDDDU);
                    }

                    // Extract from beyond the end
                    {
                        UINT32 index = bs.size() + 10;
                        UINT32 value = 0xDDDDDDDDU;
                        const RC rc = bs.Unpack(&index, &value);
                        UNIT_ASSERT_NEQ(rc, RC::OK);
                        UNIT_ASSERT_EQ(value, 0xDDDDDDDDU);
                    }

                    // Extract from bad index
                    {
                        UINT32 index = ~0U;
                        UINT32 value = 0xDDDDDDDDU;
                        const RC rc = bs.Unpack(&index, &value);
                        UNIT_ASSERT_NEQ(rc, RC::OK);
                        UNIT_ASSERT_EQ(value, 0xDDDDDDDDU);
                    }
                }

                {
                    ByteStream bs(vector<UINT08>{
                        0x30, 0x31, 0x32, 0,    // "012"
                        0x33, 0x34, 0,          // "34"
                        0,                      // ""
                        0x35, 0,                // "5"
                        0x36, 0x37              // missing NUL
                    });

                    // Extract multiple strings
                    {
                        string a;
                        string b;
                        string c;
                        string d;

                        RC rc;
                        UNIT_ASSERT_RC(bs.UnpackAll(&a, &b, &c, &d));
                        UNIT_ASSERT_EQ(a, "012");
                        UNIT_ASSERT_EQ(b, "34");
                        UNIT_ASSERT_EQ(c, "");
                        UNIT_ASSERT_EQ(d, "5");
                    }

                    // No terminating NUL
                    {
                        UINT32 index = bs.size() - 2;
                        string s = "something";
                        const RC rc = bs.Unpack(&index, &s);
                        UNIT_ASSERT_NEQ(rc, RC::OK);
                        UNIT_ASSERT_EQ(s, "something");
                    }

                    // Extract from the end
                    {
                        UINT32 index = bs.size();
                        string s = "something";
                        const RC rc = bs.Unpack(&index, &s);
                        UNIT_ASSERT_NEQ(rc, RC::OK);
                        UNIT_ASSERT_EQ(s, "something");
                    }

                    // Extract from beyond the end
                    {
                        UINT32 index = bs.size() + 1;
                        string s = "something";
                        const RC rc = bs.Unpack(&index, &s);
                        UNIT_ASSERT_NEQ(rc, RC::OK);
                        UNIT_ASSERT_EQ(s, "something");
                    }

                    // Extract at invalid index
                    {
                        UINT32 index = ~0U;
                        string s = "something";
                        const RC rc = bs.Unpack(&index, &s);
                        UNIT_ASSERT_NEQ(rc, RC::OK);
                        UNIT_ASSERT_EQ(s, "something");
                    }
                }
            }

            void TestPB()
            {
                // Test signed char and bool
                {
                    ByteStream bs; // lwte name for this variable

                    bs.PushPBSigned(static_cast<signed char>(0));
                    bs.PushPBSigned(static_cast<signed char>(1));
                    bs.PushPBSigned(static_cast<signed char>(63));
                    bs.PushPBSigned(static_cast<signed char>(64));
                    bs.PushPBSigned(static_cast<signed char>(127));
                    bs.PushPBSigned(static_cast<signed char>(-1));
                    bs.PushPBSigned(static_cast<signed char>(-64));
                    bs.PushPBSigned(static_cast<signed char>(-65));
                    bs.PushPBSigned(static_cast<signed char>(-128));

                    bs.PushPB(true);
                    bs.PushPB(false);

                    const UINT32 size = bs.size();
                    UNIT_ASSERT_EQ(size, 15);

                    static const UINT08 expected[] = {
                        0x00U,
                        0x02U, // 1
                        0x7EU, // 64
                        0x80U, 0x01U, // 64
                        0xFEU, 0x01U, // 127
                        0x01U, // -1
                        0x7FU, // -64
                        0x81U, 0x01U, // -65
                        0xFFU, 0x01U, // -128,
                        1U, // true
                        0U  // false
                    };

                    for (size_t i = 0; i < sizeof(expected) && i < bs.size(); i++)
                    {
                        UNIT_ASSERT_EQ(bs[i], expected[i]);
                    }

                    signed char signedChars[9] = { };
                    bool        bools[2]       = { };

                    RC rc;
                    UNIT_ASSERT_RC(bs.UnpackAllPB(&signedChars[0], &signedChars[1],
                            &signedChars[2], &signedChars[3], &signedChars[4],
                            &signedChars[5], &signedChars[6], &signedChars[7],
                            &signedChars[8], &bools[0], &bools[1]));
                    UNIT_ASSERT_EQ(signedChars[0], 0);
                    UNIT_ASSERT_EQ(signedChars[1], 1);
                    UNIT_ASSERT_EQ(signedChars[2], 63);
                    UNIT_ASSERT_EQ(signedChars[3], 64);
                    UNIT_ASSERT_EQ(signedChars[4], 127);
                    UNIT_ASSERT_EQ(signedChars[5], -1);
                    UNIT_ASSERT_EQ(signedChars[6], -64);
                    UNIT_ASSERT_EQ(signedChars[7], -65);
                    UNIT_ASSERT_EQ(signedChars[8], -128);
                    UNIT_ASSERT_EQ(bools[0], true);
                    UNIT_ASSERT_EQ(bools[1], false);
                }

                // Test unsigned char
                {
                    ByteStream bs; // lwte name for this variable

                    bs.PushPB(static_cast<unsigned char>(0U));
                    bs.PushPB(static_cast<unsigned char>(1U));
                    bs.PushPB(static_cast<unsigned char>(64U));
                    bs.PushPB(static_cast<unsigned char>(127U));
                    bs.PushPB(static_cast<unsigned char>(128U));
                    bs.PushPB(static_cast<unsigned char>(255U));

                    const UINT32 size = bs.size();
                    UNIT_ASSERT_EQ(size, 8);

                    static const UINT08 expected[] = {
                        0U,
                        1U,
                        64U,
                        127U,
                        0x80U, 0x01U, // 128
                        0xFFU, 0x01U  // 255
                    };

                    for (size_t i = 0; i < sizeof(expected) && i < bs.size(); i++)
                    {
                        UNIT_ASSERT_EQ(bs[i], expected[i]);
                    }

                    unsigned char values[6] = { };

                    RC rc;
                    UNIT_ASSERT_RC(bs.UnpackAllPB(&values[0], &values[1],
                            &values[2], &values[3], &values[4], &values[5]));
                    UNIT_ASSERT_EQ(values[0], 0U);
                    UNIT_ASSERT_EQ(values[1], 1U);
                    UNIT_ASSERT_EQ(values[2], 64U);
                    UNIT_ASSERT_EQ(values[3], 127U);
                    UNIT_ASSERT_EQ(values[4], 128U);
                    UNIT_ASSERT_EQ(values[5], 255U);
                }

                // Test INT64/UINT64 limits
                {
                    ByteStream bs; // lwte name for this variable

                    bs.PushPB(0x7123'4567'89AB'CDEFULL);
                    bs.PushPB(0x9123'4567'89AB'CDEFULL);
                    bs.PushPB(0xFFFF'FFFF'FFFF'FFFFULL);
                    bs.PushPB(0x7FFF'FFFF'FFFF'FFFFULL);

                    bs.PushPBSigned(0x7FFF'FFFF'FFFF'FFFFLL);
                    bs.PushPBSigned(-0x0123'4567'89AB'CDEFLL);
                    bs.PushPBSigned(static_cast<INT64>(0x8000'0000'0000'0000ULL));

                    const UINT32 size = bs.size();
                    UNIT_ASSERT_EQ(size, 9 + 10 + 10 + 9 + 10 + 9 + 10);

                    static const UINT08 expected[] = {
                        0xEFU, 0x9BU, 0xAFU, 0xCDU, 0xF8U, 0xALW, 0xD1U, 0x91U, 0x71U,
                        0xEFU, 0x9BU, 0xAFU, 0xCDU, 0xF8U, 0xALW, 0xD1U, 0x91U, 0x91U, 0x01U,
                        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x01U,
                        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x7FU,

                        0xFEU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x01U,
                        0xDDU, 0xB7U, 0xDEU, 0x9AU, 0xF1U, 0xD9U, 0xA2U, 0xA3U, 0x02U,
                        0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x01U,
                    };

                    for (size_t i = 0; i < sizeof(expected) && i < bs.size(); i++)
                    {
                        UNIT_ASSERT_EQ(bs[i], expected[i]);
                    }

                    UINT64 unsignedValues[4] = { };
                    INT64 signedValues[3]    = { };

                    RC rc;
                    UNIT_ASSERT_RC(bs.UnpackAllPB(&unsignedValues[0], &unsignedValues[1],
                            &unsignedValues[2], &unsignedValues[3],
                            &signedValues[0], &signedValues[1], &signedValues[2]));

                    UNIT_ASSERT_EQ(unsignedValues[0], 0x7123'4567'89AB'CDEFULL);
                    UNIT_ASSERT_EQ(unsignedValues[1], 0x9123'4567'89AB'CDEFULL);
                    UNIT_ASSERT_EQ(unsignedValues[2], 0xFFFF'FFFF'FFFF'FFFFULL);
                    UNIT_ASSERT_EQ(unsignedValues[3], 0x7FFF'FFFF'FFFF'FFFFULL);

                    UNIT_ASSERT_EQ((UINT64)signedValues[0], 0x7FFF'FFFF'FFFF'FFFFULL);
                    UNIT_ASSERT_EQ((UINT64)signedValues[1], 0xFEDC'BA98'7654'3211ULL);
                    UNIT_ASSERT_EQ((UINT64)signedValues[2], 0x8000'0000'0000'0000ULL);
                }

                // Test float
                {
                    ByteStream bs; // lwte name for this variable

                    bs.PushPB(0.f);
                    bs.PushPB(1.f);
                    bs.PushPB(-1.f);

                    const UINT32 size = bs.size();
                    UNIT_ASSERT_EQ(size, 3 * 4);

                    static const UINT08 expected[] = {
                        0x00U, 0x00U, 0x00U, 0x00U,
                        0x00U, 0x00U, 0x80U, 0x3FU,
                        0x00U, 0x00U, 0x80U, 0xBFU,
                    };

                    for (size_t i = 0; i < sizeof(expected) && i < bs.size(); i++)
                    {
                        UNIT_ASSERT_EQ(bs[i], expected[i]);
                    }

                    float values[3] = { };

                    RC rc;
                    UNIT_ASSERT_RC(bs.UnpackAllPB(&values[0], &values[1], &values[2]));

                    UNIT_ASSERT_EQ(values[0], 0.f);
                    UNIT_ASSERT_EQ(values[1], 1.f);
                    UNIT_ASSERT_EQ(values[2], -1.f);
                }

                // Test string
                {
                    ByteStream bs; // lwte name for this variable

                    bs.PushPB("1234");
                    bs.PushPB(string(127, '\1'));
                    bs.PushPB(string(128, '\2'));

                    const UINT32 size = bs.size();
                    UNIT_ASSERT_EQ(size, 5 + 128 + 130);

                    static const UINT08 expected[] = {
                        0x04U, 0x31U, 0x32U, 0x33U, 0x34U,
                        0x7FU, 0x01U, 0x01U
                    };

                    for (size_t i = 0; i < sizeof(expected) && i < bs.size(); i++)
                    {
                        UNIT_ASSERT_EQ(bs[i], expected[i]);
                    }

                    string s[3];

                    RC rc;
                    UNIT_ASSERT_RC(bs.UnpackAllPB(&s[0], &s[1], &s[2]));

                    UNIT_ASSERT_EQ(s[0], "1234");
                    UNIT_ASSERT_EQ(s[1], string(127, '\1'));
                    UNIT_ASSERT_EQ(s[2], string(128, '\2'));
                }

                // Test valid number
                {
                    ByteStream bs = { 0x80U, 0x80U, 0x80U, 0x80U, 0x80U,
                                      0x80U, 0x80U, 0x80U, 0x80U, 0x01U };

                    UINT64 value = 0;

                    RC rc;
                    UNIT_ASSERT_RC(bs.UnpackAllPB(&value));

                    UNIT_ASSERT_EQ(value, 0x8000'0000'0000'0000ULL);
                }

                // Test invalid number (too large)
                {
                    ByteStream bs = { 0x80U, 0x80U, 0x80U, 0x80U, 0x80U,
                                      0x80U, 0x80U, 0x80U, 0x80U, 0x02U };

                    UINT64 value = 0;

                    const RC rc = bs.UnpackAllPB(&value);

                    UNIT_ASSERT_NEQ(rc, RC::OK);
                }

                // Test invalid number (not enough bytes)
                {
                    ByteStream bs = { 0x80U, 0x80U };

                    UINT64 value = 0;

                    const RC rc = bs.UnpackAllPB(&value);

                    UNIT_ASSERT_NEQ(rc, RC::OK);
                }

                // Test invalid string (not enough bytes)
                {
                    // Length 2, but only one character
                    ByteStream bs = { 0x02U, 0x00U };

                    string value;

                    const RC rc = bs.UnpackAllPB(&value);

                    UNIT_ASSERT_NEQ(rc, RC::OK);
                }

                // Test invalid number (too large to fit in type)
                {
                    // 0x8000, 0x10000
                    ByteStream bs = { 0x80U, 0x80U, 0x02U, 0x80U, 0x80U, 0x04U };

                    UINT16 values[2] = { };

                    const RC rc = bs.UnpackAllPB(&values[0], &values[1]);

                    UNIT_ASSERT_NEQ(rc, RC::OK);
                    UNIT_ASSERT_EQ(values[0], 0x8000U);
                }

                // Test invalid number (too large to fit in type)
                {
                    // -0x8000, -0x8001
                    ByteStream bs = { 0xFFU, 0xFFU, 0x03U, 0x81U, 0x80U, 0x04U };

                    INT16 values[2] = { };

                    const RC rc = bs.UnpackAllPB(&values[0], &values[1]);

                    UNIT_ASSERT_NEQ(rc, RC::OK);
                    UNIT_ASSERT_EQ(values[0], -0x8000);
                }
            }
    };

    ADD_UNIT_TEST(ByteStreamTest);
}
