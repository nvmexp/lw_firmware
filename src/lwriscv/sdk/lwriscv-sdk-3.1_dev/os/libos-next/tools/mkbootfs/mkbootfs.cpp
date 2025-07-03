#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libelf.h"
#include "libbootfs.h"
#include <assert.h>
#include <vector>
#include <string>
#include <cstddef>
#include <fstream>
#include <iostream>

using namespace std;

vector<LwU8> loadFile(const std::string &filename)
{
    std::vector<LwU8> ret;
    std::ifstream f(filename, ios::binary);
    if (!f.is_open())
        throw runtime_error(strerror(errno) + (": " + filename));
    copy(istreambuf_iterator<char>(f), istreambuf_iterator<char>(), back_inserter(ret));
    return ret;
}

void saveFile(const std::string &filename, const std::vector<LwU8> &content)
{
    std::ofstream f(filename, ios::binary);
    if (!f.is_open())
        throw runtime_error(strerror(errno) + (": " + filename));
    copy(content.begin(), content.end(), ostreambuf_iterator<char>(f));
}

struct ArchiveWriter
{
    std::vector<LwU8> headers;
    std::vector<LwU8> data;
    std::vector<LwU64> dataRelocations;

    LibosBootfsRecord * getRecord(LwU64 recordOffset)
    {
        return (LibosBootfsRecord *)&headers[recordOffset];
    }

    LibosBootfsExelwtableMapping * getMapping(LwU64 mappingId)
    {
        return (LibosBootfsExelwtableMapping *)&headers[mappingId];
    }

    LwU64 lwrrentRecord = -1;
    LwU64 lastMapping = 0;

    ArchiveWriter()
    {
        headers.resize(sizeof(LibosBootFsArchive));
    }

    void newFile(std::string name)
    {
        // Allocate the record
        LwU64 recordOffset = headers.size();
        headers.resize(recordOffset + sizeof(LibosBootfsRecord), 0);

        // Allocate space for the filename
        LwU64 filename = headers.size();
        LwU64 filenameLength = name.size() + 1;
        filenameLength = (filenameLength + 7) &~ 7;
        headers.resize(filename + filenameLength, 0);
        
        // Fill in the filename
        LibosBootfsRecord * record = getRecord(recordOffset);
        record->filename = filename;
        strcpy((char *)&headers[filename], name.c_str());

        // Link the record in
        if (lwrrentRecord != -1) {
            LibosBootfsRecord * previousRecord = getRecord(lwrrentRecord);
            previousRecord->next = recordOffset;
        }

        printf("File: %s\n", name.c_str());
        lwrrentRecord = recordOffset;
        lastMapping = 0;        
    }

    void newMapping(LwU64 va, LwU64 memsz, LwU64 attributes, LwU64 alignment, LwU64 offset, void * buf, LwU64 length, LwU64 fileOffset)
    {
        printf("\t Mapping %08llx size:%08llx attr:%02llx align:%04llx padding:%04llx buffer-length:%08llx\n", 
            va, memsz, attributes, alignment, offset, length);

        // Append space for the mapping
        LwU64 mappingId = headers.size();
        headers.resize(headers.size() + sizeof(LibosBootfsExelwtableMapping), 0);

        // Get point of the current record
        LibosBootfsRecord * record = getRecord(lwrrentRecord);

        // Align the data section
        data.resize((data.size() + alignment-1) &~ (alignment - 1));

        // Align the section size
        memsz = (memsz + alignment - 1) &~ (alignment - 1);

        // Fill out the new mapping
        LibosBootfsExelwtableMapping * mapping = getMapping(mappingId);
        mapping->virtualAddress = va;
        mapping->offset = data.size();
        dataRelocations.push_back(mappingId + offsetof(LibosBootfsExelwtableMapping, offset));
        mapping->length = memsz;
        mapping->attributes = attributes;
        mapping->alignment = alignment;
        mapping->originalElfOffset = fileOffset - offset;   // effective offset from the start of the VA

        // Append the data      
        data.resize(data.size() + memsz);
        memcpy(&data[0] + mapping->offset + offset, buf, length);

        // Link the mapping in
        if (!lastMapping)
            record->offsetFirstMapping = mappingId;
        else
            getMapping(lastMapping)->next = mappingId;

        lastMapping = mappingId;
    }

    void save(std::string name)
    {
        // Page align the headers
        headers.resize((headers.size() + 4095) &~ 4095, 0);

        // Fill out the archive headers
        auto archive = (LibosBootFsArchive *)&headers[0];
        archive->magic      = LIBOS_BOOT_FS_MAGIC;
        archive->headerSize = sizeof(LibosBootFsArchive);
        archive->totalSize  = headers.size() + data.size();

        for (auto relo : dataRelocations) 
            *(LwU64 *)(&headers[relo]) += headers.size();
        
        std::vector<LwU8> result;
        result.insert(result.end(), headers.begin(), headers.end());
        result.insert(result.end(), data.begin(), data.end());


        saveFile(name, result);    
    }
};


int main(int argc, const char ** argv)
{
    if (argc != 3 || argv[1][0] != '@') {
        fprintf(stderr, "ramfs @filelist output.fs\n");
        exit(1);
    }

    fstream list;
    list.open(argv[1]+1 /* skip @ */,ios::in);

    std::string line;    
    ArchiveWriter writer;
    while (std::getline(list, line)) {
        // Load the file
        auto raw = loadFile(line);

        // Create the file record
        writer.newFile(line);

        // Ensure the ELF headers are mapped in their own mapping
        // (we'll need this for later)
        LibosElfImage image;
        LibosElfImageConstruct(&image, &raw[0], raw.size());

        // Is it an elf?
        if (line.size() >= 4 && line.substr(line.size()-4, 4) == ".elf")
        {
            LibosElf64ProgramHeader * phdr = 0;
            LwU64 previousEnd = 0;
            while (phdr = LibosElfProgramHeaderNext(&image, phdr))
            {
                // Align the section
                if (phdr->type == PT_LOAD)
                {
                    LwU64 align = phdr->align;
                    // PT_LOAD sections should be at least 4kb aligned
                    if (align < 4096)
                        align = 4096;

                    // Is the VA aligned? If not we'll need to left pad the data in this mapping
                    LwU64 vaOffset = phdr->vaddr & (align - 1);

                    writer.newMapping(phdr->vaddr - vaOffset, 
                                      phdr->memsz + vaOffset, 
                                      phdr->flags, 
                                      align, 
                                      vaOffset, 
                                      &raw[phdr->offset],
                                      phdr->filesz,
                                      phdr->offset);

                    previousEnd = phdr->offset + phdr->filesz;
                }
            }
        }
        else
        {
            writer.newMapping(0, 
                              raw.size(), 
                              0, 
                              1, 
                              0, 
                              &raw[0],
                              raw.size(),
                              0);            
        }
    }

    // Write the file
    writer.save(std::string(argv[2]));

    return 0;
}