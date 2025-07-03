/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file xml_node.h
//! \brief A node of an XML element tree
//!
//! Header for an XML element node class
//!

#ifndef __XML_NODE_H__
#define __XML_NODE_H__

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif

#include <string>
#include <vector>
#include <map>

class XMLNode;

typedef vector<XMLNode *> XMLNodeList;
typedef map<string, string> XMLAttributeMap;

class XMLNode
{
public:
    XMLNode(const char *name, XMLNode *parent_node);

    void add_child(XMLNode *child_node);

    string *Name() { return &m_name; }
    XMLNode *Parent() { return m_parent_node; }
    string *Body() { return &m_body; }
    XMLNodeList *Children() { return &m_child_list; }
    XMLAttributeMap *AttributeMap() { return &m_attribute_map; }

    void add_char_to_body(char ch) { m_body += ch; }

private:
    string m_name;
    XMLNode *m_parent_node;
    XMLNodeList m_child_list;
    XMLAttributeMap m_attribute_map;
    string m_body;
};

#endif
