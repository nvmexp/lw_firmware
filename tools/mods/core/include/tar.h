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
#ifndef _TAR_H_
#define _TAR_H_

// class library for dealing with TarArchives

#include "types.h"
#include <vector>
#include <string>
#include <memory>

#define RECORDSIZE      512
#define NAMSIZ          100
#define TUNMLEN         32
#define TGNMLEN         32
#define SPARSE_EXT_HDR  21
#define SPARSE_IN_HDR   4
#define CHKBLANKS       "        "      /* 8 blanks, no null */

#define MAGICLEN        8
#define GNU_MAGIC       "ustar  "
#define POSIX_MAGIC     "ustar\0" "00" // Must split into 2 parts, or "\000"
                                       // becomes a single null character
class RegExp;

struct sparse
{
    char offset[12];
    char numbytes[12];
};

union record
{
    char charptr[RECORDSIZE];

    struct header
    {
        char arch_name[NAMSIZ];
        char mode[8];
        char uid[8];
        char gid[8];
        char size[12];
        char mtime[12];
        char chksum[8];
        char linkflag;
        char arch_linkname[NAMSIZ];
        char magic[MAGICLEN];
        char uname[TUNMLEN];
        char gname[TGNMLEN];
        char devmajor[8];
        char devminor[8];
    }
    header;
};

class TarFile
{
public:
    TarFile();
    virtual ~TarFile() {};

    unsigned GetSize() const;
    const string GetFileName() const;

    /*
     * \brief Copy 'bytes' bytes from src to internal memory.
     *        Reallocates if necessary
     */
    virtual void Write(const char *src, unsigned bytes) = 0;

    /*
     * \brief Copy file data into 'dst' until one of these is reached:
     *        EOF or a total of 'bytes' bytes
     * \return number of bytes copied
     */
    virtual unsigned Read(char *dst, unsigned bytes) const = 0;

    /*
     * \brief Set '*src' to point to the file data (in memory) at the current
     *        file pointer location and advance the file pointer to end of
     *        memory (this could be EOF or end-of-chunk; see implementations).
     *        To access a full file, call this function repeatedly until src
     *        is set to nullptr and/or 0 is returned.
     * \warn This pointer could be ilwalidated if the object changes.
     *       Use with care
     * \note Does not copy any data
     * \return number of bytes available (until EOF or end-of-chunk)
     */
    virtual size_t Read(const char **src) const = 0;

    /*
     * \brief Copy file data into 'dst' until one of these is reached:
     *        EOL, EOF or a total of 'bytes' bytes
     * \return false if file pointer is beyond EOF, true otherwise
     */
    virtual bool Gets(char *dst, unsigned bytes) = 0;

    void Seek(unsigned offset) const;
    unsigned Tell() const;

    void SetFileName(const string &name);

    void WriteHeader(const void *src); // always 512 bytes
    void ReadHeader(void *dst) const; // always 512 bytes
protected:
    void updateCrc();
    void updateLimit();
    unsigned callwlate_checksum(const char *, size_t) const;

    record header;
    unsigned limit;
    mutable unsigned fpos;
};

// TarFile_SCImp is the original TarFile implementation. It organizes the data
// using small 512 byte memory chunks, thus creates a lot of memory
// fragments during buildup. It is most useful if used in the environment
// where the memory is already fragmented.
class TarFile_SCImp: public TarFile
{
public:
    TarFile_SCImp() {};
    virtual ~TarFile_SCImp();

    void Write(const char *src, unsigned bytes) override;
    unsigned Read(char *dst, unsigned bytes) const override;
    size_t Read(const char **src) const override;
    bool Gets(char *dst, unsigned bytes) override;
private:
    static constexpr UINT16 CHUNK_SIZE = 512;
    vector<char*> chunks;
};

// TarFile_BCImp is new implementation of TarFile in hope for solving the
// memory fragmentation issue caused by TarFile_SCImp. It tries to allocate
// one big memory chunk for each file during writing phase.
class TarFile_BCImp: public TarFile
{
public:
    TarFile_BCImp(unsigned sz);
    virtual ~TarFile_BCImp();

    void Write(const char *src, unsigned bytes) override;
    unsigned Read(char *dst, unsigned bytes) const override;
    size_t Read(const char **src) const override;
    bool Gets(char *dst, unsigned bytes) override;

private:
    char* m_chunk;
};

class TarArchive
{
public:
    TarArchive()                             = default;
    TarArchive(const TarArchive&)            = delete;
    TarArchive& operator=(const TarArchive&) = delete;
    TarArchive(TarArchive&& tar) : m_Files(move(tar.m_Files)) { }
    TarArchive& operator=(TarArchive&& tar)
    {
        m_Files = move(tar.m_Files);
        return *this;
    }

    bool ReadFromFile(const string &filename, bool bigmem=true);
    bool WriteToFile(const string &filename) const;

    void AddToArchive(TarFile *file);
    void AddToArchive(unique_ptr<TarFile> pInFile);

    TarFile *Find(const string &filename);
    const TarFile* Find(const string& filename) const;
    TarFile *FindPath(const string &filename);
    void FindFileNames
    (
        const string& nameBegin,
        const string& nameEnd,
        vector<string>* pNames
    ) const;
    void FindFileNames
    (
        const RegExp& nameRegExp,
        vector<string>* pNames
    ) const;

private:
    void PrintTarHeader(const record&) const;
    unsigned callwlate_checksum(const char *in, size_t size) const;

    vector<unique_ptr<TarFile>> m_Files;
};

#endif
