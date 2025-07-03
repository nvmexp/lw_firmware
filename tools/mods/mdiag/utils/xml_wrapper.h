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
//! \file xml_wrapper.h
//! \brief Wrapper for the expat XML parser
//!
//! Header for the expat XML parser wrapper
//!

#ifndef __XML_WRAPPER_H__
#define __XML_WRAPPER_H__

class XMLWrapper;
class XMLNode;

struct XMLParseData
{
    XMLWrapper *wrapper;
    int depth;
    XMLNode *root_node;
    XMLNode *parent_node;
    RC rc;
};

class XMLWrapper
{
public:
    XMLWrapper();
    ~XMLWrapper();

    RC ParseXMLFileToTree(const char *filename, XMLNode **root_node,
        bool ignore_character_data);

private:
    void DeleteTree(XMLNode *node);

    XMLNode *m_root_node;
};

#endif
