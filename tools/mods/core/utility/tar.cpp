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
#include "core/include/tar.h"
#include "core/include/massert.h"
#include "core/include/regexp.h"
#include "core/include/utility.h"   // For UNSIGNED_CAST
#include "core/include/zlib.h"
#include <cstdio>
#include <string.h>

template <class T> inline const T& mymin(const T &x, const T &y) { return (x < y) ? x : y; };
template <class T> inline const T& mymax(const T &x, const T &y) { return (x > y) ? x : y; };

TarFile::TarFile() : limit(0), fpos(0)
{
    memset(&header, 0, sizeof(header));
    snprintf(header.header.mode, sizeof(header.header.mode), "0777");
    snprintf(header.header.uid, sizeof(header.header.uid), "00");
    snprintf(header.header.gid, sizeof(header.header.gid), "00");
    snprintf(header.header.size, sizeof(header.header.size), "00");
    memcpy(header.header.magic, POSIX_MAGIC, MAGICLEN); // POSIX_MAGIC contains a \0
    snprintf(header.header.uname, sizeof(header.header.uname), "nobody");
    snprintf(header.header.gname, sizeof(header.header.gname), "nobody");
    snprintf(header.header.devmajor, sizeof(header.header.devmajor), "00");
    snprintf(header.header.devminor, sizeof(header.header.devminor), "00");
}

TarFile_SCImp::~TarFile_SCImp()
{
    for (vector<char*>::iterator i = chunks.begin(); i != chunks.end(); ++i)
    {
        delete [] *i;
    }
}

unsigned
TarFile::GetSize() const
{
    return limit;
}

const string
TarFile::GetFileName() const
{
    return string(header.header.arch_name);
}

void
TarFile::Seek(unsigned offset) const
{
    fpos = offset;
}
unsigned
TarFile::Tell() const
{
    return fpos;
}

void
TarFile_SCImp::Write(const char *src, unsigned sz)
{
    // find the chunk corresponding to fpos, adding chunks as needed
    size_t have = chunks.size();
    unsigned need = 1 + (fpos + sz) / 512;
    while (need > have)
    {
        chunks.push_back(reinterpret_cast<char*>(memset(new char[512], 0, 512)));
        ++have;
    }
    limit = mymax(limit, fpos + sz);
    unsigned start_chunk = fpos / 512;
    vector<char*>::iterator write_chunk = chunks.begin() + start_chunk;
    unsigned chunk_offset = fpos % 512;
    fpos += sz;
    while (sz)
    {
        unsigned to_write = mymin(512 - chunk_offset, sz);
        memcpy(chunk_offset + *write_chunk, src, to_write);
        sz -= to_write;
        src += to_write;
        chunk_offset = 0;
        ++write_chunk;
    }
    updateLimit();
    updateCrc();
}
void
TarFile::WriteHeader(const void *src)
{
    memcpy(&header, src, sizeof(header));
    limit = strtol(header.header.size, NULL, 8);
}
void
TarFile::updateCrc()
{
    memcpy(header.header.chksum, CHKBLANKS, 8);
    snprintf(header.header.chksum, sizeof(header.header.chksum), "0%o", callwlate_checksum((char*)&header, sizeof(header)));
}
void
TarFile::updateLimit()
{
    if (snprintf(header.header.size, sizeof(header.header.size), "%011o", limit) + 1 > (int)sizeof(header.header.size))
    {
        Printf(Tee::PriWarn, "File size 0o%o exceeds maximum supported by tar header\n",
               limit);
        snprintf(header.header.size, sizeof(header.header.size), "00");
    }
}
unsigned
TarFile_SCImp::Read(char *dst, unsigned bytes) const
{
    int start_chunk = fpos / CHUNK_SIZE;
    if (start_chunk > int(chunks.size())-1)
        return 0;
    vector<char*>::const_iterator read_chunk = chunks.begin() + start_chunk;
    unsigned chunk_offset = fpos % CHUNK_SIZE;
    while (bytes && (fpos < limit) && (read_chunk != chunks.end()))
    {
        unsigned to_read = mymin(CHUNK_SIZE - chunk_offset, bytes);
        memcpy(dst, chunk_offset + *read_chunk, to_read);
        chunk_offset = 0;
        bytes -= to_read;
        fpos += to_read;
        dst  += to_read;
        ++read_chunk;
    }
    return fpos;
}
size_t
TarFile_SCImp::Read(const char **src) const
{
    MASSERT(src);
    const size_t chunkIndex = static_cast<size_t>(fpos / CHUNK_SIZE);
    if (chunkIndex >= chunks.size())
    {
        *src = nullptr;
        return 0;
    }
    const size_t chunkOffset = static_cast<size_t>(fpos % CHUNK_SIZE);

    *src = chunks[chunkIndex] + chunkOffset;

    const size_t bytesToRead = CHUNK_SIZE - chunkOffset;
    fpos += UNSIGNED_CAST(unsigned, bytesToRead);
    return bytesToRead;
}

void
TarFile::ReadHeader(void *dst) const
{
    memcpy(dst, &header, sizeof(header));
}
unsigned
TarFile::callwlate_checksum(const char *in, size_t size) const
{
    unsigned cs = 0;
    for (unsigned ptr = 0; ptr < size; ++ptr)
        cs += unsigned(in[ptr]);
    return cs;
}
bool
TarFile_SCImp::Gets(char *dst, unsigned bytes)
{
    if (fpos >= limit)
        return false;

    vector<char*>::iterator read_chunk = chunks.begin() + fpos/512;
    unsigned chunk_offset = fpos % 512;
    unsigned read = 0;
    bool done = false;
    while (bytes && (fpos+read)< limit)
    {
        unsigned to_read = mymin(512-chunk_offset, bytes);

        for (unsigned ptr = chunk_offset; ptr < (chunk_offset+to_read); ++ptr)
        {
            char c = (*read_chunk)[ptr];
            ++read;
            if (c != '\n')
                dst[ptr-chunk_offset] = c;
            else
            {
                dst[ptr-chunk_offset] = 0;
                done = true;
                break;
            }
        }
        if (done)
            break;
        chunk_offset = 0;
        bytes -= to_read;
        dst += to_read;
        ++read_chunk;
    }
    fpos += read;
    if (!read && bytes)
        dst[0] = 0; // write a null string
    return (read != 0);
}
void
TarFile::SetFileName(const string &name)
{
    strcpy(header.header.arch_name, name.c_str());
    updateCrc();
}

void
TarArchive::PrintTarHeader(const record &r) const
{
    printf("Dumping Tar Header:\n");
    printf("  arch_name[NAMSIZ] = %s\n", r.header.arch_name);
    printf("  mode[8] = %s\n", r.header.mode);
    printf("  uid[8] = %s\n", r.header.uid);
    printf("  gid[8] = %s\n", r.header.gid);
    printf("  size[12] = %s\n", r.header.size);
    printf("  mtime[12] = %s\n", r.header.mtime);
    printf("  chksum[8] = %s\n", r.header.chksum);
    printf("  linkflag = %d\n", r.header.linkflag);
    printf("  arch_linkname[NAMSIZ] = %s\n", r.header.arch_linkname);
    printf("  magic[8] = %s\n", r.header.magic);
    printf("  uname[TUNMLEN] = %s\n", r.header.uname);
    printf("  gname[TGNMLEN] = %s\n", r.header.gname);
    printf("  devmajor[8] = %s\n", r.header.devmajor);
    printf("  devminor[8] = %s\n", r.header.devminor);
}

unsigned
TarArchive::callwlate_checksum(const char *in, size_t size) const
{
    unsigned cs = 0;
    for (unsigned ptr = 0; ptr < size; ++ptr)
        cs += unsigned(in[ptr]);
    return cs;
}

bool
TarArchive::ReadFromFile(const string &filename, bool bigmem)
{
    gzFile archive = gzopen(filename.c_str(), "rb");

    if (!archive)
        return false;

    unsigned chunks_to_fetch = 0;
    unsigned bytes_to_read = 0;
    record chunk;
    unique_ptr<TarFile> pFile;

    bool headerFound = false;

    while (sizeof(chunk) == gzread(archive, &chunk, sizeof(chunk)))
    {
        if (chunks_to_fetch == 0)
        {
            // Is this a valid header? Check magic and checksum
            if (memcmp(chunk.header.magic, POSIX_MAGIC, MAGICLEN) &&
                memcmp(chunk.header.magic, GNU_MAGIC,   MAGICLEN))
            {
                break;
            }

            unsigned checksum = strtol(chunk.header.chksum, NULL, 8);
            memcpy(chunk.header.chksum, CHKBLANKS, 8);
            if (checksum != callwlate_checksum((const char*)&chunk, sizeof(chunk)))
            {
                break;
            }

            // At least one header with valid magic and checksum has been found
            headerFound = true;

            bytes_to_read = strtol(chunk.header.size, NULL, 8);
            chunks_to_fetch =
                (bytes_to_read / 512) +
                ((bytes_to_read % 512) ? 1 : 0);

            if (bigmem)
            {
                pFile = make_unique<TarFile_BCImp>(bytes_to_read);
            }
            else
            {
                pFile = make_unique<TarFile_SCImp>();
            }
            pFile->WriteHeader(chunk.charptr);

            if (0 == chunks_to_fetch)
            {
                pFile->Seek(0);
                m_Files.push_back(move(pFile));
            }
        }
        else
        {
            unsigned to_write = mymin(unsigned(sizeof(chunk)), bytes_to_read);
            pFile->Write(chunk.charptr, to_write);

            bytes_to_read -= to_write;
            --chunks_to_fetch;
            if (!chunks_to_fetch)
            {
                pFile->Seek(0);
                m_Files.push_back(move(pFile));
            }
        }
    }
    gzclose(archive);
    return headerFound;
}

bool
TarArchive::WriteToFile(const string &filename) const
{
    gzFile archive = gzopen(filename.c_str(), "wb");
    if (!archive)
        return false;
    DEFER
    {
        gzclose(archive);
    };

    static const char zeros[512] = {0};
    for (auto& pFile : m_Files)
    {
        record chunk;
        pFile->ReadHeader(&chunk);
        if (gzwrite(archive, &chunk, sizeof(chunk)) != sizeof(chunk))
        {
            return false;
        }
        pFile->Seek(0);

        int bytes_to_write = pFile->GetSize();
        int zero_padding = bytes_to_write % sizeof(zeros);
        if (zero_padding > 0) zero_padding = sizeof(zeros) - zero_padding;
        while (bytes_to_write > 0)
        {
            int write_count = mymin(bytes_to_write, (int)sizeof(chunk));
            pFile->Read(chunk.charptr, write_count);
            if (gzwrite(archive, chunk.charptr, write_count) != write_count)
            {
                return false;
            }
            bytes_to_write -=  write_count;
        }
        if (zero_padding > 0)
        {
            if (gzwrite(archive, zeros, zero_padding) != zero_padding)
            {
                return false;
            }
        }
    }
    // tar archives end with two empty chunks.
    if (gzwrite(archive, zeros, sizeof(zeros)) != sizeof(zeros) ||
        gzwrite(archive, zeros, sizeof(zeros)) != sizeof(zeros))
    {
        return false;
    }
    return true;
}

void TarArchive::AddToArchive(TarFile* pInFile)
{
    AddToArchive(unique_ptr<TarFile>(pInFile));
}

void TarArchive::AddToArchive(unique_ptr<TarFile> pInFile)
{
    for (auto it = m_Files.begin(); it != m_Files.end(); ++it)
    {
        if ((*it)->GetFileName() == pInFile->GetFileName())
        {
            m_Files.erase(it);
            break;
        }
    }
    m_Files.push_back(move(pInFile));
}

static void
strip_path(const string filename, string &path, string &leaf)
{
    path = filename;
    leaf = filename;
    // look backwards for first file separator if any
    for (size_t ptr = filename.length() - 1; ptr > 0; --ptr)
    {
        switch (filename[ptr])
        {
        case '/':
        case '\\':
            path.erase(ptr, path.length());
            leaf.erase(0, ptr+1);
            return;
        default:
            break;
        }
    }
    path = ".";
    return;
}

TarFile*
TarArchive::Find(const string &filename)
{
    string path;
    string leaf;
    strip_path(filename, path, leaf); // flatten path for now.
    const string dotLeaf = "./" + leaf;

    for (const auto& pTarFile : m_Files)
    {
        const string fileInTar = pTarFile->GetFileName();
        if (fileInTar == leaf || fileInTar == dotLeaf || fileInTar == filename)
        {
            return pTarFile.get();
        }
    }
    return nullptr;
}

const TarFile* TarArchive::Find(const string& filename) const
{
    return const_cast<TarArchive*>(this)->Find(filename);
}

TarFile*
TarArchive::FindPath(const string &path)
{
    for (auto& pFile : m_Files)
    {
        if (pFile->GetFileName() == path)
        {
            return pFile.get();
        }
    }
    return nullptr;
}

void TarArchive::FindFileNames
(
    const string& nameBegin,
    const string& nameEnd,
    vector<string>* pNames
) const
{
    MASSERT(pNames);
    pNames->clear();
    const size_t bsz = nameBegin.size();
    const size_t esz = nameEnd.size();
    for (auto& pFile : m_Files)
    {
        const string& name = pFile->GetFileName();
        const size_t nsz = name.size();
        if (nsz >= bsz + esz &&
            name.substr(0, bsz) == nameBegin &&
            name.substr(nsz-esz) == nameEnd)
        {
            pNames->push_back(name);
        }
    }
}

void TarArchive::FindFileNames
(
    const RegExp& nameRegExp,
    vector<string>* pNames
) const
{
    MASSERT(pNames);
    pNames->clear();
    for (auto& pFile : m_Files)
    {
        if (nameRegExp.Matches(pFile->GetFileName()))
        {
            pNames->push_back(pFile->GetFileName());
        }
    }
}

TarFile_BCImp::TarFile_BCImp(unsigned sz)
{
    m_chunk = new char[sz];
    limit = sz;
}

TarFile_BCImp::~TarFile_BCImp()
{
    if (m_chunk)
    {
        delete [] m_chunk;
        m_chunk = 0;
    }
}

void TarFile_BCImp::Write(const char *src, unsigned bytes)
{
    MASSERT(src);
    if (limit < (fpos+bytes))
    {
        // reallocate the storage, this is costly.
        char* newbuf = new char[limit+bytes];
        if (m_chunk)
        {
            memcpy(newbuf, m_chunk, fpos);
            delete [] m_chunk;
        }
        m_chunk = newbuf;
        limit += bytes;
    }
    memcpy(m_chunk+fpos, src, bytes);
    fpos += bytes;
    updateLimit();
    updateCrc();
}

unsigned TarFile_BCImp::Read(char *dst, unsigned bytes) const
{
    MASSERT(dst);
    if ((fpos >= limit) || (bytes == 0))
    {
        *dst = '\0';
        return 0;
    }

    unsigned bytes_to_read = mymin(bytes, limit-fpos);
    memcpy(dst, m_chunk+fpos, bytes_to_read);
    fpos += bytes_to_read;
    return bytes_to_read;
}

size_t TarFile_BCImp::Read(const char **src) const
{
    MASSERT(src);
    if (fpos >= limit)
    {
        *src = nullptr;
        return 0;
    }

    *src = m_chunk + fpos;

    const size_t bytesToRead = static_cast<size_t>(limit - fpos);
    fpos += UNSIGNED_CAST(unsigned, bytesToRead);
    return bytesToRead;
}

bool TarFile_BCImp::Gets(char *dst, unsigned bytes)
{
    MASSERT(dst);

    if ((fpos >= limit) || (bytes == 0))
    {
        *dst = '\0';
        return false;
    }

    while (bytes-- && (fpos < limit))
    {
        if ((*dst++ = m_chunk[fpos++]) == '\n')
        {
            return true;
        }
    }
    return true;
}

