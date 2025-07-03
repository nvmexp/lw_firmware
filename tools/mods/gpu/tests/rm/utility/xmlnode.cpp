/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file xmlnode.cpp
//! \brief A node of an XML element tree
//!
//! Body for an XML element node class
//!

#include "expat.h"

#include "xmlnode.h"
#include "core/include/memcheck.h"

XmlNode::XmlNode(const char *name, XmlNode *parent_node)
: m_name(name), m_parent_node(parent_node)
{
}

void XmlNode::add_child(XmlNode *child_node)
{
    m_child_list.push_back(child_node);
}
