#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "libelf.h"
#include "lwtypes.h"

using namespace std;

vector<LwU8> loadFile(const string &filename)
{
    vector<LwU8> ret;
    ifstream f(filename, ios::binary);
    if (!f.is_open())
        throw runtime_error(strerror(errno) + (": " + filename));
    copy(istreambuf_iterator<char>(f), istreambuf_iterator<char>(), back_inserter(ret));
    return ret;
}

void saveFile(const string &filename, const vector<LwU8> &content)
{
    ofstream f(filename, ios::binary);
    if (!f.is_open())
        throw runtime_error(strerror(errno) + (": " + filename));
    copy(content.begin(), content.end(), ostreambuf_iterator<char>(f));
}

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 3)
        {
            fprintf(stderr, "usage: %s <input ELF> <output prefix>\n", argv[0]);
            return EXIT_FAILURE;
        }

        string inputELF = argv[1];
        string outputPrefix = argv[2];
        vector<LwU8> elf = loadFile(inputELF);

        if (elf.size() < sizeof(LibosElf64Header) ||
            elf[0] != 0x7F || elf[1] != 'E' || elf[2] != 'L' || elf[3] != 'F')
            throw runtime_error("The input file is not a valid ELF file");

        bool textDone = false;
        bool dataDone = false;
        LibosElf64Header *elfHdr = reinterpret_cast<LibosElf64Header *>(&elf[0]);
        LibosElf64ProgramHeader *phdr = nullptr;
        while ((phdr = LibosElfProgramHeaderNext(elfHdr, phdr)) != nullptr)
        {
            if (phdr->type == PT_LOAD)
            {
                string filename = outputPrefix;
                if (phdr->flags & PF_X)
                {
                    if (textDone)
                        throw runtime_error("Multiple text segments found");
                    textDone = true;
                    filename += ".text.bin";
                }
                else
                {
                    if (dataDone)
                        throw runtime_error("Multiple data segments found");
                    dataDone = true;
                    filename += ".data.bin";
                }
                vector<LwU8> segment = vector<LwU8>(
                    elf.begin() + phdr->offset,
                    elf.begin() + phdr->offset + phdr->filesz
                );
                saveFile(filename, segment);
            }
        }

        return 0;
    }
    catch (const exception &e)
    {
        fprintf(stderr, "fatal: %s\n", e.what());
        return EXIT_FAILURE;
    }
}
