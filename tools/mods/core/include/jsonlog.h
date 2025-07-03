/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_JSONLOG_H
#define INCLUDED_JSONLOG_H

#ifndef INCLUDED_JSCRIPT_H
#include "jscript.h"
#endif

#include "setget.h"

//------------------------------------------------------------------------------
//! A JsonItem is a JavaScript Object to be stored in modslog.jso.
//!
//! It has one required field, the "tag" field, with the boolean value true.
//!
//! Each tag defines a particular JsonItem type, with a set of required other
//! fields which must be present.
//! See https://wiki.lwpu.com/engwiki/index.php/MODS/JSON_logs.
//!
//! A JsonItem may have extra fields in addition to the required list.
//! Fields may have array values.
//!
class JsonItem
{
public:
    //! Creates a JS object as initiated from JavaScript.
    JsonItem (JSContext * ctx, JSObject * pNoParent, JSObject * pThisObj);

    //! Creates a new JS object as a property of some existing JSObject.
    JsonItem (JSContext * ctx, JSObject * pParent, const char * propName);

    //! Creates a new JsonItem that will only be used from C++, not JS
    JsonItem ();

    ~JsonItem();

    //! Bits representing logging categories for IsAllowedExternal.
    enum JsonCategory
    {
        JSONLOG_DEFAULT            = 0,
        JSONLOG_PEXTOPOLOGY        = 1,
        JSONLOG_PEXINFO            = 2,
        JSONLOG_INFOROM            = 3,
        JSONLOG_LWLINKTOPOLOGY     = 4,
        JSONLOG_LWLINKINFO         = 5,
        JSONLOG_GPUINFO            = 6,
        JSONLOG_TESTENTRY          = 7,
        JSONLOG_TESTEXIT           = 8,
        JSONLOG_CARRIERFRU         = 9,
    };

    RC WriteToLogfile();
    RC Stringify(string* pStr);

    void SetTag (const char * tagname);
    void SetLwrDateTimeField (const char * name);
    void SetCategory (JsonItem::JsonCategory category);
    void SetUniqueIntField (const char * name);
    void AddFailLoop (UINT32 loop);
    void SetField (const char * name, bool);
    void SetField (const char * name, UINT32);
    void SetField (const char * name, INT32);
    void SetField (const char * name, UINT64);
    void SetField (const char * name, INT64);
    void SetField (const char * name, double);
    void SetField (const char * name, const char *);
    void SetField (const char * name, JsArray *);
    void SetField (const char * name, JSObject *);
    void SetField (const char * name, JsonItem *);

    void RemoveField (const char * name);
    void CopyAllFields (const JsonItem& jsi);
    void RemoveAllFields ();

    // -json_limited related functions
    void JsonLimitedSetAllowedField(string field);
    bool JsonLimitedIsAllowedField(string field);
    void JsonLimitedRemoveUnallowedFields();

    JsonItem::JsonCategory GetCategory() const   { return m_Category; }
    JSContext *  GetCtx() const   { return m_pCtx; }
    JSObject *   GetObj() const   { return m_pObj; }
    const char * GetTag () const  { return m_Tag.c_str(); }

    static void FreeSharedResources();

    SETGET_PROP(JsonLimitedAllowAllFields, bool);

private:
    void SetField (const char * name, jsval v, int flags);

    string                       m_Tag = "";
    JSContext *                  m_pCtx;
    JSObject *                   m_pParent;
    JSObject *                   m_pObj;
    JsonItem::JsonCategory       m_Category = JsonItem::JSONLOG_DEFAULT;
    bool                         m_FreedByGC;
    bool                         m_JsonLimitedAllowAllFields = false;
    vector<string>               m_JsonLimitedAllowedFields;

    static int GetUniqueInt();

    static void *   s_Mutex;
    static int      s_UniqueCounter;
};

//------------------------------------------------------------------------------
//! JsonLogStream -- used by JsonItem, writes a "mods.jso" file.
//!
//! Most of this namespace should only be used by JsonItem or by JS interfaces.
//! So the namespace is mostly declared only in jsonlog.cpp.
//------------------------------------------------------------------------------
namespace JsonLogStream
{
    //! If json logfile is enabled, write the "final_rc" tag to it.
    //! Takes a string, for printing a full 12-digit code.
    //!
    //! Does not use JavaScript or Tasker resources (can be called late
    //! in the mods shutdown sequence).
    void WriteFinalRc(string finalRc);

    //! If json logfile is enabled, write the "final_rc" tag to it.
    //! Taks an INT32, for printing just an RC.
    //!
    //! Does not use JavaScript or Tasker resources (can be called late
    //! in the mods shutdown sequence).
    void WriteFinalRc(INT32 finalRc);

    //! Returns true if the json log file is open, false otherwise.
    bool IsOpen();

    //! Returns true if we can log to limited logs based on logging type, false otherwise.
    bool IsAllowedExternally(JsonItem::JsonCategory logType);

    //! Write MODS startup timestamp as a jsonItem
    void WriteInitialTimestamp(const string & ts, FileSink * pFilesink);

    //! Write MODS exit timestamp & runtime as a jsonItem
    void WriteFinalTimestamp(const string & ts, const string & rt);

    //! Encrypted stream status.
    enum LogType
    {
        JSONLOG_USE_FULL              = 0,
        JSONLOG_USE_LIMITED           = 1,
    };

};

#endif // INCLUDED_JSONLOG_H
