/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef RM_UTILITY_STREAM_H_
#define RM_UTILITY_STREAM_H_

#include <fstream>
#include "rmtstring.h"

namespace rmt
{
    //! \brief      Location within a text file
    struct FileLocation
    {
        //! \brief      File name
        String file;

        //! \brief      Line number
        unsigned line;

        //! \brief      Constrution
        inline FileLocation()
        {
            line = 0;   // Zero means n/a
        }

        //!
        //! \brief      Human-readable representation of this location
        //!
        //! \details    If the file name is not empty, this function returns
        //!             "file(line): " or "file: " depending on whether or not
        //!             the line number is specified (nonzero).  If the file name
        //!             is empty, then the empty string is returned.
        //!
        String locationString() const;
    };

    //!
    //! \brief      Line read from a plain text file
    //! \see        TextFileInputStream
    //!
    struct TextLine: String, FileLocation
    {
    //!
    //! \fn         string &assign(const string &str)
    //! \brief      Assign the string portion of this text line.
    //! \note       This function disregards the file location.
    //!

    //!
    //! \fn         string &assign(const char *s)
    //! \brief      Assign the string portion of this text line.
    //! \note       This function disregards the file location.
    //!

    //!
    //! \fn         bool empty() const
    //! \brief      Is this text line?
    //! \note       This function disregards the file location.
    //!
    };

    //!
    //! \brief      Input stream for a text file
    //! \see        TextLine
    //!
    //! \details    This class extends std::ifstream to add features for plain
    //!             text files.  Specifically, it keeps track of the file name
    //!             and line number to facilitate the annotation of error messages
    //!             and other exceptions.  Specifically, 'getline' returns a
    //!             'TextLine' object which contains this metadata.
    //!
    class TextFileInputStream: protected FileLocation, protected std::ifstream
    {
    public:

        //!
        //! \brief      Open the file for reading.
        //!
        //! \details    If the stream was previously open, it is closed and reopened.
        //!
        inline void open(const char *file)
        {
            // Initalize line counters
            this->file = String(file);
            this->line = 0;

            // Close the stream if it is already open
            if(std::ifstream::is_open())
                std::ifstream::close();

            // Discard any previous errors
            std::ifstream::clear();

            // Open the stream to the new file
            std::ifstream::open(file);
        }

        //! \brief      Read a line from the file.
        inline void getline(TextLine &text)
        {
            // Read the line
            std::getline(*this, text, '\n');

            // If that worked, count the lines.
            if (!std::ifstream::fail())
                ++line;

            // Save the location from which the line was read.
            text.file = file;
            text.line = line;
        }

        //! \brief      Close the stream
        inline void close()
        {
            std::ifstream::close();
            file.erase();       // Not applicable
            line = 0;           // Not applicable
        }

        //! \brief      Is this stream at the end of file?
        inline bool eof() const
        {
            return std::ifstream::eof();
        }

        //! \brief      Is this stream good for I/O?
        inline bool good() const
        {
            return std::ifstream::good();
        }

        //! \brief      Is this stream in a bad state?
        inline bool bad() const
        {
            return std::ifstream::bad();
        }

        //! \brief      Did the previous I/O fail?
        inline bool fail() const
        {
            return std::ifstream::fail();
        }

        //! \brief      Location of last 'getline'
        inline FileLocation location() const
        {
            return *this;
        }

        //! \brief      Human-readable representation of this location
        inline String locationString() const
        {
            return FileLocation::locationString();
        }
    };
};

#endif /* RM_UTILITY_STREAM_H_ */

