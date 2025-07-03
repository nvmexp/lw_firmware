/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2008 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file xml_node.cpp
//! \brief A node of an XML element tree
//!
//! Body for an XML element node class
//!

#include "expat.h"

#include "xml_node.h"

XMLNode::XMLNode(const char *name, XMLNode *parent_node)
: m_name(name), m_parent_node(parent_node)
{
}

void XMLNode::add_child(XMLNode *child_node)
{
    m_child_list.push_back(child_node);
}
