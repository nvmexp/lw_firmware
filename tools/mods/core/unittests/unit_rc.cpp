/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "unittest.h"
#include "core/include/rc.h"

namespace
{
    class RCTest: public UnitTest
    {
        public:
            RCTest() : UnitTest("RC") { }

            virtual ~RCTest() { }

            virtual void Run()
            {
                // Constructor defaults to OK
                {
                    RC rc;
                    UNIT_ASSERT_EQ(rc.Get(), OK);
                }

                // Initialize to a different value
                {
                    RC rc(RC::SOFTWARE_ERROR);
                    UNIT_ASSERT_EQ(rc.Get(), RC::SOFTWARE_ERROR);
                    UNIT_ASSERT_EQ(static_cast<INT32>(rc), RC::SOFTWARE_ERROR);
                }

                // Copy constructor and Clear
                {
                    RC rc(RC::TIMEOUT_ERROR);
                    RC rc2(rc);
                    rc.Clear();
                    UNIT_ASSERT_EQ(rc.Get(), OK);
                    UNIT_ASSERT_EQ(rc2.Get(), RC::TIMEOUT_ERROR);
                }

                // Overwrite with code (in the future might assert)
                {
                    RC rc(RC::UNSUPPORTED_FUNCTION);
                    rc = RC::BAD_PARAMETER;
                    UNIT_ASSERT_EQ(rc.Get(), RC::BAD_PARAMETER);
                }

                // Overwrite with rc (in the future might assert)
                {
                    RC rc(RC::BAD_COMMAND_LINE_ARGUMENT);
                    RC rc2(RC::CANNOT_ALLOCATE_MEMORY);
                    rc = rc2;
                    UNIT_ASSERT_EQ(rc.Get(), RC::CANNOT_ALLOCATE_MEMORY);
                }

                // Sticky RC
                {
                    StickyRC rc;
                    UNIT_ASSERT_EQ(rc.Get(), OK);

                    rc = RC::SOFTWARE_ERROR;
                    UNIT_ASSERT_EQ(static_cast<INT32>(rc), RC::SOFTWARE_ERROR);

                    rc = RC::BAD_PARAMETER;
                    UNIT_ASSERT_EQ(rc.Get(), RC::SOFTWARE_ERROR);

                    StickyRC rc2(RC::FILE_DOES_NOT_EXIST);
                    rc = rc2;
                    UNIT_ASSERT_EQ(rc2.Get(), RC::FILE_DOES_NOT_EXIST);
                    UNIT_ASSERT_EQ(rc.Get(), RC::SOFTWARE_ERROR);
                }
            }
    };

    ADD_UNIT_TEST(RCTest);
}
