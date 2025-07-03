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

#include "../mucc/mucc.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <cmath>
#include <limits>
using namespace Mucc;
using namespace std;

#define MUCC_INSTR_SIZE_BITS_GA100 192

//! \brief MuccInstr is a class that contains all the member functions
//! needed to set and return test instructions for a given architecture
template<class T, UINT16 s_InstructionSizeBits>
class MuccInstr
{
public:
    // Mucc Instructions are stored inside an array of Type T and the size of the array
    // is number of entries needed to fit the instruction
    using MuccInstrType = std::array<T, s_InstructionSizeBits/std::numeric_limits<T>::digits>;

    void SetUp(const std::vector<UINT32> &resetBitPos)
    {
        std::fill(m_TestInstruction.begin(), m_TestInstruction.end(), 0);
        for (UINT32 const& value: resetBitPos)
        {
            SetKthBit(value);
        }
    }

    // function to set the kth bit in a 192 bit number represented by the vector
    void SetKthBit(UINT32 k)
    {
        MASSERT(k >= 0 && k < s_InstructionSizeBits);
        UINT32 index = k/s_bitsOfT;
        m_TestInstruction[index] |= (1 << k % s_bitsOfT);
    }

    //! \param bits - vector of fixed size (6), "6" because 32 * 6 = 192. 192 is instruction size
    //! \param hi - any value between [0:191], lo - any value between [0:191]
    //! \param value needs to be inserted between bit positions hi and lo. 
    void SetBits(UINT16 hi, UINT16 lo, INT32 value)
    {
        MASSERT(hi >= lo);
        MASSERT(hi < s_InstructionSizeBits);

        // hiIndex and loIndex are used to decide which index of the vector
        // needs to be modified
        UINT08 highIndex = hi/s_bitsOfT;
        UINT08 lowIndex  = lo/s_bitsOfT;
        UINT08 diffOfHiLo = (hi - lo);
        // For negative numbers, need to restrict the number of bits that are
        // set to 1 to "diffOfHiLo + 1".
        // In the case of negative numbers, value is the 2s complemented
        // representation of that negative number. Consider -7 is the number. hi = 48 and lo = 32.
        // value = ‭0b11111111111111111111111111111001‬
        // Actually, only the lowest 16 bits need to considered and be set. So, I'm performing below bitwise &
        if (value < 0)
            value = value & (static_cast<int>(2 << (diffOfHiLo)) - 1);

        // Consider hi = 4, lo = 2, x <= 7 (i.e 2^3 - 1)
        MASSERT(value <= ((2 << (diffOfHiLo)) - 1));
        // Assuming value is less than (2^(hi - lo + 1))
        // Below case is where both highIndex and lowIndex fall to the same index. ex: hi = 30 and lo = 1
        if (highIndex == lowIndex)
        {
            m_TestInstruction[lowIndex] = (m_TestInstruction[lowIndex] & ~(((2<<diffOfHiLo)-1) << (lo%s_bitsOfT))) | (value << lo%s_bitsOfT);
        }
        // Below case is where both hi and lo don't fall to the same index. ex: hi = 32 and lo = 1
        // In that case highIndex = 1, lowIndex = 0
        // ex:2 lo = 49, hi = 64, lowIndex = 1, highIndex = 2, lowlength = 15, hilength = 1
        // ex:3 lo = 126, hi = 129, value = 5, highIndex = 4, hilength = 2, lowlength = 2
        else
        {
            UINT08 lowlength = s_bitsOfT*(lowIndex + 1) - lo;
            UINT08 hilength = hi - s_bitsOfT*(highIndex) + 1;
            m_TestInstruction[lowIndex] = (m_TestInstruction[lowIndex] & ~((( (2<<(lowlength - 1)) -1) << (s_bitsOfT - lowlength)))) | (value << (s_bitsOfT - lowlength));
            m_TestInstruction[highIndex] = (m_TestInstruction[highIndex] & ~((( (2<<(hilength - 1)) -1)))) | (value >> lowlength);
        }
    }

    //! \brief Compare two vectors to check if they are equal
    void TestTwoVectors(const vector<UINT32> &actualData, string Instruction)
    {
        ASSERT_EQ(m_TestInstruction.size(), actualData.size());

        for (unsigned i = 0; i < m_TestInstruction.size(); i++)
        {
            EXPECT_EQ(m_TestInstruction[i], actualData[i]) <<
                      "Vectors testData and actualData differ at index " << i <<
                      " for instruction" << Instruction << "\n";
        }
    }

private:
        MuccInstrType m_TestInstruction;
        static const UINT08 s_bitsOfT = std::numeric_limits<T>::digits;
};

//! Fill the dictionary that has all the machine level code for
//! all instructions that has to be tested.
template<class T, UINT16 s_InstructionSizeBits>
void SetUpGA100Instructions(map <string, MuccInstr<T, s_InstructionSizeBits>>* pTestInstructions)
{
    map <string, MuccInstr<T, s_InstructionSizeBits>>& testInstructions = *pTestInstructions;

    //by default, cke bit for RNOP (118, 150) and CNOP (102, 134) are set to 1
    //and PDBI bit (151) is set to 1 (default: USE_PATRAM_DBI directive is 1)
    std::vector<UINT32> resetBitPos = {118, 150, 102, 134, 151};
    MuccInstr<T, s_InstructionSizeBits> testInstruction;
    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(93,  87,  0x01);
    testInstruction.SetBits(125, 119, 0x01);
    testInstruction.SetBits(97,  94,  1);
    testInstruction.SetBits(129, 126, 5);
    testInstruction.SetBits(101, 98,  0);
    testInstruction.SetBits(133, 130, 4);
    testInstruction.SetBits(102, 102, 1);
    testInstruction.SetBits(134, 134, 1);
    testInstruction.SetBits(70, 70, 1);
    testInstruction.SetBits(74, 71, 2);
    testInstructions["P2 RD R4 R5 R2\nP0 RD R0 R1 R2;"] = testInstruction;

    //Load Register
    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(13, 13, 1);
    testInstruction.SetBits(17, 14, 1);
    testInstruction.SetBits(32, 18, 80);
    testInstructions["LOAD R1 0x50;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(13, 13, 1);
    testInstruction.SetBits(17, 14, 1);
    testInstruction.SetBits(32, 18, 80);
    testInstruction.SetBits(47, 33, -7);
    testInstructions["LDI R1 0x50 -7;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(13, 13, 1);
    testInstruction.SetBits(17, 14, 1);
    testInstruction.SetBits(32, 18, 80);
    testInstruction.SetBits(48, 48, 1);
    testInstructions["LDPRBS R1 0x50;"] = testInstruction;

    // value to set for INCR: mask |= (1 << register)
    // mask = 0 for first INCR
    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(64, 49, (1 << 1));

    //INCR instruction: Increment a register, using the increment specified
    //by the LDI or LDPRBS instruction.
    testInstructions["INCR R1;"] = testInstruction;

    //Column Operations
    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(102, 102, 1);
    testInstruction.SetBits(134, 134, 1);
    testInstructions["P0 CNOP\nP2 CNOP;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(93,  87,  0x01);
    testInstruction.SetBits(125, 119, 0x01);
    testInstruction.SetBits(97,  94,  2);
    testInstruction.SetBits(129, 126, 6);
    testInstruction.SetBits(101, 98,  1);
    testInstruction.SetBits(133, 130, 5);
    testInstruction.SetBits(102, 102, 1);
    testInstruction.SetBits(134, 134, 1);
    testInstruction.SetBits(70, 70, 1);
    testInstruction.SetBits(74, 71, 3);
    testInstructions["P0 RD R1 R2 R3\nP2 RD R5 R6 R3;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(93,  87,  0x05);
    testInstruction.SetBits(125, 119, 0x05);
    testInstruction.SetBits(97,  94,  1);
    testInstruction.SetBits(129, 126, 5);
    testInstruction.SetBits(101, 98,  0);
    testInstruction.SetBits(133, 130, 4);
    testInstruction.SetBits(102, 102, 1);
    testInstruction.SetBits(134, 134, 1);
    testInstruction.SetBits(70, 70, 1);
    testInstruction.SetBits(74, 71, 2);
    testInstructions["P2 RDA R4 R5 R2\nP0 RDA R0 R1 R2;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(93,  87,  0x02);
    testInstruction.SetBits(125, 119, 0x02);
    testInstruction.SetBits(97,  94,  1);
    testInstruction.SetBits(129, 126, 5);
    testInstruction.SetBits(101, 98,  0);
    testInstruction.SetBits(133, 130, 4);
    testInstruction.SetBits(102, 102, 1);
    testInstruction.SetBits(134, 134, 1);
    testInstruction.SetBits(65, 65, 1);
    testInstruction.SetBits(69, 66, 2);
    testInstructions["P2 WR R4 R5 R2\nP0 WR R0 R1 R2;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(93,  87,  0x06);
    testInstruction.SetBits(125, 119, 0x06);
    testInstruction.SetBits(97,  94,  1);
    testInstruction.SetBits(129, 126, 5);
    testInstruction.SetBits(101, 98,  0);
    testInstruction.SetBits(133, 130, 4);
    testInstruction.SetBits(102, 102, 1);
    testInstruction.SetBits(134, 134, 1);
    testInstruction.SetBits(65, 65, 1);
    testInstruction.SetBits(69, 66, 2);
    testInstructions["P2 WRA R4 R5 R2\nP0 WRA R0 R1 R2;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(93, 87, 0x48);
    testInstruction.SetBits(125, 119, 0x48);
    testInstruction.SetBits(97, 94, 1);
    testInstruction.SetBits(129, 126, 3);
    testInstruction.SetBits(101, 98, 0);
    testInstruction.SetBits(133, 130, 2);
    testInstruction.SetBits(102, 102, 1);
    testInstruction.SetBits(134, 134, 1);
    testInstructions["P0 MRS R0 R1\nP2 MRS R2 R3;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(93, 87, 0);
    testInstruction.SetBits(125, 119, 0);
    testInstruction.SetBits(97, 94, 0);
    testInstruction.SetBits(129, 126, 0);
    testInstruction.SetBits(101, 98, 0);
    testInstruction.SetBits(133, 130, 0);
    testInstruction.SetBits(102, 102, 0);
    testInstruction.SetBits(134, 134, 0);
    testInstructions["P0 CPD\nP2 CPD;"] = testInstruction;

    //Row operations
    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(118, 118, 1);
    testInstruction.SetBits(150, 150, 1);
    testInstructions["P1 RNOP\nP3 RNOP;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(109, 103, 0x08);
    testInstruction.SetBits(141, 135, 0x08);
    testInstruction.SetBits(113, 110, 1);
    testInstruction.SetBits(145, 142, 3);
    testInstruction.SetBits(117, 114, 0);
    testInstruction.SetBits(149, 146, 2);
    testInstruction.SetBits(118, 118, 1);
    testInstruction.SetBits(150, 150, 1);
    testInstructions["P1 ACT R0 R1\nP3 ACT R2 R3;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(109, 103, 0x4c);
    testInstruction.SetBits(141, 135, 0x4c);
    testInstruction.SetBits(117, 114, 0);
    testInstruction.SetBits(149, 146, 2);
    testInstruction.SetBits(118, 118, 1);
    testInstruction.SetBits(150, 150, 1);
    testInstructions["P1 REF R0\nP3 REF R2;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(109, 103, 0x64);
    testInstruction.SetBits(141, 135, 0x64);
    testInstruction.SetBits(117, 114, 0);
    testInstruction.SetBits(149, 146, 2);
    testInstruction.SetBits(118, 118, 1);
    testInstruction.SetBits(150, 150, 1);
    testInstructions["P1 REFSB R0\nP3 REFSB R2;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(109, 103, 0x0c);
    testInstruction.SetBits(141, 135, 0x0c);
    testInstruction.SetBits(113, 110, 0);
    testInstruction.SetBits(145, 142, 0);
    testInstruction.SetBits(117, 114, 1);
    testInstruction.SetBits(149, 146, 3);
    testInstruction.SetBits(118, 118, 1);
    testInstruction.SetBits(150, 150, 1);
    testInstructions["P1 PRE R1\nP3 PRE R3;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(109, 103, 0);
    testInstruction.SetBits(141, 135, 0);
    testInstruction.SetBits(113, 110, 0);
    testInstruction.SetBits(145, 142, 0);
    testInstruction.SetBits(117, 114, 0);
    testInstruction.SetBits(149, 146, 0);
    testInstruction.SetBits(118, 118, 0);
    testInstruction.SetBits(150, 150, 0);
    testInstructions["P1 PDE\nP3 PDE;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(109, 103, 0);
    testInstruction.SetBits(141, 135, 0);
    testInstruction.SetBits(113, 110, 0);
    testInstruction.SetBits(145, 142, 0);
    testInstruction.SetBits(117, 114, 0);
    testInstruction.SetBits(149, 146, 0);
    testInstruction.SetBits(118, 118, 0);
    testInstruction.SetBits(150, 150, 0);
    testInstructions["P1 RPD\nP3 RPD;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(109, 103, 0x4c);
    testInstruction.SetBits(141, 135, 0x4c);
    testInstruction.SetBits(113, 110, 0);
    testInstruction.SetBits(145, 142, 0);
    testInstruction.SetBits(117, 114, 0);
    testInstruction.SetBits(149, 146, 0);
    testInstruction.SetBits(118, 118, 0);
    testInstruction.SetBits(150, 150, 0);
    testInstructions["P1 SRE\nP3 SRE;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(109, 103, 0);
    testInstruction.SetBits(141, 135, 0);
    testInstruction.SetBits(118, 118, 1);
    testInstruction.SetBits(150, 150, 1);
    testInstructions["P1 PDX\nP3 PDX;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(109, 103, 0);
    testInstruction.SetBits(141, 135, 0);
    testInstruction.SetBits(118, 118, 1);
    testInstruction.SetBits(150, 150, 1);
    testInstructions["P1 SRX\nP3 SRX;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(109, 103, 0x54);
    testInstruction.SetBits(141, 135, 0x54);
    testInstruction.SetBits(117, 114, 1);
    testInstruction.SetBits(149, 146, 3);
    testInstruction.SetBits(118, 118, 1);
    testInstruction.SetBits(150, 150, 1);
    testInstructions["P1 PREA R1\nP3 PREA R3;"] = testInstruction;

    //Miscellaneous
    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(84, 75, 50);
    testInstructions["HOLD 50;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(85, 85, 1);
    testInstructions["TILA;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(86, 86, 1);
    testInstructions["STOP;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(151, 151, 1);
    testInstructions["PDBI 1;"] = testInstruction;

    testInstruction.SetUp(resetBitPos);
    testInstruction.SetBits(69, 65, 0x11);
    testInstructions["HACK 69 65 0x11;"] = testInstruction;
}

//! \brief This class has member variables and functions needed
//! to asemble the input assembly files
class MuccTest : public testing::Test
{
public:
    //! \brief Call the functions provided by MUCC to assemble the input file
    void Assemble(const string &inputFile)
    {
        EC ec = EC::OK;
        m_Launcher.SetLitter(LITTER_GALIT1);
        m_Preprocessor.AddSearchPath(".");
        FIRST_EC(m_Preprocessor.LoadFile(inputFile));
        ASSERT_EQ(ec, EC::OK);
        m_pProgram = m_Launcher.AllocProgram();
        FIRST_EC(m_pProgram->Assemble(&m_Preprocessor));
        ASSERT_EQ(ec, EC::OK);
    }

    //! \brief Call the functions provided by MUCC to assemble the input buffer
    void Assemble(const char* buf, size_t size, bool failureCase = false)
    {
        Launcher launcher;
        Preprocessor preprocessor;
        launcher.SetLitter(LITTER_GALIT1);
        ASSERT_EQ(preprocessor.SetBuffer(buf, size), EC::OK);
        m_pProgram = launcher.AllocProgram();
        if (failureCase)
            EXPECT_NE(m_pProgram->Assemble(&preprocessor), EC::OK);
        else
            ASSERT_EQ(m_pProgram->Assemble(&preprocessor), EC::OK);
    }

    //! \brief Input files and output files for unit-tests are stored as
    //! testname.mucc and testname.json. ex: phases.mucc and phases.json
    void PopulateTestNames()
    {
        m_TestNames.push_back("phases_load_incr_hack");
        m_TestNames.push_back("samsung_fa_pattern");
        m_TestNames.push_back("thread_mask");
    }

protected:

    Preprocessor m_Preprocessor;
    Launcher     m_Launcher;
    unique_ptr<Program> m_pProgram;
    vector<string> m_TestNames;
};


//! \brief Give the input file to MUCC to assemble
//! Output from MUCC is in a JSON format and is written to testname_testing file
//! Compare the expected output JSON file with the actual JSON file
TEST_F(MuccTest, TestSampleInstructions)
{
    PopulateTestNames();

    EC ec = EC::OK;

    for (auto test_name : m_TestNames)
    {
        ASSERT_NO_FATAL_FAILURE(Assemble(test_name + ".mucc"));
        FileHolder fileHolder;
        FIRST_EC(fileHolder.Open(test_name + "_testing.json", "w"));
        ASSERT_EQ(ec, EC::OK);

        m_pProgram->PrintJson(fileHolder.GetFile());
        fileHolder.Close();

        ifstream expectedOutFile;
        expectedOutFile.open(test_name + ".json");
        EXPECT_NE(expectedOutFile.is_open(), false);

        ifstream actualOutFile;
        actualOutFile.open(test_name + "_testing.json");
        EXPECT_NE(actualOutFile.is_open(), false);

        string expectedOutStr, actualOutStr;

        while (getline(expectedOutFile, expectedOutStr) && getline(actualOutFile, actualOutStr))
        {
            EXPECT_EQ(expectedOutStr, actualOutStr);
        }

        // additional check to ensure actualOutFile reaches eof
        getline(actualOutFile, actualOutStr);
        EXPECT_EQ(expectedOutFile.eof(), actualOutFile.eof());
    }
}

//! \brief Failure test case for checking the behavior
//! when there are multiple load instructions in a statement
TEST_F(MuccTest, LoadInstrFailTestCase)
{
    static const string failLoadCase = "LOAD R0 0x10\nLOAD R1 0x33;";
    ASSERT_NO_FATAL_FAILURE(Assemble(failLoadCase.c_str(), failLoadCase.size(), true));
}

//! \brief verifying the individual bits within instruction are set
//! according to the spec. Such test fixtures have only one instruction.
TEST_F(MuccTest, TestInstructionsManually)
{
    map <string, MuccInstr<UINT32, MUCC_INSTR_SIZE_BITS_GA100>> testInstructions;
    SetUpGA100Instructions<UINT32, MUCC_INSTR_SIZE_BITS_GA100>(&testInstructions);

    for (std::pair<string, MuccInstr<UINT32, MUCC_INSTR_SIZE_BITS_GA100>> instr : testInstructions)
    {
        ASSERT_NO_FATAL_FAILURE(Assemble(instr.first.c_str(), instr.first.size()));

        const vector<Thread>& threadVec = m_pProgram->GetThreads();
        size_t intendedSize = 1;
        ASSERT_EQ(threadVec.size(), intendedSize);

        EXPECT_NO_FATAL_FAILURE(instr.second.TestTwoVectors(threadVec[0].GetInstructions()[0].GetBits().GetData(),
                                                            instr.first));
    }
}