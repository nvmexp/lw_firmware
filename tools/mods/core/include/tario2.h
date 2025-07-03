#ifndef INCLUDE_TARIO2_H_GUARD
#define INCLUDE_TARIO2_H_GUARD

#include "zlib.h"

/* Forward declare */
struct TarHeader2;
class TapeArchive2;

/* A file inside of a tar */
class TarFile2 {
    class TarFileImpl2 *impl;

    friend class TapeArchive2;
    TarFile2(TapeArchive2 *parent, const TarHeader2 &);

public:
    ~TarFile2();

    Z_SIZE_T fwrite(const void *buffer, Z_SIZE_T size);
    Z_SIZE_T fread(void *buffer, Z_SIZE_T size);
    Z_SIZE_T fseek(Z_SIZE_T offset);
    Z_SIZE_T get_size();
    int    fflush();

    void erase();
    void rename(const std::string &name);

    bool is_opened_for_reading();
};

/* The tar file itself */
class TapeArchive2 {
    class TarImpl2 *impl;

private:
    friend class TarFile2;
    Z_SIZE_T fwrite(const void *buf, Z_SIZE_T size);
    Z_SIZE_T fread(       void *buf, Z_SIZE_T size, Z_SIZE_T from);
    Z_SIZE_T fseek(Z_SIZE_T offset, int from);
    Z_SIZE_T write_header(const TarHeader2 &);
    Z_SIZE_T read_header (TarHeader2 &);

public:
    TapeArchive2(const char *filename, const char *mode); //mode = [r|w][z] (compressed)
    ~TapeArchive2();

    const char *findfirst(const char *pattern);
    const char *findnext();
    bool exists(const char *filename);

    TarFile2 *fopen(const char *subfile, const char *mode, Z_SIZE_T file_size = ~0); //mode = [r|w][m] (in-memory)
    int fclose(TarFile2 *file);
};

/*
TapeArchive2 *tar = new TapeArchive2("simple_quad.tgz", "wz");
TarFile2 *tex = tar->fopen("texture0.tex", "ws");
tex->fwrite(buf, 4, 1);
delete tex;
delete tar;
*/

#endif /* INCLUDE_TARIO2_H_GUARD */
