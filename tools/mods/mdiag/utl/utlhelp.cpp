/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "utlhelp.h"
#include "utlutil.h"

// This function prints the documentation strings of all attributes that are
// exported from the UTL module to Python.  The strings are printed to the
// MODS log file.
//
void UtlHelp::Print()
{
    py::module utlModule = py::module::import("utl");
    py::dict utlDict = utlModule.attr("__dict__");

    // Loop through all items in the __dict__ attribute of the utl module.
    // Collect the items that represent enum types, UTL classes, and
    // module-level functions,
    for (const auto& attr : utlDict)
    {
        string key = attr.first.cast<string>();
        py::handle handle = attr.second;

        if (AttrIsValid(key))
        {
            UtlType type(key, handle, "utl." + key);

            if (type.IsEnum())
            {
                m_EnumTypes.push(type);
            }
            else if (type.IsClass())
            {
                m_ClassTypes.push(type);
            }
            else if (type.IsModuleFunction())
            {
                m_ModuleFunctions.push(type);
            }
            else
            {
                MASSERT(!"Unexpected UTL type");
            }
        }
    }

    // Print the documentation for each group separately.  Queues
    // are used because the processing of type might generate more types
    // to print.

    while (!m_ModuleFunctions.empty())
    {
        PrintModuleFunction(m_ModuleFunctions.front());
        m_ModuleFunctions.pop();
    }

    while (!m_ClassTypes.empty())
    {
        PrintClass(m_ClassTypes.front());
        m_ClassTypes.pop();
    }

    while (!m_EnumTypes.empty())
    {
        PrintEnum(m_EnumTypes.front());
        m_EnumTypes.pop();
    }
}

// Based on the name of the attribute, determine if it is valid to print
// the documentation of this attribute.
//
bool UtlHelp::AttrIsValid(const string& name)
{
    if (name.empty())
    {
        return false;
    }

    // Ignore any attributes that have the prefix "__" as they are builtin
    // attributes that don't require documentation.
    else if (name.find("__") == 0)
    {
        return false;
    }

    // Ignore the LwSwitch Array objects.  They should be transparent
    // to the user.
    else if (name.find("Array__") != string::npos)
    {
        return false;
    }

    return true;
}

// Prints a bar that's used to visually separate each item of documentation.
//
void UtlHelp::PrintHeader()
{
    string header(120, '=');

    Printf(Tee::PriNormal, "%s\n", header.c_str());
    Printf(Tee::PriNormal, "\n");
}

// Prints a smaller bar that's used to visually separate different sections
// of documentation for an item.
//
void UtlHelp::PrintSubheader(int indentLevel)
{
    string leadingSpace(indentLevel * 4, ' ');
    string subheader(120 - indentLevel * 4, '-');

    Printf(Tee::PriNormal, "%s%s\n", leadingSpace.c_str(), subheader.c_str());
    Printf(Tee::PriNormal, "\n");
}

// This function prints the __doc__ string of a Python object.  The __doc__
// strings of UTL exported items comes from the definitions in utlmodule.h.
//
void UtlHelp::PrintDocString
(
    py::handle handle,
    int indentLevel,
    bool isFunction /* false */
)
{
    int count = 0;
    string indentSpace(4 * indentLevel, ' ');
    if (py::hasattr(handle, "__doc__"))
    {
        py::object docObj = handle.attr("__doc__");

        if (!docObj.is_none())
        {
            string docString = docObj.cast<string>();

            if (docString.length() > 0)
            {
                size_t start = 0;
                size_t end = docString.find("\n");

                while (end != string::npos)
                {
                    if ((start == 0) && isFunction)
                    {
                        Printf(Tee::PriNormal, "%ssignature: %s\n",
                            indentSpace.c_str(),
                            docString.substr(start, end - start).c_str());
                    }
                    else
                    {
                        Printf(Tee::PriNormal, "%s%s\n",
                            indentSpace.c_str(),
                            docString.substr(start, end - start).c_str());
                    }
                    ++count;
                    start = end + 1;
                    end = docString.find("\n", start);
                }

                if (start < docString.length())
                {
                    Printf(Tee::PriNormal, "%s%s\n",
                        indentSpace.c_str(),
                        docString.substr(start, end).c_str());
                    ++count;
                }

                Printf(Tee::PriNormal, "\n");
            }
        }
    }

    // If no documentation is found, print this as a default.
    // Eventually this condition should be an error.
    if (count == 0)
    {
        Printf(Tee::PriNormal, "%s(No documentation found.)\n", indentSpace.c_str());
        Printf(Tee::PriNormal, "\n");
    }
}

// Print the documentation for a UTL module-level function.
//
void UtlHelp::PrintModuleFunction(const UtlType& utlFunction)
{
    PrintHeader();
    Printf(Tee::PriNormal, "function %s\n", utlFunction.m_FullName.c_str());
    Printf(Tee::PriNormal, "\n");
    PrintDocString(utlFunction.m_Handle, 1, true);
    MASSERT(!py::hasattr(utlFunction.m_Handle, "__dict__"));
}

// Print the documentation for a UTL class.
//
void UtlHelp::PrintClass(const UtlType& utlClass)
{
    PrintHeader();
    Printf(Tee::PriNormal, "class %s\n", utlClass.m_FullName.c_str());
    Printf(Tee::PriNormal, "\n");
    PrintDocString(utlClass.m_Handle, 1);

    MASSERT(py::hasattr(utlClass.m_Handle, "__dict__"));

    vector<UtlType> constants;
    vector<UtlType> dataMembers;
    vector<UtlType> instanceMethods;
    vector<UtlType> staticMethods;

    py::dict classDict = utlClass.m_Handle.attr("__dict__");

    for (const auto& attr : classDict)
    {
        string key = attr.first.cast<string>();
        py::handle handle = attr.second;

        if (AttrIsValid(key))
        {
            UtlType type(key, handle, utlClass.m_FullName + "." + key);

            // Classes have many different types of attributes.  Some of these
            // attributes (example, instance functions) will be printed in
            // this function and others (example, sub-classes) will be added
            // to a queue to be printed later.

            if (type.IsEnum())
            {
                m_EnumTypes.push(type);
            }
            else
            {
                MASSERT(py::hasattr(handle, "__class__"));
                py::object classHandle = handle.attr("__class__");
                MASSERT(py::hasattr(classHandle, "__name__"));
                py::object nameHandle = classHandle.attr("__name__");
                string typeName = nameHandle.cast<string>();

                if (typeName == "pybind11_static_property")
                {
                    constants.push_back(type);
                }
                else if (typeName == "property")
                {
                    dataMembers.push_back(type);
                }
                else if (typeName == "instancemethod")
                {
                    instanceMethods.push_back(type);
                }
                else if (typeName == "builtin_function_or_method")
                {
                    staticMethods.push_back(type);
                }
                else if (typeName == "pybind11_type")
                {
                    m_ClassTypes.push(type);
                }
                else
                {
                    UtlUtility::PyError("unrecognized class type %s.\n",
                        typeName.c_str());
                }
            }
        }
    }

    PrintClassAttributes("Instance Methods", instanceMethods, true);
    PrintClassAttributes("Static Methods", staticMethods, true);
    PrintClassAttributes("Data Members", dataMembers);
    PrintClassConstants("Constants", constants);
}

// This function prints the documentation of a group of class attributes
// that are related.
//
void UtlHelp::PrintClassAttributes
(
    const string& headerText,
    const vector<UtlType>& attrList,
    bool isFunction /* false */
)
{
    if (attrList.size() == 0)
    {
        return;
    }

    PrintSubheader(1);
    Printf(Tee::PriNormal, "    %s\n", headerText.c_str());
    Printf(Tee::PriNormal, "\n");

    for (const UtlType& type : attrList)
    {
        Printf(Tee::PriNormal, "        %s\n", type.m_Name.c_str());
        PrintDocString(type.m_Handle, 3, isFunction);
    }
}

// This function prints the documentation of a group of constants that are
// defined in a class.
//
void UtlHelp::PrintClassConstants
(
    const string& headerText,
    const vector<UtlType>& attrList
)
{
    if (attrList.size() == 0)
    {
        return;
    }

    PrintSubheader(1);
    Printf(Tee::PriNormal, "    %s\n", headerText.c_str());
    Printf(Tee::PriNormal, "\n");

    for (const UtlType& type : attrList)
    {
        Printf(Tee::PriNormal, "        %s\n", type.m_Name.c_str());
    }

    Printf(Tee::PriNormal, "\n");
}

// This function prints the documentation for an enumerated type.
//
void UtlHelp::PrintEnum(const UtlType& utlEnum)
{
    PrintHeader();
    Printf(Tee::PriNormal, "enum %s\n", utlEnum.m_FullName.c_str());
    Printf(Tee::PriNormal, "\n");
    PrintDocString(utlEnum.m_Handle, 1);
    PrintSubheader(1);
    Printf(Tee::PriNormal, "    Possible Values:\n");
    Printf(Tee::PriNormal, "\n");

    py::dict enumDict = utlEnum.m_Handle.attr("__dict__");

    for (const auto& attr : enumDict)
    {
        string key = attr.first.cast<string>();

        if (AttrIsValid(key))
        {
            Printf(Tee::PriNormal, "        %s.%s\n",
                utlEnum.m_FullName.c_str(), key.c_str());
        }
    }

    Printf(Tee::PriNormal, "\n");
}


// The constructor for a UTL type information object.
//
UtlHelp::UtlType::UtlType
(
    const string& name,
    py::handle handle,
    const string& fullName
)
:   m_Name(name),
    m_Handle(handle),
    m_FullName(fullName)
{
}

// This function determines if this object represents an enumerated type.
//
bool UtlHelp::UtlType::IsEnum()
{
    if (!py::hasattr(m_Handle, "__dict__"))
    {
        return false;
    }

    int count = 0;
    py::dict typeDict = m_Handle.attr("__dict__");

    // If this object is an enumerated type, each attribute of this object
    // will be an instantiation of the object's type.
    for (const auto& attr : typeDict)
    {
        string key = attr.first.cast<string>();
        py::handle handle = attr.second;

        if (UtlHelp::AttrIsValid(key))
        {
            ++count;

            py::object handleClass = handle.attr("__class__");
            //py::object handleClassClass = handleClass.attr("__class__");
            //py::object typeHandleClass = m_Handle.attr("__class__");;

            if (handleClass != m_Handle)
            {
                return false;
            }
        }
    }

    return (count > 0);
}

// This function determines if this object represents a class type.
// Note: the function IsEnum should always be called before IsClass because
// an enumerated type will also return true for IsClass.
//
bool UtlHelp::UtlType::IsClass()
{
    py::object typeHandleClass = m_Handle.attr("__class__");
    string className = typeHandleClass.attr("__name__").cast<string>();

    return (className == "pybind11_type");
}

// This function determines if this object represents a module-level
// function type.
//
bool UtlHelp::UtlType::IsModuleFunction()
{
    py::object typeHandleClass = m_Handle.attr("__class__");
    string className = typeHandleClass.attr("__name__").cast<string>();

    return (className == "builtin_function_or_method");
}
