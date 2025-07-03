/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef _ICRCPROFILE_H_
#define _ICRCPROFILE_H_

#include "sim/IIface.h"
#include "sim/ITypes.h"

#include "CrcInfo.h"

class ArgReader;

class ICrcProfile : public IIfaceObject
{
public:
    // file IO (returns 1 on success, 0 on failure)
    virtual int SaveToFile(const char *filename = 0) = 0;
    virtual const string& GetFileName() const = 0;
    virtual void SetFileName(const string& filename) = 0;
    virtual const string& GetTestDir() const = 0;
    virtual const string& GetTestName() const = 0;
    virtual const string& GetImgDir() const = 0;
    virtual void SetDump(bool dump) = 0;
    virtual bool GetDump() const = 0;

    // editting parts of the profile
    virtual void WriteHex(const char *section_name, const char *key_name, unsigned value, bool measured = false) = 0;
    virtual void WriteComment(const char *section_name, const char *key_name, const char *value) = 0;

    // copies all crc values associated with a specified prefix to a new prefix
    virtual bool CopyCrcValues
    (
        const char* section_name,
        const char* orig_prefix,
        const char* new_prefix
    ) = 0;

    // reading methods return a provided default value if the key isn't found
    virtual const char *ReadStr(const char *section_name, const char *key_name,
                                const char *default_value = 0) const = 0;
    virtual const char *ReadStrReadAll(const char* section_name, const char* key_name,
                                       const char* default_falue = 0) const = 0;
    virtual unsigned ReadUInt(const char *section_name, const char *key_name,
                              unsigned default_value = 0) const = 0;

    virtual void RemoveProfileData(const char *section_name, const char *key_name) const = 0;

    // either compares or sets (if dump != 0) a CRC in the profile
    //  returns 1 on success, 0 on failure
    virtual int CheckCrc(const char *section_name, const char *key_name, UINT32 crc, int dump) = 0;
    virtual bool Loaded() = 0;

    virtual bool Initialize(const ArgReader* params) = 0;
};

#endif // _ICRCPROFILE_H_
