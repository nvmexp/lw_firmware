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
//! \file xml_wrapper.cpp
//! \brief Wrapper for the expat XML parser
//!
//! Body for the expat XML parser wrapper
//!

#include "core/include/utility.h"
#include "mdiag/sysspec.h"
#include "core/include/massert.h"

#define XMLCALL

#include "expat.h"        // XML parser
#include "xml_node.h"
#include "xml_wrapper.h"

void XMLCALL ElementStartHandler(void *user_data, const char *name,
    const char **attributes)
{
    XMLParseData *data = (XMLParseData *) user_data;

    if (data->rc == OK)
    {
        XMLNode *node = new XMLNode(name, data->parent_node);

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

        XMLAttributeMap *map = node->AttributeMap();
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

void XMLCALL ElementEndHandler(void *user_data, const char *name)
{
    XMLParseData *data = (XMLParseData *) user_data;

    if (data->rc == OK)
    {
        MASSERT(data->parent_node);

        data->parent_node = data->parent_node->Parent();

        --data->depth;
    }
}

void XMLCALL CharacterDataHandler(void *user_data,
    const XML_Char *ch, int length)
{
    XMLParseData *data = (XMLParseData *) user_data;

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

XMLWrapper::XMLWrapper()
: m_root_node(0)
{
}

XMLWrapper::~XMLWrapper()
{
    if (m_root_node)
    {
        DeleteTree(m_root_node);
        m_root_node = 0;
    }
}

void XMLWrapper::DeleteTree(XMLNode *node)
{
    XMLNodeList *child_list = node->Children();
    XMLNodeList::iterator iter;

    for (iter = child_list->begin(); iter != child_list->end(); ++iter)
    {
        DeleteTree(*iter);
    }

    delete node;
}

RC XMLWrapper::ParseXMLFileToTree(const char *filename,
    XMLNode **root_node, bool ignore_character_data)
{
    RC rc;
    FILE *fp;
    char buffer[BUFSIZ];
    bool done;
    XMLParseData data;

    DebugPrintf("BEGIN XMLWrapper::parse_xml_file_to_tree()\n");

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

    XML_SetElementHandler(parser, ElementStartHandler,
        ElementEndHandler);

    if (!ignore_character_data)
    {
        XML_SetCharacterDataHandler(parser, CharacterDataHandler);
    }

    rc = Utility::OpenFile(filename, &fp, "rb");

    if (rc == OK)
    {
        do
        {
            size_t length = fread(buffer, 1, sizeof(buffer), fp);
            done = length < sizeof(buffer);

            DebugPrintf("BEGIN XML_Parse()\n");

            if (XML_Parse(parser, buffer, length, done) == XML_STATUS_ERROR)
            {
                DebugPrintf("%s at line %u of %s\n",
                    XML_ErrorString(XML_GetErrorCode(parser)),
                    XML_GetLwrrentLineNumber(parser),
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
