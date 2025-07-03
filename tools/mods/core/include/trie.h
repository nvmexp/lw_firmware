/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All information
 * contained herein is proprietary and confidential to LWPU Corporation.  Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/massert.h"
#include "core/include/types.h"

#include <memory>
#include <stack>
#include <string>
#include <vector>

//!
//! \brief Trie data structure
//!
//! Child ordering in the internal representation is not garanteed.
//!
//! \tparam TKeyIterable Key type. Must be iterable.
//!
template <class TKeyIterable>
class Trie
{
public:
    using KeyIterator = typename TKeyIterable::iterator;
    using KeyConstIterator = typename TKeyIterable::const_iterator;
    using KeyPart = typename KeyIterator::value_type;

    Trie();
    Trie(const Trie<TKeyIterable>& other);
    Trie(Trie<TKeyIterable>&& other);
    ~Trie();

    bool Empty() const { return m_NumNodes == 0; }
    UINT32 Size() const { return m_NumNodes; }
    void Clear();

    void Insert(TKeyIterable key);
    void Insert(KeyConstIterator begin, KeyConstIterator end);

    void Remove(TKeyIterable key);
    void Remove(KeyConstIterator begin, KeyConstIterator end);

    bool Contains(TKeyIterable key) const;
    bool Contains
    (
        KeyConstIterator begin
        ,KeyConstIterator end
    ) const;

    std::vector<KeyPart> Follows(TKeyIterable key) const;
    std::vector<KeyPart> Follows
    (
        KeyConstIterator begin
        ,KeyConstIterator end
    ) const;

    Trie<TKeyIterable>& operator=(const Trie<TKeyIterable>& other);
    Trie<TKeyIterable>& operator=(Trie<TKeyIterable>&& other);

protected:
    struct Node
    {
        KeyPart keyPart;                //!< Part of the key represented
        bool keyEnd;                    //!< True if node is end of a key
        std::unique_ptr<Node> pNext;    //!< Next node at this tree level
        std::unique_ptr<Node> pChild;   //!< Child node

        explicit Node(const KeyPart& kp) :
            keyPart(kp), keyEnd(false), pNext(nullptr), pChild(nullptr) {}
    };

    //!
    //! \brief Used for matching nodes in trees during copy construction.
    //!
    struct NodePair
    {
        std::unique_ptr<Node>* ppThis;          //!< Node on this tree
        const std::unique_ptr<Node>* ppOther;   //!< Corresponding node on other tree

        NodePair() : ppThis(nullptr), ppOther(nullptr) {}
        NodePair
        (
            std::unique_ptr<Node>* ppThis
            ,const std::unique_ptr<Node>* ppOther
        ) : ppThis(ppThis), ppOther(ppOther) {}
    };

    std::unique_ptr<Node> m_pRoot;      //!< Root of the tree
    UINT32 m_NumNodes;                  //!< Number of nodes in the tree

    // TODO: Possible enhancements
    // UINT32 m_Depth;                  //!< Deepest level of the tree
    // - sort nodes in each level
    // const_iterator Find(TKeyIterable)

    bool IsLeaf(Node* pNode) const;

    Node* Search(KeyConstIterator begin, KeyConstIterator end) const;
};

//------------------------------------------------------------------------------
//!
//! \brief Default constructor.
//!
template <class TKeyIterable>
Trie<TKeyIterable>::Trie()
    : m_pRoot(nullptr), m_NumNodes(0) {}

//!
//! \brief Copy constructor.
//!
template <class TKeyIterable>
Trie<TKeyIterable>::Trie(const Trie<TKeyIterable>& other)
    : m_pRoot(nullptr), m_NumNodes(0)
{
    // Traverse other tree and copy its nodes as we go
    //
    std::stack<NodePair> toVisit;
    NodePair pair;

    // Nothing to do if tree is empty
    if (!other.m_pRoot) { return; }

    // DFS tree traversal
    toVisit.push(NodePair(&m_pRoot, &other.m_pRoot));

    while (!toVisit.empty())
    {
        pair = toVisit.top();
        toVisit.pop();

        // Add the current node to our tree
        //
        *pair.ppThis = make_unique<Node>((*pair.ppOther)->keyPart);
        (*pair.ppThis)->keyEnd = (*pair.ppOther)->keyEnd;

        if (!IsLeaf(pair.ppOther->get()))
        {
            // Add adjacent nodes to visit
            //
            if ((*pair.ppOther)->pChild)
            {
                toVisit.push(NodePair(&(*pair.ppThis)->pChild,
                                      &(*pair.ppOther)->pChild));
            }
            if ((*pair.ppOther)->pNext)
            {
                toVisit.push(NodePair(&(*pair.ppThis)->pNext,
                                      &(*pair.ppOther)->pNext));
            }
        }
    }

    // Update node count
    m_NumNodes = other.m_NumNodes;
}

//!
//! \brief Move constructor.
//!
template <class TKeyIterable>
Trie<TKeyIterable>::Trie(Trie<TKeyIterable>&& other)
{
    m_pRoot = std::move(other.m_pRoot);
    m_NumNodes = other.m_NumNodes;
}

//!
//! \brief Destructor.
//!
template <class TKeyIterable>
Trie<TKeyIterable>::~Trie()
{
    // Manually destruct to avoid unique_ptr destructor stack overflow.
    Clear();
}

//!
//! \brief Clear the contents.
//!
template <class TKeyIterable>
void Trie<TKeyIterable>::Clear()
{
    // NOTE: Letting unique_ptrs handle the destruction could cause a stack
    // overflow. Avoid this by traversing the tree depth first and explicitly
    // destructing each node.
    //
    // nodes contain unique_ptrs, and unique_ptr destructors call the destructor
    // of the object they own. If the tree is large, it is possible to cause a
    // stack overflow with the unique_ptr destructor stack frames when the root
    // node is destructed.

    std::stack<std::unique_ptr<Node>*> toDelete;
    std::stack<std::unique_ptr<Node>*> toVisit;
    std::unique_ptr<Node>* ppLwr;

    // Nothing to do if tree is empty
    if (!m_pRoot) { return; }

    // DFS tree traversal
    toVisit.push(&m_pRoot);

    while (!toVisit.empty())
    {
        ppLwr = toVisit.top();
        toVisit.pop();

        if (IsLeaf(ppLwr->get()))
        {
            // Leaf node, can be deleted
            ppLwr->reset();
            --m_NumNodes;

            // After a leaf, attempt to delete additional nodes in the
            // deletion stack to minimize stack size.
            std::unique_ptr<Node>* ppNode;

            while (!toDelete.empty())
            {
                ppNode = toDelete.top();

                // Can delete if there is no child or next
                if (IsLeaf(ppNode->get()))
                {
                    ppNode->reset();
                    toDelete.pop();
                    --m_NumNodes;
                }
                else
                {
                    // Can't delete, stop trying.
                    break;
                }
            }
        }
        else
        {
            // Add adjacent nodes and mark current node for deletion.
            //
            // NOTE: Make sure to push the next node last, as the next nodes
            // need to be visited before the child nodes. If they aren't, the
            // child could free the next node before it is visited, resulting in
            // a segfault.
            if ((*ppLwr)->pChild) { toVisit.push(&(*ppLwr)->pChild); }
            if ((*ppLwr)->pNext)  { toVisit.push(&(*ppLwr)->pNext);  }

            toDelete.push(ppLwr);
        }
    }

    // Delete remaining nodes
    while (!toDelete.empty())
    {
        toDelete.top()->reset();
        toDelete.pop();
        --m_NumNodes;
    }
}

//!
//! \brief Insert the given key.
//!
//! \param key Iteratable key with input iterator support.
//!
template <class TKeyIterable>
void Trie<TKeyIterable>::Insert(TKeyIterable key)
{
    Insert(std::begin(key), std::end(key));
}

//!
//! \brief Insert the given key.
//!
//! \param begin Input iterator to the beginning of the key.
//! \param end Input iterator to the end of the key.
//!
template <class TKeyIterable>
void Trie<TKeyIterable>::Insert
(
    KeyConstIterator begin
    ,KeyConstIterator end
)
{
    MASSERT(begin != end);

    KeyConstIterator it = begin;
    std::unique_ptr<Node>* ppNode = &m_pRoot;

    while (it != end)
    {
        // Find current key part (*it) at current tree depth. If it does not
        // exist, create it
        //
        if (!(*ppNode))
        {
            // No node, create one
            *ppNode = std::make_unique<Node>(*it);
            ++m_NumNodes;
        }
        else if ((*ppNode)->keyPart != *it)
        {
            // Diff key part, try next node
            ppNode = &(*ppNode)->pNext;
            continue;
        }

        // Key part has been found in tree or inserted. Move to the next level
        // in the tree for the next key part.
        //
        ++it;

        // Move to child
        if (it != end)
        {
            ppNode = &(*ppNode)->pChild;
        }
    }

    // Update the node to be the key's end
    (*ppNode)->keyEnd = true;
}

//!
//! \brief Remove the given key.
//!
//! \param key Iteratable key with input iterator support.
//!
template <class TKeyIterable>
void Trie<TKeyIterable>::Remove(TKeyIterable key)
{
    Remove(std::begin(key), std::end(key));
}

//!
//! \brief Remove the given key.
//!
//! \param begin Input iterator to the beginning of the key.
//! \param end Input iterator to the end of the key.
//!
template <class TKeyIterable>
void Trie<TKeyIterable>::Remove
(
    KeyConstIterator begin
    ,KeyConstIterator end
)
{
    KeyConstIterator it = begin;
    std::stack<std::unique_ptr<Node>*> toRemove;
    std::unique_ptr<Node>* ppNode = &m_pRoot;

    // Find nodes corresponding to the given key
    //
    while (it != end)
    {
        // Find current key part (*it) at current tree depth
        //
        if (!(*ppNode))
        {
            // No node, done searching
            break;
        }
        else if ((*ppNode)->keyPart == *it)
        {
            // Same key part, record and move to child
            toRemove.push(ppNode);
            ppNode = &(*ppNode)->pChild;
            ++it;
        }
        else
        {
            // Diff key part, try next child
            ppNode = &(*ppNode)->pNext;
            continue;
        }
    }

    // Stop if we didn't find the full key, didn't find anything, or given key
    // didn't end at a known key
    if (it != end || toRemove.empty() || !((*toRemove.top())->keyEnd))
    {
        return;
    }

    // Node is no longer a valid key end point
    (*toRemove.top())->keyEnd = false;

    // Remove nodes
    //
    while (!toRemove.empty())
    {
        ppNode = toRemove.top();
        toRemove.pop();

        // If node has a child, do not remove
        if ((*ppNode)->pChild) { break; }

        // Check for next node
        if ((*ppNode)->pNext)
        {
            // Has a next node. Move the next node over to take this node's
            // place in the tree

            // Save pointer to the node to remove
            unique_ptr<Node> pNode = std::move(*ppNode);

            // Replace parent's child pointer with the next of the current node
            *ppNode = std::move(pNode->pNext);

            // Clear reference to moved next node and remove
            pNode->pNext = nullptr;
            pNode.reset();

            // NOTE: Top node in the deletion stack now has a child and won't
            // get removed.
        }
        else
        {
            // Remove the node
            ppNode->reset();
        }

        --m_NumNodes;
    }
}

//!
//! \brief Check if the given key is inserted.
//!
//! \param key Iteratable key with input iterator support.
//! \return True if the key is found, false otherwise.
//!
template <class TKeyIterable>
bool Trie<TKeyIterable>::Contains(TKeyIterable key) const
{
    return Contains(std::begin(key), std::end(key));
}

//!
//! \brief Check if the given key is inserted.
//!
//! \param begin Input iterator to the beginning of the key.
//! \param end Input iterator to the end of the key.
//! \return True if the key is found, false otherwise.
//!
template <class TKeyIterable>
bool Trie<TKeyIterable>::Contains
(
    KeyConstIterator begin
    ,KeyConstIterator end
) const
{
    Node* pNode = Search(begin, end);

    // Contained if the key's final node is a key end point
    return pNode && pNode->keyEnd;
}

//!
//! \brief Return all the values that could follow the given key.
//!
//! Return an empty vector if the key is not found or if there are no values
//! that follow.
//!
//! \param key Iteratable key with input iterator support.
//!
template <class TKeyIterable>
std::vector<typename Trie<TKeyIterable>::KeyPart>
Trie<TKeyIterable>::Follows(TKeyIterable key) const
{
    return std::move(Follows(std::begin(key), std::end(key)));
}

//!
//! \brief Return all the values that could follow the given key.
//!
//! Return an empty vector if the key is not found or if there are no values
//! that follow.
//!
//! \param begin Input iterator to the beginning of the key.
//! \param end Input iterator to the end of the key.
//!
template <class TKeyIterable>
std::vector<typename Trie<TKeyIterable>::KeyPart>
Trie<TKeyIterable>::Follows
(
    KeyConstIterator begin
    ,KeyConstIterator end
) const
{
    std::vector<KeyPart> childKeys;

    Node* pNode = nullptr;

    // Find the node associated with the end of the key
    //
    if (begin == end)
    {
        // Want to find all possible starting values
        pNode = m_pRoot.get();
    }
    else
    {
        // Find the node corresponding to the given starting values
        pNode = Search(begin, end);
        if (pNode)
        {
            // Want to walk the node's children
            pNode = pNode->pChild.get();
        }
    }

    // Walk nodes at this level to find what follows
    //
    while (pNode)
    {
        childKeys.push_back(pNode->keyPart);
        pNode = pNode->pNext.get();
    }

    childKeys.shrink_to_fit();
    return childKeys;
}

//!
//! \brief Copy assignment operator.
//!
//! Does not use the copy-and-swap idiom for performance.
//!
//! \param other Object to copy.
//! \return Reference to the current object.
//!
template <class TKeyIterable>
Trie<TKeyIterable>&
Trie<TKeyIterable>::operator=(const Trie<TKeyIterable>& other)
{
    // Check for self assignment
    if (&other == this) { return *this; }

    Trie<TKeyIterable> tmp(other);
    *this = std::move(tmp);
    return *this;
}

//!
//! \brief Move assignment operator.
//!
//! \param other Object to move.
//! \return Reference to the current object.
//!
template <class TKeyIterable>
Trie<TKeyIterable>&
Trie<TKeyIterable>::operator=(Trie<TKeyIterable>&& other)
{
    m_pRoot = std::move(other.m_pRoot);
    m_NumNodes = other.m_NumNodes;
    return *this;
}

//!
//! \brief Return true if the given node is a leaf node.
//!
//! \param pNode Pointer to a node.
//! \return True if given node is a leaf node, false otherwise.
//!
template <class TKeyIterable>
bool Trie<TKeyIterable>::IsLeaf(Node* pNode) const
{
    return !(pNode->pChild || pNode->pNext);
}

//!
//! \brief Get the node corresponding to the given key, or nullptr if not found.
//!
//! \param begin Input iterator to the beginning of the key.
//! \param end Input iterator to the end of the key.
//! \return Node corresponding to the given key, or nullptr if not found.
//!
template <class TKeyIterable>
typename Trie<TKeyIterable>::Node*
Trie<TKeyIterable>::Search
(
    KeyConstIterator begin
    ,KeyConstIterator end
) const
{
    KeyConstIterator it = begin;
    Node* pNode = nullptr;

    if (it != end)
    {
        pNode = m_pRoot.get();

        // Walk the tree
        while (it != end)
        {
            if (!pNode)
            {
                // No node, nothing to find
                return nullptr;
            }
            else if (pNode->keyPart != *it)
            {
                // Diff key, try next child
                pNode = pNode->pNext.get();
                continue;
            }

            ++it;

            if (it != end)
            {
                // Move to child
                pNode = pNode->pChild.get();
            }
        }
    }

    return pNode;
}
