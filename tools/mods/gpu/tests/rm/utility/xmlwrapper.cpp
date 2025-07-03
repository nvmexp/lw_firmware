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
//! \file xml_wrapper.cpp
//! \brief Wrapper for the expat XML parser
//!
//! Body for the expat XML parser wrapper
//!

#include "core/include/massert.h"
#include "core/include/utility.h"

#define XMLCALL

#include "expat.h"
#include "xmlnode.h"
#include "xmlwrapper.h"
#include "core/include/memcheck.h"

void XMLCALL ElementStartHandle(void *user_data, const char *name,
    const char **attributes)
{
    XmlParseData *data = (XmlParseData *) user_data;

    if (data->rc == OK)
    {
        XmlNode *node = new XmlNode(name, data->parent_node);

        if (data->parent_node)
        {
            data->parent_node->add_child(node);
        }

        // If this node has no parent, it must be the root node.
        else
        {
            MASSERT(!data->root_node);
            data->root_node = node;
        }

        XmlAttributeMap *map = node->AttributeMap();
        int i;

        for (i = 0; attributes[i]; i += 2)
        {
            MASSERT(map->find(attributes[i]) == map->end());

            (*map)[attributes[i]] = attributes[i + 1];
        }

        data->parent_node = node;

        ++data->depth;
    }
}

void XMLCALL ElementEndHandle(void *user_data, const char *name)
{
    XmlParseData *data = (XmlParseData *) user_data;

    if (data->rc == OK)
    {
        MASSERT(data->parent_node);

        data->parent_node = data->parent_node->Parent();

        --data->depth;
    }
}

void XMLCALL CharacterDataHandle(void *user_data,
    const XML_Char *ch, int length)
{
    XmlParseData *data = (XmlParseData *) user_data;

    if (data->rc == OK)
    {
        MASSERT(data->parent_node);

        int i;

        for (i = 0; i < length; ++i)
        {
            data->parent_node->add_char_to_body(ch[i]);
        }
    }
}

XmlWrapper::XmlWrapper()
: m_root_node(0)
{
}

XmlWrapper::~XmlWrapper()
{
    if (m_root_node)
    {
        DeleteTree(m_root_node);
        m_root_node = 0;
    }
}

//! Delete Node of xml tree
//-----------------------------------------------------------------------------
void XmlWrapper::DeleteTree(XmlNode *node)
{
    XmlNodeList *child_list = node->Children();
    XmlNodeList::iterator iter;

    for (iter = child_list->begin(); iter != child_list->end(); ++iter)
    {
        DeleteTree(*iter);
    }

    delete node;
}

//! Transform xml file into xml tree
//-----------------------------------------------------------------------------
RC XmlWrapper::ParseXmlFileToTree(const char *filename, XmlNode **root_node)
{
    RC rc;
    FILE *fp;
    char buffer[BUFSIZ];
    bool done;
    XmlParseData data;

    Printf(Tee::PriHigh, "BEGIN XmlWrapper::parse_xml_file_to_tree()\n");

    XML_Parser parser = XML_ParserCreate(NULL);

    MASSERT(parser);

    if (m_root_node)
    {
        DeleteTree(m_root_node);
        m_root_node = 0;
    }

    data.wrapper = this;
    data.depth = 0;
    data.root_node = 0;
    data.parent_node = 0;
    data.rc = OK;

    XML_SetUserData(parser, &data);

    XML_SetElementHandler(parser, ElementStartHandle,
        ElementEndHandle);

    XML_SetCharacterDataHandler(parser, CharacterDataHandle);

    rc = Utility::OpenFile(filename, &fp, "r");

    if (rc == OK)
    {
        do
        {
            size_t length = fread(buffer, 1, sizeof(buffer), fp);
            done = length < sizeof(buffer);

            Printf(Tee::PriHigh, "BEGIN XML_Parse()\n");

            if (XML_Parse(parser, buffer, (UINT32)length, done) == XML_STATUS_ERROR)
            {
                Printf(Tee::PriHigh, "%s at line %d of %s\n",
                    XML_ErrorString(XML_GetErrorCode(parser)),
                    (UINT32)XML_GetLwrrentLineNumber(parser),
                    filename);

                rc = RC::SOFTWARE_ERROR;
            }
            else if (data.rc != OK)
            {
                rc = data.rc;
            }

        } while (!done && (rc == OK));

        fclose(fp);

        if (rc == OK)
        {
            MASSERT(data.root_node);
            m_root_node = data.root_node;
            *root_node = data.root_node;
        }
    }

    XML_ParserFree(parser);

    return rc;
}
