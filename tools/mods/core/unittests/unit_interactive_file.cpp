/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "unittest.h"
#include "core/include/xp.h"

namespace
{
    class InteractiveFileTest: public UnitTest
    {
        public:
            InteractiveFileTest(): UnitTest("InteractiveFile") { }
            void Run() override;

        private:
            void DeleteFile(const string& filename);
    };

    ADD_UNIT_TEST(InteractiveFileTest);

    void InteractiveFileTest::DeleteFile(const string& filename)
    {
        if (Xp::DoesFileExist(filename))
        {
            RC rc;
            UNIT_ASSERT_RC(Xp::EraseFile(filename));

            const bool exists = Xp::DoesFileExist(filename);
            UNIT_ASSERT_EQ(exists, false);
        }
    }

    void InteractiveFileTest::Run()
    {
        const string testName = "mods_interactife_file_test.txt";
        static const char thisIsATest[]    = "this is a test";
        static const char shorterString[]  = "shorter str";

        DeleteFile(testName);

        //============================================================
        // Try to open (but not create) a non-existent file
        {
            Xp::InteractiveFile file;

            // Open non-existent file as ReadOnly - fails
            {
                const RC rc = file.Open(testName.c_str(), Xp::InteractiveFile::ReadOnly);
                UNIT_ASSERT_EQ(rc, RC::FILE_DOES_NOT_EXIST);

                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, false);
            }

            // Open non-existent file as WriteOnly - fails
            {
                const RC rc = file.Open(testName.c_str(), Xp::InteractiveFile::WriteOnly);
                UNIT_ASSERT_EQ(rc, RC::FILE_DOES_NOT_EXIST);

                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, false);
            }

            // Open non-existent file as ReadWrite - fails
            {
                const RC rc = file.Open(testName.c_str(), Xp::InteractiveFile::ReadWrite);
                UNIT_ASSERT_EQ(rc, RC::FILE_DOES_NOT_EXIST);

                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, false);
            }
        }

        //============================================================
        // Create a new file, then try to read from it (it's empty)
        {
            Xp::InteractiveFile file;

            // Ensure the file does not exist before we try to create it
            {
                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, false);
            }

            // Create file and ensure it's been created
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::Create));

                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, true);
            }

            // Read from the file just created, should read an empty string
            {
                string value;
                RC rc;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, "");
            }

            // Reading int fails (empty string is not an int)
            {
                int value;
                const RC rc = file.Read(&value);
                UNIT_ASSERT_EQ(rc, RC::ILWALID_INPUT);
            }

            // Reading UINT64 fails (empty string is not a number)
            {
                UINT64 value;
                const RC rc = file.Read(&value);
                UNIT_ASSERT_EQ(rc, RC::ILWALID_INPUT);
            }

            file.Close();

            DeleteFile(testName);
        }

        //============================================================
        // Create a new file, write a string and read it back
        {
            Xp::InteractiveFile file;

            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::Create));

                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, true);
            }

            // Write a string to the file just created
            {
                RC rc;
                UNIT_ASSERT_RC(file.Write(thisIsATest));
            }

            // Verify the string written
            {
                RC rc;
                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, thisIsATest);
            }
        }

        //============================================================
        // Truncate an existing file and write a shorter string (via Create)
        {
            Xp::InteractiveFile file;

            // Ensure the file exists
            {
                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, true);
            }

            // Re-create the file
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::Create));

                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, true);
            }

            // Write a shorter string than before to the file
            {
                RC rc;
                UNIT_ASSERT_RC(file.Write(shorterString));
            }

            file.Close();

            // Open the file again and ensure the contents are as expected
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadOnly));

                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, shorterString);
            }
        }

        //============================================================
        // Try to write to a ReadOnly file
        {
            Xp::InteractiveFile file;

            // Open the file as ReadOnly
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadOnly));
            }

            // Write to a ReadOnly file fails
            {
                const RC rc = file.Write("x");
                UNIT_ASSERT_EQ(rc, RC::SOFTWARE_ERROR);
            }

            file.Close();

            // Reopen the file and ensure the contents are as before the failed write
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadOnly));

                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, shorterString);
            }
        }

        //============================================================
        // Truncate an existing file by opening it as WriteOnly
        {
            Xp::InteractiveFile file;

            {
                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, true);
            }

            // Open the file as WriteOnly, should truncate the file
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::WriteOnly));
            }

            // Close the file without performing any reads or writes
            file.Close();

            {
                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, true);
            }

            // Open the file as ReadOnly and ensure it is empty
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadOnly));

                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, "");
            }
        }

        //============================================================
        // Read from and write to a WriteOnly file
        {
            Xp::InteractiveFile file;

            {
                const bool exists = Xp::DoesFileExist(testName);
                UNIT_ASSERT_EQ(exists, true);
            }

            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::WriteOnly));
            }

            // This will be overwritten below
            {
                RC rc;
                UNIT_ASSERT_RC(file.Write(shorterString));
            }

            // Overwrite previous data
            {
                RC rc;
                UNIT_ASSERT_RC(file.Write(thisIsATest));
            }

            // Read from a WriteOnly file fails
            {
                string value;
                const RC rc = file.Read(&value);
                UNIT_ASSERT_EQ(rc, RC::SOFTWARE_ERROR);
            }

            file.Close();

            // Ensure the contents are as expected
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadOnly));

                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, thisIsATest);
            }
        }

        //============================================================
        // Write a shorter string to a WriteOnly file
        {
            Xp::InteractiveFile file;

            // Write a longer string first
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::WriteOnly));

                UNIT_ASSERT_RC(file.Write(thisIsATest));
            }

            file.Close();

            // Now reopen the file and write a shorter string
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::WriteOnly));

                UNIT_ASSERT_RC(file.Write(shorterString));
            }

            file.Close();

            // Check if contents are correct
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadOnly));

                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, shorterString);
            }
        }

        DeleteFile(testName);

        //============================================================
        // Test a ReadWrite file
        {
            Xp::InteractiveFile file;

            // Write initial data
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::Create));

                UNIT_ASSERT_RC(file.Write(thisIsATest));

                file.Close();
            }

            // Open as ReadWrite
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadWrite));
            }

            // Read right after opening
            {
                RC rc;
                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, thisIsATest);
            }

            // Read again, the same data returned
            {
                RC rc;
                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, thisIsATest);
            }

            // Write twice, the second write is retained
            {
                RC rc;
                UNIT_ASSERT_RC(file.Write(thisIsATest));
                UNIT_ASSERT_RC(file.Write(shorterString));
            }

            // Read after write
            {
                RC rc;
                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, shorterString);
            }
        }

        //============================================================
        // Write a shorter string to a ReadWrite file
        {
            Xp::InteractiveFile file;

            // Write a longer string first
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadWrite));

                UNIT_ASSERT_RC(file.Write(thisIsATest));
            }

            file.Close();

            // Now reopen the file and write a shorter string
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadWrite));

                UNIT_ASSERT_RC(file.Write(shorterString));
            }

            file.Close();

            // Check if contents are correct
            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadOnly));

                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, shorterString);
            }
        }

        //============================================================
        // Test writing and reading int and UINT64
        {
            Xp::InteractiveFile file;

            {
                RC rc;
                UNIT_ASSERT_RC(file.Open(testName.c_str(), Xp::InteractiveFile::ReadWrite));
            }

            constexpr int negativeInt = -65535;
            const string negativeIntAsStr = Utility::ToString(negativeInt);

            // Write negative int
            {
                RC rc;
                UNIT_ASSERT_RC(file.Write(negativeInt));
            }

            // Read back as int
            {
                RC rc;
                int value = 123;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, negativeInt);
            }

            // Read back as string
            {
                RC rc;
                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, negativeIntAsStr);
            }

            // Reading negative number as UINT64 fails
            {
                UINT64 value = 123;
                const RC rc = file.Read(&value);

                UNIT_ASSERT_EQ(rc, RC::ILWALID_INPUT);
                UNIT_ASSERT_EQ(value, 123);
            }

            constexpr int positiveInt = 42;
            const string positiveIntAsStr = Utility::ToString(positiveInt);

            // Write positive int
            {
                RC rc;
                UNIT_ASSERT_RC(file.Write(positiveInt));
            }

            // Read back as int
            {
                RC rc;
                int value = 123;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, positiveInt);
            }

            // Read back as string
            {
                RC rc;
                string value;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, positiveIntAsStr);
            }

            // Read back as UINT64
            {
                RC rc;
                UINT64 value = 123;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, positiveInt);
            }

            // Write a huge UINT64
            const UINT64 huge = ~static_cast<UINT64>(10U);
            {
                RC rc;
                UNIT_ASSERT_RC(file.Write(huge));
            }

            // Read back as UINT64
            {
                RC rc;
                UINT64 value = 123;
                UNIT_ASSERT_RC(file.Read(&value));

                UNIT_ASSERT_EQ(value, huge);
            }

            // Read back as int
            {
                int value = 123;
                const RC rc = file.Read(&value);

                UNIT_ASSERT_EQ(rc, RC::ILWALID_INPUT);
                UNIT_ASSERT_EQ(value, 123);
            }
        }

        DeleteFile(testName);
    }
}
