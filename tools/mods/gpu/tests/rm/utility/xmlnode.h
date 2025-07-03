/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2012 by LWPU Corporation.  All rights reserved.  All
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

#ifndef __XMLNODE_H__
#define __XMLNODE_H__

#include "core/include/types.h"
#include <string>
#include <vector>
#include <map>

class XmlNode;

typedef vector<XmlNode *> XmlNodeList;
typedef map<string, string> XmlAttributeMap;

class XmlNode
{
public:
    XmlNode(const char *name, XmlNode *parent_node);

    void add_child(XmlNode *child_node);

    string &Name() { return m_name; }
    XmlNode *Parent() { return m_parent_node; }
    string &Body() { return m_body; }
    XmlNodeList *Children() { return &m_child_list; }
    XmlAttributeMap *AttributeMap() { return &m_attribute_map; }

    void add_char_to_body(char ch) { m_body += ch; }

private:
    string m_name;
    XmlNode *m_parent_node;
    XmlNodeList m_child_list;
    XmlAttributeMap m_attribute_map;
    string m_body;
};

#endif
