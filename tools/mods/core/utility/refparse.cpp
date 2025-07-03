/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Parser for chip reference manuals (e.g. lw_ref.h, mcp_ref.h)

#include "core/include/refparse.h"
#include "core/include/utility.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/zlib.h"
#include "core/include/cmdline.h"
#include "core/include/xp.h"
#include <string.h>
#include <ctype.h>
#include <fstream>
#include <sstream>
#include <boost/filesystem.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/algorithm/string.hpp>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#endif

UINT32 RefManualRegisterField::GetMask() const
{
    // Be careful not to shift left by 32
    int NumBitsMinusOne = m_HighBit - m_LowBit;
    return ((2 << NumBitsMinusOne) - 1) << m_LowBit;
}

UINT32 RefManualRegisterField::GetConstantValue() const
{
    for (unsigned int i = 0; m_IsConstant && i < m_Values.size(); i++)
    {
        if (m_Values[i].IsConstant)
        {
            // just return the first declared constant
            return m_Values[i].Value << m_LowBit; // align it to lsb
        }
    }
    return 0;
}

int RefManualRegisterField::GetNumValues() const
{
    return (int)m_Values.size();
}

const RefManualFieldValue *RefManualRegisterField::GetValue(int Index) const
{
    return &m_Values[Index];
}

const RefManualFieldValue *RefManualRegisterField::GetInitValue() const
{
    // XXX
    return NULL;
}

const RefManualFieldValue *RefManualRegisterField::FindValue(const char *Name) const
{
    for (unsigned int i = 0; i < m_Values.size(); i++)
    {
        if (m_Values[i].Name == Name)
        {
            return &m_Values[i];
        }
    }
    return NULL;
}

void RefManualRegisterField::InitAttrFromAccess()
{
    m_IsReadable = (m_Access[0] == 'R') || (m_Access[0] == 'C');
    m_IsWriteable = (m_Access[1] == 'W');
    m_IsConstant = (m_Access[0] == 'C');
    m_IsUnWriteable = (m_Access[2] == 'U') || (m_Access[0] == 'C');
    m_IsUnknown = (m_Access[2] == 'X');
}

UINT32 RefManualRegister::GetOffset(int i) const
{
    MASSERT((i >= 0) && (i < m_ArraySizeI));
    return m_Offset + i*m_StrideI;
}

UINT32 RefManualRegister::GetOffset(int i, int j) const
{
    MASSERT((i >= 0) && (i < m_ArraySizeI));
    MASSERT((j >= 0) && (j < m_ArraySizeJ));
    return m_Offset + i*m_StrideI + j*m_StrideJ;
}

int RefManualRegister::GetDimensionality() const
{
    return (m_StrideJ > 0) ? 2 :
           (m_StrideI > 0) ? 1 : 0;
}

UINT32 RefManualRegister::GetFieldMask() const
{
    UINT32 Mask = 0;
    for (const auto& field : m_Fields)
    {
        if (!field.IsAlias())
        {
            Mask |= field.GetMask();
        }
    }
    return Mask;
}

UINT32 RefManualRegister::GetReadableFieldMask() const
{
    UINT32 Mask = 0;
    for (const auto& field : m_Fields)
    {
        if (!field.IsAlias() && field.IsReadable())
        {
            Mask |= field.GetMask();
        }
    }
    return Mask;
}

UINT32 RefManualRegister::GetWriteableFieldMask() const
{
    UINT32 Mask = 0;
    for (const auto& field : m_Fields)
    {
        if (!field.IsAlias() && field.IsWriteable())
        {
            Mask |= field.GetMask();
        }
    }
    return Mask;
}

UINT32 RefManualRegister::GetUnWriteableFieldMask() const
{
    UINT32 Mask = 0;
    for (const auto& field : m_Fields)
    {
        if (!field.IsAlias() && field.IsUnWriteable())
        {
            Mask |= field.GetMask();
        }
    }
    return Mask;
}

UINT32 RefManualRegister::GetConstantFieldMask() const
{
    UINT32 Mask = 0;
    for (const auto& field : m_Fields)
    {
        if (!field.IsAlias() && field.IsConstant())
        {
            Mask |= field.GetMask();
        }
    }
    return Mask;
}

UINT32 RefManualRegister::GetConstantFieldValue() const
{
    UINT32 Value = 0;
    for (const auto& field : m_Fields)
    {
        if (!field.IsAlias())
        {
            Value |= field.GetConstantValue();
        }
    }
    return Value;
}

UINT32 RefManualRegister::GetTaskFieldMask() const
{
    UINT32 Mask = 0;
    for (const auto& field : m_Fields)
    {
        if (!field.IsAlias() && field.IsTask())
        {
            Mask |= field.GetMask();
        }
    }
    return Mask;
}

UINT32 RefManualRegister::GetClearOnOneFieldMask() const
{
    UINT32 Mask = 0;
    for (const auto& field : m_Fields)
    {
        if (!field.IsAlias() && field.IsClearOnOne())
        {
            Mask |= field.GetMask();
        }
    }
    return Mask;
}

int RefManualRegister::GetNumFields() const
{
    return (int)m_Fields.size();
}

const RefManualRegisterField *RefManualRegister::GetField(int Index) const
{
    if((Index >= 0) && (Index < (int) m_Fields.size()))
        return &m_Fields[Index];
    else
        return 0;
}

const RefManualRegisterField *RefManualRegister::FindField(const char *Name) const
{
   for (unsigned int i = 0; i < m_Fields.size(); i++)
   {
      if (m_Fields[i].GetName() == Name)
      {
         return &m_Fields[i];
      }
   }
   return NULL;
}

void RefManualRegisterGroup::AddRegister(RefManualRegister *Reg)
{
    m_Registers.push_back(Reg);
}

int RefManualRegisterGroup::GetNumRegisters() const
{
    return (int)m_Registers.size();
}

const RefManualRegister *RefManualRegisterGroup::GetRegister(int Index) const
{
    return m_Registers[Index];
}

const RefManualRegister *RefManualRegisterGroup::FindRegister(const char *Name) const
{
    for (unsigned int i = 0; i < m_Registers.size(); i++)
    {
        if (m_Registers[i]->GetName() == Name)
        {
            return m_Registers[i];
        }
    }
    return NULL;
}

void RefManual::ParseAliasFieldValue
(
    const char* Name,
    const char* Definition,
    const char* Access
)
{
    RefManualFieldValue Value;

    Value.Name = Name;
    Value.IsConstant = false;
    Value.initType = InitNone;
    Value.IsTask = false;

    if ((1 != sscanf(Definition, "0x%x", &Value.Value )) && (1 != sscanf(Definition, "%d", &Value.Value)))
    {
        return;
    }

    for (int i = (int)m_Registers.size()-1; i >= 0; i--)
    {
        RefManualRegister *pReg = &m_Registers[i];
        if (!strncmp(pReg->m_Name.c_str(), Name, pReg->m_Name.length()))
        {
            for (int j = (int)pReg->m_Fields.size()-1; j >= 0; j--)
            {
                RefManualRegisterField *pField = &pReg->m_Fields[j];
                if (!strncmp(pField->m_Name.c_str(), Name, pField->m_Name.length()))
                {
                    pField->m_Values.push_back(Value);
                    return;
                }
            }
        }
    }
}

void RefManual::ParseAliasField
(
    const char* Name,
    const char* Definition,
    const char* Access
)
{
    int lowBit = 0;
    int highBit = 0;

    if (2 != sscanf(Definition, "%d:%d", &highBit, &lowBit))
    {
        UINT32 val = 0;
        if ((1 == sscanf(Definition, "0x%x", &val)) || (1 == sscanf(Definition, "%d", &val)))
        {
            ParseAliasFieldValue(Name, Definition, Access);
        }
        return;
    }

    for (size_t i = m_Registers.size(); i > 0; i--)
    {
        RefManualRegister *pReg = &m_Registers[i - 1];
        if (!strncmp(pReg->m_Name.c_str(), Name, pReg->m_Name.length()))
        {
            // By default give alias field "R+W" hints.
            pReg->m_Fields.emplace_back(Name, Access, true, lowBit, highBit);
            return;
        }
    }
}

RC RefManual::Serialize(const char* FileName) const
{
    RC rc;
    try
    {
        // Write dummy file (.<hostname>.<pid>.ref.ser) in MODS runspace,
        // then rename/move to formal name to avoid race issue
        char hostname[256];
        gethostname(hostname, sizeof(hostname));
        stringstream ss;
        string manualPath = LwDiagUtils::StripFilename(FileName);
        ss << manualPath.c_str() << "/." << hostname << "." << Xp::GetPid() << ".ref.ser";
        string serialName = ss.str();

        std::ofstream ofs;
        ofs.open(serialName.c_str(), std::ofstream::out | std::ofstream::binary);
        boost::archive::binary_oarchive oarch(ofs);
        oarch << *this;
        ofs.close();
        if (rename(serialName.c_str(), FileName) != 0)
        {
            Printf(Tee::PriWarn, "Failed to rename %s into %s, errno is %d\n", 
                serialName.c_str(), FileName, errno);
        }
    }
    catch (const std::exception& e)
    {
        Printf(Tee::PriNormal, "Error writing serialized manuals file: %s.\n", e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

RC RefManual::Deserialize(const char* FileName)
{
    RC rc;
    try
    {
        std::ifstream ifs;
        ifs.open(FileName, std::ifstream::in | std::ifstream::binary);
        if (ifs.fail())
        {
            rc = RC::SOFTWARE_ERROR;
        }
        boost::archive::binary_iarchive iarch(ifs);
        iarch >> *this;
        ifs.close();
    }
    catch (const std::exception& e)
    {
        Printf(Tee::PriNormal, "Error reading serialized manuals file: %s.\n", e.what());
        rc = RC::SOFTWARE_ERROR;
    }

    return rc;
}

// XXX HA/HB stuff
RC RefManual::Parse
(
    const char *FileName,
    bool UseSerial,             // UseSerial defaults to false
    const char *UnitName,       // UnitName defaults to null
    const char *Start,          // Start defaults to null
    const char *End             // End defaults to null
)
{
    FILE *f;
    RC rc;
    char buf[4096], *p, *pEnd;
    char Name[4096], Definition[4096], Access[4096];
    bool seenStart = false;
    bool regIsArray = false;
    bool zipFile = false; // indicates if we are parsing a zip file
    int  lwrrentLine = 0;
    bool fieldIsArray = false;
    UINT32 fieldArrayOffsetLow;
    UINT32 fieldArrayOffsetHigh;
    UINT32 fieldArrayScaleLow;
    UINT32 fieldArrayScaleHigh;
    UINT32 fieldArrayStrideLow;
    UINT32 fieldArrayStrideHigh;
    int fieldArrayLowBit;
    int fieldArrayHighBit;

    m_bParseTried = true;

    // Try to open compressed manual
    const string zFileName = Utility::DefaultFindFile(string(FileName)+string(".gz"), true);
    gzFile lgzFile = gzopen(zFileName.c_str(), "r");
    if (lgzFile)
    {
        zipFile = true;
    }
    else
    {
       // Compressed manual not found, try uncompressed
        zipFile = false;
        CHECK_RC(Utility::OpenFile(FileName, &f, "rt"));
    }
    const string actualFileName = zipFile ? zFileName : FileName;

    // Try to use serialized manuals first
    // Only use serialized manuals on emulation since it provides negligible improvements
    // on other platforms. Serialized files will always be in MODS runspace.
    const string serialFileName = CommandLine::ProgramPath() + string(FileName) + ".ser";
    // Check if file exists
    FILE* serialFile = fopen(serialFileName.c_str(), "r");
    // Parse normally if manuals file is more recent than serialized file
    if (serialFile && UseSerial && boost::filesystem::last_write_time(actualFileName) <
        boost::filesystem::last_write_time(serialFileName))
    {
        fclose(serialFile);
        Printf(Tee::PriNormal, "Serialized ref manuals found. Loading %s.\n", serialFileName.c_str());
        RC rc = Deserialize(serialFileName.c_str());
        if (rc == OK)
        {
            return OK;
        }
        else
        {
            Printf(Tee::PriNormal, "Failed to open serialized manuals. Falling back to parsing method.\n");
        }
    }

    // Can't use serialized file, parse normally
    if (UnitName)
    {
        Printf(Tee::PriNormal, "Loading ref manual '%s' for unit '%s'... \n",
               actualFileName.c_str(), UnitName);
    }
    else
    {
        Printf(Tee::PriNormal, "Loading ref manual '%s'...\n",
               actualFileName.c_str());
    }

    if (Start)
    {
        Printf(Tee::PriNormal, "start fence post for these registers is %s\n", Start );
    }
    if (End)
    {
        Printf(Tee::PriNormal, "end fence post for these registers is %s\n", End );
    }

    while ((!zipFile && !feof(f)) || (zipFile && !gzeof(lgzFile)))
    {
        if (zipFile)
        {
            gzgets(lgzFile, buf, sizeof(buf));
        }
        else
        {
            if (0 == fgets(buf, sizeof(buf), f))
            {
                if (!feof(f))
                {
                    Printf(Tee::PriNormal, "Error reading manual\n");
                }
                break;
            }
        }
        lwrrentLine++;

        // Skip lines that precede the fence post; added for mcp
        if (Start && (seenStart == false))
        {
            if (strncmp(buf, Start, strlen(Start)))
            {
                continue;
            }
            seenStart = true;
        }
        else if (End && (seenStart == true))
        {
            if (!strncmp(buf, End, strlen(End)))
            {
                break;
            }
        }

        // Move past the leading whitespace
        p = buf;
        while (isspace(*p))
            p++;

        // Skip lines that are not preprocessor defines
        const int defLen = 7; // length of "#define"
        if (strncmp(p, "#define", defLen))
        {
            continue;
        }

        p += defLen;
        while (isspace(*p))
            p++;

        // Name is the next full chunk of text w/o whitespace
        pEnd = p;
        while (isgraph(*pEnd))
            pEnd++;
        *pEnd++ = 0;
        strcpy(Name, p);
        // skip if Name is not prefixed by UnitName
        if (UnitName && strncmp(Name, UnitName, strlen(UnitName)))
        {
            continue;
        }

        p = pEnd;
        while (isspace(*p))
            p++;

        // Followed by definition
        pEnd = p;

        // If there is no definition, the name represents something other
        // than a register (for example, an instance block).  Create a
        // dummy definition so that the fields can still be used.
        if ((p[0] == '/') && (p[1] == '*'))
        {
            // Set to max UINT32 value so that if a user attempts to
            // access this as a register, it will fail.
            strcpy(Definition, "0xffffffff");
        }
        else
        {
            while (isgraph(*pEnd))
                pEnd++;
            *pEnd++ = 0;
            strcpy(Definition, p);
            p = pEnd;
            while (isspace(*p))
                p++;
        }

        // Now we expect something along the lines of "/* ????? */"
        // Those five characters are the Access
        if ((strlen(p) < 11) ||
            (p[0] != '/') || (p[1] != '*') || (p[2] != ' ') ||
            (p[8] != ' ') || (p[9] != '*') || (p[10] != '/'))
        {
           continue;
        }
        p[8] = 0;
        strcpy(Access, &p[3]);

        if (!strcmp(Access, "----C"))
        {
            // Class
            // XXX
        }
        else if (Access[4] == 'R' || Access[4] == 'P' || Access[4] == 'G')
        {
            // 'R' = Register
            // 'P' = Pseudo Register
            // 'G' = (legacy) General purpose configuration register
            // Bug 765789:
            // To SW, 'P' registers are same as 'R' registers.
            RefManualRegister Reg;
            regIsArray=false;

            Reg.m_Name = Name;
            Reg.m_Access = Access;
            Reg.m_ArraySizeI = 1;
            Reg.m_ArraySizeJ = 1;
            Reg.m_StrideI = 0;
            Reg.m_StrideJ = 0;
            if (1 != sscanf(Definition, "%x", &Reg.m_Offset))
            {
                Printf(Tee::PriError, "%s(%d) %s : Invalid Offset %s\n",
                    FileName, lwrrentLine, Name, Definition);
                MASSERT(0);
            }
            Reg.m_IsReadable = (Access[0] == 'R');
            Reg.m_IsWriteable = (Access[1] == 'W');
            Reg.m_IsUnWriteable = (Access[2] == 'U') || (Access[0] == 'C');

            m_Registers.push_back(Reg);
        }
        else if (Access[4] == 'A')
        {
            // Array of registers
            RefManualRegister Reg;
            regIsArray=true;
            // Strip off (i) or (i,j) from name
            p = strchr(Name, '(');
            if (!p)
            {
                // You'd think this would never happen, but it does...
                continue;
            }
            *p = 0;
            Reg.m_Name = Name;

            UINT32 temp;
            bool skip_reg = false;

            // A little tricky to parse all the possibilities
            // This is most likely incomplete
            if (3 == sscanf(Definition, "(%x+(i)*%i+(j)*%i)", &Reg.m_Offset, &Reg.m_StrideI, &Reg.m_StrideJ))
            {
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 0;
            }
            else if (3 == sscanf(Definition, "(%x+(j)*%i+(i)*%i)", &Reg.m_Offset, &Reg.m_StrideJ, &Reg.m_StrideI))
            {
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 0;
            }
            else if (3 == sscanf(Definition, "(%x+((i)*%i)+((j)*%i))", &Reg.m_Offset, &Reg.m_StrideI, &Reg.m_StrideJ))
            {
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 0;
            }
            else if (2 == sscanf(Definition, "(%x+((dev)*%d))", &Reg.m_Offset, &Reg.m_StrideI))
            {
                Reg.m_StrideJ = 0;
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 1;
            }
            else if (3 == sscanf(Definition, "(%x+((dev)*%d)+((i)*%d))", &Reg.m_Offset, &Reg.m_StrideI, &Reg.m_StrideJ))
            {
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 0;
            }
            else if (3 == sscanf(Definition, "(%x+((j)*%i)+((i)*%i))", &Reg.m_Offset, &Reg.m_StrideJ, &Reg.m_StrideI))
            {
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 0;
            }
            else if (2 == sscanf(Definition, "(%x+(i)*%i)", &Reg.m_Offset, &Reg.m_StrideI))
            {
                Reg.m_StrideJ = 0;
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 1;
            }
            else if (2 == sscanf(Definition, "(%x+(j)*%i)", &Reg.m_Offset, &Reg.m_StrideI))
            {
                Reg.m_StrideJ = 0;
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 1;
            }
            else if (2 == sscanf(Definition, "(%x+((i)*%i))", &Reg.m_Offset, &Reg.m_StrideI))
            {
                Reg.m_StrideJ = 0;
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 1;
            }
            else if (2 == sscanf(Definition, "(%x+((j)*%i))", &Reg.m_Offset, &Reg.m_StrideI))
            {
                Reg.m_StrideJ = 0;
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 1;
            }
            else if (1 == sscanf(Definition, "(%x+(i))", &Reg.m_Offset))
            {
                Reg.m_StrideI = 1;
                Reg.m_StrideJ = 0;
                Reg.m_ArraySizeI = 0;
                Reg.m_ArraySizeJ = 1;
            }
            else if (4 == sscanf(Definition, "(((((i)>>%d)+%d)<<%d)+%x)", &temp, &temp, &temp, &Reg.m_Offset))
            {
                skip_reg = true;
            }
            else if (3 == sscanf(Definition, "(((i)>>%d)*%d+%x)", &temp, &temp, &Reg.m_Offset))
            {
                skip_reg = true;
            }
            else
            {
                Printf(Tee::PriError, "%s(%d) %s : Invalid array register definition %s\n",
                       FileName, lwrrentLine, Name, Definition);
                MASSERT(0);
            }

            Reg.m_IsReadable = (Access[0] == 'R');
            Reg.m_IsWriteable = (Access[1] == 'W');
            Reg.m_IsUnWriteable = (Access[2] == 'U') || (Access[0] == 'C');

            if (!skip_reg)
                m_Registers.push_back(Reg);
        }
        else if (Access[4] == 'F')
        {
            // Field
            RefManualRegisterField Field(Name, Access);
            fieldIsArray = false;

            if (6 == sscanf(Definition,
                    "((((i)+%d)*%d)+%d):((((i)+%d)*%d)+%d)",
                    &fieldArrayOffsetHigh,
                    &fieldArrayStrideHigh,
                    &fieldArrayHighBit,
                    &fieldArrayOffsetLow,
                    &fieldArrayStrideLow,
                    &fieldArrayLowBit))
            {
                MASSERT(fieldArrayOffsetLow == fieldArrayOffsetHigh);
                MASSERT(fieldArrayStrideLow == fieldArrayStrideHigh);
                fieldArrayScaleLow = 1;
                fieldArrayScaleHigh = 1;
                fieldIsArray = true;

                // Strip off (i) from name
                p = strchr(Name, '(');
                *p = 0;
                Field.m_Name = Name;
            }
            else if (8 == sscanf(Definition,
                    "(((%d+((i)*%d))*%d)+%d):(((%d+((i)*%d))*%d)+%d)",
                    &fieldArrayOffsetHigh,
                    &fieldArrayScaleHigh,
                    &fieldArrayStrideHigh,
                    &fieldArrayHighBit,
                    &fieldArrayOffsetLow,
                    &fieldArrayScaleLow,
                    &fieldArrayStrideLow,
                    &fieldArrayLowBit))
            {
                MASSERT(fieldArrayOffsetLow == fieldArrayOffsetHigh);
                MASSERT(fieldArrayScaleLow == fieldArrayScaleHigh);
                MASSERT(fieldArrayStrideLow == fieldArrayStrideHigh);
                fieldIsArray = true;

                // Strip off (i) from name
                p = strchr(Name, '(');
                *p = 0;
                Field.m_Name = Name;
            }
            else if (2 != sscanf(Definition, "%d:%d", &Field.m_HighBit, &Field.m_LowBit))
            {
                // Ignore unrecognized formats.
                continue;
            }
            // XXX LW41 manual has a field that doesn't obey this.  Ick.
            //MASSERT(Field.m_HighBit >= Field.m_LowBit);
            Field.InitAttrFromAccess();
            Field.m_IsTask = false;
            Field.m_IsClearOnOne = false;

            // Ideally, a register would immediately be followed by all its fields.
            // In practice not only is this not the case, but there are even fields
            // with no corresponding register, etc.  However, since the common case
            // is that a register is immediately or almost immediately followed by
            // its fields, do the search backwards.
            for (int i = (int)m_Registers.size()-1; i >= 0; i--)
            {
                RefManualRegister *pReg = &m_Registers[i];
                if (!strncmp(pReg->m_Name.c_str(), Name, pReg->m_Name.length()))
                {
                    pReg->m_Fields.push_back(Field);
                    break;
                }
            }
        }
        else if ((Access[4] == 'V') || (Access[4] == 'T') || (Access[4] == 'C'))
        {
            // Value (either a standard Value, or a "Task" or "Clear" value)
            // NOTE: due to looseness in ref manual specs, a "Clear" value can be indicated
            // by either (Access[4] == 'C') or the name of the field ending in "CLEAR"
            RefManualFieldValue Value;

            Value.Name = Name;
            Value.IsConstant = Access[0] == 'C';
            Value.IsTask = false;

            if ((Access[2] == 'I' ) || (Access[2] == 'W' ) || ( Access[2] == 'C' ))
            {
                Value.initType = InitWarm;
                if ( Access[2] == 'C' ) {
                    Value.initType = InitCold;
                }
            }
            else
            {
                Value.initType = InitNone;
            }

            if ((1 != sscanf( Definition, "0x%x", &Value.Value )) && (1 != sscanf( Definition, "%d", &Value.Value))) {
                continue;
            }

            if (Access[4] == 'C')
            {
                // Clear-on-1 fields should have a value of 1
                MASSERT(Value.Value == 1);
            }

            for (int i = (int)m_Registers.size()-1; i >= 0; i--)
            {
                RefManualRegister *pReg = &m_Registers[i];
                if (!strncmp(pReg->m_Name.c_str(), Name, pReg->m_Name.length()))
                {
                    for (int j = (int)pReg->m_Fields.size()-1; j >= 0; j--)
                    {
                        RefManualRegisterField *pField = &pReg->m_Fields[j];
                        if (!strncmp(pField->m_Name.c_str(), Name, pField->m_Name.length()))
                        {
                            pField->m_Values.push_back(Value);
                            if (Access[4] == 'T')
                            {
                                Value.IsTask = true;
                                pField->m_IsTask = true;
                            }
                            if (Access[4] == 'C')
                            {
                                pField->m_IsClearOnOne = true;
                            }
                            goto SearchDone;
                        }
                    }
                }
            }
SearchDone: ;
        }
        else if (Access[4] == 'D')
        {
            // save info for register group
            UINT32 Hi, Lo;

            if (sscanf (Definition, "%x:%x", &Hi, &Lo) == 2)
            {
                // create a new group struct
                RefManualRegisterGroup Grp (Name, Lo, Hi);

                // brand new group name
                m_RegisterGroups.push_back (Grp);
            }
        }
        else if ((Access[4] == 'M') || (Access[4] == 'B'))
        {
            // memory or state bundle, ignore it
        }
        //Some nonarray registers can have field with __SIZE !!
        else if (Access[4] == ' ')
        {
            if (strstr(Name, "__SIZE_1") && regIsArray)
            {
                RefManualRegister *pReg = &m_Registers.back();
                if(pReg->m_ArraySizeI <= 1) //Avoid redifining size, it happens!!
                {
                    if (1 != sscanf(Definition, "%d", &pReg->m_ArraySizeI))
                    {
                        Printf(Tee::PriError, "%s(%d) %s : Invalid size %s\n",
                              FileName, lwrrentLine, Name, Definition);
                        MASSERT(0);
                    }
                }
            }
            else if (strstr(Name, "__SIZE_2") && regIsArray)
            {
                RefManualRegister *pReg = &m_Registers.back();
                if(pReg->m_ArraySizeJ <= 1) //Avoid redifining size, it happens!!
                {
                    if (1 != sscanf(Definition, "%d", &pReg->m_ArraySizeJ))
                    {
                        Printf(Tee::PriError, "%s(%d) %s : Invalid size %s\n",
                            FileName, lwrrentLine, Name, Definition);
                        MASSERT(0);
                    }
                }
            }
            else if (strstr(Name, "__SIZE_1") && fieldIsArray)
            {
                int arraySize = 0;

                if (1 != sscanf(Definition, "%d", &arraySize))
                {
                    Printf(Tee::PriError, "%s(%d) %s : Invalid size %s\n",
                        FileName, lwrrentLine, Name, Definition);
                    MASSERT(0);
                }

                // Rather than storing array size and stride as is done for
                // register arrays, create a field for each entry in the field
                // array.  (There aren't many field arrays, so this takes up
                // less space and is also easier to implement.)

                // Grab the most recent field from the most recent register.
                RefManualRegister *pReg = &m_Registers.back();
                MASSERT(pReg->m_Fields.size() > 0);
                RefManualRegisterField *pField = &(pReg->m_Fields.back());

                // This field has the base name for the field array.  Grab
                // that name, and then fix the field so that it represents
                // the first index in the array.
                string baseName = pField->m_Name;
                pField->m_Name += "(0)";
                pField->m_LowBit = fieldArrayOffsetLow * fieldArrayStrideLow +
                    fieldArrayLowBit;
                pField->m_HighBit = fieldArrayOffsetHigh * fieldArrayStrideHigh +
                    fieldArrayHighBit;
                string access = pField->m_Access;

                // Create fields for the rest of the array.
                for (int i = 1; i < arraySize; ++i)
                {
                    RefManualRegisterField field(
                        Utility::StrPrintf("%s(%d)", baseName.c_str(), i).c_str(),
                        access.c_str());

                    field.InitAttrFromAccess();

                    field.m_LowBit = (i * fieldArrayScaleLow + fieldArrayOffsetLow) *
                        fieldArrayStrideLow +
                        fieldArrayLowBit;
                    field.m_HighBit = (i * fieldArrayScaleHigh + fieldArrayOffsetHigh) *
                        fieldArrayStrideHigh +
                        fieldArrayHighBit;
                    pReg->m_Fields.push_back(field);
                }
            }
            else
            {
                // Register alias
                // Import alias field, still ignore others.
                ParseAliasField(Name, Definition, Access);
            }
        }
        else if ((Access[4] == '-') || (Access[4] == 'S') ||
                 (Access[4] == 'L'))
        {
            // Strange, I have no idea what this is
        }
        else
        {
            Printf(Tee::PriError, "%s(%d) : Invalid register type %c\n",
                   FileName, lwrrentLine, Access[4]);
            MASSERT(0);
        }
    }

    if (zipFile)
        gzclose(lgzFile);  // close gzipped file if one was used.
    else
        fclose(f);

    Printf(Tee::PriNormal, "done.\n");

    if (UseSerial)
    {
        Printf(Tee::PriNormal, "Serializing ref manuals to file %s.\n", serialFileName.c_str());
        // Ignore return code since serialization does not affect this run
        Serialize(serialFileName.c_str());
    }

    return OK;
}

int RefManual::GetNumRegisters() const
{
    return (int)m_Registers.size();
}

const RefManualRegister *RefManual::GetRegister(int Index) const
{
    return &m_Registers[Index];
}

const RefManualRegister *RefManual::FindRegister(const char *Name) const
{
    for (unsigned int i = 0; i < m_Registers.size(); i++)
    {
        if (m_Registers[i].GetName() == Name)
        {
            return &m_Registers[i];
        }
    }
    return NULL;
}

const RefManualRegister *RefManual::FindRegister(UINT32 RegisterOffset) const
{
    UINT32 i;
    INT32 j, k;

    for (i = 0; i < m_Registers.size(); i++)
    {
        if(m_Registers[i].GetDimensionality() == 2)
        {
            for (j = 0; j < m_Registers[i].m_ArraySizeI; j++)
            {
                for(k = 0; k < m_Registers[i].m_ArraySizeJ; k++)
                {
                    if( m_Registers[i].GetOffset(j, k) == RegisterOffset )
                    {
                        return &m_Registers[i];
                    }
                }
            }
        }
        else if(m_Registers[i].GetDimensionality() == 1)
        {
            for(j = 0; j < m_Registers[i].m_ArraySizeI; j++)
            {
                if( m_Registers[i].GetOffset(j) == RegisterOffset )
                {
                    return &m_Registers[i];
                }
            }
        }
        else
        {
            if(m_Registers[i].m_Offset == RegisterOffset)
            {
                return &m_Registers[i];
            }
        }
    }
    return NULL;
}

RC RefManual::ParseRegExpr(const char *Name, ParsedRegExpr *pParsed) const
{
    MASSERT(pParsed != NULL);

    // Support 3 kinds of RegName:
    // 1.NonArrayRegName 2.ArrayRegName(i) 3.ArrayRegName(i,j)
    string regName(Name);
    string strIdx;
    size_t end = regName.find_last_of(')');
    size_t start = regName.find_last_of('(');
    if ((start < end) &&
        (end == regName.size() - 1))
    {
        strIdx = regName.substr (start, end - start + 1);
        regName.erase(start, end);
    }

    // Remove any leading and trailing spaces
    boost::trim(regName);

    pParsed->IdxI = 0;
    pParsed->IdxJ = 0;
    pParsed->pRefReg = NULL;

    const RefManualRegister *pRefReg = FindRegister(regName.c_str());
    if (pRefReg != NULL)
    {
        UINT32 idxI = 0;
        UINT32 idxJ = 0;
        if (!strIdx.empty())
        {
            if (pRefReg->GetDimensionality() == 2) // 2D array
            {
                if ((2 != sscanf(strIdx.c_str(), "(%i,%i)", &idxI, &idxJ)) ||
                    (idxI >= (UINT32)pRefReg->GetArraySizeI())             ||
                    (idxJ >= (UINT32)pRefReg->GetArraySizeJ()))
                {
                    Printf(Tee::PriError, "register index (%d,%d) is invalid.\n", idxI, idxJ);
                    return RC::BAD_PARAMETER;
                }
            }
            else if (pRefReg->GetDimensionality() == 1) //1D array
            {
                // require to return valid index i
                if ((1 != sscanf(strIdx.c_str(), "(%i)", &idxI)) ||
                    (idxI >= (UINT32)pRefReg->GetArraySizeI()))
                {
                    Printf(Tee::PriError, "register index %d is invalid.\n", idxI);
                    return RC::BAD_PARAMETER;
                }
            }
        }

        pParsed->IdxI = idxI;
        pParsed->IdxJ = idxJ;
        pParsed->pRefReg = pRefReg;
        return OK;
    }

    return RC::BAD_PARAMETER; // Not found
}

int RefManual::GetNumGroups() const
{
    return (int)m_RegisterGroups.size();
}

const RefManualRegisterGroup *RefManual::GetGroup(int Index) const
{
    return &m_RegisterGroups[Index];
}

const RefManualRegisterGroup *RefManual::FindGroup(const char *Name) const
{
    for (unsigned int i = 0; i < m_RegisterGroups.size(); i++)
    {
        if (m_RegisterGroups[i].GetName() == Name)
        {
            return &m_RegisterGroups[i];
        }
    }
    return NULL;
}
