/* TAR file I/O library, for reading/writing tar files.
 *
 * This library is a significant improvement over existing
 * libraries used at LW in the following ways:
 *  - Multiple simultanious tar files opened and read/written at the same time.
 *  - Support for multiple simultanious read/write of sub-files inside any tar
 *    file.
 *  - No buffering of the entire data set.
 *  - Support for streaming data in and out.
 *
 * This allows us to (for example) buffer up files that are written piece-wise,
 * such as the pushbuffer, but also stream out files at the same time, like
 * textures.
 *
 * Limitations:
 *  - Only one subfile can be opened for streaming at one time, per
 *    tar file.
 *  - Sparse files are NOT supported.
 *  - UID settings are NOT supported.
 */

#include <algorithm>
#include <vector>
#include <string>
#include <deque>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#ifndef SIZE_MAX
#ifdef _WIN64
#define SIZE_MAX _UI64_MAX
#else
#define SIZE_MAX UINT_MAX
#endif
#endif

#define TARIO_INTERNAL_CODE  /* Tell tario2.h how to set-up */
#include "core/include/tario2.h"
#include "core/include/types.h"
#include "core/include/massert.h"
#include "core/include/tee.h"

#define STREAM_BUFFER_SIZE       0x10000
#define STREAM_BUFFER_MEDIUM_SIZE 0x1000
#define STREAM_BUFFER_SMALL_SIZE   0x100

#ifdef _MSC_VER /* MSVC is yet to be standard compliant... */
#   define snprintf _snprintf
#endif

typedef unsigned char ubyte;

/* This represents the data in the header of a tar file block */
struct TarHeader2 {

    struct {
        unsigned read   : 1;
        unsigned stream : 1;
    } mode;

    std::string filename;

    Z_SIZE_T offset_of_file_data_in_tar;
    Z_SIZE_T size;
    Z_SIZE_T modification_time;
    Z_SIZE_T access_time;
    Z_SIZE_T create_time;

    TarHeader2()
        : offset_of_file_data_in_tar(0),
          size(0),
          modification_time(0),
          access_time(0),
          create_time(0) {
        this->mode.read   = 0;
        this->mode.stream = 0;
    }
};

/* Tar sub-file implementation details */
class TarFileImpl2 {

    friend class TarFile2;
    friend class TapeArchive2;

    std::vector<ubyte> buffer;
    Z_SIZE_T lwrrent_position_in_file;
    Z_SIZE_T lwrrent_position_in_buffer;

    TapeArchive2 *tar;
    TarHeader2    header;

    bool opened;
    bool erased;

    TarFileImpl2(bool stream_write)
        : buffer(stream_write ? STREAM_BUFFER_SIZE : 0),
          lwrrent_position_in_file(0),
          lwrrent_position_in_buffer(0),
          tar(NULL), opened(false), erased(false) {
    };
};

TarFile2::TarFile2(TapeArchive2 *parent, const TarHeader2 &header) {

    /* Parameters were validated by TapeArchive2::fopen() */
    this->impl = new TarFileImpl2(header.mode.stream && !header.mode.read);
    this->impl->tar                        = parent;
    this->impl->lwrrent_position_in_buffer = 0;
    this->impl->lwrrent_position_in_file   = header.offset_of_file_data_in_tar;
    this->impl->header                     = header;
}

TarFile2::~TarFile2() {
    /* If the file is opened, then close it. fclose() will get our
     * data flushed, so we don't need to do that here.
     */
    if (this->impl && this->impl->opened) {
        this->impl->tar->fclose(this);
    }
    delete this->impl;
}

Z_SIZE_T TarFile2::get_size() {

    /* Parent archive was already closed / deleted. Nothing more to do */
    if (!this->impl->tar) {
        return 0;
    }

    if (this->impl->header.mode.stream) {
        return this->impl->lwrrent_position_in_buffer
             + this->impl->lwrrent_position_in_file
             - this->impl->header.offset_of_file_data_in_tar;
    }
    return this->impl->header.size;
}

/* Writes a block of data to the file.
 * If the file is opened for streaming, the data will be written to physical
 * disk. Otherwise, the data is aclwmulated in memory until the file gets
 * closed.
 */
Z_SIZE_T TarFile2::fwrite(const void *buffer, Z_SIZE_T size) {

    /* Parent archive was already closed / deleted. Nothing more to do */
    if (!this->impl->tar) {
        return 0;
    }

    const Z_SIZE_T orig_size = size;

    Printf(Tee::PriDebug, "Tar: fwrite(filename = %s, buffer = %p, size = %llu), "
            "read: %i, stream: %i\n",
            this->impl->header.filename.c_str(), buffer, (Z_SIZE_T)size,
            this->impl->header.mode.read, this->impl->header.mode.stream);

    if (this->impl->header.mode.read || !this->impl->opened) {
        // We can't write to a file opened for reading, or not opened at all
        return 0;
    }

    /* If we're not streaming out the data, grow our buffer */
    if (!this->impl->header.mode.stream) {
        if (this->impl->lwrrent_position_in_buffer + size < STREAM_BUFFER_SMALL_SIZE) {
            this->impl->buffer.resize(STREAM_BUFFER_SMALL_SIZE);
        }
        else if (this->impl->lwrrent_position_in_buffer + size < STREAM_BUFFER_MEDIUM_SIZE) {
            this->impl->buffer.resize(STREAM_BUFFER_MEDIUM_SIZE);
        }
        else {
            this->impl->buffer.resize(max(Z_SIZE_T(STREAM_BUFFER_SIZE),
                                      this->impl->lwrrent_position_in_buffer + size));
        }
    }
    else {
        size = min(size, this->impl->lwrrent_position_in_buffer
                            - this->impl->header.size);
    }

    Z_SIZE_T pos = 0;
    while (size > 0) {
        /* Fill up our buffer */
        const Z_SIZE_T lwr_pos = this->impl->lwrrent_position_in_buffer;
        Z_SIZE_T copied = min(size, this->impl->buffer.size() - lwr_pos);

        if (!copied) { /* Buffer full? Bail */
            return orig_size - size;
        }

        memcpy(&this->impl->buffer[lwr_pos], ((const char*)buffer) + pos, copied);

        pos += copied;
        this->impl->lwrrent_position_in_buffer += copied;

        /* If the buffer is full and we're streaming, flush it */
        if (this->impl->header.mode.stream
         && lwr_pos + copied == this->impl->buffer.size()) {
            int ret = this->fflush();
            if (ret != 0) {
                return orig_size - size;
            }
        }

        size -= copied;
    }

    /* If we streamed out all the data we needed to stream out, close
     * the file to release its resources and allow other files to be streamed
     * out, even if the user hasn't explicitly closed this file.
     */
    if (this->impl->header.mode.stream
     && this->impl->lwrrent_position_in_buffer == this->impl->header.size) {
        this->impl->tar->fclose(this);
    }

    return orig_size;
}

/* Reads a block of data from the file.
 */
Z_SIZE_T TarFile2::fread(void *buffer, Z_SIZE_T size) {

    /* Parent archive was already closed / deleted. Nothing more to do */
    if (!this->impl->tar) {
        return 0;
    }

    Printf(Tee::PriDebug, "Tar: fread(filename = %s, buffer = %p, size = %llu), "
            "read: %i, stream: %i\n",
            this->impl->header.filename.c_str(), buffer, (Z_SIZE_T)size,
            this->impl->header.mode.read, this->impl->header.mode.stream);

    if (!this->impl->header.mode.read) {
        return 0;  // We can't read from a file opened for writing
    }

    Z_SIZE_T  ret = this->impl->tar->fread(buffer, size, this->impl->lwrrent_position_in_file);
    this->impl->lwrrent_position_in_file += ret;

    return ret;
}

/* Reads a block of data from the file.
 */
Z_SIZE_T TarFile2::fseek(Z_SIZE_T offset) {

    /* Parent archive was already closed / deleted. Nothing more to do */
    if (!this->impl->tar) {
        return 0;
    }

    Printf(Tee::PriDebug, "Tar: fseek(filename = %s, offset = %llu), "
            "read: %i, stream: %i\n",
            this->impl->header.filename.c_str(), (Z_SIZE_T)offset,
            this->impl->header.mode.read, this->impl->header.mode.stream);

    if (!this->impl->header.mode.read) {
        return 0;  // We can't read from a file opened for writing
    }

    /* Fast case - we're not actually seeking */
    if (offset == this->impl->lwrrent_position_in_file
                - this->impl->header.offset_of_file_data_in_tar) {
        return offset;
    }
    /* Do a normal seek */
    Z_SIZE_T ret = this->impl->tar->fseek(
                         this->impl->header.offset_of_file_data_in_tar + offset,
                         SEEK_SET);
    this->impl->lwrrent_position_in_file = this->impl->header.offset_of_file_data_in_tar + offset;
    return ret;
}

/* Flush out the current buffer for this file, if the file is being
 * streamed out.
 *
 * If the file is being read, or if it's being written in-memory,
 * then don't do anything.
 *
 * Returns 0 on success, -1 on error.
 */
int TarFile2::fflush() {

    /* Parent archive was already closed / deleted. Nothing more to do */
    if (!this->impl->tar) {
        return 0;
    }

    Printf(Tee::PriDebug, "Tar: fflush(filename = %s, size = %llu), "
            "read: %i, stream: %i\n",
            this->impl->header.filename.c_str(),
            (Z_SIZE_T)this->impl->lwrrent_position_in_buffer,
            this->impl->header.mode.read, this->impl->header.mode.stream);

    if (!this->impl->tar) {
        return -1;
    }

    /* If we're stream-writting, write out the buffer.
     * Otherwise, don't do anything.
     */
    if (!this->impl->header.mode.read
     && this->impl->header.mode.stream) {
        const Z_SIZE_T size = this->impl->lwrrent_position_in_buffer;
        Z_SIZE_T ret = this->impl->tar->fwrite(&this->impl->buffer[0], size);
        this->impl->lwrrent_position_in_file += ret;
        this->impl->lwrrent_position_in_buffer -= ret;

        Printf(Tee::PriDebug, "Tar: Wrote %llu bytes\n", (Z_SIZE_T)ret);

        if (ret != size) {
            return -1;
        }
    }
    return 0;
}

bool TarFile2::is_opened_for_reading() {
    return this->impl->header.mode.read;
}

void TarFile2::rename(const std::string &name) {

    /* Parent archive was already closed / deleted. Nothing more to do */
    if (!this->impl->tar) {
        return;
    }

    if (this->impl->header.mode.stream
     || this->impl->header.mode.read) {
        MASSERT(!"Cannot rename file");
    }
    this->impl->header.filename = name;
}

void TarFile2::erase() {

    /* Parent archive was already closed / deleted. Nothing more to do */
    if (!this->impl->tar) {
        return;
    }

    if (this->impl->header.mode.stream
     || this->impl->header.mode.read) {
        MASSERT(!"Cannot erase file");
    }

    this->impl->erased = true;
    if (this->impl->opened) {
        this->impl->tar->fclose(this);
    }
    delete this->impl;

    this->impl = NULL;
}

/* Tape archive implementation details */
class TarImpl2 {
    friend class TapeArchive2;

    /* Files that were built up in memory, but then closed
     * before the current stream-ed file was itself closed.
     * We couldn't write out their data then (since another file
     * was opened), so we store it here, waiting for an opportunity.
     * This vector must be NULL if the file was opened for reading.
     */
    std::vector<TarFile2*> pending_files;

    /* List of current opened files in the tar */
    std::deque<TarFile2*> opened_files;

    /* List of files in the tar */
    std::vector<TarHeader2> files;

    /* Name of the tar file */
    std::string filename;

    struct {
        unsigned read       : 1;
        unsigned compressed : 1;
    } mode;

    union {
        FILE   *libc_file;
        gzFile  gzip_file;
    };

    Z_SIZE_T lwr_pos_in_file;

    /* Pattern of the current findfirst/findnext */
    std::string find_pattern;

    /* Position of the current findfirst/findnext */
    Z_SIZE_T lwr_find_pos_in_files;

    TarImpl2()
        : lwr_pos_in_file(0), lwr_find_pos_in_files(0) {
        this->mode.read       = 0;
        this->mode.compressed = 0;
        this->gzip_file = NULL;
        this->libc_file = NULL;
    }
};

/* Opens up a tar file for reading or writing.
 * 'filename' is the name of the tar file (which may include paths).
 * 'mode' contains the open mode, which is a string representing
 * how the file is to be treated.
 *    'r' - Read-only.
 *    'w' - Write-only.
 *    'z' - Compressed.
 *
 * Files are always opened in binary mode. "rz" is redundent, since
 * the compression enable is inferred automatically when opening
 * a tar for reading.
 */
TapeArchive2::TapeArchive2(const char *filename, const char *mode) {

    /* Parse the mode bits */
    bool read       = false;
    bool write      = false;
    bool compressed = false;

    Printf(Tee::PriDebug, "Tar: fopen(filename = %s, mode = %s)\n", filename, mode);

    for (Z_SIZE_T i = 0; mode[i]; i++) {
        switch (mode[i]) {
        case 'r':
            if (write) {
                MASSERT(!"Can't open a tar file for both reading and writing.");
            }
            if (read) {
                MASSERT(!"Error in mode specification: Two 'r'?");
            }
            read = true;
            break;
        case 'w':
            if (write) {
                MASSERT(!"Error in mode specification: Two 'w'?");
            }
            if (read) {
                MASSERT(!"Can't open a tar file for both reading and writing.");
            }
            write = true;
            break;
        case 'z':
            if (compressed) {
                MASSERT(!"Error in mode specification: Two 'z'?");
            }
            compressed = true;
            break;
        case 'b':
            /* Ignore */
            break;
        default:
            MASSERT(!"Invalid mode.");
        }
    }
    if (!read && !write) {
        write = true;
    }
    assert(read != write);

    Printf(Tee::PriDebug, "Tar: read: %i, write: %i, compressed: %i\n", read, write, compressed);

    this->impl = new TarImpl2();

    this->impl->mode.read       = read;
    this->impl->mode.compressed = compressed;
    this->impl->filename        = filename;

    /* Try to open the file */
    if (compressed || read) {
        this->impl->gzip_file = gzopen(filename, (read ? "rb" : "wb"));
        this->impl->mode.compressed = 1;
        if (!this->impl->gzip_file && compressed) {
            delete this->impl;
            MASSERT(!"Unable to open tar file.");
        }
    }
    if (!this->impl->gzip_file) {
        this->impl->libc_file = ::fopen(filename, (read ? "rb" : "wb"));
        if (!this->impl->libc_file) {
            delete this->impl;
            MASSERT(!"Unable to open tar file.");
        }
        this->impl->mode.compressed = 0;
    }

    this->impl->lwr_pos_in_file = 0;

    /* If we opened the file for reading, find out where all the files
     * offsets are.
     */
    if (read) {
        Z_SIZE_T ret;
        do {
            TarHeader2 header;

            ret = this->read_header(header);
            // If header is not 512 means this is a broken header or a tgz file end.
            if (ret != 512) {
                break;
            }
            header.mode.read = 1;

            Z_SIZE_T pos = this->fseek((header.size + 511) & -512, SEEK_LWR);
            if (ret == 512 && pos == 0 && header.size != 0) {
                delete this->impl;
                MASSERT(!"Unable to open tar file.");
            }

            this->impl->files.push_back(header);

        } while(1);
        this->fseek(0, SEEK_SET);
    }
}

TapeArchive2::~TapeArchive2() {
    /* Ok, lots to do here. First, we need to close the file lwrrently
     * opened for streaming. This will have the side-effect of flushing
     * all pending files.
     *
     * Then, fclose all other opened files.
     */

    Printf(Tee::PriDebug, "Tar: Closing tar file!\n");

    std::deque<TarFile2*> old_opened_files = this->impl->opened_files;

    /* First, find and close the current file opened for streaming */
    std::deque<TarFile2*>::iterator i;
    bool done;
    do {
        done = true;
        for (i  = this->impl->opened_files.begin();
             i != this->impl->opened_files.end(); i++) {
            if ((*i)->impl->header.mode.stream) {
                this->fclose(*i);
                done = false;
                break;
            }
        }
    } while (!done);

    /* Then, fclose all opened files */
    while (this->impl->opened_files.begin() != this->impl->opened_files.end()) {
        assert(!(*this->impl->opened_files.begin())->impl->header.mode.stream);
        this->fclose(*this->impl->opened_files.begin());
    }

    /* Then, remove references to the tape archive from the remainng
     * subfile objects.
     */
    for (i = old_opened_files.begin(); i != old_opened_files.end(); ++i) {
        (*i)->impl->tar = NULL;
    }

    assert(this->impl->pending_files.size() == 0);

    if (!this->impl->mode.read) {
        unsigned char zeros[512];
        std::fill(zeros, zeros + sizeof(zeros), 0);

        /* Align on 512 byte boundary */
        this->fwrite(zeros, sizeof(zeros) - (this->impl->lwr_pos_in_file & 511));

        /* Write 2 null blocks */
        this->fwrite(zeros, sizeof(zeros));
        this->fwrite(zeros, sizeof(zeros));
    }

    /* Finally, close the tar */
    if (this->impl->mode.compressed) {
        gzclose(this->impl->gzip_file);
    }
    else {
        ::fclose(this->impl->libc_file);
    }
}

/* Write some unformatted bytes to the tar */
Z_SIZE_T TapeArchive2::fwrite(const void *buf, Z_SIZE_T size) {

    Printf(Tee::PriDebug, "Tar: fwrite(buffer = %p, size = %llu) - read: %i gzip: %i\n",
            buf, (Z_SIZE_T)size, this->impl->mode.read, this->impl->mode.compressed);

    if (this->impl->mode.read) {
        return 0;
    }
    if (this->impl->mode.compressed) {
        int ret = gzwrite(this->impl->gzip_file, const_cast<const voidp>(buf), (unsigned int)size);
        Printf(Tee::PriDebug, "Tar: gzwrite returned: %i\n", ret);
        if (ret != (int)size && (ret != 0)) {
            int errnum;
            const char *err = gzerror(this->impl->gzip_file, &errnum);
            Printf(Tee::PriError, "Tar: Requested size 0x%x does not match 0x%x while writing gzip file: %u, %s\n", (int)size, ret, errnum, err);
            if (ret < 0) {
                Printf(Tee::PriError, "Tar: Error returned from gzwrite: %u, %s\n", errnum, err);
                return 0;
            }
        }
        this->impl->lwr_pos_in_file += ret;
        return Z_SIZE_T(ret);
    }
    else {
        Z_SIZE_T ret = ::fwrite(buf, 1, size, this->impl->libc_file);
        Printf(Tee::PriDebug, "Tar: fwrite returned: %llu\n", (Z_SIZE_T)ret);
        this->impl->lwr_pos_in_file += ret;
        return ret;
    }
}

/* Readsome unformatted bytes from the tar */
Z_SIZE_T TapeArchive2::fread(void *buf, Z_SIZE_T size, Z_SIZE_T from) {
    if (!this->impl->mode.read) {
        Printf(Tee::PriError, "Tar: File not open in reading mode\n");
        return 0;
    }
    if (this->impl->lwr_pos_in_file != from) {
        this->fseek(from, SEEK_SET);
    }
    if (this->impl->mode.compressed) {
        int ret = gzread(this->impl->gzip_file, buf, (unsigned int)size);

        if (ret != (int)size && (ret != 0 || !gzeof(this->impl->gzip_file))) {
            int errnum;
            const char *err = gzerror(this->impl->gzip_file, &errnum);
            Printf(Tee::PriError, "Tar: Requested size 0x%x does not match 0x%x while reading gzip file: %u, %s\n", (int)size, ret, errnum, err);
            if (ret < 0) {
                Printf(Tee::PriError, "Tar: Error returned from gzread: %u, %s\n", errnum, err);
                return 0;
            }
        }
        this->impl->lwr_pos_in_file += Z_SIZE_T(ret);
        return Z_SIZE_T(ret);
    }
    else {
        Z_SIZE_T ret = Z_SIZE_T(::fread(buf, 1, size, this->impl->libc_file));
        this->impl->lwr_pos_in_file += Z_SIZE_T(ret);
        return ret;
    }
}

Z_SIZE_T TapeArchive2::fseek(Z_SIZE_T offset, int from) {
    if (!this->impl->mode.read) {
        MASSERT(!"Cannot seek in writable file.");
    }
    if (this->impl->mode.compressed) {
        /* This is a workaround for some broken tgz files that are
         * confusing zlib and causes it to corrupt its internal
         * state.
         */
        INT64 ret;
        if (from == SEEK_SET && offset == 0 && this->impl->mode.read) {
            gzclose(this->impl->gzip_file);
            this->impl->gzip_file = gzopen(this->impl->filename.c_str(), "rb");
        }
//#if defined(_LARGEFILE64_SOURCE) && defined(_LFS64_LARGEFILE) && _LFS64_LARGEFILE-0
        ret = gzseek64(this->impl->gzip_file, INT64(offset), from);
//#else
//        ret = gzseek(this->impl->gzip_file, long(offset), from);
//#endif
        if (ret < 0 && !gzeof(this->impl->gzip_file)) {
            return 0;
        }
    }
    else {
        int ret = ::fseek(this->impl->libc_file, (unsigned long)offset, from);
        if (ret != 0) {
            return 0;
        }
    }
    switch (from) {
    case SEEK_SET: this->impl->lwr_pos_in_file  = offset; break;
    case SEEK_LWR: this->impl->lwr_pos_in_file += offset; break;
    default: MASSERT(!"Unsupported seek mode.");
    }
    return offset;
}

/* Open a file inside the tar. 'subfile' contains the path to the file
 * inside the tar. 'mode' tells how the file should be opened.
 *   'r' - Read-only.
 *   'w' - Write-only.
 *   'm' - Force in-memory buffering (no streaming).
 *
 * Default access is write-only streaming.
 *
 * File_size is an optional parameter telling tario how bug the subfile
 * will be. This parameter is ignored if the file is opened for reading.
 * This parameter optionally limits the size of memory buffered files.
 * This parameter is required for streaming writes of file data. If this
 * parameter is not supplied for streaming writes, then buffered writes
 * are used instead.
 *
 * NULL is returned on failure. Otherwise, a valid TarFile2* is returned.
 */
TarFile2 *TapeArchive2::fopen(const char *subfile, const char *mode, Z_SIZE_T file_size) {

    /* Parse the mode bits */
    bool read      = false;
    bool write     = false;
    bool in_memory = false;

    Printf(Tee::PriDebug, "Tar: fopen(subfile = %s, mode = %s, size = 0x%llx)\n", subfile, mode, (Z_SIZE_T)file_size);

    for (Z_SIZE_T i = 0; mode[i]; i++) {
        switch (mode[i]) {
        case 'r':
            if (write) {
                return NULL;
            }
            if (read) {
                return NULL;
            }
            read = true;
            break;
        case 'w':
            if (write) {
                return NULL;
            }
            if (read) {
                return NULL;
            }
            write = true;
            break;
        case 'm':
            if (in_memory) {
                return NULL;
            }
            in_memory = true;
            break;
        case 'b':
            /* Ignore */
            break;
        default:
            return NULL;
        }
    }
    if (!read && !write) {
        write = true;
    }
    assert(read != write);

    Printf(Tee::PriDebug, "Tar: read: %i, write: %i, in_memory: %i\n", read, write, in_memory);

    /* Can't open a subfile in a different mode than the parent */
    if ((read && !this->impl->mode.read)
     || (write && this->impl->mode.read)) {
        return NULL;
    }

    TarHeader2 header;

    /* Strip the directory name (which we don't support) */
    std::string subfilename(subfile);
    std::string::size_type pos = subfilename.find_last_of("/\\");
    if (pos != subfilename.npos) {
        subfilename.erase(0, pos + 1);
    }

    Printf(Tee::PriDebug, "Tar: subfile: %s\n", subfilename.c_str());

    /* If we're opened for reading, look for the file in our database */
    if (read) {
        bool found = false;
        std::vector<TarHeader2>::iterator i;
        for (i = this->impl->files.begin(); i != this->impl->files.end(); i++) {
            if (i->filename == subfilename) {
                header = *i;
                found = true;
                break;
            }
        }
        /* File not found? */
        if (!found) {
            Printf(Tee::PriWarn, "Tar: subfile not found.\n");
            return NULL;
        }
    }
    else {
        /* Create the tar subfile */
        header.access_time       = 0x80000000;
        header.create_time       = 0x80000000;
        header.modification_time = 0x80000000;
        header.filename          = subfilename;
        header.size              = file_size;
        header.mode.read         = read;

        bool already_streaming = false;

        std::deque<TarFile2*>::iterator i;
        for (i  = this->impl->opened_files.begin();
             i != this->impl->opened_files.end(); i++) {
            if ((*i)->impl->opened && (*i)->impl->header.mode.stream) {
                already_streaming = true;
                break;
            }
        }

        header.mode.stream = !(in_memory || already_streaming
                            || file_size == ~0u);

        /* If the file was opened for write streaming, create it in the tar */
        if (write && header.mode.stream) {
            Printf(Tee::PriDebug, "Tar: File opened for streaming!\n");
            Z_SIZE_T ret = this->write_header(header);
            if (ret != 512) {
                return NULL;
            }
        }
        header.offset_of_file_data_in_tar = this->impl->lwr_pos_in_file;
    }

    TarFile2 *file = NULL;

    file = new TarFile2(this, header);
    if (!file) {
        return NULL;
    }

    file->impl->opened = true;

    /* Register this file as being opened */
    this->impl->opened_files.push_back(file);

    return file;
}

int TapeArchive2::fclose(TarFile2 *file) {

    /* Parent archive was already closed / deleted. Nothing more to do */
    if (!file->impl->tar) {
        return 0;
    }

    Printf(Tee::PriDebug, "Tar: fclose(file = %s);\n", file->impl->header.filename.c_str());

    /* Ok, this is a little complicated.
     *
     * If this file is opened for streaming, then we can just flush it out.
     *
     * Otherwise, if any files are opened for streaming, then we need to
     * copy the contents of our file into the "pending file" queue, to
     * be written later, once the streaming file is closed.
     *
     * Otherwise, if the file is not streaming, and there are no streaming
     * files opened, then write the file to disk.
     *
     * Finally, if no files are opened for streaming after us, then
     * flush out all pending files.
     */
    bool flush_pending = false;
    std::deque<TarFile2*> &open = this->impl->opened_files;

    /* If the file is opened for streaming, flush it out */
    int ret = 0;
    if (file->impl->opened) {
        if (file->impl->header.mode.stream) {
            Printf(Tee::PriDebug, "Tar: Flushing file data.\n");
            ret = file->fflush();
            flush_pending = true;
        }
        else if (!file->impl->erased) {
            /* Are any files opened for streaming? */
            bool any_streaming = false;

            std::deque<TarFile2*>::const_iterator i;
            for (i = open.begin(); i != open.end(); i++) {
                if ((*i)->impl->opened && (*i)->impl->header.mode.stream) {
                    any_streaming = true;
                    break;
                }
            }

            /* Update the file size in the header */
            file->impl->header.size = file->impl->lwrrent_position_in_buffer;

            /* Another file is streaming. We can't flush this one out
             * yet. So make a copy in the pending file list.
             */
            if (any_streaming) {
                TarFile2 *tar = new TarFile2(this, file->impl->header);
                tar->impl->buffer.swap(file->impl->buffer);
                tar->impl->lwrrent_position_in_buffer =
                                         file->impl->lwrrent_position_in_buffer;
                this->impl->pending_files.push_back(tar);

                Printf(Tee::PriDebug, "Tar: Adding file to pending queue\n");
            }
            /* Otherwise, we're free to write the file to disk */
            else {
                /* Write the header */
                this->write_header(file->impl->header);
                /* Colwert the file to stream temporarily and flush it */
                file->impl->header.mode.stream = 1;
                ret = file->fflush();
                file->impl->header.mode.stream = 0;
                flush_pending = true;
            }
        }
    }
    else {
        flush_pending = true;
    }

    file->impl->opened = false;  /* Prevent other operations on the file */
    file->impl->header.mode.stream = 0;
    {   std::vector<ubyte> temp;
        file->impl->buffer.swap(temp);
    }
    open.erase(std::find(open.begin(), open.end(), file), open.end());

    /* Flush the pending files, if possible */
    if (flush_pending) {
        std::vector<TarFile2*>::const_iterator i;
        for (i  = this->impl->pending_files.begin();
             i != this->impl->pending_files.end(); i++) {
            this->write_header((*i)->impl->header);
            (*i)->impl->header.mode.stream = 1;
            ret |= (*i)->fflush();
            (*i)->impl->opened = false;
            delete *i;
        }
        this->impl->pending_files.clear();
    }

    return ret;
}

Z_SIZE_T TapeArchive2::write_header(const TarHeader2 &header) {

    char buf[768] = { };

    /* Pad the last file to a multiple of 512 bytes */
    if (this->impl->lwr_pos_in_file & 511) {
        this->fwrite(buf, sizeof(buf) - (this->impl->lwr_pos_in_file & 511));
    }

    Printf(Tee::PriDebug, "Tar: Writing header. Current position in file: %llu bytes\n", (Z_SIZE_T)this->impl->lwr_pos_in_file);

    /* Build up the header */

    /* Strip the directory name (which we don't support) */
    std::string filename(header.filename.c_str());
    std::string::size_type pos = filename.find_last_of("/\\");
    if (pos != filename.npos) {
        filename = filename.substr(pos + 1);
    }

    if (filename.size() < 100) {
        snprintf(buf, 100, "%s", filename.c_str());
    }
    else {
        Printf(Tee::PriError, "Tar: File with > 100 characters in its name!");
        return 0;  /* Error out - we don't support files with a filename > 100 bytes */
    }

    snprintf(buf + 100, 8, "0777");  /* File mode */
    snprintf(buf + 108, 8, "00");    /* owner id */
    snprintf(buf + 116, 8, "00");    /* group id */
    //snprintf(buf + 136,12, "%o", header.modification_time);

    /* File size */
#if SIZE_MAX >= (8ull << 30ull)
    if (header.size < (8ull << 30ull)) {
        snprintf(buf + 124, 13, "0%llo", header.size);
    }
    else {
        Printf(Tee::PriError, "Tar: File >= 8 GB!");
        return 0;  /* Error out - we don't support files larger than 8 GB yet */
    }
#else
    snprintf(buf + 124,12, "0%o", (UINT32)header.size);
#endif
    snprintf(buf + 136, 13, "010000000000"); /* Some time in 2004 */

    /* Copy 8 spaces for the checksum (for now).
     */
    snprintf(buf + 148, 12, "        ");

    /* USTAR indicator */
    snprintf(buf + 257, 7, "ustar ");
    snprintf(buf + 263, 2, " ");

    snprintf(buf + 265, 32, "nobody");    /* User name */
    snprintf(buf + 297, 32, "nobody");    /* Group name */
    snprintf(buf + 329,  8, "00");        /* Device major number */
    snprintf(buf + 337,  8, "00");        /* Device minor number */

    /* Note: These 4 fields below are needed for WinZip to open our file.
     * For some reason, the USTAR file format isn't properly recognized.
     */
    snprintf(buf + 345, 8, "00");         /* Access time */
    snprintf(buf + 357, 8, "00");         /* Create time */
    snprintf(buf + 369,12, "00");         /* Offset */
    snprintf(buf + 483,12, "00");         /* Real size */

    /* Compute CRC */
    Z_SIZE_T checksum = 0;
    for (Z_SIZE_T i = 0; i < sizeof(buf); i++) {
        checksum += buf[i];
    }

    /* Write the CRC */
    snprintf(buf + 148, 8, "%06o", (UINT32)checksum);

    Printf(Tee::PriDebug, "Tar: Filename = %s, size = %llu, checksum = %06o\n", filename.c_str(), (Z_SIZE_T)header.size, (UINT32)checksum);

    /* Write the header */
    Z_SIZE_T ret = this->fwrite(buf, sizeof(buf));

    Printf(Tee::PriDebug, "Tar: Wrote header. Current position in file: %llu bytes\n", (Z_SIZE_T)this->impl->lwr_pos_in_file);

    return ret;
}

Z_SIZE_T TapeArchive2::read_header(TarHeader2 &header) {

    Printf(Tee::PriDebug, "Tar: Reading header. Current position in file: %llu bytes\n", (Z_SIZE_T)this->impl->lwr_pos_in_file);

    char buf[512];
    std::fill(buf, buf + sizeof(buf), 0);

    /* Read the header bytes */
    Z_SIZE_T ret = this->fread(buf, sizeof(buf), this->impl->lwr_pos_in_file);
    if (ret != 512)
    {
        Printf(Tee::PriDebug, "Tar: Failed to read header or it's the end of tgz file\n");
        return 0;
    }

    /* Build up the header */
    header.filename.assign(buf);
    header.size = 0; //in case of 64bit, clear to avoid garbage in high 32bit
    /* Support base-256 coding for file size */
    if (buf[124] & (0x01<<7))
    {
        for (int i = 1; i < 12; i++) {
            header.size *= 256;
            header.size += (unsigned char)buf[124 + i];
        }
    }
    /* size is Legacy ASCII digit octal number encoding */
    else
    {
        /* for bug1535056.
         * If file size >= 1G, then there will no '\0' in the size section
         * sscanf will keep reading until meet `\0` which caused the size is wrong.
         * Change to only read 12 bytes which is size information from the buffer
         * */
        if (sscanf(buf + 124, "%12llo", &header.size) < 1) 
        {
            Printf(Tee::PriDebug, "Tar: Failed to read file size, filename: %s, filesize: %llu lwr_pos_in_file: %llu, buf+124: %s\n"
                , header.filename.c_str(), header.size, this->impl->lwr_pos_in_file, buf+124);
            return 0;  /* Stop reading, no more file left */
        }
    }
    header.offset_of_file_data_in_tar = this->impl->lwr_pos_in_file;

    return ret;
}

const char *TapeArchive2::findfirst(const char *pattern) {

    this->impl->find_pattern = pattern;
    this->impl->lwr_find_pos_in_files = 0;

    return findnext();
}

#define FF_MATCH_TRY 0
#define FF_MATCH_ONE 1
#define FF_MATCH_ANY 2

struct FF_MATCH_DATA
{
    FF_MATCH_DATA() : type(0), s1(NULL), s2(NULL) {}

    int type;
    const char *s1;
    const char *s2;
};

/* ff_match:
 *  Matches two strings ('*' matches any number of characters,
 *  '?' matches any character).
 * Original code at https://alleg.svn.sourceforge.net/svnroot/alleg/allegro/branches/4.2/src/unix/ufile.c
 */
static int ff_match(const char *s1, const char *s2)
{
    /* handle NULL arguments */
    if ((!s1) || (!s2)) {
        return 0;
    }

    const char *s1end = s1 + strlen(s1);
    Z_SIZE_T size = strlen(s2);

    std::vector<FF_MATCH_DATA> data(size * 2 + 1);

    int index = 0;
    data[0].s1 = s1;
    data[0].s2 = s2;
    data[0].type = FF_MATCH_TRY;
    int c1 = 0;
    int c2 = 0;

    while (index >= 0) {
      s1 = data[index].s1;
      s2 = data[index].s2;
      c1 = *s1;
      c2 = *s2;

      switch (data[index].type) {

      case FF_MATCH_TRY:
         if (c2 == 0) {
            /* pattern exhausted */
            if (c1 == 0)
               return 1;
            else
               index--;
         }
         else if (c1 == 0) {
            /* string exhausted */
            while (*s2 == '*')
               s2++;
            if (*s2 == 0)
               return 1;
            else
               index--;
         }
         else if (c2 == '*') {
            /* try to match the rest of pattern with empty string */
            data[index++].type = FF_MATCH_ANY;
            data[index].s1 = s1end;
            data[index].s2 = s2 + 1;
            data[index].type = FF_MATCH_TRY;
         }
         else if ((c2 == '?') || (c1 == c2)) {
            /* try to match the rest */
            data[index++].type = FF_MATCH_ONE;
            data[index].s1 = s1 + 1;
            data[index].s2 = s2 + 1;
            data[index].type = FF_MATCH_TRY;
         }
         else
            index--;
         break;

      case FF_MATCH_ONE:
         /* the rest of string did not match, try earlier */
         index--;
         break;

      case FF_MATCH_ANY:
         /* rest of string did not match, try add more chars to string tail */
         if (--data[index + 1].s1 >= s1) {
            data[index + 1].type = FF_MATCH_TRY;
            index++;
         }
         else
            index--;
         break;

      default:
         /* this is a bird? This is a plane? No it's a bug!!! */
         return 0;
      }
   }

   return 0;
}

const char *TapeArchive2::findnext() {

    while (this->impl->lwr_find_pos_in_files < this->impl->files.size()) {

        /* Get the next header, increment our search position for next time */
        TarHeader2 &header = this->impl->files[this->impl->lwr_find_pos_in_files++];

        if (ff_match(header.filename.c_str(), this->impl->find_pattern.c_str())) {
            return header.filename.c_str();
        }
    }

    return NULL;
}

