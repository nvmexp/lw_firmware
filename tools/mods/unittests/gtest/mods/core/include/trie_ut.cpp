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

#include "core/include/trie.h"
#include "core/include/types.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

class TrieTest : public testing::Test
{
public:
    std::string RandString(UINT32 length)
    {
        const char asciiRange = '~' - '!';
        std::string s;
        s.reserve(length);

        for (UINT32 n = 0; n < length; ++n)
        {
            s += '!' + (std::rand() % asciiRange);
        }

        return std::move(s);
    }

protected:
    virtual void SetUp()
    {
        m_TrieReference.Insert("UUUU");
        m_TrieReference.Insert("TTTT");
        m_TrieReference.Insert("FFFF");
        m_TrieReference.Insert("TTUU");
        m_TrieReference.Insert("FFUU");
        m_TrieReference.Insert("TFUU");
        m_TrieReference.Insert("TUUU");
        m_TrieReference.Insert("FUUU");
        m_TrieReference.Insert("UUTT");
        m_TrieReference.Insert("UUTF");
        m_TrieReference.Insert("UUTU");
        m_TrieReference.Insert("UUFT");
        m_TrieReference.Insert("UUFF");
        m_TrieReference.Insert("UUFU");
        m_TrieReference.Insert("UUUT");
        m_TrieReference.Insert("UUUF");
        m_TrieReference.Insert("ABCD");
        m_TrieReference.Insert("BCDE");
        m_TrieReference.Insert("BCXE");
        m_TrieReference.Insert("EFGH");
        m_TrieReference.Insert("Z0123");
        m_TrieReference.Insert("Z4567");
        m_TrieReference.Insert("Z89");
        m_TrieReference.Insert("AND");
        m_TrieReference.Insert("AN");
        m_TrieReference.Insert("012345");
        m_TrieReference.Insert("01234");
        m_TrieReference.Insert("012");
        m_TrieReference.Insert("01");
    }

    Trie<std::string> m_TrieEmpty;     //!< Empty tree, should not change
                                       //!< contents during tests.
    Trie<std::string> m_TrieReference; //!< Reference tree, should not change
                                       //!< contents during tests.
    Trie<std::string> m_TrieVariable;  //!< Contain variable contents, modified
                                       //!< during tests.
};

//!
//! \brief Test if Trie empty on construction.
//!
TEST_F(TrieTest, IsInitiallyEmpty)
{
    EXPECT_TRUE(m_TrieEmpty.Empty()) << "Trie not empty on construction";
    EXPECT_EQ(m_TrieEmpty.Size(), 0U) << "Trie not empty on construction";
}

//!
//! \brief Test inserting keys that already exist in the Trie.
//!
TEST_F(TrieTest, InsertExistingKeys)
{
    Trie<std::string> trie(m_TrieReference); // Copy the reference tree
    UINT32 size = trie.Size();

    trie.Insert("UUUU");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("TTTT");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("FFFF");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("TTUU");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("FFUU");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("TFUU");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("TUUU");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("FUUU");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("UUTT");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("UUTF");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("UUTU");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("UUFT");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("UUFF");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("UUFU");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("UUUT");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("UUUF");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("ABCD");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("BCDE");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("BCXE");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("EFGH");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("Z0123");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("Z4567");
    EXPECT_EQ(trie.Size(), size);
    trie.Insert("Z89");
    EXPECT_EQ(trie.Size(), size);

    EXPECT_EQ(trie.Size(), size);
}

//!
//! \brief Test Trie::Clear on an empty tree.
//!
TEST_F(TrieTest, ClearEmptyTree)
{
    Trie<std::string> trie;
    EXPECT_TRUE(trie.Empty());

    trie.Clear();
    EXPECT_EQ(trie.Size(), 0U);
}

//!
//! \brief Test Trie::Clear on a simple tree.
//!
TEST_F(TrieTest, ClearSimpleTree)
{
    Trie<std::string> trie;
    trie.Insert("ABC");
    trie.Insert("ABX");
    trie.Insert("ABY");
    trie.Insert("AB");
    EXPECT_FALSE(trie.Empty());
    EXPECT_EQ(trie.Size(), 5U) << "Trie does not have correct number of nodes for inserted keys";

    trie.Clear();
    EXPECT_EQ(trie.Size(), 0U);

    EXPECT_FALSE(trie.Contains("ABC")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("ABX")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("ABY")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("AB")) << "Found inserted key after clear";
}

//!
//! \brief Test Trie::Clear on a complex tree.
//!
TEST_F(TrieTest, ClearComplexTree)
{
    Trie<std::string> trie;
    trie.Insert("UUUU");
    trie.Insert("TTTT");
    trie.Insert("FFFF");
    trie.Insert("TTUU");
    trie.Insert("FFUU");
    trie.Insert("TFUU");
    trie.Insert("TUUU");
    trie.Insert("FUUU");
    trie.Insert("UUTT");
    trie.Insert("UUTF");
    trie.Insert("UUTU");
    trie.Insert("UUFT");
    trie.Insert("UUFF");
    trie.Insert("UUFU");
    trie.Insert("UUUT");
    trie.Insert("UUUF");
    trie.Insert("ABCD");
    trie.Insert("BCDE");
    trie.Insert("BCXE");
    trie.Insert("EFGH");
    trie.Insert("Z0123");
    trie.Insert("Z4567");
    trie.Insert("Z89");

    trie.Clear();
    EXPECT_EQ(trie.Size(), 0U) << "Not empty after clear";

    EXPECT_FALSE(trie.Contains("UUUU")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("TTTT")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("FFFF")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("TTUU")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("FFUU")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("TFUU")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("TUUU")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("FUUU")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("UUTT")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("UUTF")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("UUTU")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("UUFT")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("UUFF")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("UUFU")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("UUUT")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("UUUF")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("ABCD")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("BCDE")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("BCXE")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("EFGH")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("Z0123")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("Z4567")) << "Found inserted key after clear";
    EXPECT_FALSE(trie.Contains("Z89")) << "Found inserted key after clear";
}

//!
//! \brief Test Trie::Find with existing keys.
//!
TEST_F(TrieTest, FindExistingKey)
{
    EXPECT_TRUE(m_TrieReference.Contains("UUUU"));
    EXPECT_TRUE(m_TrieReference.Contains("TTTT"));
    EXPECT_TRUE(m_TrieReference.Contains("FFFF"));
    EXPECT_TRUE(m_TrieReference.Contains("TTUU"));
    EXPECT_TRUE(m_TrieReference.Contains("FFUU"));
    EXPECT_TRUE(m_TrieReference.Contains("TFUU"));
    EXPECT_TRUE(m_TrieReference.Contains("TUUU"));
    EXPECT_TRUE(m_TrieReference.Contains("FUUU"));
    EXPECT_TRUE(m_TrieReference.Contains("UUTT"));
    EXPECT_TRUE(m_TrieReference.Contains("UUTF"));
    EXPECT_TRUE(m_TrieReference.Contains("UUTU"));
    EXPECT_TRUE(m_TrieReference.Contains("UUFT"));
    EXPECT_TRUE(m_TrieReference.Contains("UUFF"));
    EXPECT_TRUE(m_TrieReference.Contains("UUFU"));
    EXPECT_TRUE(m_TrieReference.Contains("UUUT"));
    EXPECT_TRUE(m_TrieReference.Contains("UUUF"));
    EXPECT_TRUE(m_TrieReference.Contains("ABCD"));
    EXPECT_TRUE(m_TrieReference.Contains("BCDE"));
    EXPECT_TRUE(m_TrieReference.Contains("BCXE"));
    EXPECT_TRUE(m_TrieReference.Contains("EFGH"));
    EXPECT_TRUE(m_TrieReference.Contains("Z0123"));
    EXPECT_TRUE(m_TrieReference.Contains("Z4567"));
    EXPECT_TRUE(m_TrieReference.Contains("Z89"));
    EXPECT_TRUE(m_TrieReference.Contains("AND"));
    EXPECT_TRUE(m_TrieReference.Contains("AN"));
    EXPECT_TRUE(m_TrieReference.Contains("012345"));
    EXPECT_TRUE(m_TrieReference.Contains("01234"));
    EXPECT_TRUE(m_TrieReference.Contains("012"));
    EXPECT_TRUE(m_TrieReference.Contains("01"));
}

//!
//! \brief Test Trie::Find with missing keys.
//!
TEST_F(TrieTest, FindMissingKeys)
{
    // Empty key
    EXPECT_FALSE(m_TrieReference.Contains("")) << "Found empty key";

    // Non-existant key
    EXPECT_FALSE(m_TrieReference.Contains("0")) << "Found non-existant key";
    EXPECT_FALSE(m_TrieReference.Contains("JKBD")) << "Found non-existant key";
    EXPECT_FALSE(m_TrieReference.Contains("UNED")) << "Found non-existant key";
    EXPECT_FALSE(m_TrieReference.Contains("ALSASDIW")) << "Found non-existant key";

    // Non-existant key that is the start of a valid key
    EXPECT_FALSE(m_TrieReference.Contains("U")) << "Found non-existant key that is part of an existing key"; //$
    EXPECT_FALSE(m_TrieReference.Contains("UU")) << "Found non-existant key that is part of an existing key"; //$
    EXPECT_FALSE(m_TrieReference.Contains("UUU")) << "Found non-existant key that is part of an existing key"; //$
    EXPECT_FALSE(m_TrieReference.Contains("A")) << "Found non-existant key that is part of an existing key"; //$

    // Non-existant key that starts with an existing key
    EXPECT_FALSE(m_TrieReference.Contains("Z01234")) << "Found non-existant key starting with existing key"; //$
    EXPECT_FALSE(m_TrieReference.Contains("ABCDEFGHI")) << "Found non-existant key starting with existing key"; //$
}

//!
//! \brief Test Trie::Find large keys.
//!
TEST_F(TrieTest, FindBigKeys)
{
    std::string s0(10000, 'x');
    std::string s1(10000, 'y');
    std::string s2(9999, 'y');

    m_TrieReference.Insert(s0);
    EXPECT_TRUE(m_TrieReference.Contains(s0));

    m_TrieReference.Insert(s1);
    EXPECT_TRUE(m_TrieReference.Contains(s1));
    EXPECT_FALSE(m_TrieReference.Contains(s2));
}

//!
//! \brief Test Trie::Follows.
//!
TEST_F(TrieTest, Follows)
{
#define EXPECT_VAL_FOLLOWS(VAL)                         \
    do                                                  \
    {                                                   \
        it = std::find(v.begin(), v.end(), (VAL));      \
        EXPECT_NE(it, v.end());                         \
        v.erase(it);                                    \
    } while (0)

    std::vector<char> v;
    std::vector<char>::iterator it;

    v = m_TrieReference.Follows("UUUU");
    EXPECT_EQ(v.size(), 0U);
    v.clear();

    v = m_TrieReference.Follows("UUU");
    EXPECT_EQ(v.size(), 3U);
    EXPECT_VAL_FOLLOWS('U');
    EXPECT_VAL_FOLLOWS('T');
    EXPECT_VAL_FOLLOWS('F');
    v.clear();

    v = m_TrieReference.Follows("UU");
    EXPECT_EQ(v.size(), 3U);
    EXPECT_VAL_FOLLOWS('U');
    EXPECT_VAL_FOLLOWS('T');
    EXPECT_VAL_FOLLOWS('F');
    v.clear();

    v = m_TrieReference.Follows("U");
    EXPECT_EQ(v.size(), 1U);
    EXPECT_VAL_FOLLOWS('U');
    v.clear();

    v = m_TrieReference.Follows("");
    EXPECT_EQ(v.size(), 8U);
    EXPECT_VAL_FOLLOWS('U');
    EXPECT_VAL_FOLLOWS('T');
    EXPECT_VAL_FOLLOWS('F');
    EXPECT_VAL_FOLLOWS('A');
    EXPECT_VAL_FOLLOWS('B');
    EXPECT_VAL_FOLLOWS('E');
    EXPECT_VAL_FOLLOWS('Z');
    EXPECT_VAL_FOLLOWS('0');
    v.clear();

    v = m_TrieReference.Follows("A");
    EXPECT_EQ(v.size(), 2U);
    EXPECT_VAL_FOLLOWS('B');
    EXPECT_VAL_FOLLOWS('N');
    v.clear();

    v = m_TrieReference.Follows("B");
    EXPECT_EQ(v.size(), 1U);
    EXPECT_VAL_FOLLOWS('C');
    v.clear();

    v = m_TrieReference.Follows("Z");
    EXPECT_EQ(v.size(), 3U);
    EXPECT_VAL_FOLLOWS('0');
    EXPECT_VAL_FOLLOWS('4');
    EXPECT_VAL_FOLLOWS('8');
    v.clear();

    v = m_TrieReference.Follows("BC");
    EXPECT_EQ(v.size(), 2U);
    EXPECT_VAL_FOLLOWS('D');
    EXPECT_VAL_FOLLOWS('X');
    v.clear();

    v = m_TrieReference.Follows("ABC");
    EXPECT_EQ(v.size(), 1U);
    EXPECT_VAL_FOLLOWS('D');
    v.clear();

    v = m_TrieReference.Follows("UUAB");
    EXPECT_EQ(v.size(), 0U);
    v.clear();
}

//!
//! \brief Test removal of existing keys in a simple internal Trie structure.
//!
TEST_F(TrieTest, RemoveExistingKeySimpleTree)
{
    Trie<std::string> trie;
    EXPECT_TRUE(trie.Empty());

    trie.Insert("ABC");
    trie.Insert("ABX");
    trie.Insert("ABY");
    trie.Insert("AB");
    EXPECT_FALSE(trie.Empty());
    EXPECT_EQ(trie.Size(), 5U) << "Trie does not have correct number of nodes for inserted keys";

    // Remove key where all nodes are part of other keys (ie. no nodes removed)
    trie.Remove("AB");
    EXPECT_EQ(trie.Size(), 5U) << "Incorrect removal of key where all nodes are part of other keys"; //$

    // Remove node that has no next nodes (remove 'Y' node)
    trie.Remove("ABY");
    EXPECT_EQ(trie.Size(), 4U) << "Incorrect removal of node with no next node";

    // Remove node that has active next nodes (remove 'C' node with next node
    // 'X')
    trie.Remove("ABC");
    EXPECT_EQ(trie.Size(), 3U) << "Incorrect removal of node with next node";

    trie.Remove("ABX");
    EXPECT_EQ(trie.Size(), 0U) << "Trie not empty after removing all inserted keys";
}

//!
//! \brief Test removal of existing keys in a complex internal Trie structure.
//!
TEST_F(TrieTest, RemoveExistingKeyComplexTree)
{
    Trie<std::string> trie;
    EXPECT_TRUE(trie.Empty());

    trie.Insert("UUUU");
    trie.Insert("TTTT");
    trie.Insert("FFFF");
    trie.Insert("TTUU");
    trie.Insert("FFUU");
    trie.Insert("TFUU");
    trie.Insert("TUUU");
    trie.Insert("FUUU");
    trie.Insert("UUTT");
    trie.Insert("UUTF");
    trie.Insert("UUTU");
    trie.Insert("UUFT");
    trie.Insert("UUFF");
    trie.Insert("UUFU");
    trie.Insert("UUUT");
    trie.Insert("UUUF");
    trie.Insert("ABCD");
    trie.Insert("BCDE");
    trie.Insert("BCXE");
    trie.Insert("EFGH");
    trie.Insert("Z0123");
    trie.Insert("Z4567");
    trie.Insert("Z89");
    EXPECT_FALSE(trie.Empty());

    // Remove each inserted key in a random order. Check the size of the tree
    // after each removal to see if it is sensible.
    //
    UINT32 size = trie.Size();

    trie.Remove("UUFU");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("FUUU");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("UUTT");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("UUTF");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("ABCD");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("BCDE");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("UUFT");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("UUFF");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("UUUU");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("TTTT");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("BCXE");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("EFGH");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("TUUU");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("Z0123");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 5U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("UUUT");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("UUUF");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("Z4567");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 5U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("Z89");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 3U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("TTUU");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("FFUU");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("TFUU");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("FFFF");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";
    size = trie.Size();

    trie.Remove("UUTU");
    EXPECT_GE(size, trie.Size()) << "Node count increased after removal";
    EXPECT_LE(size - trie.Size(), 4U) << "More nodes were removed than key size";

    // Tree should now be empty
    EXPECT_EQ(trie.Size(), 0U) << "Trie not empty after removing all inserted keys";
}

//!
//! \brief Test removal of keys that do not exist in the Trie.
//!
TEST_F(TrieTest, RemoveMissingKey)
{
    Trie<std::string> trie;
    trie.Insert("ABC");
    trie.Insert("ABCD");
    trie.Insert("AFGHJK");
    trie.Insert("ABXY");
    trie.Insert("ABXZ");
    EXPECT_FALSE(trie.Empty());

    UINT32 size = trie.Size();

    // First level difference
    trie.Remove("012");
    EXPECT_EQ(size, trie.Size());

    // Second level difference
    trie.Remove("A0");
    EXPECT_EQ(size, trie.Size());
    EXPECT_TRUE(trie.Contains("ABC"));
    EXPECT_TRUE(trie.Contains("ABCD"));
    EXPECT_TRUE(trie.Contains("AFGHJK"));
    EXPECT_TRUE(trie.Contains("ABXY"));
    EXPECT_TRUE(trie.Contains("ABXZ"));

    // Third level difference
    trie.Remove("AB0");
    EXPECT_EQ(size, trie.Size());
    trie.Remove("ABX");
    EXPECT_EQ(size, trie.Size());
    trie.Remove("AFG");
    EXPECT_EQ(size, trie.Size());
    EXPECT_TRUE(trie.Contains("ABC"));
    EXPECT_TRUE(trie.Contains("ABCD"));
    EXPECT_TRUE(trie.Contains("AFGHJK"));
    EXPECT_TRUE(trie.Contains("ABXY"));
    EXPECT_TRUE(trie.Contains("ABXZ"));

    // Past end of existing key
    trie.Remove("AFGHJK0");
    EXPECT_EQ(size, trie.Size());
    trie.Remove("ABXZ0");
    EXPECT_EQ(size, trie.Size());
    trie.Remove("ABZY0");
    EXPECT_EQ(size, trie.Size());
    trie.Remove("ABC0");
    EXPECT_EQ(size, trie.Size());
    EXPECT_TRUE(trie.Contains("ABC"));
    EXPECT_TRUE(trie.Contains("ABCD"));
    EXPECT_TRUE(trie.Contains("AFGHJK"));
    EXPECT_TRUE(trie.Contains("ABXY"));
    EXPECT_TRUE(trie.Contains("ABXZ"));

    // Part of existing key
    trie.Remove("A");
    EXPECT_EQ(size, trie.Size());
    trie.Remove("AB");
    EXPECT_EQ(size, trie.Size());
    trie.Remove("AFG");
    EXPECT_EQ(size, trie.Size());
    trie.Remove("AFGH");
    EXPECT_EQ(size, trie.Size());
    trie.Remove("AFGHJ");
    EXPECT_EQ(size, trie.Size());
    trie.Remove("ABX");
    EXPECT_EQ(size, trie.Size());
    EXPECT_TRUE(trie.Contains("ABC"));
    EXPECT_TRUE(trie.Contains("ABCD"));
    EXPECT_TRUE(trie.Contains("AFGHJK"));
    EXPECT_TRUE(trie.Contains("ABXY"));
    EXPECT_TRUE(trie.Contains("ABXZ"));
}

//!
//! \brief Test if Trie remains consistent after inserts and removes.
//!
TEST_F(TrieTest, TreeIntegrityAfterModification)
{
    Trie<std::string> trie;
    EXPECT_TRUE(trie.Empty()) << "Trie not empty on construction";

    trie.Insert("ABC");
    EXPECT_FALSE(trie.Empty());
    EXPECT_EQ(trie.Size(), 3U);

    trie.Insert("ABD");
    EXPECT_EQ(trie.Size(), 4U);

    trie.Insert("BCDE");
    EXPECT_EQ(trie.Size(), 8U);

    trie.Remove("ABC");
    EXPECT_EQ(trie.Size(), 7U);

    trie.Insert("ABCDE");
    EXPECT_EQ(trie.Size(), 10U);

    trie.Remove("ABC");
    EXPECT_EQ(trie.Size(), 10U);

    trie.Remove("ABCDE");
    EXPECT_EQ(trie.Size(), 7U);

    EXPECT_TRUE(trie.Contains("ABD"));
    EXPECT_TRUE(trie.Contains("BCDE"));
}

//!
//! \brief Test that overloads of Trie::Find, Trie::Insert, Trie::Remove, and
//! produce the same results.
//!
TEST_F(TrieTest, ConsistentBehaviourInOverloads)
{
    // Insert(1) == Insert(2)
    // Find(1) == Find(2)
    std::string s("ABCDEFGHIJKLM");
    std::string sub1 = s.substr(0, s.size() - 1);
    std::string sub2 = s.substr(0, s.size() - 2);
    std::string sub3 = s.substr(0, s.size() - 3);
    std::string sub4 = s.substr(0, s.size() - 4);
    std::string sub5 = s.substr(0, s.size() - 5);
    Trie<std::string> trie;

    // Test Insert(1), Find(1), Find(2)
    trie.Insert(s);
    EXPECT_EQ(trie.Size(), s.size());
    EXPECT_TRUE(trie.Contains(s));
    EXPECT_TRUE(trie.Contains(s.begin(), s.end()));
    EXPECT_FALSE(trie.Contains(sub1));
    EXPECT_FALSE(trie.Contains(sub1.begin(), sub1.end()));
    EXPECT_FALSE(trie.Contains(sub2));
    EXPECT_FALSE(trie.Contains(sub2.begin(), sub2.end()));
    EXPECT_FALSE(trie.Contains(sub3));
    EXPECT_FALSE(trie.Contains(sub3.begin(), sub3.end()));
    EXPECT_FALSE(trie.Contains(sub4));
    EXPECT_FALSE(trie.Contains(sub4.begin(), sub4.end()));
    EXPECT_FALSE(trie.Contains(sub5));
    EXPECT_FALSE(trie.Contains(sub5.begin(), sub5.end()));

    // Test Remove(1), Find(1), Find(2)
    trie.Remove(s);
    EXPECT_TRUE(trie.Empty());
    EXPECT_FALSE(trie.Contains(s));
    EXPECT_FALSE(trie.Contains(s.begin(), s.end()));

    // Test Insert(2)
    trie.Insert(s.begin(), s.end());
    EXPECT_EQ(trie.Size(), s.size());
    EXPECT_TRUE(trie.Contains(s));
    EXPECT_FALSE(trie.Contains(sub1));
    EXPECT_FALSE(trie.Contains(sub2));
    EXPECT_FALSE(trie.Contains(sub3));
    EXPECT_FALSE(trie.Contains(sub4));
    EXPECT_FALSE(trie.Contains(sub5));

    // Test Remove(2)
    trie.Remove(s.begin(), s.end());
    EXPECT_TRUE(trie.Empty());
    EXPECT_FALSE(trie.Contains(s));

    // TODO(stewarts): Follows(1), Follows(2)
}

//!
//! \brief Test Trie move constructor on empty tree.
//!
TEST_F(TrieTest, MoveConstructorEmptyTree)
{
    Trie<std::string> refTrie;
    EXPECT_TRUE(refTrie.Empty());

    const UINT32 size = refTrie.Size();

    Trie<std::string> trie(std::move(refTrie));
    EXPECT_TRUE(trie.Empty());
    EXPECT_EQ(size, trie.Size());
}

//!
//! \brief Test Trie move constructor on simple tree.
//!
TEST_F(TrieTest, MoveConstructorSimpleTree)
{
    Trie<std::string> refTrie;
    refTrie.Insert("ABC");
    refTrie.Insert("ABX");
    refTrie.Insert("ABY");
    refTrie.Insert("AB");
    EXPECT_FALSE(refTrie.Empty());
    EXPECT_EQ(refTrie.Size(), 5U) << "Trie does not have correct number of nodes for inserted keys"; //$

    // Ensure keys are in the reference Trie
    EXPECT_TRUE(refTrie.Contains("ABC")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABX")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABY")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("AB")) << "Missing key in reference tree";

    // Move construct Trie
    Trie<std::string> trie(std::move(refTrie));
    EXPECT_EQ(trie.Size(), 5U) << "Number of nodes changed after move";

    // Ensure same keys are in the new Trie
    EXPECT_TRUE(trie.Contains("ABC")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("ABX")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("ABY")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("AB")) << "Missing key after move";
}

//!
//! \brief Test Trie move constructor on complex tree.
//!
TEST_F(TrieTest, MoveConstructorComplexTree)
{
    Trie<std::string> refTrie;
    refTrie.Insert("UUUU");
    refTrie.Insert("TTTT");
    refTrie.Insert("FFFF");
    refTrie.Insert("TTUU");
    refTrie.Insert("FFUU");
    refTrie.Insert("TFUU");
    refTrie.Insert("TUUU");
    refTrie.Insert("FUUU");
    refTrie.Insert("UUTT");
    refTrie.Insert("UUTF");
    refTrie.Insert("UUTU");
    refTrie.Insert("UUFT");
    refTrie.Insert("UUFF");
    refTrie.Insert("UUFU");
    refTrie.Insert("UUUT");
    refTrie.Insert("UUUF");
    refTrie.Insert("ABCD");
    refTrie.Insert("BCDE");
    refTrie.Insert("BCXE");
    refTrie.Insert("EFGH");
    refTrie.Insert("Z0123");
    refTrie.Insert("Z4567");
    refTrie.Insert("Z89");
    EXPECT_FALSE(refTrie.Empty()) << "Empty after insertions";

    // Ensure keys are in the reference Trie
    EXPECT_TRUE(refTrie.Contains("UUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TTTT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FFFF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TTUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FFUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TFUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUUT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUUF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABCD")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("BCDE")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("BCXE")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("EFGH")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z0123")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z4567")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z89")) << "Missing key in reference tree";

    UINT32 refSize = refTrie.Size();

    // Move construct Trie
    Trie<std::string> trie(std::move(refTrie));
    EXPECT_EQ(trie.Size(), refSize) << "Number of nodes changed after move";

    // Ensure same keys are in the new Trie
    EXPECT_TRUE(trie.Contains("UUUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("TTTT")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("FFFF")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("TTUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("FFUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("TFUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("TUUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("FUUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUTT")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUTF")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUTU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUFT")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUFF")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUFU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUUT")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUUF")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("ABCD")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("BCDE")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("BCXE")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("EFGH")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("Z0123")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("Z4567")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("Z89")) << "Missing key after move";
}

//!
//! \brief Test Trie copy constructor on empty tree.
//!
TEST_F(TrieTest, CopyConstructorEmptyTree)
{
    Trie<std::string> refTrie;
    EXPECT_TRUE(refTrie.Empty());

    Trie<std::string> trie(refTrie);
    EXPECT_TRUE(trie.Empty());
    EXPECT_TRUE(refTrie.Empty());
    EXPECT_EQ(refTrie.Size(), trie.Size());
}

//!
//! \brief Test Trie copy constructor on simple tree.
//!
TEST_F(TrieTest, CopyConstructorSimpleTree)
{
    Trie<std::string> refTrie;
    refTrie.Insert("ABC");
    refTrie.Insert("ABX");
    refTrie.Insert("ABY");
    refTrie.Insert("AB");
    EXPECT_FALSE(refTrie.Empty());
    EXPECT_EQ(refTrie.Size(), 5U) << "Trie does not have correct number of nodes for inserted keys"; //$

    // Ensure keys are in the reference Trie
    EXPECT_TRUE(refTrie.Contains("ABC")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABX")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABY")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("AB")) << "Missing key in reference tree";

    // Copy construct Trie
    Trie<std::string> trie(refTrie);
    EXPECT_EQ(trie.Size(), 5U) << "Number of nodes changed after copy";
    EXPECT_EQ(trie.Size(), refTrie.Size()) << "Different number of nodes each tree";

    // Ensure same keys are in the new Trie
    EXPECT_TRUE(trie.Contains("ABC")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("ABX")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("ABY")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("AB")) << "Missing key in copy of Trie";

    // Ensure keys are still in the reference Trie
    EXPECT_TRUE(refTrie.Contains("ABC")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("ABX")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("ABY")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("AB")) << "Missing key in copied Trie";
}

//!
//! \brief Test Trie copy constructor on simple tree.
//!
TEST_F(TrieTest, CopyConstructorComplexTree)
{
    Trie<std::string> refTrie;
    refTrie.Insert("UUUU");
    refTrie.Insert("TTTT");
    refTrie.Insert("FFFF");
    refTrie.Insert("TTUU");
    refTrie.Insert("FFUU");
    refTrie.Insert("TFUU");
    refTrie.Insert("TUUU");
    refTrie.Insert("FUUU");
    refTrie.Insert("UUTT");
    refTrie.Insert("UUTF");
    refTrie.Insert("UUTU");
    refTrie.Insert("UUFT");
    refTrie.Insert("UUFF");
    refTrie.Insert("UUFU");
    refTrie.Insert("UUUT");
    refTrie.Insert("UUUF");
    refTrie.Insert("ABCD");
    refTrie.Insert("BCDE");
    refTrie.Insert("BCXE");
    refTrie.Insert("EFGH");
    refTrie.Insert("Z0123");
    refTrie.Insert("Z4567");
    refTrie.Insert("Z89");
    EXPECT_FALSE(refTrie.Empty()) << "Empty after insertions";

    // Ensure keys are in the reference Trie
    EXPECT_TRUE(refTrie.Contains("UUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TTTT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FFFF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TTUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FFUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TFUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUUT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUUF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABCD")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("BCDE")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("BCXE")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("EFGH")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z0123")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z4567")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z89")) << "Missing key in reference tree";

    UINT32 refSize = refTrie.Size();

    // Copy construct Trie
    Trie<std::string> trie(refTrie);
    EXPECT_EQ(trie.Size(), refSize) << "Number of nodes changed after copy";
    EXPECT_EQ(trie.Size(), refTrie.Size()) << "Different number of nodes each tree";

    // Ensure same keys are in the new Trie
    EXPECT_TRUE(trie.Contains("UUUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("TTTT")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("FFFF")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("TTUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("FFUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("TFUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("TUUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("FUUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUTT")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUTF")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUTU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUFT")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUFF")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUFU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUUT")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUUF")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("ABCD")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("BCDE")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("BCXE")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("EFGH")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("Z0123")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("Z4567")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("Z89")) << "Missing key in copy of Trie";

    // Ensure keys are still in the reference Trie
    EXPECT_TRUE(refTrie.Contains("UUUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("TTTT")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("FFFF")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("TTUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("FFUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("TFUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("TUUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("FUUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUTT")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUTF")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUTU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUFT")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUFF")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUFU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUUT")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUUF")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("ABCD")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("BCDE")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("BCXE")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("EFGH")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("Z0123")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("Z4567")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("Z89")) << "Missing key in copied Trie";
}

//!
//! \brief Test Trie move assignment operator on simple tree.
//!
TEST_F(TrieTest, MoveAssignmentOperatorEmptyTree)
{
    Trie<std::string> refTrie;
    EXPECT_TRUE(refTrie.Empty());

    const UINT32 size = refTrie.Size();

    Trie<std::string> trie = std::move(refTrie);
    EXPECT_TRUE(trie.Empty());
    EXPECT_EQ(size, trie.Size());
}

//!
//! \brief Test Trie move assignment operator on simple tree.
//!
TEST_F(TrieTest, MoveAssignmentOperatorSimpleTree)
{
        Trie<std::string> refTrie;
    refTrie.Insert("ABC");
    refTrie.Insert("ABX");
    refTrie.Insert("ABY");
    refTrie.Insert("AB");
    EXPECT_FALSE(refTrie.Empty());
    EXPECT_EQ(refTrie.Size(), 5U) << "Trie does not have correct number of nodes for inserted keys"; //$

    // Ensure keys are in the reference Trie
    EXPECT_TRUE(refTrie.Contains("ABC")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABX")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABY")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("AB")) << "Missing key in reference tree";

    // Move assign Trie
    Trie<std::string> trie = std::move(refTrie);
    EXPECT_EQ(trie.Size(), 5U) << "Number of nodes changed after move";

    // Ensure same keys are in the new Trie
    EXPECT_TRUE(trie.Contains("ABC")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("ABX")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("ABY")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("AB")) << "Missing key after move";
}

//!
//! \brief Test Trie move assignment operator on complexe tree.
//!
TEST_F(TrieTest, MoveAssignmentOperatorComplexTree)
{
    Trie<std::string> refTrie;
    refTrie.Insert("UUUU");
    refTrie.Insert("TTTT");
    refTrie.Insert("FFFF");
    refTrie.Insert("TTUU");
    refTrie.Insert("FFUU");
    refTrie.Insert("TFUU");
    refTrie.Insert("TUUU");
    refTrie.Insert("FUUU");
    refTrie.Insert("UUTT");
    refTrie.Insert("UUTF");
    refTrie.Insert("UUTU");
    refTrie.Insert("UUFT");
    refTrie.Insert("UUFF");
    refTrie.Insert("UUFU");
    refTrie.Insert("UUUT");
    refTrie.Insert("UUUF");
    refTrie.Insert("ABCD");
    refTrie.Insert("BCDE");
    refTrie.Insert("BCXE");
    refTrie.Insert("EFGH");
    refTrie.Insert("Z0123");
    refTrie.Insert("Z4567");
    refTrie.Insert("Z89");
    EXPECT_FALSE(refTrie.Empty()) << "Empty after insertions";

    // Ensure keys are in the reference Trie
    EXPECT_TRUE(refTrie.Contains("UUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TTTT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FFFF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TTUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FFUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TFUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUUT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUUF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABCD")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("BCDE")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("BCXE")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("EFGH")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z0123")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z4567")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z89")) << "Missing key in reference tree";

    UINT32 refSize = refTrie.Size();

    // Move assign Trie
    Trie<std::string> trie = std::move(refTrie);
    EXPECT_EQ(trie.Size(), refSize) << "Number of nodes changed after move";

    // Ensure same keys are in the new Trie
    EXPECT_TRUE(trie.Contains("UUUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("TTTT")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("FFFF")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("TTUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("FFUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("TFUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("TUUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("FUUU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUTT")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUTF")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUTU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUFT")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUFF")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUFU")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUUT")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("UUUF")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("ABCD")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("BCDE")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("BCXE")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("EFGH")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("Z0123")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("Z4567")) << "Missing key after move";
    EXPECT_TRUE(trie.Contains("Z89")) << "Missing key after move";
}

//!
//! \brief Test Trie copy assignment operator on empty tree.
//!
TEST_F(TrieTest, CopyAssignmentOperatorEmptyTree)
{
    Trie<std::string> refTrie;
    EXPECT_TRUE(refTrie.Empty());

    Trie<std::string> trie = refTrie;
    EXPECT_TRUE(trie.Empty());
    EXPECT_TRUE(refTrie.Empty());
    EXPECT_EQ(refTrie.Size(), trie.Size());
}

//!
//! \brief Test Trie copy assignment operator on simple tree.
//!
TEST_F(TrieTest, CopyAssignmentOperatorSimpleTree)
{
    Trie<std::string> refTrie;
    refTrie.Insert("ABC");
    refTrie.Insert("ABX");
    refTrie.Insert("ABY");
    refTrie.Insert("AB");
    EXPECT_FALSE(refTrie.Empty());
    EXPECT_EQ(refTrie.Size(), 5U) << "Trie does not have correct number of nodes for inserted keys"; //$

    // Ensure keys are in the reference Trie
    EXPECT_TRUE(refTrie.Contains("ABC")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABX")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABY")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("AB")) << "Missing key in reference tree";

    // Copy assign Trie
    Trie<std::string> trie = refTrie;
    EXPECT_EQ(trie.Size(), 5U) << "Number of nodes changed after copy";
    EXPECT_EQ(trie.Size(), refTrie.Size()) << "Different number of nodes each tree";

    // Ensure same keys are in the new Trie
    EXPECT_TRUE(trie.Contains("ABC")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("ABX")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("ABY")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("AB")) << "Missing key in copy of Trie";

    // Ensure keys are still in the reference Trie
    EXPECT_TRUE(refTrie.Contains("ABC")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("ABX")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("ABY")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("AB")) << "Missing key in copied Trie";
}

//!
//! \brief Test Trie copy assignment operator on complex tree.
//!
TEST_F(TrieTest, CopyAssignmentOperatorComplexTree)
{
    Trie<std::string> refTrie;
    refTrie.Insert("UUUU");
    refTrie.Insert("TTTT");
    refTrie.Insert("FFFF");
    refTrie.Insert("TTUU");
    refTrie.Insert("FFUU");
    refTrie.Insert("TFUU");
    refTrie.Insert("TUUU");
    refTrie.Insert("FUUU");
    refTrie.Insert("UUTT");
    refTrie.Insert("UUTF");
    refTrie.Insert("UUTU");
    refTrie.Insert("UUFT");
    refTrie.Insert("UUFF");
    refTrie.Insert("UUFU");
    refTrie.Insert("UUUT");
    refTrie.Insert("UUUF");
    refTrie.Insert("ABCD");
    refTrie.Insert("BCDE");
    refTrie.Insert("BCXE");
    refTrie.Insert("EFGH");
    refTrie.Insert("Z0123");
    refTrie.Insert("Z4567");
    refTrie.Insert("Z89");
    EXPECT_FALSE(refTrie.Empty()) << "Empty after insertions";

    // Ensure keys are in the reference Trie
    EXPECT_TRUE(refTrie.Contains("UUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TTTT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FFFF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TTUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FFUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TFUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("TUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("FUUU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUTU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUFU")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUUT")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("UUUF")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("ABCD")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("BCDE")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("BCXE")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("EFGH")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z0123")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z4567")) << "Missing key in reference tree";
    EXPECT_TRUE(refTrie.Contains("Z89")) << "Missing key in reference tree";

    UINT32 refSize = refTrie.Size();

    // Copy assign Trie
    Trie<std::string> trie = refTrie;
    EXPECT_EQ(trie.Size(), refSize) << "Number of nodes changed after copy";
    EXPECT_EQ(trie.Size(), refTrie.Size()) << "Different number of nodes each tree";

    // Ensure same keys are in the new Trie
    EXPECT_TRUE(trie.Contains("UUUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("TTTT")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("FFFF")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("TTUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("FFUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("TFUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("TUUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("FUUU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUTT")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUTF")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUTU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUFT")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUFF")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUFU")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUUT")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("UUUF")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("ABCD")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("BCDE")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("BCXE")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("EFGH")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("Z0123")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("Z4567")) << "Missing key in copy of Trie";
    EXPECT_TRUE(trie.Contains("Z89")) << "Missing key in copy of Trie";

    // Ensure keys are still in the reference Trie
    EXPECT_TRUE(refTrie.Contains("UUUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("TTTT")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("FFFF")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("TTUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("FFUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("TFUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("TUUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("FUUU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUTT")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUTF")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUTU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUFT")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUFF")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUFU")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUUT")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("UUUF")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("ABCD")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("BCDE")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("BCXE")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("EFGH")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("Z0123")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("Z4567")) << "Missing key in copied Trie";
    EXPECT_TRUE(refTrie.Contains("Z89")) << "Missing key in copied Trie";
}

//!
//! \brief Test random insertions and deletions on the Trie.
//!
TEST_F(TrieTest, RandomModification)
{
    const UINT32 maxKeySize = 100;
    std::srand(12345); // Arbitrary seed

    Trie<std::string> trie;
    ASSERT_TRUE(trie.Empty());

    std::vector<std::string> inserted;

    UINT32 size = trie.Size();
    UINT32 iterations = 1000;

    // Insert random
    for (UINT32 n = 0; n < iterations; ++n)
    {
        inserted.push_back(RandString(std::rand() % maxKeySize));
        std::string& s = inserted.back();
        if (s.size() == 0)
        {
            // Can't insert keys of size 0
            inserted.pop_back();
            continue;
        }

        trie.Insert(s);
        ASSERT_GE(trie.Size(), size) << "Node count decreased after insert";
        ASSERT_LE(trie.Size() - size, s.size()) << "More nodes were inserted than key size";
        size = trie.Size();
    }

    // Remove random
    for (UINT32 n = 0; n < iterations; ++n)
    {
        std::string s = RandString(std::rand() % maxKeySize);
        trie.Remove(s);
        ASSERT_GE(size, trie.Size()) << "Node count increased after removal";
        ASSERT_LE(size - trie.Size(), s.size()) << "More nodes were removed than key size";
        size = trie.Size();
    }

    // Remove inserted
    for (const auto& s : inserted)
    {
        trie.Remove(s);
        ASSERT_GE(size, trie.Size()) << "Node count increased after removal";
        ASSERT_LE(size - trie.Size(), s.size()) << "More nodes were removed than key size";
        size = trie.Size();
    }

    // Ensure Trie has no nodes left
    ASSERT_EQ(trie.Size(), 0U) << "Trie contains nodes after all keys removed";
}
