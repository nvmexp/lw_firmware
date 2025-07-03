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
#ifndef _PROFILE_H_
#define _PROFILE_H_

#include <set>
#include "types.h"
#include "CIface.h"
#include "ICrcProfile.h"

// the point of a crc profile is to keep track of all the different CRCs
//  a given test (or tests) can result in under one or more configurations,
//  etc.
typedef struct ProfileData
{
    char *name;
    char *value;
    char *origValue;
    char *comment;
    bool measured;

    struct ProfileData *next;

} ProfileData;

typedef struct ProfileSection
{

    char *name;
    ProfileData *head;

    struct ProfileSection *next;
} ProfileSection;

class  ArgReader;

class CrcProfile : public CIfaceObject, public ICrcProfile
{
public:
  enum ProfileType
  {
      NORMAL,
      GOLD
  };

  CrcProfile(ProfileType type = NORMAL);

  ~CrcProfile();

  BEGIN_BASE_INTERFACE
  ADD_QUERY_INTERFACE_METHOD(IID_CHIP_IFACE, ICrcProfile)
  ADD_QUERY_INTERFACE_METHOD(IID_CRCPROFILE_IFACE, ICrcProfile)
  END_BASE_INTERFACE

  // initialize crc profile
  bool Initialize(const ArgReader* params);

  // file IO (returns 1 on success, 0 on failure)
  int LoadFromFile(const char *filename);
  int LoadFromBuffer(const char *str);
  int SaveToFile(const char *filename = 0);
  const string& GetFileName() const;
  void SetFileName(const string& filename);
  const string& GetTestName() const;
  const string& GetTestDir() const;
  const string& GetImgDir() const;
  void SetDump(bool dump);
  bool GetDump() const;

  // editting parts of the profile
  void WriteStr(const char *sectionName, const char *keyName, const char *value, bool measured = false);
  void WriteHex(const char *sectionName, const char *keyName, unsigned value, bool measured = false);
  void WriteComment(const char *sectionName, const char *keyName, const char *value);

  // copies all crc values associated with a specified prefix to a new prefix
  bool CopyCrcValues(const char* sectionName, const char* origPrefix, const char* newPrefix);

  // check for existence of section without printing any error messages
  bool SectionExists(const char *sectionName) const;

  // reading methods return a provided default value if the key isn't found
  const char *ReadStr(const char *sectionName, const char *keyName,
                      const char *defaultValue = 0) const;
  const char *ReadStrReadAll(const char* sectionName, const char* keyName,
                             const char* defaultValue = 0) const;
  unsigned ReadUInt(const char *sectionName, const char *keyName,
                    unsigned defaultValue = 0) const;

  void RemoveProfileData(const char *sectionName, const char *keyName) const;

  // either compares or sets (if dump != 0) a CRC in the profile
  //  returns 1 on success, 0 on failure
  int CheckCrc(const char *sectionName, const char *keyName, UINT32 crc, int dump);
  bool Loaded() { return loaded; }

protected:
  ProfileSection *sections;
  //type of data source for profile data and string data
  enum ProfileSrc {ProfileFileName, ProfileStrBuffer};
  int UpdateFile(const char *filename, const char *access) const;
  int LoadData(const char *data, ProfileSrc type);
  bool loaded;
  bool printDate;

private:
  bool KeyCopied(const string& key) const;
  bool KeyWritten(const string& key) const;

  set<string> m_CopiedKeys;
  set<string> m_WrittenKeys;

  ProfileType    m_Type;
  string         m_FileName;
  string         m_TestName;
  string         m_TestDir;
  string         m_ImgDir;
  bool           m_Dump = false;
  bool           m_CrcMissOk = false;
};

//Abtract reading from a file or a string
class CrcProfileStream
{
public:
    CrcProfileStream(FileIO *fptr, const char *buffer);
    inline char *getLine(char *buffer, int size);
private:
    const char *bufptr; //current position of string pointer
    FileIO *fptr; //current file pointer
};

#endif