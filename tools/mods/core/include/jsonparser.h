/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \file Utility functions to help parse JSON files

#pragma once

#include "core/include/rc.h"
#include "document.h"

using JsonTree = rapidjson::GenericValue<rapidjson::UTF8<char>,
                                         rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator> >;
namespace JsonParser
{
    //!
    //! \brief Form rapidJSON document by parsing the input JSON file
    //!
    //! \param      fileName  JSON file name
    //! \param[out] doc       Output rapidJson document
    //!
    RC ParseJson(const string& fileName, rapidjson::Document* doc);

    //!
    //! \brief Retreive corresponding value from JSON tree assuming that it exists
    //!
    RC RetrieveJsolwal(const JsonTree& tree, const char* valName, UINT32& retVal);
    RC RetrieveJsolwal(const JsonTree& tree, const char* valName, INT32& retVal);
    RC RetrieveJsolwal(const JsonTree& tree, const char* valName, FLOAT64& retVal);
    RC RetrieveJsolwal(const JsonTree& tree, const char* valName, string& retVal);

    //!
    //! \brief Find the value of a given key in the given JSON tree
    //!        Returns an error if the key is not found
    //!
    //! \param       tree     JSON tree
    //! \param       valName  Name of value to retrive
    //! \param[out]  retVal   Value corresponding to given name
    //!
    template <typename T>
    RC FindJsolwal(const JsonTree& tree, const string& valName, T& retVal)
    {
        RC rc;

        if (!tree.HasMember(valName.c_str()))
        {
            Printf(Tee::PriError, "Could not find \"%s\" in JSON file\n", valName.c_str());
            return RC::CANNOT_PARSE_FILE;
        }

        // Empty string or "-" means N/A for that field. No need to retrieve value, directly return
        if (tree[valName.c_str()].IsString())
        {
            string str = tree[valName.c_str()].GetString();
            if (str == "" || str == "-")
            {
                return rc;
            }
        }

        CHECK_RC(RetrieveJsolwal(tree, valName.c_str(), retVal));
        return rc;
    }

    //!
    //! \brief Find the value of a given key in the given JSON tree
    //!        Returns an error if the key is not found
    //!
    //! \param       tree     JSON tree
    //! \param       valName  Name of value to retrive
    //! \param[out]  retVal   Value corresponding to given name
    //!
    RC FindJsonUintFromString
    (
        const JsonTree& tree,
        const string& valName,
        UINT32* pRetVal
    );

    //!
    //! \brief Find the value of a given key in the given JSON tree
    //!        Does not return an error if the key is not found
    //!
    //! \param       tree     JSON tree
    //! \param       valName  Name of value to retrive
    //! \param[out]  pRetVal  Value corresponding to given name
    //!
    template <typename T>
    RC TryFindJsolwal(const JsonTree& tree, const string& valName, T* pRetVal)
    {
        RC rc;

        if (!tree.HasMember(valName.c_str()))
        {
            *pRetVal = T();
        }
        else
        {
            // Empty string or "-" means N/A for that field. No need to retrieve value,
            // directly return
            if (tree[valName.c_str()].IsString())
            {
                string str = tree[valName.c_str()].GetString();
                if (str == "" || str == "-")
                {
                    return rc;
                }
            }

            CHECK_RC(RetrieveJsolwal(tree, valName.c_str(), *pRetVal));
        }
        return rc;
    }

    //!
    //! \brief Find the value of a given key in the given JSON tree
    //!        Does not return an error if the key is not found
    //!
    //! \param       tree     JSON tree
    //! \param       valName  Name of value to retrive
    //! \param[out]  retVal   Value corresponding to given name
    //!
    RC TryFindJsonUintFromString
    (
        const JsonTree& tree,
        const string& valName,
        UINT32* pRetVal
    );
    
    //!
    //! \brief Find the value of a given key in the given JSON tree
    //!        Does not return an error if the key is not found
    //!
    //! \param       tree     JSON tree
    //! \param       valName  Name of value to retrive
    //! \param       defVal   default value to set if key is not found
    //! \param[out]  retVal   Value corresponding to given name
    //!
    RC TryFindJsonUintFromString
    (
        const JsonTree& tree,
        const string& valName,
        UINT32 defVal,
        UINT32* pRetVal
    );

    //!
    //! \brief Check if a given section exists in the JSON tree
    //!
    //! \param  tree          JSON tree
    //! \param  sectionName   Name of section to find
    //!
    RC CheckHasSection(const JsonTree& tree, const char* sectionName);
    //!
    //! \brief Check if a given section exists in the rapidJSON document
    //!
    //! \param  tree          rapidJSON document
    //! \param  sectionName   Name of section to find
    //!
    RC CheckHasSection(const rapidjson::Document& tree, const char* sectionName);
};
