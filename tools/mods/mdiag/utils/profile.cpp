/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// the point of a crc profile is to keep track of all the different CRCs
//  a given test (or tests) can result in under one or more configurations,
//  etc.

#include "mdiag/sysspec.h"
#include "profile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include "core/include/cmdline.h"
#include "core/include/utility.h"
#include "utils.h"

//------------------------------------------------------------------------
// FindStringVal - Extract the content from first word in string, or 
//    extract content from quotes.
//------------------------------------------------------------------------
static char *FindStringVal(char *str)
{

    char *start = str;
    while (isspace(*start))
    {
        start++;
    }
    char *end = start;
    
    if (start[0] == '"')
    {
        while (*start == '"')
        {
            end = ++start;
        }
        while (*end != '"')
        {
            if (!*end)
            {
                WarnPrintf("Cannot find terminating quote in CRC profile.\n");
                break;
            }
            end++;
        }
        *end = 0;
    }
    else
    {
        while (*end && !isspace(*end))
        {
            end++;
        }
        *end = 0;
    }

    return start;
}

//------------------------------------------------------------------------
// WriteStringVal
//------------------------------------------------------------------------
static void WriteStringVal(const char *str, FileIO *fd)
{
    if (str)
    {
        // check if str contains spaces
        if (strchr(str, ' '))
        {
            // don't quote comment strings, leave as-is
            if (str[0] == '#')
                fd->FilePrintf("%s", str);
            else
                fd->FilePrintf("\"%s\"", str);
        }
        else
        {
            fd->FilePrintf("%s", str);
        }
    }
}

//------------------------------------------------------------------------
// CreateProfileData
//------------------------------------------------------------------------
static ProfileData *CreateProfileData(const char *keyName, bool measured = false)
{
    ProfileData *key;

    key = new ProfileData;
    key->next = NULL;
    key->value = NULL;
    key->origValue = NULL;
    key->name = Utility::StrDup(keyName);
    key->comment = NULL;
    key->measured = measured;

    return key;
}

//------------------------------------------------------------------------
// DestroyProfileData
//------------------------------------------------------------------------
static void DestroyProfileData(ProfileData *key)
{
    delete [] key->name;
    delete [] key->value;
    delete [] key->origValue;
    delete [] key->comment;
    delete key;
}

//------------------------------------------------------------------------
// GetProfileDataEntry - returns pointer to entry of specified keyName, or
//    NULL if the keyName didn't exist and 'create' is 0.
//
// if headPtr is null (*headPtr == NULL just means an empty list), return 0
//   this lets us chain the functions for fetches which don't create.
//------------------------------------------------------------------------
static ProfileData** GetProfileDataEntry(ProfileData **headPtr, const char *keyName, 
                                             int create, bool measured = false)
{
    if (!headPtr) 
        return 0;

    while(*headPtr)
    {
        if (!strcmp((*headPtr)->name, keyName))
        {
            return headPtr;
        }

        headPtr = &((*headPtr)->next);
    }

    if (create)
    {
        *headPtr = CreateProfileData(keyName, measured);
        return headPtr;
    }
    else
        return 0;
}

//------------------------------------------------------------------------
// GetProfileDataValue - returns pointer to value entry of specified keyName, or
//    NULL if the keyName didn't exist and 'create' is 0
//
// if headPtr is null (*headPtr == NULL just means an empty list), return 0
//   this lets us chain the functions for fetches which don't create
//------------------------------------------------------------------------
static char **GetProfileDataValue(ProfileData **headPtr, const char *keyName,
                                      int create, bool measured = false)
{
    if (!headPtr) 
        return 0;

    while(*headPtr)
    {
        if (!strcmp((*headPtr)->name, keyName) && !(*headPtr)->measured)
        {
            return(&((*headPtr)->value));
        }

        headPtr = &((*headPtr)->next);
    }

    if (create)
    {
        *headPtr = CreateProfileData(keyName, measured);
        return(&((*headPtr)->value));
    }
    else
        return 0;
}

//------------------------------------------------------------------------
// GetProfileDataValue - returns pointer to value entry of specified keyName, or
//    NULL if the keyName didn't exist and 'create' is 0
//
// if headPtr is null (*headPtr == NULL just means an empty list), return 0
//   this lets us chain the functions for fetches which don't create
//------------------------------------------------------------------------
static char **GetProfileDataValueIgnoredMeasured
(
    ProfileData **headPtr,
    const char *keyName,
    int create, bool measured = false)
{
    headPtr = GetProfileDataEntry(headPtr, keyName, create, measured);

    if (headPtr)
        return (&((*headPtr)->value));
    else
        return 0;
}

//------------------------------------------------------------------------
// GetProfileDataComment - returns pointer to comment entry of specified keyName, or
//    NULL if the keyName didn't exist
//
// if headPtr is null (*headPtr == NULL just means an empty list), return 0
//   this lets us chain the functions for fetches which don't create
//------------------------------------------------------------------------
static char **GetProfileDataComment(ProfileData **headPtr, const char *keyName)
{
    headPtr = GetProfileDataEntry(headPtr, keyName, 0);

    if (headPtr) 
        return (&((*headPtr)->comment));
    else
        return 0;
}

//------------------------------------------------------------------------
// SetKeyEntry
//------------------------------------------------------------------------
static void SetKeyEntry(char **entryPtr, const char *newEntry)
{
    DebugPrintf("SetKeyEntry newEntry=%s\n", newEntry);

    delete [] *entryPtr;

    *entryPtr = Utility::StrDup(newEntry);
}

//------------------------------------------------------------------------
// PrintProfileData
//------------------------------------------------------------------------
static void PrintProfileData(const ProfileData *key, FileIO *fd)
{
    bool value_changed = false;
    WriteStringVal(key->name, fd);
    fd->FilePrintf(" = ");
    WriteStringVal(key->value, fd);

    // we will keep the comment on a crc key only if it has not been changed
    if (key->origValue && (strcmp(key->value, key->origValue) != 0))
        value_changed = 1;
    if (key->comment && !value_changed)
    {
        fd->FilePrintf(" ");
        WriteStringVal(key->comment, fd);
    }
    fd->FilePrintf("\n");
}

//------------------------------------------------------------------------
// CreateProfileSection
//------------------------------------------------------------------------
static ProfileSection *CreateProfileSection(const char *sectionName)
{
    ProfileSection *sect;

    sect = new ProfileSection;
    sect->next = NULL;
    sect->head = NULL;
    sect->name = Utility::StrDup(sectionName);

    return sect;
}

//------------------------------------------------------------------------
// DestroyProfileSection
//------------------------------------------------------------------------
static void DestroyProfileSection(ProfileSection *sect)
{
    ProfileData *key, *nextKey;
    for (key = sect->head; key; key = nextKey)
    {
        nextKey = key->next;
        DestroyProfileData(key);
    }
    delete [] sect->name;
    delete sect;
}

//------------------------------------------------------------------------
// GetProfileSection - create if neccesary and 'create' != 0
//------------------------------------------------------------------------
static ProfileSection *GetProfileSection(ProfileSection **headPtr,
                                            const char *sectionName, int create)
{
    while(*headPtr)
    {
        if (!strcmp((*headPtr)->name, sectionName))
            return (*headPtr);

        headPtr = &((*headPtr)->next);
    }

    if (create)
        return ((*headPtr = CreateProfileSection(sectionName)));
    else
        return 0;
}

// read-only version for type-safety
static ProfileSection *GetProfileSectionRO(ProfileSection * const *headPtr,
                                               const char *sectionName)
{
    while(*headPtr)
    {
        if (!strcmp((*headPtr)->name, sectionName))
            return (*headPtr);

        headPtr = &((*headPtr)->next);
    }
    return 0;
}

//------------------------------------------------------------------------
// PrintProfileSection
//------------------------------------------------------------------------
static void PrintProfileSection(const ProfileSection *sect, FileIO *fd)
{
    const ProfileData *key;

    fd->FilePrintf("[%s]\n", sect->name);

    for (key = sect->head; key; key = key->next)
    {
        PrintProfileData(key, fd);
    }
}

static void InsertProfileData(ProfileSection *sect, ProfileData *insertKey)
{
    ProfileData *key, *lastKey = sect->head;

    if (strcmp(insertKey->name,lastKey->name) < 0)      // if it should go first
    {                                                 // then make insertKey the head of the list
        insertKey->next = sect->head;
        sect->head = insertKey;
        return;
    }

    for (key = lastKey->next; key; key = key->next)
    {
        if (strcmp(insertKey->name,key->name) < 0)         // if it should go before key
        {                                               // then insert between lastKey and key
            insertKey->next = key;
            lastKey->next = insertKey;
            return;
        }
        lastKey = key;
    }

    lastKey->next = insertKey;                          // oops, insertKey belongs at the end!
    insertKey->next = NULL;
}

//------------------------------------------------------------------------
// SortProfileSection ; use a stupid bubble sort
//------------------------------------------------------------------------
static void SortProfileSection(ProfileSection *sect)
{
    ProfileData *key, *lastKey, *nextKey;

    lastKey = sect->head;
    if (lastKey == NULL) 
        return;                     
    for (key = lastKey->next; key; key = nextKey)
    {
        nextKey = key->next;                           // gotta save this in case we move things
        if (strcmp(key->name,lastKey->name) < 0)       // if out of order then bubble up
        {
            lastKey->next = nextKey;                    // first remove me
            InsertProfileData(sect,key);                      // and then insert me back in
        }
        else
            lastKey = key;
    }
}

CrcProfile::CrcProfile(ProfileType type)
    : m_Type(type)
{
    this->sections = 0;
    this->loaded = false;
    this->printDate = false;
}

CrcProfile::~CrcProfile()
{
    ProfileSection *nextSection;

    while(sections)
    {
        nextSection = sections->next;
        DestroyProfileSection(sections);
        sections = nextSection;
    }
}

//------------------------------------------------------------------------
// initializes the crc profile
//------------------------------------------------------------------------
bool CrcProfile::Initialize(const ArgReader* params)
{
    size_t pos = (size_t)0;
    string buffer = "";

    m_CrcMissOk = params->ParamPresent("-crcMissOK") > 0;

    if (params->ParamPresent("-crcpath") > 0)
    {
        const char* arg = params->ParamStr("-crcpath");
        if (arg)
        {
            string trarg = UtilStrTranslate(arg, DIR_SEPARATOR_CHAR2, DIR_SEPARATOR_CHAR);
            if ((trarg.size() > 1) && (trarg[0] == '.') && (trarg[1] == DIR_SEPARATOR_CHAR))
            {
                trarg.erase(0, 2); // remove "./" or ".\" at begin of path
            }
            buffer += trarg;
        }
    }
    else
    {
        string input_filename = params->ParamStr("-i", "test");
        input_filename = UtilStrTranslate(input_filename, DIR_SEPARATOR_CHAR2, DIR_SEPARATOR_CHAR);
        if ((input_filename[0] == '.') && (input_filename[1] == DIR_SEPARATOR_CHAR))
            input_filename.erase(0, 2); // remove "./" or ".\" at the begin of path

        if (params->ParamStr("-crcLocal") > 0)
        {
            char prefix_in[4096], prefix[4096];
            strcpy(prefix_in, params->ParamStr("-crcLocal"));

            char input[4096];
            clean_filename_slashes(input, input_filename.c_str());
            clean_filename_slashes(prefix, prefix_in);

            // ok, our input string is now clean.
            // now figure out whether the substitution and match strings.
            char match_list_data[4096]; // this will be the name we are matching, like O:\traces
            char *match_list = match_list_data;
            char sub[4096]; // this will be the name we are substituting, like L:\perforce\arch\traces
            extract_sub_and_match_list(sub, match_list, prefix);

            int got_match = 0;
            // match_list looks like O:\traces,P:\.
            while (match_list[0] != '\0')
            {
                char match[4096];
                match_list = extract_next_match_string(match, match_list);
                // now copy input to buffer, substituting sub for match if match is a prefix of input:
                // match should be O:\traces, and sub should be L:\perforce\arch\traces
                int compare = memcmp(match, input, strlen(match));
                if (compare == 0)
                {
                    // got a match
                    buffer += sub;
                    // note that this is version of filename with extra / and \ stripped out
                    buffer += input + strlen(match);

                    got_match = 1;
                    break; // did a substitution, so dont find any more matches
                }
            }
            if (!got_match)
            {
                // there was no match
                buffer += input_filename; // with the extra slashes
            }
            char debug_print[4096];
            sprintf(debug_print, "crcLocal is \"%s\"\n", buffer.c_str());
            InfoPrintf(debug_print);
        }
        else
        {
            buffer += input_filename;
        }

        buffer = UtilStrTranslate(buffer, DIR_SEPARATOR_CHAR2,
            DIR_SEPARATOR_CHAR);
        pos = buffer.find_last_of(DIR_SEPARATOR_CHAR);
        if (pos != string::npos)
            buffer.erase(pos);
        else
            buffer = "";
    }

    m_TestDir = buffer.c_str();
    m_FileName += buffer.c_str();
    if (!m_FileName.empty())
    {
        m_FileName += DIR_SEPARATOR_CHAR;
    }
    if (m_Type == NORMAL)
    {
        if (params->ParamPresent("-crcfilename") > 0)
            m_FileName += params->ParamStr("-crcfilename", "test.crc");
        else
            m_FileName += "test.crc";
    }
    else
        m_FileName += "golden.crc";

    struct stat statbuf;
    if (stat(m_FileName.c_str(), &statbuf) == 0)
    {
        // Load crc file if it exists.
        LoadFromFile(m_FileName.c_str());
    }

    pos = buffer.find_last_of(DIR_SEPARATOR_CHAR);
    if (pos != string::npos)
    {
        m_TestName = buffer.substr(pos + 1);
        buffer.erase(pos);
    }
    else if (!buffer.empty())
    {
        m_TestName = buffer.c_str();
        buffer = "";
    }
    else
        m_TestName = "test";

    if (pos)
    {
        m_ImgDir = buffer.c_str();
        m_ImgDir += DIR_SEPARATOR_CHAR;
    }

    return true;
}

// read profile data from an ascii file (returns 1 on success, 0 on failure)
int CrcProfile::LoadFromFile(const char *filename)
{
    InfoPrintf("Loading crc profile from: %s\n", filename);
    return LoadData(filename, ProfileFileName);
}

// read profile data from a string (returns 1 on success, 0 on failure)
int CrcProfile::LoadFromBuffer(const char *str)
{
    return LoadData(str, ProfileStrBuffer);
}

//Helper function, takes a filename or a buffer containing profile info
int CrcProfile::LoadData(const char *filename, ProfileSrc inputType)
{
    FileIO *file = NULL;
    char buffer[4096];
    int line_num;
    char *end_sectionName, *equals;
    ProfileSection *lwrrentSection;

    if (inputType == ProfileFileName)
    {
        // NOTE: no longer possible to let global variable override file name
        file = Sys::GetFileIO(filename, "r");
        if (!file)
        {
            ErrPrintf("Couldn't open CRC file %s for reading\n", filename);
            return 0;
        }
    }

    CrcProfileStream CrcInfo(file, filename);

    line_num = 0;
    lwrrentSection = 0;
    while(CrcInfo.getLine(buffer, 4095))
    {
        line_num++;

        switch(*buffer)
        {
        case '#':
            // comment, do nothing
            break;

        case '[':
            // section name
            end_sectionName = strchr(buffer, ']');
            if (end_sectionName)
            {
                *end_sectionName = 0;
                lwrrentSection = GetProfileSection(&sections, buffer+1, 1 /* create */);
            }
            else
                WarnPrintf("error parsing profile '%s', line %d.\n", filename, line_num);

        default:
            // if it has an equal sign, it's an assignment...
            equals = strchr(buffer, '=');
            if (equals)
            {
                if (lwrrentSection)
                {
                    *equals = 0;

                    char* comment = strchr(equals+1, '#');

                    char* keyName = FindStringVal(buffer);
                    char* val = FindStringVal(equals+1);
                    
                    // Find the keyName entry, create one if not exist.
                    ProfileData** entry = GetProfileDataEntry(&(lwrrentSection->head), keyName, 1);
                    // Only initialize value when first read
                    if ((*entry)->value == NULL)
                    {
                        (*entry)->value = Utility::StrDup(val);
                    }
                    (*entry)->origValue = Utility::StrDup(val);
                    
                    if (comment)
                    {
                        comment = Utility::RemoveHeadTailWhitespace(comment); // this cleanup lead/trail whitespace
                        (*entry)->comment = Utility::StrDup(comment);
                    }
                }
                else
                    WarnPrintf("assignment '%s' found before section declaration in '%s', line %d.\n",
                                buffer, filename, line_num);
            }
            break;
        }
    }

    if (file)
    {
        file->FileClose();
        Sys::ReleaseFileIO(file);
    }

    loaded = true;

    return 1;
}

//Helper function for SaveToFile and AddToFile
int CrcProfile::UpdateFile(const char *filename, const char *access) const
{
    DebugPrintf("CrcProfile::UpdateFile(%s, %s)\n", filename, access);

    FileIO *file;
    time_t now;
    ProfileSection *section;

    file = Sys::GetFileIO(filename, access);
    if (!file)
    {
        return 0;
    }

    if (printDate)
    {
        time(&now);
        file->FilePrintf("# Generated on %s\n", ctime(&now));
    }

    section = sections;
    while(section)
    {
        SortProfileSection(section);         // sort keys in ascending order
        PrintProfileSection(section, file);
        // file->FilePrintf("\n"); // do this to match old trace_cels output format
        section = section->next;
    }

    file->FileClose();
    Sys::ReleaseFileIO(file);

    return 1;
}

// Write to file with current sections
int CrcProfile::SaveToFile(const char *filename) 
{
    const char* tmp = m_FileName.c_str();
    if (filename != NULL)
        tmp = filename;
    else
    {
        // Check if file get updated from other MODS proccess and merge them before write.
        LoadFromFile(tmp);
    }
    return UpdateFile(tmp, "w");
}

// editting parts of the profile
void CrcProfile::WriteStr(const char *sectionName, const char *keyName,
                          const char *value, bool measured)
{
    // none of these can fail (except for running out of memory, which is lwrrently
    //  handled poorly)
    ProfileSection* sect = GetProfileSection(&sections, sectionName, 1);
    char** entry = GetProfileDataValueIgnoredMeasured(&(sect->head), keyName, 1, measured);
    SetKeyEntry(entry, value);
    m_WrittenKeys.insert(keyName);
}

void CrcProfile::WriteHex(const char *sectionName, const char *keyName,
                          unsigned value, bool measured)
{
    char buffer[32]; // enough to hold an int

    sprintf(buffer, "0x%08x", value);
    WriteStr(sectionName, keyName, buffer, true);
}

void CrcProfile::WriteComment(const char *sectionName, const char *keyName, const char *value)
{
    SetKeyEntry(GetProfileDataComment(&(GetProfileSection(&sections,
                                                              sectionName,
                                                              1 /* create if needed */
                                                              )->head),
                                        keyName),
                  value);
}

// copies all crc values associated with a specified prefix to a new prefix
bool CrcProfile::CopyCrcValues
(
    const char* sectionName,
    const char* origPrefix,
    const char* newPrefix
)
{
    if (strcmp(origPrefix, newPrefix) == 0)
        return true;

    if (KeyCopied(origPrefix) == true)
    {
        DebugPrintf("Key %s already copied\n", origPrefix);
        return true;
    }

    ProfileSection* psect = GetProfileSectionRO(&sections, sectionName);
    if (psect == NULL)
    {
        ErrPrintf("Specified section: %s does not exist in crc profile!\n", sectionName);
        return false;
    }

    DebugPrintf("Matching crc profile key values with prefix: %s\n", origPrefix);
    UINT32 len = strlen(origPrefix);
    ProfileData* key = psect->head;

    while(key != NULL)
    {
        // bug 391894, enforce crc key length to prevent extra copying
        if ((strncmp(origPrefix, key->name, len) == 0) &&
            ((strlen(key->name)-3) == len))
        {
            string buf = newPrefix;
            char* c = key->name + len;
            buf += c;

            if (KeyWritten(buf) != true)
            {
                // do not copy a key that was just written
                DebugPrintf("Matched crc profile key: %s\n", key->name);
                DebugPrintf("New crc profile key name: %s\n", buf.c_str());

                ProfileData* pkey = CreateProfileData(buf.c_str(), true);
                pkey->value = Utility::StrDup(key->value);
                InsertProfileData(psect, pkey);
            }
        }
        key = key->next;
    }

    m_CopiedKeys.insert(origPrefix);
    return true;
}

// check for existence of section without printing any error messages
bool CrcProfile::SectionExists(const char *sectionName) const
{
    if (!sectionName)
        return false;

    ProfileSection *psect = GetProfileSectionRO(&sections, sectionName);

    return (psect != NULL);
}

// reading methods return a provided default value if the key isn't found
const char *CrcProfile::ReadStr(const char *sectionName, const char *keyName,
                                const char *defaultValue /*= 0*/) const
{
    /*const*/ char **valuePtr = 0;

    if (!sectionName)
    {
        ErrPrintf("CrcProfile::ReadStr(): Invalid sectionName\n");
        return defaultValue;
    }

    if (!keyName)
    {
        ErrPrintf("CrcProfile::ReadStr(): Invalid keyName\n");
        return defaultValue;
    }

    ProfileSection *psect = GetProfileSectionRO(&sections, sectionName);
    if (psect)
        valuePtr = GetProfileDataValue(&(psect->head), keyName, 0 /* do not create key on miss */);
    else
    {
        if (loaded)
        {
            if (!m_CrcMissOk)
                ErrPrintf("CrcProfile::ReadStr(): Couldn't find section \"%s\" in CRC file\n", sectionName);
            else
                WarnPrintf("CrcProfile::ReadStr(): Couldn't find section \"%s\" in CRC file\n", sectionName);
        }
        else
            InfoPrintf("CrcProfile::ReadStr(): Couldn't find section \"%s\" in CRC file\n", sectionName);
    }

    if (valuePtr)
        return (*valuePtr);
    else
        return defaultValue;
}

// reading methods return a provided default value if the key isn't found, read all entries
const char* CrcProfile::ReadStrReadAll(const char* sectionName, const char* keyName,
                                       const char* defaultValue) const
{
    /*const*/ char **valuePtr = 0;

    if (!sectionName)
    {
        ErrPrintf("CrcProfile::ReadStr(): Invalid sectionName\n");
        return defaultValue;
    }

    if (!keyName)
    {
        ErrPrintf("CrcProfile::ReadStr(): Invalid keyName\n");
        return defaultValue;
    }

    ProfileSection *psect = GetProfileSectionRO(&sections, sectionName);
    if (psect)
        valuePtr = GetProfileDataValueIgnoredMeasured(&(psect->head), keyName, 0 /* do not create key on miss */);
    else
    {
        if (loaded)
        {
            if (!m_CrcMissOk)
                ErrPrintf("CrcProfile::ReadStrReadALl(): Couldn't find section \"%s\" in CRC file\n", sectionName);
            else
                WarnPrintf("CrcProfile::ReadStrReadALl(): Couldn't find section \"%s\" in CRC file\n", sectionName);
        }
        else
            InfoPrintf("CrcProfile::ReadStrReadAll(): Couldn't find section \"%s\" in CRC file\n", sectionName);
    }

    if (valuePtr)
        return (*valuePtr);
    else
        return defaultValue;
}

unsigned CrcProfile::ReadUInt(const char *sectionName, const char *keyName,
                              unsigned defaultValue /*= 0*/) const
{
    /*const*/ char **valuePtr = NULL;
    unsigned long result;
    char *endpos;

    ProfileSection *psect = GetProfileSectionRO(&sections, sectionName);
    if (psect)
    {
        valuePtr = GetProfileDataValue(&psect->head,
                                          keyName,
                                          0 /* do not create key on miss */);
    }
    else
    {
        if (loaded)
        {
            if (!m_CrcMissOk)
                ErrPrintf("CrcProfile::ReadUInt(): Couldn't find section \"%s\" in CRC file\n", sectionName);
            else
                WarnPrintf("CrcProfile::ReadUInt(): Couldn't find section \"%s\" in CRC file\n", sectionName);
        }
        else
            InfoPrintf("CrcProfile::ReadUInt(): Couldn't find section \"%s\" in CRC file\n", sectionName);
    }

    if (valuePtr)
    {
        errno = 0;
        result = strtoul(*valuePtr, &endpos, 0);
        if (*endpos || (errno != 0))
            return defaultValue;
        else
            return result;
    }
    else
        return defaultValue;
}

void CrcProfile::RemoveProfileData(const char *sectionName, const char *keyName) const
{
    ProfileSection *sect = GetProfileSectionRO(&sections, sectionName);
    if (sect)
    {
        ProfileData *key, *lastKey, *nextKey;

        // search for all the keys that match keyName and remove them
        lastKey = NULL;
        for (key = sect->head; key; key = nextKey)
        {
            nextKey = key->next;                           // gotta save this in case we free things
            if (strcmp(key->name,keyName) == 0)
            {
                WarnPrintf("Removing CRC=%s from profile\n", key->name);
                if (lastKey)
                    lastKey->next = nextKey;
                else
                    sect->head = nextKey;
                DestroyProfileData(key);
            }
            else
                lastKey = key;
        }
    }
}

// either compares or sets (if dump != 0) a CRC in the profile
//  returns 1 on success, 0 on failure, -1 on missing key
int CrcProfile::CheckCrc(const char *sectionName, const char *keyName,
                         UINT32 crc, int dump)
{
    UINT32 goldelwalue;

    if (dump)
    {
        // read checksum value and print it out
        InfoPrintf("CrcProfile::CheckCrc() Saving [%s] %s = 0x%08x\n",
                    sectionName, keyName, crc);

        // update the profile
        WriteHex(sectionName, keyName, crc);
        return 1;
    }
    else
    {
        // read checksum value and print it out, manual compare
        InfoPrintf("Measured CRC=0x%08x\n", crc);

        // compare with the profile value - first see if key exists
        DebugPrintf("CrcProfile::CheckCrc() "
                    "Checking section %s, key %s, against crc 0x%08x\n",
                    sectionName, keyName, crc);
        if (ReadStr(sectionName, keyName))
        {
            goldelwalue = ReadUInt(sectionName, keyName, 0);

            if (crc == goldelwalue)
            {
                InfoPrintf("CRC OK.\n");
                return 1;
            }
            else
            {
                ErrPrintf("CRC WRONG!! Golden Value = 0x%08x Measured Value = 0x%08x\n",
                          goldelwalue, crc);
                return 0;
            }
        }
        else
        {
            ErrPrintf("CrcProfile::CheckCrc: No golden crc value found for section \"%s\", key \"%s\".\n",
                      sectionName, keyName);
            return -1;
        }
    }
}

//------------------------------------------------------------------------
// check if key has been copied
//------------------------------------------------------------------------
bool CrcProfile::KeyCopied(const string& key) const
{
    set<string>::const_iterator it = m_CopiedKeys.find(key);
    set<string>::const_iterator itend = m_CopiedKeys.end();
    return it != itend;
}

//------------------------------------------------------------------------
// check if key was written
//------------------------------------------------------------------------
bool CrcProfile::KeyWritten(const string& key) const
{
    set<string>::const_iterator it = m_WrittenKeys.find(key);
    set<string>::const_iterator itend = m_WrittenKeys.end();
    return it != itend;
}

//Initialize buffer read
CrcProfileStream::CrcProfileStream(FileIO *fileptr, const char *buffer)
{
    fptr = fileptr;
    bufptr = buffer;
}

inline char *
CrcProfileStream::getLine(char *buffer, int bufSize)
{
    if (fptr) //reading from file
        return fptr->FileGetStr(buffer, bufSize);

    //read a line of data from string; "sgets()"
    int read = 0;
    const char *bptr = bufptr;

    //skip over end of line stuff
    while((*bptr == '\n' || *bptr == '\r') && *bptr != '\0')
        bptr++;

    bufptr = bptr; //start copying past end of line chars

    if (!*bptr)
        return 0;

    //look for end of line or end of string
    while (*bptr != '\n' && *bptr != '\r' && *bptr != '\0')
    {
        read++;
        bptr++;
    }

    if (bufSize) //leave room for '\0'
        bufSize--;

    read = bufSize < read ? bufSize : read; // MIN(bufSize, read)

    strncpy(buffer, bufptr, read);
    buffer[read] = '\0';

    bufptr += read; //remember what we read
    return buffer;
}

//------------------------------------------------------------------------
// sets the output filename
//------------------------------------------------------------------------
void CrcProfile::SetFileName(const string& filename)
{
    m_FileName = filename;
}

//------------------------------------------------------------------------
// returns the crc output filename
//------------------------------------------------------------------------
const string& CrcProfile::GetFileName() const
{
    return m_FileName;
}

//------------------------------------------------------------------------
// returns testname
//------------------------------------------------------------------------
const string& CrcProfile::GetTestName() const
{
    return m_TestName;
}

//------------------------------------------------------------------------
// returns the testdir
//------------------------------------------------------------------------
const string& CrcProfile::GetTestDir() const
{
    return m_TestDir;
}

//------------------------------------------------------------------------
// returns imgdir
//------------------------------------------------------------------------
const string& CrcProfile::GetImgDir() const
{
    return m_ImgDir;
}

//------------------------------------------------------------------------
// sets the dump flag
//------------------------------------------------------------------------
void CrcProfile::SetDump(bool dump)
{
    m_Dump = dump;
}

//------------------------------------------------------------------------
// gets the dump flag
//------------------------------------------------------------------------
bool CrcProfile::GetDump() const
{
    return m_Dump;
}
