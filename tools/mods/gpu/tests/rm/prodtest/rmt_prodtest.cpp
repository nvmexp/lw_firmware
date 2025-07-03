/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2004-2018, 2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//!
//! \file rmt_prodtest.cpp
//! \brief This Test validates the PROD field settings of a chip
//!        on which it exelwtes. And, it needs a data file as input.
//!
//!
//! If anyone wants to extend the support for other chips too, need to follow
//! the following steps.
//! 1. First generate the .dat file for the gpu version by running the script
//!    that presents @ ....drivers/resman/kernel/inc/tools/ref2h-genprod.pl
//! 2. Run the script with two arguments.The details about arguments can be
//!    found in the script.
//! 3. Need to check in the .dat files that have generated
//!    @..diag/mods/gpu/tests/traces.
//! 4. In method ProdTest::Run(), include the corresponding case.
//! 5. Add .dat & filter files in @..diag/mods/gpu/tests/rm/makesrc.inc
//!
//! The purpose of this test is to approve or list the PROD fields that have
//! been purposely changed away.
//!

#include "gpu/tests/rmtest.h"
#include <stdio.h>
#include <string.h>

#include "core/include/jscript.h"
#include "gpu/tests/gputestc.h"
#include "core/include/channel.h"
#include "lwos.h"
#include "core/include/platform.h"
#include "lwmisc.h"
#include "core/include/disp_test.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gralloc.h"
#include "core/include/utility.h"
#include "class/cl9097.h" // FERMI_A

#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "core/include/memcheck.h"

#define MAX_REGNAME_LEN         256
#define MAX_FIELDNAME_LEN       256
#define MAX_DATNAME_LEN         256

typedef struct
{
    UINT32 addr;
    UINT32 flags;
    UINT32 fieldCnt;
    UINT32 nameLen;
} PRODREGDEF;

typedef struct
{
    UINT32 addr;
    UINT32 val;
} PRODREGIDXDEF;

typedef struct
{
    UINT32 startBit;
    UINT32 endBit;
    UINT32 value;
    UINT32 nameLen;
} PRODFIELDDEF;

class ProdTest: public RmTest
{
public:
    ProdTest();
    virtual ~ProdTest();

    virtual string IsTestSupported();
    virtual RC Setup();
    virtual RC Run();
    virtual RC Cleanup();

    SETGET_PROP(regMatch, string);

    virtual void FindFilePath(string file);
    void RemoveComment(char *str, char defaultIdentifier = '#');

private:

    string m_regMatch;
    list<string> m_regSkipList;
    list<string> m_regFiltMidList;
    list<string> m_regFiltEndList;
    string m_modsDirPath;
    Channel *       m_pCh;
    LwRm::Handle    m_hCh;
    GrClassAllocator *m_3dAlloc;
};

//! \brief ProdTest constructor.
//!
//! \sa Setup
//----------------------------------------------------------------------------
ProdTest::ProdTest()
{
    SetName("ProdTest");
    m_3dAlloc = new ThreeDAlloc();
    m_3dAlloc->SetOldestSupportedClass(FERMI_A);
}

//! \brief ProdTest destructor.
//!
//! \sa Cleanup
//----------------------------------------------------------------------------
ProdTest::~ProdTest()
{
}

//! \brief Whether or not the test is supported in the current environment
//! returns true implying that all platforms are supported
//----------------------------------------------------------------------------
string ProdTest::IsTestSupported()
{
    return RUN_RMTEST_TRUE;
}

//! \brief Setup all necessary state before running the test.
//!
//! Setup is used to reserve all the required resources used by this test,
//!
//! \return RC -> OK if everything is allocated, test-specific RC if something
//!         failed while selwring resources.
//! \sa Run
//----------------------------------------------------------------------------
RC ProdTest::Setup()
{
    RC rc = OK;

    CHECK_RC_CLEANUP(m_TestConfig.AllocateChannel(&m_pCh, &m_hCh, LW2080_ENGINE_TYPE_GR(0)));
    CHECK_RC_CLEANUP(m_3dAlloc->Alloc(m_hCh, GetBoundGpuDevice()));

    return rc;

Cleanup:
    if (m_pCh)
        m_TestConfig.FreeChannel(m_pCh);

    return rc;
}

//! \brief Run the test!
//!
//! After the setup is successful the allocated resources can be used.
//! Run function uses the allocated resources.
//!
//! \return OK if the test passed, specific RC if it failed
//-----------------------------------------------------------------------------
RC ProdTest::Run()
{
    RC rc;
    PRODREGDEF reg;
    PRODREGIDXDEF regIdx;
    PRODFIELDDEF field;
    FILE *fprod = NULL;
    UINT32 regVal;
    UINT32 fieldVal;
    UINT32 mask;
    UINT32 shift;
    UINT32 i;
    UINT32 regMismatch = 0;
    UINT32 fieldMismatch = 0;
    char regName[MAX_REGNAME_LEN];
    char fieldName[MAX_FIELDNAME_LEN];
    char datName[MAX_DATNAME_LEN];
    char fullDatName[MAX_DATNAME_LEN];
    char chipStr[MAX_DATNAME_LEN];
    char csvName[MAX_DATNAME_LEN];
    char diffsName[MAX_DATNAME_LEN];
    char absolutePath[MAX_DATNAME_LEN];
    UINT32 gpuId;
    bool regPrint;
    bool diffsPrint;
    FILE *fpCsv = NULL;
    FILE *fpDiffs = NULL;
    GpuSubdevice *pSubdev = GetBoundGpuSubdevice();
    UINT32 skipit;
    char *filterLast = NULL;
    char skipRegFile[MAX_DATNAME_LEN];
    FILE *fpSkipRegFile = NULL;
    char filterRegFileEnd[MAX_DATNAME_LEN];
    FILE *fpFilterRegFileEnd = NULL;
    char filterRegFileMid[MAX_DATNAME_LEN];
    FILE *fpFilterRegFileMid = NULL;

    //
    // We select the relevant data file, based on Gpu version.
    //
    gpuId = pSubdev->DeviceId();

    // Clear all the char arrays
    memset(datName, 0, sizeof(datName));
    memset(fullDatName, 0, sizeof(fullDatName));
    memset(chipStr, 0, sizeof(chipStr));
    memset(absolutePath, 0, sizeof(absolutePath));
    memset(csvName, 0 , sizeof(csvName));
    memset(diffsName, 0 , sizeof(diffsName));
    memset(skipRegFile, 0, sizeof(skipRegFile));
    memset(filterRegFileEnd, 0, sizeof(filterRegFileEnd));
    memset(filterRegFileMid, 0, sizeof(filterRegFileMid));

    // datName is used to search elw directories for skip and data files.
#ifdef LW_WINDOWS
    strcpy(datName,"\\");
#else
    strcpy(datName,"/");
#endif

#if LWCFG(GLOBAL_GPU_IMPL_GH202)
    if (gpuId >= Gpu::GH202)
    {
        // For GH2XX, use the lower 4 bits of the devid preceded by a 2
        sprintf(&chipStr[strlen(chipStr)], "r_gh2%02x", gpuId & 0x0f);
    }
    else
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GH100)
    if (gpuId >= Gpu::GH100)
    {
        // For GH1XX, use the lower 4 bits of the devid preceded by a 1
        sprintf(&chipStr[strlen(chipStr)], "r_gh1%02x", gpuId & 0x0f);
    }
    else
#endif
#if LWCFG(GLOBAL_GPU_IMPL_GA100)
    if (gpuId >= Gpu::GA100)
    {
        // For GA1XX, use the lower 4 bits of the devid preceded by a 1
        sprintf(&chipStr[strlen(chipStr)], "r_ga1%02x", gpuId & 0x0f);
    }
    else
#endif
    if (gpuId >= Gpu::TU102)
    {
       // For TU1XX, use the lower 4 bits of the devid preceded by a 1
       sprintf(&chipStr[strlen(chipStr)], "r_tu1%02x", gpuId & 0x0f);
    }
    // todo
    // else if (gpuId == Gpu::GV00)
    // {
    //     strcat(chipStr, "r_gv100");
    // }
    else if (gpuId >= Gpu::GP100)
    {
       // For GP10x, use the lower 4 bits of the devid preceded by a 1
       sprintf(&chipStr[strlen(chipStr)], "r_gp1%02x", gpuId & 0x0f);
    }
    else if (gpuId >= Gpu::GM200)
    {
        // For GM2xx, use the lower 4 bits of the devid preceded by a 2.
        sprintf(&chipStr[strlen(chipStr)], "r_gm2%02x", gpuId & 0x0f);
    }
    else if (gpuId >= Gpu::GM107)
    {
        // For GM1xx, use the lower byte of the devid preceded by a 1.
        sprintf(&chipStr[strlen(chipStr)], "r_gm1%02x", gpuId & 0xff);
    }
    else
    {
        sprintf(&chipStr[strlen(chipStr)], "??%x", gpuId);
    }
    strcat(datName, chipStr);
    strcat(datName, ".dat");

    //
    // Build skip files and data file names
    // Search MODS_RUNSPACE and LD_LIBRAY_PATH for the files
    //
    FindFilePath(datName);

    // Colwert path from string to char[] until Bug 1271357 can be implemented
    strncpy(absolutePath, m_modsDirPath.c_str(),m_modsDirPath.length());

    // Set up paths, based on LD_LIBRARY_PATH or MODS_RUNSPACE
    strcpy(fullDatName, absolutePath);
    strcpy(csvName, absolutePath);
    strcpy(diffsName, absolutePath);
    strcpy(skipRegFile, absolutePath);
    strcpy(filterRegFileEnd, absolutePath);
    strcpy(filterRegFileMid, absolutePath);

    // Append file names based on OS
 #ifdef LW_WINDOWS
    strcat(csvName, "\\prod_values.csv");
    strcat(diffsName, "\\prod_diffs.csv");
    strcat(skipRegFile, "\\skip_regs_");
    strcat(filterRegFileEnd, "\\filter_regs_end_");
    strcat(filterRegFileMid, "\\filter_regs_mid_");
#else
    strcat(csvName, "/prod_values.csv");
    strcat(diffsName, "/prod_diffs.csv");
    strcat(skipRegFile, "/skip_regs_");
    strcat(filterRegFileEnd, "/filter_regs_end_");
    strcat(filterRegFileMid, "/filter_regs_mid_");
#endif

    // Add correct chip string
    strcat(skipRegFile, chipStr);
    strcat(filterRegFileEnd, chipStr);
    strcat(filterRegFileMid, chipStr);

    // End with ".txt"
    strcat(skipRegFile, ".txt");
    strcat(filterRegFileEnd, ".txt");
    strcat(filterRegFileMid, ".txt");

    // Append the relative datName to the absolute path for datName and try to open
    strcat(fullDatName, datName);
    if( (fprod = fopen(fullDatName,"rb" )) == NULL)
    {
        Printf(Tee::PriHigh, "Could not open the file %s\n", datName);
        return RC::CANNOT_OPEN_FILE;
    }
    if( (fpCsv = fopen(csvName,"w" )) == NULL)
    {
        Printf(Tee::PriHigh, "Could not open the file %s\n", csvName);
        fclose(fprod);
        return RC::CANNOT_OPEN_FILE;
    }
    if( (fpDiffs = fopen(diffsName,"w" )) == NULL)
    {
        Printf(Tee::PriHigh, "Could not open the file %s\n", diffsName);
        fclose(fprod);
        return RC::CANNOT_OPEN_FILE;
    }

    if( (fpSkipRegFile = fopen(skipRegFile,"r" )) == NULL)
    {
        Printf(Tee::PriHigh, "Could not open the file %s, so no register skipping enabled\n", skipRegFile);
    }
    else
    {
        char skipRegName[512];
        list<string>::iterator it;

        while(fgets(skipRegName, sizeof(skipRegName), fpSkipRegFile))
        {
            Printf(Tee::PriLow, "Skip Register len %d Register Name =%s\n", (int)strlen(skipRegName), skipRegName);
            skipRegName[strlen(skipRegName)-1] = 0; // get rid of newline
            if (skipRegName[0] != 0)
            {
                //
                // Get rid of leading and trailing spaces and tabs,
                // and remove in-line comments
                //
                RemoveComment(skipRegName);
                if (skipRegName[0] != 0)
                {
                    string theReg = skipRegName;
                    m_regSkipList.push_back(theReg);
                }
            }
        }

        for(it = m_regSkipList.begin(); it != m_regSkipList.end(); it++)
            Printf(Tee::PriHigh, "Skip Register %s\n", it->c_str());
    }

    if( (fpFilterRegFileMid = fopen(filterRegFileMid,"r" )) == NULL)
    {
        Printf(Tee::PriHigh, "Could not open the file %s, so no register filtering\n", filterRegFileMid);
    }
    else
    {
        char filtRegName[512];
        list<string>::iterator it;

        while(fgets(filtRegName, sizeof(filtRegName), fpFilterRegFileMid))
        {
            filtRegName[strlen(filtRegName)-1] = 0; // get rid of newline
            if (filtRegName[0] != 0)
            {
                //
                // Get rid of leading and trailing spaces and tabs,
                // and remove in-line comments
                //
                RemoveComment(filtRegName);
                if (filtRegName[0] != 0)
                {
                    string theFilt = filtRegName;
                    m_regFiltMidList.push_back(theFilt);
                }
            }
        }

        for(it = m_regFiltMidList.begin(); it != m_regFiltMidList.end(); it++)
            Printf(Tee::PriHigh, "Filter Registers with %s\n", it->c_str());
    }

    if( (fpFilterRegFileEnd = fopen(filterRegFileEnd,"r" )) == NULL)
    {
        Printf(Tee::PriHigh, "Could not open the file %s, so no register filtering\n", filterRegFileEnd);
    }
    else
    {
        char filtRegName[512];
        list<string>::iterator it;

        while(fgets(filtRegName, sizeof(filtRegName), fpFilterRegFileEnd))
        {
            filtRegName[strlen(filtRegName)-1] = 0; // get rid of newline
            if (filtRegName[0] != 0)
            {
                //
                // Get rid of leading and trailing spaces and tabs,
                // and remove in-line comments
                //
                RemoveComment(filtRegName);
                if (filtRegName[0] != 0)
                {
                    string theFilt = filtRegName;
                    m_regFiltEndList.push_back(theFilt);
                }
            }
        }

        for(it = m_regFiltEndList.begin(); it != m_regFiltEndList.end(); it++)
            Printf(Tee::PriHigh, "Filter registers with %s\n", it->c_str());
    }

    // Print the heading
    fprintf(fpCsv, "Register, Mismatch? (Y/N), StartBit:EndBit, Expected, Got\n");
    fflush(fpCsv);
    fprintf(fpDiffs, "Register, StartBit:EndBit, Expected, Got\n");
    fflush(fpDiffs);

    // Start reading structure from the opened file.
    while(fread(&reg,sizeof(reg),1,fprod))
    {
        skipit = 0;

        // Read the register name
        MASSERT(reg.nameLen < MAX_REGNAME_LEN);
        if (fread(regName, reg.nameLen, 1, fprod) != 1)
        {
            Printf(Tee::PriHigh, "Malformed data file %s, premature EOF\n", datName);
            return RC::FILE_READ_ERROR;
        }
        regName[reg.nameLen] = '\0';

        //
        // The only flags supported indicate that this is an index register.
        // Index registers require the writing of an index value to the
        // index address register before reading the register.
        //
        if (reg.flags)
        {
            if (fread(&regIdx, sizeof(regIdx), 1, fprod) != 1)
            {
                Printf(Tee::PriHigh, "Malformed data file %s, premature EOF\n", datName);
                return RC::FILE_READ_ERROR;
            }
        }

        if (m_regMatch != "")
        {
            if (strncmp(regName, m_regMatch.c_str(), m_regMatch.size()) != 0)
                skipit = 1;
        }

        // check skip regname filter
        Printf(Tee::PriLow, "Before END & MID pattern match..Register Name: %s\n", regName);
        if (!skipit)
        {
            list<string>::iterator it;
            for(it = m_regSkipList.begin(); it != m_regSkipList.end(); it++)
            {
                Printf(Tee::PriLow, "Filter registers with %s\n", it->c_str());
                if (strncmp(regName, it->c_str(), it->size()) == 0)
                {
                    skipit = 1;
                    break;
                }
            }
        }

        // check regname ending filters
        if (!skipit)
        {
            list<string>::iterator it;
            size_t                 filterLen;
            for(it = m_regFiltEndList.begin(); it != m_regFiltEndList.end(); it++)
            {
                Printf(Tee::PriLow, "Filter registers with %s\n", it->c_str());
                filterLen = strlen(it->c_str());
                // Instead of the last '_' (underscore char) find the possible
                // starting point for the match, which can be 'string length of
                // of the filter' before end of the 'regname' string.
                filterLast = &(regName[strlen(regName) - filterLen]);
                if (strncmp(filterLast, it->c_str(), filterLen) == 0)
                {
                    skipit = 1;
                    break;
                }
            }
        }

        // check regname mid filters
        if (!skipit)
        {
            list<string>::iterator it;
            for(it = m_regFiltMidList.begin(); it != m_regFiltMidList.end(); it++)
            {
                Printf(Tee::PriLow, "Filter registers with %s\n", it->c_str());
                if (strstr(regName, it->c_str()) != 0)
                {
                    skipit = 1;
                    break;
                }
            }
        }

        if (!skipit)
        {
            regPrint = false;
            diffsPrint = false;
            Printf(Tee::PriLow, "Reading %s(0x%x)\n", regName, reg.addr);
            regVal = pSubdev->RegRd32(reg.addr);
            for (i = 0; i < reg.fieldCnt; i++)
            {
                if (fread(&field, sizeof(field), 1, fprod) != 1)
                {
                    Printf(Tee::PriHigh, "Malformed data file %s, premature EOF\n", datName);
                    return RC::FILE_READ_ERROR;
                }

                MASSERT(field.nameLen < MAX_FIELDNAME_LEN);

                // Read the field name
                if (fread(fieldName, field.nameLen, 1, fprod) != 1)
                {
                    Printf(Tee::PriHigh, "Malformed data file %s, premature EOF\n", datName);
                    return RC::FILE_READ_ERROR;
                }
                fieldName[field.nameLen] = '\0';

                if (reg.flags)
                {
                    //
                    // For now skip all index registers because they are in dev_disp
                    // anyway and really shouldn't have prod values.
                    //
                    continue;
                }

                // Get the value of the field using the field description.
                mask = DRF_MASK((field.endBit):(field.startBit));
                shift = DRF_SHIFT((field.endBit):(field.startBit));
                fieldVal = (regVal >> shift) & mask;

                // Check for a mismatch
                if (field.value != fieldVal)
                {
                    fieldMismatch++;
                    //
                    // TOFIX: Finally when we build the exclusion list, return error
                    //        Until then silently ignore the errors.
                    //
                    rc = RC::SOFTWARE_ERROR;
                }

                if (!regPrint)
                {
                    fprintf(fpCsv,
                        "%s (0x%08x)\n", regName, reg.addr);
                    regPrint = true;
                }

                fprintf(fpCsv,
                    "    %s, %s, %d:%d, 0x%04x, 0x%04x\n",
                    fieldName,
                    (field.value != fieldVal ? "YES" : "NO"),
                    field.startBit,
                    field.endBit,
                    field.value,
                    fieldVal);
                fflush(fpCsv);

                if (field.value != fieldVal)
                {
                    if (!diffsPrint)
                    {
                        fprintf(fpDiffs,
                            "%s (0x%08x)\n", regName, reg.addr);
                        regMismatch++;
                        diffsPrint = true;
                    }
                    fprintf(fpDiffs,
                        "    %s, %d:%d, 0x%04x, 0x%04x\n",
                        fieldName,
                        field.startBit,
                        field.endBit,
                        field.value,
                        fieldVal);
                    fflush(fpDiffs);
                }
            }
        }
        else
        {
            for (i = 0; i < reg.fieldCnt; i++)
            {
                if (fread(&field, sizeof(field), 1, fprod) != 1)
                {
                    Printf(Tee::PriHigh, "Malformed data file %s, premature EOF\n", datName);
                    return RC::FILE_READ_ERROR;
                }

                // Read the field name
                if (fread(fieldName, field.nameLen, 1, fprod) != 1)
                {
                    Printf(Tee::PriHigh, "Malformed data file %s, premature EOF\n", datName);
                    return RC::FILE_READ_ERROR;
                }

            }
        }
    }

    if(!feof(fprod))
        return RC::FILE_READ_ERROR;

    fclose(fprod);
    fclose(fpCsv);
    fclose(fpDiffs);

    Printf(Tee::PriHigh, "Register Mismatches: %d\n", regMismatch);
    Printf(Tee::PriHigh, "Field Mismatches: %d\n", fieldMismatch);
    Printf(Tee::PriHigh, "Check file %s for details of all prod values\n", csvName);
    Printf(Tee::PriHigh, "Check file %s for details of differences\n", diffsName);

    if (regMismatch == 0)
        return OK;
    else
        return rc;
}

//!
//! Search the standard ELW directories for prod test files.
//! Prodtest will not always be ran from mods runspace, MODS_RUNSPACE may not
//! always be set.  Need to be able to run from HW tree,  and this is accomplished
//! by searching LD_LIBRARY_PATH
//!
//----------------------------------------------------------------------------
void ProdTest::FindFilePath(string file)
{
    vector<string> searchPath;
    string modsPath = "";
    ifstream *pInputFile = NULL;

    searchPath.push_back(".");

    // Get a list of paths within which we search for prod files
    Utility::AppendElwSearchPaths(&searchPath,"LD_LIBRARY_PATH");
    Utility::AppendElwSearchPaths(&searchPath, "MODS_RUNSPACE");

    for (vector<string>::const_iterator it = searchPath.begin(); it != searchPath.end(); ++it)
    {
        const string & dirname = *it;
        string fullpath = "";

        fullpath = dirname;
        fullpath +="/";
        fullpath += file;
        pInputFile = new ifstream (fullpath.c_str());

        if (pInputFile->is_open())
        {
            m_modsDirPath = dirname;
            // Found the dir name, since falling out of loop,  need to free here.
            delete pInputFile;
            pInputFile = NULL;
            break;
        }
        delete pInputFile;
        pInputFile = NULL;
    }
}

//!
//! Get rid of spaces and tabs from the beginning and end of the line
//! And make the line comment as soon as it finds '#' character.
//!
//----------------------------------------------------------------------------

void ProdTest::RemoveComment(char *str, char defaultIdentifier)
{
    size_t startIndex;
    size_t lastSpcLen;
    size_t len;
    size_t j;
    char c;

    // Find the first non-space, non-tab character
    len = strlen(str);
    startIndex = 0;
    while (startIndex < len)
    {
        if (str[startIndex] == ' ' || str[startIndex] == '\t')
        {
            startIndex++;
        }
        else
        {
            break;
        }
    }

    //
    // Move the rest of the string to the beginning.
    // If we find a '#', ignore that character and everything after it.
    // Also remove trailing spaces and tabs.
    //
    lastSpcLen = 0;
    for (j = 0; startIndex < len; startIndex++)
    {
        c = str[startIndex];
        if (c == defaultIdentifier)
        {
            break;
        }

        // How many trailing spaces/tabs do we have?
        if (c == ' ' || c == '\t')
        {
            lastSpcLen++;
        }
        else
        {
            lastSpcLen = 0;
        }

        str[j] = c;
        j++;
    }
    str[j - lastSpcLen] = '\0';
}

//!
//! Cleanup all the allocated resources that the test selwred during Setup.
//! Cleanup can be called immediately if Setup fails or Run fails.
//!
//! \sa Setup
//----------------------------------------------------------------------------
RC ProdTest::Cleanup()
{
    m_3dAlloc->Free();
    m_TestConfig.FreeChannel(m_pCh);
    delete m_3dAlloc;
    return OK;
}
//----------------------------------------------------------------------------
// JS Linkage
//----------------------------------------------------------------------------

//! \brief Linkage to JavaScript
//!
//! Set up a JavaScript class that creates and owns a C++ AllocObjectTest
//! object.
//!
//----------------------------------------------------------------------------
JS_CLASS_INHERIT(ProdTest, RmTest,
                 "Gpu PROD Setting test.");
CLASS_PROP_READWRITE(ProdTest,regMatch,string,
                     "run the regname input");

