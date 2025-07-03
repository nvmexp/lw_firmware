/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012-2017, 2019-2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//!
//! \file
//!
//! Implements the CTP file parser class
//!

#include "gpu/tests/rm/utility/rmtstring.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"

#include "uctfilereader.h"
#include "uctchip.h"
#include "uctfield.h"
#include "uctsinespec.h"
#include "uctpartialpstate.h"
#include "uctvflwrve.h"

// Extension used by the ctp files
const char *uct::CtpFileReader::CTP_EXTENSION = ".ctp";

// Path separator used by the operating system
#ifdef _WIN32
const char uct::CtpFileReader::PATH_SEPARATOR = '\\';
#else
const char uct::CtpFileReader::PATH_SEPARATOR = '/';
#endif

// Create a new field
uct::Field *uct::CtpFileReader::IncludeField::newField(const CtpStatement &statement)
{
    return new IncludeField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::CtpFileReader::IncludeField::clone() const
{
    return new IncludeField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::CtpFileReader::IncludeField::parse()
{
    // Error if there is no file name
    if (empty())
        return ReaderException("CTP file group name (and/or path) are required in an 'include' field.",
            *this, __PRETTY_FUNCTION__);

    // Okay
    return Exception();
}

// Apply the field to the file reader
uct::ExceptionList uct::CtpFileReader::IncludeField::apply(CtpFileReader &reader) const
{
    // Add this file group to the list
    reader.includeFiles.push(*this);

    // Done
    return Exception();
}

// Create a new field
uct::Field *uct::CtpFileReader::DryRunField::newField(const CtpStatement &statement)
{
    return new DryRunField(statement);
}

// Create an exact copy of 'this'.
uct::Field *uct::CtpFileReader::DryRunField::clone() const
{
    return new DryRunField(*this);
}

// Interpret the statement argument.
uct::ExceptionList uct::CtpFileReader::DryRunField::parse()
{
    // Nothing to do for dryrun=false but return a Null exception.
    if (isFalse())
        return Exception();

    // If true, insert an exception to prevent tests from running.
    if (isTrue())
        return ReaderException("'dryrun' specified to prevent tests from running",
            *this, __PRETTY_FUNCTION__, ReaderException::DryRun);

    // Anything else is a syntax error.
    return ReaderException(*this + ": Invalid 'dryrun' argument keyword.",
        *this, __PRETTY_FUNCTION__);
}

// Initialize 'path' and 'base'
void uct::CtpFileReader::CtpFileGroup::initialize(rmt::String filepath)
{
    // Discard '.ctp' file suffix (if any)
    filepath.rtrim(CTP_EXTENSION);

    // Breakout the path from the basename
    size_t pos = filepath.rfind(PATH_SEPARATOR);
    if (pos == CtpStatement::npos)
    {
        path.erase();
        base = filepath;
    }

    else
    {
        path = filepath.substr(0, pos);
        base = filepath.substr(pos + 1);;
    }
}

// Find a path prefix that contains the specified file.
bool uct::CtpFileReader::CtpFileGroup::resolveFilePath(const rmt::String& fileName, rmt::String &prefix)
{
    // Can we open this file without a prefix?
    // Maybe it's a absolute file path?  Or maybe relative to PWD?
    // Use block-local object for ifs so destructor is called in a timely manner.
    {
        ifstream ifs;

        ifs.open(fileName);
        if(ifs.good())
        {
            Printf(Tee::PriHigh, "%s: found without path search (%s)\n", fileName.c_str(), __FUNCTION__);
            prefix.erase();
            return true;
        }
    }

    // If that didn't work, try perfixing "ctp/" to the filename since we usually
    // put the files in their own directory named thusly.
    // Use block-local object for 'ifs' so destructor/constructor are called.
    {
        ifstream ifs;
        std::string fn = std::string("ctp") + PATH_SEPARATOR + fileName;

        ifs.open(fn.c_str());
        if(ifs.good())
        {
            Printf(Tee::PriHigh, "%s: found relative to present working directory (%s)\n",
                fn.c_str(), __FUNCTION__);
            prefix = std::string("ctp") + PATH_SEPARATOR;
            return true;
        }
    }

    // Else we need to search for the file
    {
        // Get a list of paths within which we search for CTP files
        vector<string> searchPaths;
        Utility::AppendElwSearchPaths(&searchPaths, "MODS_RUNSPACE");
        Utility::AppendElwSearchPaths(&searchPaths, "LD_LIBRARY_PATH");

        // Find file, and if found resovle to full path.
        // If the file was not found, 'FindFile' returns an empty string.
        prefix = Utility::FindFile(fileName, searchPaths);

        // If that didn't work, try perfixing "ctp/" to the filename since we usually
        // put the files in their own directory named thusly.
        if (prefix.empty())
        {
            prefix = Utility::FindFile(rmt::String("ctp") + PATH_SEPARATOR + fileName, searchPaths);

            // That worked, so add it to the prefix.
            if (!prefix.empty())
            {
                if (prefix[prefix.length() - 1] != PATH_SEPARATOR)
                    prefix += PATH_SEPARATOR;
                prefix += "ctp";
            }
        }

        // Good news
        if (!prefix.empty())
            Printf(Tee::PriHigh, "%s: found in: '%s' (%s)\n", fileName.c_str(), prefix.c_str(), __FUNCTION__);
    }

    // Add the path separator if needed.
    if (!prefix.empty() && prefix[prefix.length() - 1] != PATH_SEPARATOR)
        prefix += PATH_SEPARATOR;

    // If not found, 'prefix' is the empty string.
    return !prefix.empty();
}

// Find a path prefix that contains at least one file in this group
bool uct::CtpFileReader::CtpFileGroup::resolvePath(const uct::PlatformSuffix &suffix, rmt::String &prefix) const
{
    // Comments herein use "mypath/fred.ctp" as an example on gf119.
    // So prefix="mypath", base="fred", and family.chip="gf119".
    PlatformSuffix::const_iterator pSuffix;
    rmt::String file;

    // Scan each family name
    for (pSuffix = suffix.begin(); pSuffix != suffix.end(); ++pSuffix)
    {
        // Assemble the name/path that contains the chip suffix.
        // (e.g. "mypath/fred.gf119.ctp", "mypath/fred.gf11x.ctp", "mypath/fred.gf1xx.ctp", etc.)
        file = name(*pSuffix);

        // Search for a path 'prefix' that contains at least one of the CTP files.
        // Once we find a path with any file in the group, we use that path for all files in the group.
        // Note that the 'file' (and 'path') may be absolute, making 'prefix' empty, but returning true.
        // (e.g. "c:\mypath\fred.gf119.ctp" or "/mypath/fred.gf119.ctp")
        if (resolveFilePath(file, prefix))
            return true;
    }

    // Not found
    prefix.erase();
    return false;
}

// Get the list of file paths represented by this group for the specified chip suffixes
bool uct::CtpFileReader::CtpFileGroup::getFileList(const uct::PlatformSuffix &suffix, CtpFileList &fileList) const
{
    PlatformSuffix::const_iterator pSuffix;
    rmt::String prefix;

    // Discard any existing entries
    fileList.clear();

    // Search for a path that contains at least one of these files.
    // If we can't find a path, then return false to indicate an error.
    if (!resolvePath(suffix, prefix))
        return false;

    // Add the first file that exists to the list.
    // As an example, if we assume path="mypath", base="fred", prefix="/mods/",
    // and suffix.chip="gf119"; then 'file' would iterate through:
    //  /mods/mypath/fred.emu.gf119.ctp
    //  /mods/mypath/fred.emu.gf11x.ctp
    //  /mods/mypath/fred.emu.gf1xx.ctp
    //  /mods/mypath/fred.emu.gfxxx.ctp
    //  /mods/mypath/fred.emu.fermi.ctp
    //  /mods/mypath/fred.emu.ctp
    //  /mods/mypath/fred.gf119.ctp
    //  /mods/mypath/fred.gf11x.ctp
    //  /mods/mypath/fred.gf1xx.ctp
    //  /mods/mypath/fred.gfxxx.ctp
    //  /mods/mypath/fred.fermi.ctp
    //  /mods/mypath/fred.ctp
    //
    // TODO: We should returns all files in a priority order.  However, before
    // we can do that, we have to add logic to CtpFileReader::scanFile to handle
    // the conflict and apply a priority.
    //
    for (pSuffix = suffix.begin(); pSuffix != suffix.end(); ++pSuffix)
    {
        ifstream ifs;
        rmt::String file = prefix + name(*pSuffix);

        ifs.open(file);
        if(ifs.good())
        {
            Printf(Tee::PriHigh, "%s: added to list (%s)\n", file.c_str(), rmt::String::function(__PRETTY_FUNCTION__).c_str());
            fileList.push_back(file);
            return true;
        }
    }

    // If we can't open any of the suffixed files, then resolvePath should have returned false.
    MASSERT(false);
    return false;
}

// Get the next file group from the queue
uct::CtpFileReader::CtpFileGroup uct::CtpFileReader::CtpFileGroupQueue::pop()
{
    CtpFileGroup group;
    bool bRedundant;

    // Stop searching if the queue is (or becomes) empty.
    while (!empty())
    {
        // Get the next element and remove it from the queue
        group = queue<CtpFileGroup>::front();
        queue<CtpFileGroup>::pop();

        // Add it to the 'done' list
        bRedundant = !done.insert(group.base).second;

        // Return it unless it's already done or is empty
        if (!bRedundant && !group.end())
            return group;
    }

    // Indicate the end-of-queue
    return CtpFileGroup();
}

// Initialize the file reader
uct::ExceptionList uct::CtpFileReader::initialize(const rmt::String& ctpFilePath,
                    GpuSubdevice *pSubdevice, const rmt::String &exclude)
{
    // Determine chip-specific and platform-specific CTP file name suffixes.
    suffixList.initialize(pSubdevice);

    // Start off by pushing the initial file into the process queue.
    includeFiles.initialize(ctpFilePath);

    // Get the list of supported clock domains.
    return context.initialize(pSubdevice, exclude);
}

//
// Read a block of text from the current ctp file.
//
// A block is a set of lines that starts with a 'name' field or contains only
// a directive field.
//
uct::ExceptionList uct::CtpFileReader::readNextBlock(FieldVector &block, bool &eof)
{
    rmt::String t;
    ExceptionList status;

    // Get rid of any leftover state
    block.clear();

    //
    // While the file has content, read a line until a block is read.
    // Test the severity of the error and continue if we can.
    //
    do
    {
        //
        // Add it to the block if unless it is a comment.
        // Parsing is completed when the statement is added to the block.
        // Specifically, the right side of the equal sign is parsed.
        //
        if (statement.isBlockType())
            status.push(block.push(statement));

        // Check to see if it indicates the end of the CTP file.
        eof = statement.isEof();

        //
        // If not, read the next line and add any exception to the status.
        // Parsing is started when the statement is read.
        // Specifically, the left side of the equal sign is parsed.
        //
        // It's okay to call CtpStatement::read  even if the input stream is
        // at end-of-file; it simply returns eof again.
        //
        if (!eof)
            status.push(statement.read(is));

    //
    // Exit with the current block if this is the start of a new one,
    // if this is the end-of-file, or if there is a fatal error.
    //
    } while (!statement.isBlockStart() && !eof && !status.isFatal());

    // Done
    return status;
}

// Read and parse CTP files block by block.
uct::ExceptionList uct::CtpFileReader::scanFile(GpuSubdevice *m_pSubdevice)
{
    ExceptionList status;
    CtpFileGroup fileGroup;
    CtpFileList fileList;
    CtpFileList::const_iterator pFilePath;
    FieldVector block;

    // While we still have file groups to be processed
    while ( !(fileGroup = includeFiles.pop()).end())
    {
        // Get the list of files associated with this group for this chip.
        if (!fileGroup.getFileList(suffixList, fileList))
        {
            status.push(ReaderException(fileGroup.name()+ ": No such CTP file group found",
                fileGroup.from, __PRETTY_FUNCTION__, ReaderException::Fatal));
        }

        // Iterate through each file in the group.
        else for(pFilePath = fileList.begin(); pFilePath != fileList.end(); ++pFilePath)
        {
            // New file
            Printf(Tee::PriHigh, "\n======== Sourcing %s  (%s)\n", pFilePath->c_str(),
                rmt::String::function(__FUNCTION__).c_str());

            // Discard any previous state.
            statement.clear();
            statement.file = *pFilePath;

            // Open the file
            is.open(*pFilePath);
            if (!is.good())
            {
                status.push(ReaderException(*pFilePath + ": Failed to open file", statement, __PRETTY_FUNCTION__));
            }

            // File is open
            else
            {
                bool eof;
                do
                {
                    // Read and parse the next block of statements
                    status.push(readNextBlock(block, eof));

                    if (status.isFatal() || eof)
                        break;

                    // Scan the block based on its type.
                    switch (block.blockType())
                    {
                    // Directives are applied to this reader.  One statement per block.
                    case FieldVector::DirectiveBlock:
                        MASSERT(block.size() == 1);
                        status.push(block.start()->apply(*this));
                        break;

                    // Create a trial specification for this statement block.
                    case FieldVector::TrialSpecBlock:
                    {
                        TrialSpec trial;
                        ExceptionList trialStatus;

                        trial.context = context;
                        trialStatus.push(block.apply(trial));

                        if (trialStatus.isOkay() && !vTrials.push(trial))
                            trialStatus.push(ReaderException(trial.name +
                                ": Redefinition of trial specification",
                                block, __PRETTY_FUNCTION__));

                        status.push(trialStatus);

                        break;
                    }

                    // Create a partial p-state for this statement block.
                    case FieldVector::PartialPStateBlock:
                    {
                        shared_ptr<PartialPState> pstate =
                            make_shared<PartialPState>();
                        ExceptionList pstateStatus;

                        pstateStatus.push(block.apply(*pstate));

                        if (pstateStatus.isOkay() && !mTestSpecs.push(pstate))
                            pstateStatus.push(ReaderException(pstate->name +
                                ": Redefinition of pstate definition",
                                block, __PRETTY_FUNCTION__));

                        status.push(pstateStatus);

                        break;
                    }

                    // Create a Sine Spec for this statement block
                    case FieldVector::SineSpecBlock:
                    {
                        shared_ptr<SineSpec> sinespec = make_shared<SineSpec>();
                        ExceptionList sinespecStatus;
                        Exception ex;

                        sinespecStatus.push(block.apply(*sinespec));
                        if(sinespecStatus.isOkay() && !mTestSpecs.push(sinespec))
                            sinespecStatus.push(ReaderException(sinespec->name +
                                ": Redifinition of sine spec",
                                block, __PRETTY_FUNCTION__));

                        status.push(sinespecStatus);

                        break;
                    }

                    // Create an VfLwrve Test Spec for this statement block
                    case FieldVector::VfLwrveBlock:
                    {
                        shared_ptr<VfLwrve> vflwrve = make_shared<VfLwrve>();
                        ExceptionList vflwrveStatus;
                        Exception ex;

                        vflwrveStatus.push(block.apply(*vflwrve, m_pSubdevice));
                        if(vflwrveStatus.isOkay() && !mTestSpecs.push(vflwrve))
                            vflwrveStatus.push(ReaderException(vflwrve->name +
                                ": Redefinition of vflwrve test spec",
                                block, __PRETTY_FUNCTION__));

                        status.push(vflwrveStatus);
                        break;
                    }

                    //
                    // Only empty blocks should have unknown type since comments
                    // and blank lines are discarded in readNextBlock.
                    //
                    case FieldVector::Unknown:
                        if (block.size())
                        {
                            rmt::String name;
                            const Field *nameField = block.name();

                            // Extract the name if the name field exists
                            if (nameField)
                                name = *nameField + ": ";

                            status.push(ReaderException(name +
                                "Unknown block type. No identifying statements.",
                                block, __PRETTY_FUNCTION__));
                        }

                        break;

                    //
                    // Block contains statements of both types.  Probably the CTP
                    // file is missing the 'name' field which starts a new block.
                    //
                    default:
                        status.push(ReaderException("Ambiguous block type. Missing 'name'?", block, __PRETTY_FUNCTION__));
                        break;
                    }
                } while (!status.isFatal());
            }
        }
    }

    // Done.
    is.close();

    return status;
}

// Print the parsed fields.
void uct::CtpFileReader::dumpContent() const
{
    TestSpec::NameMap::const_iterator mItTest;
    TrialSpec::List::const_iterator pTrial;

    // Print each Test spec
    Printf(Tee::PriHigh,
        "\n\n==== Test Summary  (%s)\n",
        rmt::String::function(__PRETTY_FUNCTION__).c_str());
    for(mItTest = mTestSpecs.begin();
        mItTest != mTestSpecs.end(); ++mItTest)
        Printf(Tee::PriHigh, "%s\n", mItTest->second->debugString().c_str());

    // Print each trial specification.
    Printf(Tee::PriHigh, "\n==== Trial Specification Summary  (%s)\n",
        rmt::String::function(__PRETTY_FUNCTION__).c_str());

    for(pTrial = vTrials.begin(); pTrial != vTrials.end(); ++pTrial)
        Printf(Tee::PriHigh, "%s\n", pTrial->debugString().c_str());

    Printf(Tee::PriHigh, "\n\n");
}

