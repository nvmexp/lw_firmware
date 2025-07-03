/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 1993-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */


/***************************** Balanced Tree *******************************\
*                                                                           *
*   A generic library to red black tree -- every operation is O(log(n))     *
*   check http://en.wikipedia.org/wiki/Red-black_tree or similar www pages  *
*                                                                           *
\***************************************************************************/

#include "libos.h"
#include "librbtree.h"

/*!
 *   Left Rotation
 * 
 *         X               Y     
 *        /  \            / \
 *       C    Y    ->    X   B
 *           / \        / \
 *          A   B      C   A
 */
static void LibosTreeRotateLeft(LibosTreeHeader *x, void (*mutator)(LibosTreeHeader * node))
{
    LibosTreeHeader *y = x->right;

    x->right = y->left;
    
    if (!y->left->isNil)
    {
        y->left->parent = x;
    }

    y->parent = x->parent;

    if (!x->parent->isNil)
    {
        if (x == x->parent->left)
        {
            x->parent->left = y;
        }
        else
        {
            x->parent->right = y;
        }
    }
    else
    {
        // x->parent is header node
        LibosTreeHeader * headerNode = x->parent;

        // Update tree root
        headerNode->parent = y;
    }

    // link x and y
    y->left = x;
    x->parent = y;

    // X is the lower subtree and must be updated first
    if (mutator) {
        mutator(x);
        mutator(y);
    }
}

/*!
 *   Right Rotation
 * 
 *          Y            X     
 *         / \          / \   
 *        X   B     -> C   Y  
 *       / \              / \ 
 *      C   A            A   B
 */
static void LibosTreeRotateRight(LibosTreeHeader *x, void (*mutator)(LibosTreeHeader * node))
{
    LibosTreeHeader *y = x->left;

    x->left = y->right;
    if (!y->right->isNil)
    {
        y->right->parent = x;
    }

    y->parent = x->parent;
    if (!x->parent->isNil)
    {
        if (x == x->parent->right)
        {
            x->parent->right = y;
        }
        else
        {
            x->parent->left = y;
        }
    }
    else
    {
        // x->parent is header node
        LibosTreeHeader * headerNode = x->parent;

        headerNode->parent = y;
    }

    // link x and y
    y->right = x;
    x->parent = y;

    // Y is the lower subtree and must be updated first
    if (mutator)
    {
        mutator(y);    
        mutator(x);
    }
}

// rbt helper function:
//  - maintain red-black tree balance after inserting node x
// @todo: Return new proot
void LibosTreeInsertFixup(LibosTreeHeader *x, void (*mutator)(LibosTreeHeader * node))
{
    LibosTreeHeader * header;
    
    // Nodes must be inserted as leaves
    // The header is the only nil node
    x->isRed = LW_TRUE;
    x->isNil = LW_FALSE;
    
    LibosAssert(x->left->isNil && x->right->isNil);
    header = x->left;
    
    // check red-black properties
    while(!x->parent->isNil && x->parent->isRed)
    {
        // we have a violation
        if (x->parent == x->parent->parent->left)
        {
            LibosTreeHeader *y = x->parent->parent->right;
            if (y->isRed)
            {
                // uncle is RED
                x->parent->isRed = LW_FALSE;
                y->isRed = LW_FALSE;
                x->parent->parent->isRed = LW_TRUE;
                x = x->parent->parent;
            }
            else
            {
                // uncle is BLACK
                if (x == x->parent->right)
                {
                    // make x a left child
                    x = x->parent;
                    LibosTreeRotateLeft(x, mutator);
                }

                // recolor and rotate
                x->parent->isRed = LW_FALSE;
                x->parent->parent->isRed = LW_TRUE;
                LibosTreeRotateRight(x->parent->parent, mutator);
            }
        }
        else
        {
            // mirror image of above code
            LibosTreeHeader *y = x->parent->parent->left;
            if (y->isRed)
            {
                // uncle is RED
                x->parent->isRed = LW_FALSE;
                y->isRed = LW_FALSE;
                x->parent->parent->isRed = LW_TRUE;
                x = x->parent->parent;
            }
            else
            {
                // uncle is BLACK
                if (x == x->parent->left)
                {
                    x = x->parent;
                    LibosTreeRotateRight(x, mutator);
                }
                x->parent->isRed = LW_FALSE;
                x->parent->parent->isRed = LW_TRUE;
                LibosTreeRotateLeft(x->parent->parent, mutator);
            }
        }
    }
    
    header->parent->isRed = LW_FALSE;
}

static void LibosTreeDeleteFixup(LibosTreeHeader *parentOfX, LibosTreeHeader *x, void (*mutator)(LibosTreeHeader * node))
{
    while (!parentOfX->isNil && (x->isNil || !x->isRed))
    {
        if (parentOfX->left == x)
        {
            LibosTreeHeader *w = parentOfX->right;
            if (w->isRed)
            {
                w->isRed = LW_FALSE;
                parentOfX->isRed = LW_TRUE;
                LibosTreeRotateLeft(parentOfX, mutator);
                w = parentOfX->right;
            }

            if (w->isNil)
                x = parentOfX;
            else if (!w->left->isRed && !w->right->isRed)
            {
                w->isRed = LW_TRUE;
                x = parentOfX;
            }
            else
            {
                if (!w->right->isRed)
                {
                    w->left->isRed = LW_FALSE;
                    w->isRed = LW_TRUE;
                    LibosTreeRotateRight(w, mutator);
                    w = parentOfX->right;
                }
                w->isRed = parentOfX->isRed;
                parentOfX->isRed = LW_FALSE;
                w->right->isRed = LW_FALSE;
                LibosTreeRotateLeft(parentOfX, mutator);
                break;
            }
        }
        else
        {
            LibosTreeHeader *w = parentOfX->left;
            if (w->isRed)
            {
                w->isRed = LW_FALSE;
                parentOfX->isRed = LW_TRUE;
                LibosTreeRotateRight(parentOfX, mutator);
                w = parentOfX->left;
            }

            if (w->isNil)
                x = parentOfX;
            else if (!w->right->isRed && !w->left->isRed)
            {
                w->isRed = LW_TRUE;
                x = parentOfX;
            }
            else
            {
                if (!w->left->isRed)
                {
                    w->right->isRed = LW_FALSE;
                    w->isRed = LW_TRUE;
                    LibosTreeRotateLeft(w, mutator);
                    w = parentOfX->left;
                }
                w->isRed = parentOfX->isRed;
                parentOfX->isRed = LW_FALSE;
                w->left->isRed = LW_FALSE;
                LibosTreeRotateRight(parentOfX, mutator);
                break;
            }
        }
        parentOfX = x->parent;
    }
    x->isRed = LW_FALSE;
}

void
LibosTreeUnlink
(
    LibosTreeHeader * pNode,
    void (*mutator)(LibosTreeHeader * node)
)
{
    LibosTreeHeader *x;
    LibosTreeHeader *y;
    LibosTreeHeader *z = pNode;
    LibosTreeHeader *parentOfX;
    LwU32 yWasBlack;

    // unlink
    if (z->left->isNil || z->right->isNil)
    {
        // y has a SENTINEL node as a child
        y = z;
    }
    else
    {
        // find tree successor
        y = z->right;
        while (!y->left->isNil)
        {
            y = y->left;
        }
    }

    // x is y's only child
    if (!y->left->isNil)
    {
        x = y->left;
    }
    else
    {
        x = y->right;
    }

    // remove y from the parent chain
    parentOfX = y->parent;
    if (!x->isNil)
    {
        x->parent = parentOfX;
    }
    if (!y->parent->isNil)
    {
        if (y == y->parent->left)
        {
            y->parent->left = x;
        }
        else
        {
            y->parent->right = x;
        }
    }
    else
    {
        // y->parent is the header node since it's nil
        LibosTreeHeader * header = y->parent;
        
        // Set tree root
        header->parent = x;
    }

    yWasBlack = !y->isRed;
    if (y != z)
    {
        // we need to replace z with y so the memory for z can be freed
        y->parent = z->parent;
        if (!z->parent->isNil)
        {
            if (z == z->parent->left)
            {
                z->parent->left = y;
            }
            else
            {
                z->parent->right = y;
            }
        }
        else
        {
            // z->parent is the header node since it's nil
            LibosTreeHeader * header = z->parent;
                        
            // Update the tree root
            header->parent = y;
        }

        y->isRed = z->isRed;

        y->left = z->left;
        if (!z->left->isNil)
        {
            z->left->parent = y;
        }
        y->right = z->right;
        if (!z->right->isNil)
        {
            z->right->parent = y;
        }

        if (parentOfX == z)
        {
            parentOfX = y;
        }
    }

    if (yWasBlack)
        LibosTreeDeleteFixup(parentOfX, x, mutator);
}

void LibosTreeConstruct(LibosTreeHeader *header)
{
    header->parent = header->left = header->right = header;
    header->isNil = LW_TRUE;
    header->isRed = LW_FALSE;
}