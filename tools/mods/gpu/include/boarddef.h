/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//! \@file boarddef.h -- define a database for gpu board feature descriptions.

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_BOARDDEF_H
#define INCLUDED_BOARDDEF_H

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif
#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif
#ifndef INCLUDED_STL_MEMORY
#include <memory>
#define INCLUDED_STL_MEMORY
#endif
#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_JSCRIPT_H
#include "core/include/jscript.h"
#endif
#include "core/include/gpu.h"

class GpuSubdevice;
struct JSObject;

class BoardDB
{
public:
    // Type definitions:

    enum PhysicalMatchType
    {
        PERFECT,
        SVID_GENERIC,
        GENERIC
    };

    //! Enum that describes how certain strings should be handled
    //! when performing a physical match
    enum StringMatchMethod
    {
        REGEX_THIS,   //!< Match using "this" as a regular expression
        REGEX_THAT,   //!< Match using "that" as a regular expression
        EXACT         //!< Exact string match
    };

    struct OfficialChipSKU
    {
        string m_Name;
        bool   m_Internal;
    };

    //! The part of the board def that identifies a unique board.
    //! This is the database key for the board.
    struct Signature
    {
        Signature();
        UINT32 m_Version;
        UINT32 m_Intbanks;
        UINT32 m_Rows;
        UINT32 m_Columns;
        UINT32 m_PciDevId;          // obsolete v2
        bool   m_Has5BitPciDevId;   // obsolete v2
        UINT32 m_GpuId;
        UINT32 m_Straps;            // obsolete v2
        UINT32 m_FBBar;
        UINT32 m_SVID;
        UINT32 m_SSID;
        UINT32 m_BoardId;
        string m_BoardProjectAndSku;
        string m_VBIOSChipName;
        vector<OfficialChipSKU> m_OfficialChipSKUs; // added v2
        string m_PciClassCode;
    };

    //! The info about a board (once identified), that ValidSkuTest can check.
    //! This is the rest of the data about the board.
    struct OtherInfo
    {
        OtherInfo();
        UINT32 m_FBBus;
        UINT32 m_Extbanks;
        UINT32 m_PciExpressLanes;
        vector<string> m_OfficialChipSKUs;
        vector<bool> m_RegBits;
        bool m_Plx;

        bool m_UpStreamDepthInfoPresent;
        UINT32 m_UpStreamDepth;
        bool m_ExternalDPInfoPresent;
        UINT32 m_NumExternalDPs;

        bool m_InfoRomPresent;
        JSObject * m_pJsChilInfo;
        JSObject * m_pJsPwrSensorInfo;
        JSObject * m_pJsPrimarionInfo;
        JSObject * m_pJsHybridInfo;
        JSObject * m_pJsGc6Info;
        JSObject * m_pJsInaInfo;
        JSObject * m_pJsRgbMlwInfo;
    };

    //! The entire board description.
    struct BoardEntry
    {
        BoardEntry();
        Signature m_Signature;
        OtherInfo m_OtherInfo;
        JSObject * m_JSData;

        string m_BoardName;
        UINT32 m_BoardDefIndex; //BoardDef Index within one boards.js entry

        void Print( bool onlySignature, Tee::Priority outputPri ) const;
    };

    // Methods:

    //! Called from JavaScript.
    static RC Parse();

    //! Read-only access to the singleton.
    static const BoardDB & Get() { return s_Instance; };

    //! Print all parsed board descriptions to the mods logfile.
    RC PrintParsedBoards() const;

    //! Return the possible matches for the board this gpu is attached to.
    RC GetPhysicalMatch(GpuSubdevice *pGpuSubdev,
                        PhysicalMatchType matchType,
                        StringMatchMethod method,
                        vector<BoardEntry >* pMatchedBoardEntries) const;

    //! Get the database key for the board this gpu is attached to.
    RC GetDUTSignature(GpuSubdevice *pGpuSubdev, Signature* pSig) const;

    UINT32 GetDUTSupportedVersion(GpuSubdevice *pGpuSubdev) const;

    //! Get the board description at a given index.
    const BoardEntry* GetBoardEntry(Gpu::LwDeviceId GpuId, string BoardName,
                                    UINT32 Index) const;

    bool   BoardChangeRequested() const { return m_bBCRPresent; }
    UINT32 GetBCRVersion() const { return m_bBCRVersion; }
    RC     GetWebsiteEntry(const BoardEntry **ppWebsiteEntry) const;
    RC     ConfirmTestRequired(string testName, bool *pbConfReq) const;
    RC     ConfirmEntry() const;

    // Any non-const functions to BoardDB must be static to prevent exposing a
    // non-const pointer to the database and have external functions modify it
    // in unexpected ways
    static void ConfirmBoard(const string &BoardName, UINT32 BoardIndex);
    static void LogTest(string testName, UINT32 rc);
    static void SetDUTVersionOverride(UINT32 version);
    static void SetResultsFile(const string &filename);
    static void AllowInternalSkus(bool bAllowInternal);

    ~BoardDB();

private:
    struct Impl;
    unique_ptr<Impl> m_pImpl;

    static BoardDB s_Instance;

    bool           m_bBCRParsed;
    bool           m_bBCRPresent;
    UINT32         m_bBCRVersion;
    string         m_BCRequestString;
    JsArray        m_BCRequestCmds;
    string         m_BCBoardString;
    string         m_BCTestsString;
    string         m_BCResultFilename;
    bool           m_bAllowInternalSkus;

    UINT32         m_DutVersionOverride;

    //! Private constructor -- this is a singleton class.
    BoardDB();

    //! Not implemented -- disables copy constructor.
    BoardDB(const BoardDB &);
    //! Not implemented -- disables assignment.
    BoardDB & operator=(const BoardDB &);

    bool DoRegexMatch
    (
        const string &thisString,
        const string &thatString,
        BoardDB::StringMatchMethod method
    ) const;
    void PrintInfo
    (
        Tee::Priority pri,
        Gpu::LwDeviceId gpuId,
        const string& boadname,
        const vector<BoardEntry>& boardVector
    ) const;
    bool MatchSignature
    (
        const Signature& thisSig,
        const Signature& refSig,
        StringMatchMethod method,
        PhysicalMatchType matchType
    ) const;
    RC ParseBoardChangeRequest();
    void InternalConfirmBoard(const string &BoardName, UINT32 BoardIndex);
    void InternalLogTest(string testName, UINT32 rc);
    void InternalSetDUTVersionOverride(UINT32 version);
    void InternalSetResultsFile(const string &filename);
    void InternalAllowInternalSkus(bool bAllowInternal);
};

#endif
