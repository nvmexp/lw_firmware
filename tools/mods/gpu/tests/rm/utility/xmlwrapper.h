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
//! \file xmlwrapper.h
//! \brief Wrapper for the expat XML parser
//!
//! Header for the expat XML parser wrapper
//!

#ifndef __XMLWRAPPER_H__
#define __XMLWRAPPER_H__

class XmlWrapper;
class XmlNode;

struct XmlParseData
{
    XmlWrapper *wrapper;
    int depth;
    XmlNode *root_node;
    XmlNode *parent_node;
    int rc;
};

class XmlWrapper
{
public:
    XmlWrapper();
    ~XmlWrapper();

    RC ParseXmlFileToTree(const char *filename, XmlNode **root_node);

private:
    void DeleteTree(XmlNode *node);

    XmlNode *m_root_node;
};

#endif
