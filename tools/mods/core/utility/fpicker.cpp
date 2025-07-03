/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file fpicker.cpp
 * @brief implement class FancyPicker.
 */

#include "core/include/fpicker.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include <algorithm>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <cerrno>
#include <limits>

//@{
/**
 * WHY MACROS INSTEAD OF CONST?
 *
 * GCC computes initializer expressions such as (1.0 / TwoExp32) at compile time.
 * VC++ doesn't compute them until runtime (pre-main).
 *
 * Using a macro for TwoExp32Ilw sidesteps this problem.
 */
const double TwoExp32         = 4294967296.0;      // 0x100000000 (won't fit in UINT32)
const double TwoExp32Less1    = 4294967295.0;      //  0xffffffff
#define TwoExp32Ilw           (1.0/TwoExp32)
#define TwoExp32Less1Ilw      (1.0/TwoExp32Less1)
// const double TwoExp32Ilw       = 1.0 / TwoExp32;
// const double TwoExp32Less1Ilw  = 1.0 / TwoExp32Less1;
//@}

//------------------------------------------------------------------------------
//
// helper classes
//
//------------------------------------------------------------------------------

class FpHelper
{
public:
    union IorF
    {
        UINT64 i;
        float  f;
        IorF() { i = 0ULL; }
    };

public:
    FpHelper(FancyPicker::FpContext * pFpCtx);
    FpHelper(const FpHelper * pFpHelper);
    void SetName(string name);
    const char* GetName() const { return m_Name.c_str(); }
    virtual ~FpHelper();
    virtual bool   ConstructorOK() const;
    virtual void   Restart();
    virtual void   Pick();
    virtual void   FPick();
    virtual bool   Ready() const;
    virtual FancyPicker::eMode       Mode() const;
    virtual FancyPicker::eWrapMode   WrapMode() const;
    virtual FpHelper* Clone() const;
    virtual void Compile();
    virtual void AddItem (UINT32 Count, UINT64 Min, UINT64 Max);
    virtual void FAddItem (UINT32 Count, float Min, float Max);
    virtual RC   GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const;
    virtual RC GetNumItems(UINT32 *pNumItems) const;
    virtual RC GetRawItems(FancyPicker::PickerItems *pItems) const;
    virtual RC GetFRawItems(FancyPicker::PickerFItems *pItems) const;
    virtual void RecordPick() { }
    virtual RC   CheckUsed() const { return RC::OK; }
public:
    // Public data, yuck.
    // Still, only FancyPicker and FpHelper derivatives can see this
    // declaration.

    IorF                     m_Value;
    FancyPicker::FpContext * m_pFpCtx;
    bool                     m_PickOnRestart;
    FancyPicker::eType       m_Type;

protected:
    static RC CheckUsedPicks(const char* name, const vector<bool>& picks, UINT32 numItems);

private:
    string m_Name;
};

//------------------------------------------------------------------------------
class RandomFpHelper : public FpHelper
{
public:
    RandomFpHelper(const FpHelper * pFpHelper);
    virtual ~RandomFpHelper();

    virtual bool   ConstructorOK() const;
    virtual void   Restart();
    virtual void   Pick();
    virtual void   FPick();
    virtual bool   Ready() const;
    virtual FancyPicker::eMode       Mode() const;
    virtual FancyPicker::eWrapMode   WrapMode() const;
    virtual FpHelper* Clone() const;

    virtual void AddItem (UINT32 Count, UINT64 Min, UINT64 Max);
    virtual void FAddItem (UINT32 Count, float Min, float Max);
    virtual RC   GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const;
    virtual RC GetNumItems(UINT32 *pNumItems) const;
    virtual RC GetRawItems(FancyPicker::PickerItems *pItems) const;
    virtual RC GetFRawItems(FancyPicker::PickerFItems *pItems) const;
    virtual void RecordPick();
    virtual RC   CheckUsed() const { return CheckUsedPicks(GetName(), m_Picks, m_NumItems); }

    friend RC FancyPicker::ToJsval(jsval*) const;     // FancyPicker::ToJsval directly reads our data.

    virtual void Compile();

private:
    struct Item
    {
        UINT32   Prob;         //!< probability, aclwmulative, scaled to 2**32
                               //!<   BEFORE COMPILE: relative probability
        IorF     Min;          //!< minimum value (float or UINT32)
        double   BinScaler;    //!<
        double   Scaler;       //!< (max-min+1)/(2**32)  (0.0 if AddOne())
        double   ExtraScaler;  //!<
        UINT32   ExtraBins;    //!<
        UINT64   BinSize;      //!<
    };

    enum State
    {
        Empty                //!< No Add* calls have been made.
        ,IntRaw              //!< UINT32 items added, Compile() before Pick().
        ,IntCompiled         //!< UINT32 items added, ready to Pick().
        ,FloatRaw            //!< float items added, Compile() before FPick().
        ,FloatCompiled       //!< float items added, ready to FPick().
    };

    Item *   m_Items;       //!< array of items
    int      m_NumItems;    //!< num added so far
    int      m_SizeItems;   //!< capacity of m_Items array
    State    m_State;       //!< current state.
    UINT32   m_RelProbSum;  //!< sum of relative probabilities

    vector<bool> m_Picks;
    UINT32       m_LastPick = 0;
};

//------------------------------------------------------------------------------
class ShuffleFpHelper : public FpHelper
{
public:
    ShuffleFpHelper(const FpHelper * pFpHelper);
    virtual ~ShuffleFpHelper();

    virtual bool   ConstructorOK() const;
    virtual void   Restart();
    virtual void   Pick();
    virtual void   FPick();
    virtual bool   Ready() const;
    virtual FancyPicker::eMode       Mode() const;
    virtual FancyPicker::eWrapMode   WrapMode() const;
    virtual FpHelper* Clone() const;
    virtual void Compile();
    virtual void AddItem (UINT32 Count, UINT64 Min, UINT64 Max);
    virtual void FAddItem (UINT32 Count, float Min, float Max);
    virtual RC   GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const;
    virtual RC GetNumItems(UINT32 *pNumItems) const;
    virtual RC GetRawItems(FancyPicker::PickerItems *pItems) const;
    virtual RC GetFRawItems(FancyPicker::PickerFItems *pItems) const;
    virtual void RecordPick();
    virtual RC   CheckUsed() const { return CheckUsedPicks(GetName(), m_Picks, m_NumItems); }

    friend RC FancyPicker::ToJsval(jsval*) const;     // FancyPicker::ToJsval directly reads our data.

private:
    void GrowItems();
    void MakeDeck();
    void Shuffle();

    struct Item
    {
        UINT32   Count;      //!< Num copies of this item in deck
        IorF     Val;        //!< value (float or UINT32)
    };

    enum State
    {
        Empty                //!< No Add* calls have been made.
        ,IntRaw              //!< UINT32 items added, Compile() before Pick().
        ,IntCompiled         //!< UINT32 items added, ready to Pick().
        ,FloatRaw            //!< float items added, Compile() before FPick().
        ,FloatCompiled       //!< float items added, ready to FPick().
    };

    Item *   m_Items;       //!< array of items
    int      m_NumItems;    //!< num added so far
    int      m_SizeItems;   //!< capacity of m_Items array
    IorF *   m_Deck;        //!< m_TotalCount items
    IorF *   m_DeckNext;    //!< next item to use from m_Deck
    State    m_State;       //!< current state.
    UINT32   m_TotalCount;  //!< sum of per-item Counts

    vector<bool> m_Picks;

    static const UINT32 s_MaxReasonableTotalCount;
};

//------------------------------------------------------------------------------
class StepFpHelper : public FpHelper
{
public:
    StepFpHelper(const FpHelper * pFpHelper, UINT64 Begin, INT64 Step, UINT64 End, FancyPicker::eWrapMode wm);
    StepFpHelper(const FpHelper * pFpHelper, float  Begin, float Step, float  End, FancyPicker::eWrapMode wm);
    virtual ~StepFpHelper();

    virtual bool   ConstructorOK() const;
    virtual void   Restart();
    virtual void   Pick();
    virtual void   FPick();
    virtual bool   Ready() const;
    virtual FancyPicker::eMode       Mode() const;
    virtual FancyPicker::eWrapMode   WrapMode() const;
    virtual FpHelper* Clone() const;
    virtual RC     GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const;

    friend RC FancyPicker::ToJsval(jsval*) const;     // FancyPicker::ToJsval directly reads our data.

private:
    union SIorF
    {
        INT64  i;
        float  f;
        SIorF() { i = 0ULL; }
    };
    SIorF    m_Begin;
    SIorF    m_Step;
    SIorF    m_End;
    UINT32   m_MaxStepNum;
    FancyPicker::eWrapMode m_WrapMode;
};

//------------------------------------------------------------------------------
class ListFpHelper : public FpHelper
{
public:
    ListFpHelper(const FpHelper * pFpHelper, FancyPicker::eWrapMode wm);
    virtual ~ListFpHelper();

    virtual bool   ConstructorOK() const;
    virtual void   Restart();
    virtual void   Pick();
    virtual void   FPick();
    virtual bool   Ready() const;
    virtual FancyPicker::eMode       Mode() const;
    virtual FancyPicker::eWrapMode   WrapMode() const;
    virtual FpHelper* Clone() const;
    virtual RC     GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const;
    virtual RC GetNumItems(UINT32 *pNumItems) const;
    virtual RC GetRawItems(FancyPicker::PickerItems *pItems) const;
    virtual RC GetFRawItems(FancyPicker::PickerFItems *pItems) const;

    void AddItemRepeat (UINT64 Item, int RepeatCount);
    void FAddItemRepeat (float Item, int RepeatCount);

    friend RC FancyPicker::ToJsval(jsval*) const;     // FancyPicker::ToJsval directly reads our data.

private:
    struct Item
    {
        UINT32 Rpt;
        IorF   Val;
    };

    Item *   m_Items;       //!< Allocated array of items
    int      m_SizeItems;   //!< Size of allocated array
    int      m_NumItems;    //!< number of items added so far
    UINT32   m_RptSum;      //!< sum of Rpt of all Items.

    FancyPicker::eWrapMode m_WrapMode;
};

//------------------------------------------------------------------------------
class JsFpHelper : public FpHelper
{
public:
    JsFpHelper(const FpHelper * pFpHelper, jsval pick, jsval restart);
    virtual ~JsFpHelper();
    virtual bool   ConstructorOK() const;
    virtual void   Restart();
    virtual void   Pick();
    virtual void   FPick();
    virtual bool   Ready() const;
    virtual FancyPicker::eMode       Mode() const;
    virtual FancyPicker::eWrapMode   WrapMode() const;
    virtual FpHelper* Clone() const;
    virtual RC     GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const;

    friend RC FancyPicker::ToJsval(jsval*) const;     // FancyPicker::ToJsval directly reads our data.

private:
    jsval m_Pick;
    jsval m_Restart;
};

//------------------------------------------------------------------------------
//
// class FancyPicker
//
//------------------------------------------------------------------------------

// class static data
FancyPicker::FpContext  FancyPicker::s_DefaultContext;

//
// constructor, destructor
//
FancyPicker::FancyPicker()
  : m_OverridenHelper(nullptr),
    m_SetFromJs(false)
{
    m_Helper = new FpHelper(&s_DefaultContext);
}
FancyPicker::FancyPicker(const FancyPicker& fPicker)
  : m_Helper(fPicker.m_Helper->Clone()),
    m_OverridenHelper(nullptr),
    m_SetFromJs(fPicker.m_SetFromJs)
{
}
FancyPicker::~FancyPicker()
{
    StopOverride();
    delete m_Helper;
}

void FancyPicker::SetName(const char* name)
{
    if (m_Helper)
    {
        m_Helper->SetName(name);
    }

    m_DefaultName = name;
}

void FancyPicker::SetHelperName(const char* name)
{
    if (m_Helper)
    {
        if (name[0] == 0 && m_Helper->GetName()[0] == 0)
        {
            m_Helper->SetName(m_DefaultName.c_str());
        }
        else
        {
            m_Helper->SetName(name);
        }
    }
}

//
//! Configure to always return the same value.
/**
 * Equivalent to STEP mode with step = 0.
 */
RC FancyPicker::ConfigConst (UINT32 value)
{
    RC rc = ConfigStep(value, 0, value, CLAMP);
    m_Helper->m_PickOnRestart = true;
    m_Helper->Pick();
    return rc;
}
RC FancyPicker::ConfigConst64(UINT64 value)
{
    RC rc = ConfigStep64(value, 0, value, CLAMP);
    m_Helper->m_PickOnRestart = true;
    m_Helper->Pick();
    return rc;
}

RC FancyPicker::FConfigConst(float value)
{
    RC rc = FConfigStep(value, 0.0f, value, CLAMP);
    m_Helper->m_PickOnRestart = true;
    m_Helper->FPick();
    return rc;
}

//
// Configure for RANDOM operation
//
RC FancyPicker::ConfigRandom(const char* name)
{
    RC rc;
    CHECK_RC(ConfigRandom64(name));
    m_Helper->m_Type = T_INT;
    return OK;
}
RC FancyPicker::ConfigRandom64(const char* name)
{
    RandomFpHelper * pNewHelper = new RandomFpHelper(m_Helper);
    if ((!pNewHelper) || (!pNewHelper->ConstructorOK()))
        return RC::CANNOT_ALLOCATE_MEMORY;

    delete m_Helper;
    m_Helper = pNewHelper;
    m_Helper->m_Type = T_INT64;

    SetHelperName(name);

    return OK;
}
RC FancyPicker::ConfigShuffle(const char* name)
{
    RC rc;
    CHECK_RC(ConfigShuffle64(name));
    m_Helper->m_Type = T_INT;
    return OK;
}
RC FancyPicker::ConfigShuffle64(const char* name)
{
    ShuffleFpHelper * pNewHelper = new ShuffleFpHelper(m_Helper);
    if ((!pNewHelper) || (!pNewHelper->ConstructorOK()))
        return RC::CANNOT_ALLOCATE_MEMORY;

    delete m_Helper;
    m_Helper = pNewHelper;
    m_Helper->m_Type = T_INT64;

    SetHelperName(name);

    return OK;
}
void FancyPicker::CompileRandom ()
{
    MASSERT(m_Helper);
    MASSERT(m_Helper->m_Type != T_UNKNOWN);
    m_Helper->Compile();
}
void FancyPicker::AddRandItem(UINT32 RelativeProbability, UINT32 Item)
{
    AddRandRange64(RelativeProbability, Item, Item);
}
void FancyPicker::AddRandRange(UINT32 RelativeProbability, UINT32 Min, UINT32 Max)
{
    AddRandRange64(RelativeProbability, Min, Max);
}
void FancyPicker::AddRandItem64(UINT32 RelativeProbability, UINT64 Item)
{
    AddRandRange64(RelativeProbability, Item, Item);
}
void FancyPicker::AddRandRange64(UINT32 RelativeProbability, UINT64 Min, UINT64 Max)
{
    MASSERT(m_Helper);
    MASSERT((m_Helper->m_Type == T_INT) || (m_Helper->m_Type == T_INT64));

    if( Max < Min )
    {
        UINT64 tmp = Max;
        Max = Min;
        Min = tmp;
    }
    m_Helper->AddItem(RelativeProbability, Min, Max);
}
RC FancyPicker::FConfigRandom(const char* name)
{
    RandomFpHelper * pNewHelper = new RandomFpHelper(m_Helper);
    if ((!pNewHelper) || (!pNewHelper->ConstructorOK()))
        return RC::CANNOT_ALLOCATE_MEMORY;

    delete m_Helper;
    m_Helper = pNewHelper;
    m_Helper->m_Type = T_FLOAT;

    SetHelperName(name);

    return OK;
}
RC FancyPicker::FConfigShuffle(const char* name)
{
    ShuffleFpHelper * pNewHelper = new ShuffleFpHelper(m_Helper);
    if ((!pNewHelper) || (!pNewHelper->ConstructorOK()))
        return RC::CANNOT_ALLOCATE_MEMORY;

    delete m_Helper;
    m_Helper = pNewHelper;
    m_Helper->m_Type = T_FLOAT;

    SetHelperName(name);

    return OK;
}
void FancyPicker::FAddRandItem  (UINT32 RelativeProbability, float  Item)
{
    FAddRandRange(RelativeProbability, Item, Item);
}
void FancyPicker::FAddRandRange (UINT32 RelativeProbability, float  Min, float  Max)
{
    MASSERT(m_Helper);
    MASSERT(m_Helper->m_Type == T_FLOAT);

    if( Max < Min )
    {
        float tmp = Max;
        Max = Min;
        Min = tmp;
    }
    m_Helper->FAddItem(RelativeProbability, Min, Max);
}

//
// configure for STEP operation
//
RC FancyPicker::ConfigStep(UINT32 Begin, int Step, UINT32 End, eWrapMode wm)
{
    RC rc;
    CHECK_RC(ConfigStep64(Begin, Step, End, wm));
    m_Helper->m_Type = T_INT;
    return OK;
}
RC FancyPicker::ConfigStep64(UINT64 Begin, INT64 Step, UINT64 End, eWrapMode wm)
{
    StepFpHelper * pNewHelper =
        new StepFpHelper(m_Helper, Begin, Step, End, wm);
    if ((!pNewHelper) || (!pNewHelper->ConstructorOK()))
        return RC::CANNOT_ALLOCATE_MEMORY;

    delete m_Helper;
    m_Helper = pNewHelper;
    m_Helper->m_Type = T_INT64;

    return OK;
}
RC FancyPicker::FConfigStep(float Begin, float Step, float End, eWrapMode wm)
{
    StepFpHelper * pNewHelper =
        new StepFpHelper(m_Helper, Begin, Step, End, wm);
    if ((!pNewHelper) || (!pNewHelper->ConstructorOK()))
        return RC::CANNOT_ALLOCATE_MEMORY;

    delete m_Helper;
    m_Helper = pNewHelper;
    m_Helper->m_Type = T_FLOAT;

    return OK;
}

//
// configure for LIST operation
//
RC FancyPicker::ConfigList(eWrapMode wm)
{
    RC rc;
    CHECK_RC(ConfigList64(wm));
    m_Helper->m_Type = T_INT;
    return OK;
}
RC FancyPicker::ConfigList64(eWrapMode wm)
{
    ListFpHelper * pNewHelper = new ListFpHelper(m_Helper, wm);
    if ((!pNewHelper) || (!pNewHelper->ConstructorOK()))
        return RC::CANNOT_ALLOCATE_MEMORY;

    delete m_Helper;
    m_Helper = pNewHelper;
    m_Helper->m_Type = T_INT64;

    return OK;
}
void FancyPicker::AddListItem  (UINT32 Item, int RepeatCount /* = 1 */)
{
    AddListItem64(Item, RepeatCount);
}
void FancyPicker::AddListItem64  (UINT64 Item, int RepeatCount /* = 1 */)
{
    MASSERT(m_Helper);
    MASSERT((m_Helper->m_Type == T_INT) || (m_Helper->m_Type == T_INT64));
    MASSERT(m_Helper->Mode() == LIST);

    ListFpHelper * pLH = (ListFpHelper *)m_Helper;

    pLH->AddItemRepeat(Item, RepeatCount);
}
RC FancyPicker::FConfigList(eWrapMode wm)
{
    ListFpHelper * pNewHelper = new ListFpHelper(m_Helper, wm);
    if ((!pNewHelper) || (!pNewHelper->ConstructorOK()))
        return RC::CANNOT_ALLOCATE_MEMORY;

    delete m_Helper;
    m_Helper = pNewHelper;
    m_Helper->m_Type = T_FLOAT;

    return OK;
}
void FancyPicker::FAddListItem (float  Item, int RepeatCount /* = 1 */)
{
    MASSERT(m_Helper);
    MASSERT(m_Helper->m_Type == T_FLOAT);
    MASSERT(m_Helper->Mode() == LIST);

    ListFpHelper * pLH = (ListFpHelper *)m_Helper;

    pLH->FAddItemRepeat(Item, RepeatCount);
}

//
// configure for JS operation
//
RC FancyPicker::ConfigJs  (jsval funcPtr, jsval restartPtr /* = 0 */)
{
    RC rc;
    CHECK_RC(Config64Js(funcPtr, restartPtr));
    m_Helper->m_Type = T_INT;
    return OK;
}
RC FancyPicker::Config64Js  (jsval funcPtr, jsval restartPtr /* = 0 */)
{
    JsFpHelper * pNewHelper = new JsFpHelper(m_Helper, funcPtr, restartPtr);
    if ((!pNewHelper) || (!pNewHelper->ConstructorOK()))
        return RC::CANNOT_ALLOCATE_MEMORY;

    delete m_Helper;
    m_Helper = pNewHelper;
    m_Helper->m_Type = T_INT64;

    return OK;
}
RC FancyPicker::FConfigJs (jsval funcPtr, jsval restartPtr /* = 0 */)
{
    JsFpHelper * pNewHelper = new JsFpHelper(m_Helper, funcPtr, restartPtr);
    if ((!pNewHelper) || (!pNewHelper->ConstructorOK()))
        return RC::CANNOT_ALLOCATE_MEMORY;

    delete m_Helper;
    m_Helper = pNewHelper;
    m_Helper->m_Type = T_FLOAT;

    return OK;
}

//
// If this mode is enabled, we generate a new value in the sequence only
// on the first Pick after each Restart, then always return that value
// on all subsequent Pick calls.
//
void FancyPicker::SetPickOnRestart(bool enable)
{
    m_Helper->m_PickOnRestart = enable;
}

//
// Set the FpContext to be used during calls to Pick().
//
void FancyPicker::SetContext(FpContext * pFpCtx)
{
    MASSERT(pFpCtx);
    m_Helper->m_pFpCtx = pFpCtx;
}

//
// UnConfig(), then configure the picker from a jsval array.
// If type is T_UNKNOWN, guesses from JS, else retains old T_INT or T_FLOAT.
// Returns error if the jsval is invalid or type mismatch.
//
RC FancyPicker::FromJsval(jsval js)
{
    RC             rc;
    JavaScriptPtr  pJavaScript;
    eMode          mode = UNINITIALIZED;
    bool           por  = false;  // default, if no "PickOnReset" string in jsval
    eWrapMode      wrap = WRAP;   // default, if no "wrap" or "clamp" string in jsval
    eType          type = m_Helper->m_Type; // default is to retain current type.

    //
    // Get the outer array.
    //
    JsArray OuterArray;
    size_t  OuterSize;
    size_t  OuterIndex;

    if(OK != (rc = pJavaScript->FromJsval(js, &OuterArray)))
    {
        Printf(Tee::PriHigh, "FancyPicker spec is not an array.\n");
        return rc;
    }
    OuterSize = OuterArray.size();
    OuterIndex = 0;

    //
    // Get all the string options.
    //
    string tmpString;
    bool BadStringFound = false;
    while ((OuterIndex < OuterSize) && JSVAL_IS_STRING(OuterArray[OuterIndex]))
    {
        //
        // Copy string object into char array, colwert to lower case.
        // Break now if the string is longer than "PickOnRestart", which
        // is the longest legal string.
        //
        JSString *jsStr = nullptr;
        CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex], &jsStr));
        {
            JSContext *cx = nullptr;
            CHECK_RC(pJavaScript->GetContext(&cx));
            tmpString = DeflateJSString(cx, jsStr);
        }

        OuterIndex++;

        const int s_size = sizeof("18446744073709551616");
        char s[s_size];

        if (tmpString.size() > (s_size-1))
        {
            BadStringFound = true;
            break;
        }
        size_t i;
        for (i = 0; i < tmpString.size(); i++)
            s[i] = tolower(tmpString[i]);
        s[i] = '\0';

        //
        // Check the string against each legal string.
        // Use a linear search, since the list is so short.
        // Check the more common strings first.
        //
        if (0 == strcmp(s, "list"))
        {
            mode = LIST;
            continue;
        }
        if (0 == strcmp(s, "step"))
        {
            mode = STEP;
            continue;
        }
        if (0 == strcmp(s, "pickonrestart"))
        {
            por = true;
            continue;
        }
        if (0 == strcmp(s, "js"))
        {
            mode = JS;
            continue;
        }
        if (0 == strcmp(s, "clamp"))
        {
            wrap = CLAMP;
            continue;
        }
        if (0 == strcmp(s, "float"))
        {
            type = T_FLOAT;
            continue;
        }
        if (0 == strcmp(s, "wrap"))
        {
            wrap = WRAP;   // this is the default
            continue;
        }
        if ((0 == strcmp(s, "const")) || (0 == strcmp(s, "constant")))
        {
            mode = CONSTANT;
            continue;
        }
        if ((0 == strcmp(s, "rand")) || (0 == strcmp(s, "random")))
        {
            mode = RANDOM; // this is the default
            continue;
        }
        if (0 == strcmp(s, "shuffle"))
        {
            mode = SHUFFLE;
            continue;
        }
        if (0 == strcmp(s, "int") ||
            0 == strcmp(s, "uint"))
        {
            type = T_INT;
            continue;
        }
        if (0 == strcmp(s, "int64") ||
            0 == strcmp(s, "uint64"))
        {
            type = T_INT64;
            continue;
        }

        // UINT64 numbers can also be strings, only set bad string found if the
        // value cannot be colwerted to a number
        errno = 0;
        UINT64 val = Utility::Strtoull(s, nullptr, 0);
        if ((static_cast<UINT64>((numeric_limits<UINT64>::max)()) == val) && errno)
            BadStringFound = true;
        else
            OuterIndex--;

        // Unrecognized string.
        break;
    }
    if (BadStringFound)
    {
        Printf(Tee::PriHigh, "Bad string in FancyPicker spec: \"%s\"\n", tmpString.c_str());
        return RC::BAD_PICK_ITEMS;
    }

#define JSVAL_IS_VALID(v) \
    (JSVAL_IS_NUMBER(v) || ((type == T_INT64) && JSVAL_IS_STRING(v)))

    if (mode == UNINITIALIZED)
    {
        // User didn't specify mode.  Try to guess.

        JsArray      jsa;

        size_t OuterLeft = OuterSize - OuterIndex;

        if (OuterLeft < 1)
            return RC::BAD_PICK_ITEMS;

        if ((OuterLeft == 1) && JSVAL_IS_VALID(OuterArray[OuterIndex]))
        {
            mode = CONSTANT;
        }
        else if ((OuterLeft == 3) &&
                 JSVAL_IS_VALID(OuterArray[OuterIndex + 0]) &&
                 JSVAL_IS_VALID(OuterArray[OuterIndex + 1]) &&
                 JSVAL_IS_VALID(OuterArray[OuterIndex + 2]))
        {
            mode = STEP;
        }
        else
        {
            // LIST   allows n | [n] | [n,n]
            // RANDOM allows           [n,n] | [n,n,n]

            size_t i;
            for(i = OuterIndex; i < OuterSize; i++)
            {
                if (JSVAL_IS_VALID(OuterArray[i]))
                {
                    mode = LIST;
                }
                else
                {
                    if (OK != pJavaScript->FromJsval(OuterArray[i], &jsa))
                        return RC::BAD_PICK_ITEMS;

                    switch (jsa.size())
                    {
                        case 1:     mode = LIST;  break;
                        case 3:     mode = RANDOM; break;

                        case 2:     break;   // can't tell yet

                        default:    return RC::BAD_PICK_ITEMS; // wrong either way
                    }
                }
                if (mode != UNINITIALIZED)
                    break;
            }
            if (mode == UNINITIALIZED)
                mode = RANDOM;          // legal either way, default to RANDOM
        }
    }

    //
    // Now that we know what mode we are in, and have a decent chance of getting
    // a valid config, delete old config and set up new.
    //
    JsArray InnerArray;
    size_t  InnerSize;

    switch (mode)
    {
        case CONSTANT:
        {
            if (OuterSize - OuterIndex != 1)
                return RC::BAD_PICK_ITEMS;

            // Set pick on restart always for const pickers.
            por = true;

            if (type == T_INT)
            {
                UINT32 value;
                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex], &value));

                CHECK_RC(ConfigConst(value));
            }
            else if (type == T_INT64)
            {
                UINT64 value;
                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex], &value));

                CHECK_RC(ConfigConst64(value));
            }
            else // T_FLOAT
            {
                double value;
                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex], &value));

                CHECK_RC(FConfigConst((float)value));
            }
            OuterIndex++;
            break;
        }
        case RANDOM:
        {
            if (OuterSize - OuterIndex < 1)
                return RC::BAD_PICK_ITEMS;

            if (type == T_INT)
            {
                CHECK_RC(ConfigRandom());

                while ((OuterIndex < OuterSize) &&
                       (OK == pJavaScript->FromJsval(OuterArray[OuterIndex], &InnerArray)))
                {
                    OuterIndex++;

                    InnerSize = InnerArray.size();

                    if ((InnerSize < 2) || (InnerSize > 3))
                        return RC::BAD_PICK_ITEMS;
                    UINT32 prob, min, max;
                    CHECK_RC(pJavaScript->FromJsval(InnerArray[0], &prob));
                    CHECK_RC(pJavaScript->FromJsval(InnerArray[1], &min));
                    if (InnerSize == 2)
                    {
                        AddRandItem(prob, min);
                    }
                    else
                    {
                        CHECK_RC(pJavaScript->FromJsval(InnerArray[2], &max));
                        AddRandRange(prob, min, max);
                    }
                }
            }
            else if (type == T_INT64)
            {
                CHECK_RC(ConfigRandom64());

                while ((OuterIndex < OuterSize) &&
                       (OK == pJavaScript->FromJsval(OuterArray[OuterIndex], &InnerArray)))
                {
                    OuterIndex++;

                    InnerSize = InnerArray.size();

                    if ((InnerSize < 2) || (InnerSize > 3))
                        return RC::BAD_PICK_ITEMS;

                    UINT32 prob;
                    UINT64 min, max;
                    CHECK_RC(pJavaScript->FromJsval(InnerArray[0], &prob));
                    CHECK_RC(pJavaScript->FromJsval(InnerArray[1], &min));
                    if (InnerSize == 2)
                    {
                        AddRandItem64(prob, min);
                    }
                    else
                    {
                        CHECK_RC(pJavaScript->FromJsval(InnerArray[2], &max));
                        AddRandRange64(prob, min, max);
                    }
                }
            }
            else // T_FLOAT
            {
                CHECK_RC(FConfigRandom());

                while ((OuterIndex < OuterSize) &&
                       (OK == pJavaScript->FromJsval(OuterArray[OuterIndex], &InnerArray)))
                {
                    OuterIndex++;

                    InnerSize = InnerArray.size();

                    if ((InnerSize < 2) || (InnerSize > 3))
                        return RC::BAD_PICK_ITEMS;

                    UINT32 prob;
                    double min, max;

                    CHECK_RC(pJavaScript->FromJsval(InnerArray[0], &prob));
                    CHECK_RC(pJavaScript->FromJsval(InnerArray[1], &min));
                    if (InnerSize == 2)
                    {
                        FAddRandItem(prob, (float)min);
                    }
                    else
                    {
                        CHECK_RC(pJavaScript->FromJsval(InnerArray[2], &max));
                        FAddRandRange(prob, (float)min, (float)max);
                    }
                }
            }
            CompileRandom();
            break;
        }
        case SHUFFLE:
        {
            if (OuterSize - OuterIndex < 1)
                return RC::BAD_PICK_ITEMS;

            if (type == T_INT)
                rc = ConfigShuffle();
            else if (type == T_INT64)
                rc = ConfigShuffle64();
            else
                rc = FConfigShuffle();

            if (OK != rc)
                return rc;

            while ((OuterIndex < OuterSize) &&
                   (OK == pJavaScript->FromJsval(OuterArray[OuterIndex], &InnerArray)))
            {
                OuterIndex++;

                InnerSize = InnerArray.size();

                if (InnerSize != 2)
                    return RC::BAD_PICK_ITEMS;

                UINT32 count;
                CHECK_RC(pJavaScript->FromJsval(InnerArray[0], &count));

                if (type == T_INT)
                {
                    UINT32 x;
                    if (OK == (rc = pJavaScript->FromJsval(InnerArray[1], &x)))
                        AddRandItem(count, x);
                }
                else if (type == T_INT64)
                {
                    UINT64 x;
                    if (OK == (rc = pJavaScript->FromJsval(InnerArray[1], &x)))
                        AddRandItem64(count, x);
                }
                else
                {
                    double x;
                    if (OK == (rc = pJavaScript->FromJsval(InnerArray[1], &x)))
                        FAddRandItem(count, (float)x);
                }
                if (OK != rc)
                    return rc;
            }
            CompileShuffle();
            break;
        }
        case STEP:
        {
            if (OuterSize - OuterIndex != 3)
                return RC::BAD_PICK_ITEMS;

            if (type == T_INT)
            {
                UINT32 begin, end;
                INT32  step;

                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex + 0], &begin));
                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex + 1], &step));
                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex + 2], &end));

                CHECK_RC(ConfigStep(begin, step, end, wrap));
            }
            else if (type == T_INT64)
            {
                UINT64 begin, end;
                INT64  step;

                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex + 0], &begin));
                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex + 1], &step));
                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex + 2], &end));

                CHECK_RC(ConfigStep64(begin, step, end, wrap));
            }
            else // T_FLOAT
            {
                double begin, step, end;

                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex + 0], &begin));
                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex + 1], &step));
                CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex + 2], &end));

                CHECK_RC(FConfigStep((float)begin, (float)step, (float)end, wrap));
            }
            OuterIndex += 3;
            break;
        }
        case LIST:
        {
            if (OuterSize - OuterIndex < 1)
                return RC::BAD_PICK_ITEMS;

            if (type == T_INT)
            {
                CHECK_RC(ConfigList(wrap));

                while (OuterIndex < OuterSize)
                {
                    INT32  reps = 1;
                    UINT32 value;
                    if (OK == pJavaScript->FromJsval(OuterArray[OuterIndex], &InnerArray))
                    {
                        InnerSize = InnerArray.size();

                        if (InnerSize == 1)
                        {
                            CHECK_RC(pJavaScript->FromJsval(InnerArray[0], &value));
                        }
                        else if (InnerSize == 2)
                        {
                            CHECK_RC(pJavaScript->FromJsval(InnerArray[0], &reps));
                            CHECK_RC(pJavaScript->FromJsval(InnerArray[1], &value));
                        }
                        else
                            return RC::BAD_PICK_ITEMS;
                    }
                    else
                    {
                        CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex], &value));
                    }
                    OuterIndex++;

                    AddListItem(value, reps);
                }
            }
            else if (type == T_INT64)
            {
                CHECK_RC(ConfigList64(wrap));

                while (OuterIndex < OuterSize)
                {
                    INT32  reps = 1;
                    UINT64 value;
                    if (OK == pJavaScript->FromJsval(OuterArray[OuterIndex], &InnerArray))
                    {
                        InnerSize = InnerArray.size();

                        if (InnerSize == 1)
                        {
                            CHECK_RC(pJavaScript->FromJsval(InnerArray[0], &value));
                        }
                        else if (InnerSize == 2)
                        {
                            CHECK_RC(pJavaScript->FromJsval(InnerArray[0], &reps));
                            CHECK_RC(pJavaScript->FromJsval(InnerArray[1], &value));
                        }
                        else
                            return RC::BAD_PICK_ITEMS;
                    }
                    else
                    {
                        CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex], &value));
                    }
                    OuterIndex++;

                    AddListItem64(value, reps);
                }
            }
            else // T_FLOAT
            {
                CHECK_RC(FConfigList(wrap));

                while (OuterIndex < OuterSize)
                {
                    INT32   reps = 1;
                    FLOAT64 value;
                    if (OK == pJavaScript->FromJsval(OuterArray[OuterIndex], &InnerArray))
                    {
                        InnerSize = InnerArray.size();

                        if (InnerSize == 1)
                        {
                            CHECK_RC(pJavaScript->FromJsval(InnerArray[0], &value));
                        }
                        else if (InnerSize == 2)
                        {
                            CHECK_RC(pJavaScript->FromJsval(InnerArray[0], &reps));
                            CHECK_RC(pJavaScript->FromJsval(InnerArray[1], &value));
                        }
                        else
                            return RC::BAD_PICK_ITEMS;
                    }
                    else
                    {
                        CHECK_RC(pJavaScript->FromJsval(OuterArray[OuterIndex], &value));
                    }
                    OuterIndex++;

                    FAddListItem((float)value, reps);
                }
            }
            break;
        }
        case JS:
        {
            if ((OuterSize - OuterIndex < 1) || (OuterSize - OuterIndex > 2))
                return RC::BAD_PICK_ITEMS;

            jsval pick;
            jsval restart;

            pick = OuterArray[OuterIndex + 0];
            if (OuterSize - OuterIndex > 1)
                restart = OuterArray[OuterIndex + 1];
            else
                restart = 0;

            if (type == T_INT)
                rc = ConfigJs(pick, restart);
            else if (type == T_INT64)
                rc = Config64Js(pick, restart);
            else
                rc = FConfigJs(pick, restart);

            if (OK != rc)
                return rc;

            OuterIndex = OuterSize;
            break;
        }
        default:
            MASSERT(0); // shoudn't get here!
    }
    if (OuterIndex < OuterSize)
        return RC::BAD_PICK_ITEMS;

    SetPickOnRestart(por);

    m_SetFromJs = true;

    return OK;
}

//
// Return a jsval  containing this object's config.
//
RC FancyPicker::ToJsval(jsval * pjsv) const
{
    JavaScriptPtr  pJavaScript;
    JsArray        OuterArray;
    jsval          tmpjs;
    RC             rc;

    // PickOnRestart

    if (PickOnRestart())
    {
        CHECK_RC(pJavaScript->ToJsval("PickOnRestart", &tmpjs));
        OuterArray.push_back(tmpjs);
    }

    // Type

    if (T_INT == Type())
    {
        CHECK_RC(pJavaScript->ToJsval("int", &tmpjs));
    }
    else if (T_INT64 == Type())
    {
        CHECK_RC(pJavaScript->ToJsval("int64", &tmpjs));
    }
    else
    {
        CHECK_RC(pJavaScript->ToJsval("float", &tmpjs));
    }
    OuterArray.push_back(tmpjs);

    // Mode

    switch (Mode())
    {
        default:
        case UNINITIALIZED:
        case PARTIAL:
        {
            MASSERT(0); // shouldn't get here
            break;
        }

        case CONSTANT:
        {
            CHECK_RC(pJavaScript->ToJsval("const", &tmpjs));
            OuterArray.push_back(tmpjs);

            // this is actually a step helper.
            StepFpHelper * pPriv = (StepFpHelper *)m_Helper;

            if (Type() == T_INT)
            {
                CHECK_RC(pJavaScript->ToJsval(static_cast<UINT32>(pPriv->m_Begin.i), &tmpjs));
            }
            else if (Type() == T_INT64)
            {
                string sInt64 = Utility::StrPrintf("%llu", pPriv->m_Begin.i);
                CHECK_RC(pJavaScript->ToJsval(sInt64, &tmpjs));
            }
            else
            {
                CHECK_RC(pJavaScript->ToJsval((double)(pPriv->m_Begin.f), &tmpjs));
            }
            OuterArray.push_back(tmpjs);

            break;
        }

        case RANDOM:
        {
            CHECK_RC(pJavaScript->ToJsval("random", &tmpjs));
            OuterArray.push_back(tmpjs);

            RandomFpHelper * pPriv = (RandomFpHelper *)(m_Helper);
            RandomFpHelper::Item * pItem;

            double ProbScalerIlw       = pPriv->m_RelProbSum * TwoExp32Ilw;
            double PrevScaledProbSum   = 0.0;
            UINT32 CheckRelProbSum     = 0;

            for (pItem = pPriv->m_Items; pItem < pPriv->m_Items + pPriv->m_NumItems; ++pItem)
            {
                JsArray InnerArray;

                // relative probability (must undo scaling done by Compile()):

                double tmp           = pItem->Prob + 1.0 - PrevScaledProbSum;
                PrevScaledProbSum    = pItem->Prob + 1.0;
                UINT32 relProb       = UINT32(tmp * ProbScalerIlw + 0.5);
                CheckRelProbSum     += relProb;

                CHECK_RC(pJavaScript->ToJsval(relProb, &tmpjs));
                InnerArray.push_back(tmpjs);

                // min

                if (Type() == T_INT)
                {
                    CHECK_RC(pJavaScript->ToJsval(static_cast<UINT32>(pItem->Min.i), &tmpjs));
                }
                else if (Type() == T_INT64)
                {
                    string sInt64 = Utility::StrPrintf("%llu", pItem->Min.i);
                    CHECK_RC(pJavaScript->ToJsval(sInt64, &tmpjs));
                }
                else
                {
                    CHECK_RC(pJavaScript->ToJsval((double)(pItem->Min.f), &tmpjs));
                }
                InnerArray.push_back(tmpjs);

                // max, only if this is a range item

                if (pItem->Scaler != 0.0)
                {
                    if (T_INT == Type())
                    {
                        UINT32 max = UINT32(pItem->Scaler * TwoExp32 - 1.0 + pItem->Min.i);
                        CHECK_RC(pJavaScript->ToJsval(max, &tmpjs));
                    }
                    else if (T_INT64 == Type())
                    {
                        UINT64 max = pItem->Min.i + pItem->ExtraBins +
                                     UINT64(pItem->BinScaler * TwoExp32Less1) * pItem->BinSize +
                                     UINT64(pItem->Scaler * TwoExp32) - 1;
                        string sInt64 = Utility::StrPrintf("%llu", max);
                        CHECK_RC(pJavaScript->ToJsval(sInt64, &tmpjs));
                    }
                    else
                    {
                        double max = pItem->Scaler * TwoExp32 + pItem->Min.f;
                        CHECK_RC(pJavaScript->ToJsval(max, &tmpjs));
                    }
                    InnerArray.push_back(tmpjs);
                }

                // add item to outer array

                CHECK_RC(pJavaScript->ToJsval(&InnerArray, &tmpjs));
                OuterArray.push_back(tmpjs);

            } // for each item in RandomFpHelper->m_Items

            // If the sum doesn't come out the same, we have a rounding/precision bug.
            MASSERT(CheckRelProbSum == pPriv->m_RelProbSum);

            break;
        }

        case SHUFFLE:
        {
            CHECK_RC(pJavaScript->ToJsval("shuffle", &tmpjs));
            OuterArray.push_back(tmpjs);

            ShuffleFpHelper * pPriv = (ShuffleFpHelper *)(m_Helper);
            ShuffleFpHelper::Item * pItem;

            for (pItem = pPriv->m_Items; pItem < pPriv->m_Items + pPriv->m_NumItems; ++pItem)
            {
                JsArray InnerArray;

                CHECK_RC(pJavaScript->ToJsval(pItem->Count, &tmpjs));
                InnerArray.push_back(tmpjs);

                if (Type() == T_INT)
                {
                    CHECK_RC(pJavaScript->ToJsval(static_cast<UINT32>(pItem->Val.i), &tmpjs));
                }
                else if (Type() == T_INT64)
                {
                    string sInt64 = Utility::StrPrintf("%llu", pItem->Val.i);
                    CHECK_RC(pJavaScript->ToJsval(sInt64, &tmpjs));
                }
                else
                {
                    CHECK_RC(pJavaScript->ToJsval((double)(pItem->Val.f), &tmpjs));
                }
                InnerArray.push_back(tmpjs);

                CHECK_RC(pJavaScript->ToJsval(&InnerArray, &tmpjs));
                OuterArray.push_back(tmpjs);
            }
            break;
        }

        case LIST:
        {
            CHECK_RC(pJavaScript->ToJsval("list", &tmpjs));
            OuterArray.push_back(tmpjs);

            ListFpHelper * pPriv = (ListFpHelper *)(m_Helper);
            ListFpHelper::Item * pItem;

            for (pItem = pPriv->m_Items; pItem < pPriv->m_Items + pPriv->m_NumItems; ++pItem)
            {
                if (pItem->Rpt > 1)
                {
                    JsArray InnerArray;

                    // repeat

                    CHECK_RC(pJavaScript->ToJsval(pItem->Rpt, &tmpjs));
                    InnerArray.push_back(tmpjs);

                    // value

                    if (Type() == T_INT)
                    {
                        CHECK_RC(pJavaScript->ToJsval(static_cast<UINT32>(pItem->Val.i), &tmpjs));
                    }
                    else if (Type() == T_INT64)
                    {
                        string sInt64 = Utility::StrPrintf("%llu", pItem->Val.i);
                        CHECK_RC(pJavaScript->ToJsval(sInt64, &tmpjs));
                    }
                    else
                    {
                        CHECK_RC(pJavaScript->ToJsval((double)(pItem->Val.f), &tmpjs));
                    }
                    InnerArray.push_back(tmpjs);

                    // add pair onto outer array

                    CHECK_RC(pJavaScript->ToJsval(&InnerArray, &tmpjs));
                    OuterArray.push_back(tmpjs);
                }
                else
                {
                    // add value onto outer array

                    if (Type() == T_INT)
                    {
                        CHECK_RC(pJavaScript->ToJsval(static_cast<UINT32>(pItem->Val.i), &tmpjs));
                    }
                    else if (Type() == T_INT64)
                    {
                        string sInt64 = Utility::StrPrintf("%llu", pItem->Val.i);
                        CHECK_RC(pJavaScript->ToJsval(sInt64, &tmpjs));
                    }
                    else
                    {
                        CHECK_RC(pJavaScript->ToJsval((double)(pItem->Val.f), &tmpjs));
                    }
                    OuterArray.push_back(tmpjs);
                }
            } // for each item in ListFpHelper->m_Items

            break;
        }

        case STEP:
        {
            CHECK_RC(pJavaScript->ToJsval("step", &tmpjs));
            OuterArray.push_back(tmpjs);

            StepFpHelper * pPriv = (StepFpHelper *)m_Helper;

            if (Type() == T_INT)
            {
                // begin
                CHECK_RC(pJavaScript->ToJsval(static_cast<UINT32>(pPriv->m_Begin.i), &tmpjs));
                OuterArray.push_back(tmpjs);

                // step (treat as signed)
                CHECK_RC(pJavaScript->ToJsval((INT32)(pPriv->m_Step.i), &tmpjs));
                OuterArray.push_back(tmpjs);

                // end
                CHECK_RC(pJavaScript->ToJsval(static_cast<UINT32>(pPriv->m_End.i), &tmpjs));
                OuterArray.push_back(tmpjs);
            }
            else if (Type() == T_INT64)
            {
                // begin
                string sInt64 = Utility::StrPrintf("%llu", pPriv->m_Begin.i);
                CHECK_RC(pJavaScript->ToJsval(sInt64, &tmpjs));
                OuterArray.push_back(tmpjs);

                // step (treat as signed)
                sInt64 = Utility::StrPrintf("%lld", pPriv->m_Step.i);
                CHECK_RC(pJavaScript->ToJsval(sInt64, &tmpjs));
                OuterArray.push_back(tmpjs);

                // end
                sInt64 = Utility::StrPrintf("%llu", pPriv->m_End.i);
                CHECK_RC(pJavaScript->ToJsval(sInt64, &tmpjs));
                OuterArray.push_back(tmpjs);
            }
            else
            {
                // begin
                CHECK_RC(pJavaScript->ToJsval((double)(pPriv->m_Begin.f), &tmpjs));
                OuterArray.push_back(tmpjs);

                // step
                CHECK_RC(pJavaScript->ToJsval((double)(pPriv->m_Step.f), &tmpjs));
                OuterArray.push_back(tmpjs);

                // end
                CHECK_RC(pJavaScript->ToJsval((double)(pPriv->m_End.f), &tmpjs));
                OuterArray.push_back(tmpjs);
            }
            break;
        }

        case JS:
        {
            CHECK_RC(pJavaScript->ToJsval("js", &tmpjs));
            OuterArray.push_back(tmpjs);

            JsFpHelper * pPriv = (JsFpHelper *)m_Helper;

            OuterArray.push_back(pPriv->m_Pick);

            if (pPriv->m_Restart)
            {
                OuterArray.push_back(pPriv->m_Restart);
            }
            break;
        }
    }

    CHECK_RC(pJavaScript->ToJsval(&OuterArray, pjsv));

    return OK;
}

FancyPicker::eType FancyPicker::Type() const
{
    return m_Helper->m_Type;
}

FancyPicker::eMode FancyPicker::Mode() const
{
    eMode m = m_Helper->Mode();

    if ((m != UNINITIALIZED) && (! m_Helper->Ready()))
        return PARTIAL;

    return m;
}

FancyPicker::eWrapMode FancyPicker::WrapMode() const
{
    return m_Helper->WrapMode();
}

bool FancyPicker::PickOnRestart() const
{
    return m_Helper->m_PickOnRestart;
}

bool FancyPicker::IsInitialized() const
{
    eMode m = Mode();

    if ((m == UNINITIALIZED) || (m == PARTIAL))
        return false;

    return true;
}

//
// If !PickOnRestart,
//   if JS, call restart function.
// If PickOnRestart,
//   choose a new value to use for all calls to Pick.
//
void FancyPicker::Restart()
{
    MASSERT(m_Helper->Ready());

    m_Helper->Restart();

    if (m_Helper->m_PickOnRestart)
    {
        if ((m_Helper->m_Type == T_INT) || (m_Helper->m_Type == T_INT64))
            m_Helper->Pick();
        else
            m_Helper->FPick();
    }
}

//
// Advance to and return the next value in the sequence.
//
UINT32 FancyPicker::Pick()
{
    MASSERT(m_Helper->m_Type == T_INT);
    return static_cast<UINT32>(Pick64());
}
UINT64 FancyPicker::Pick64()
{
    MASSERT((m_Helper->m_Type == T_INT64) || (m_Helper->m_Type == T_INT));
    MASSERT(m_Helper->Ready());

    // If the caller hasn't called SetContext() yet, we will be using
    // the default Random generator, which won't be reliably seeded.
    // This will lead to runtime checksum miscompares.
    MASSERT(m_Helper->m_pFpCtx != &s_DefaultContext);

    // Guard against a mis-configured picker.
    // You're trying to call Pick() on a picker configured to ONLY
    // pick on restart!
    // Note: CONST configured pickers are the only exception.
    MASSERT((!m_Helper->m_PickOnRestart) ||
            (m_Helper->Mode() == FancyPicker::CONSTANT));

    if (!m_Helper->m_PickOnRestart)
    {
        m_Helper->Pick();
    }
    if (m_OverridenHelper)
    {
        if (!m_OverridenHelper->m_PickOnRestart)
        {
            m_OverridenHelper->Pick();
        }
    }

    m_Helper->RecordPick();

    return m_Helper->m_Value.i;
}
float  FancyPicker::FPick()
{
    MASSERT(m_Helper->m_Type == T_FLOAT);
    MASSERT(m_Helper->Ready());

    // If the caller hasn't called SetContext() yet, we will be using
    // the default Random generator, which won't be reliably seeded.
    // This will lead to runtime checksum miscompares.
    MASSERT(m_Helper->m_pFpCtx != &s_DefaultContext);

    // Guard against a mis-configured picker.
    // You're trying to call Pick() on a picker configured to ONLY
    // pick on restart!
    // Note: CONST configured pickers are the only exception.
    MASSERT((!m_Helper->m_PickOnRestart) ||
            (m_Helper->Mode() == FancyPicker::CONSTANT));

    if (!m_Helper->m_PickOnRestart)
    {
        m_Helper->FPick();
    }
    if (m_OverridenHelper)
    {
        if (!m_OverridenHelper->m_PickOnRestart)
        {
            m_OverridenHelper->FPick();
        }
    }

    m_Helper->RecordPick();

    return m_Helper->m_Value.f;
}

//
// Return current value (result of last Pick or FPick).
//
UINT32 FancyPicker::GetPick() const
{
    MASSERT(m_Helper->m_Type == T_INT);
    return static_cast<UINT32>(m_Helper->m_Value.i);
}
UINT64 FancyPicker::GetPick64() const
{
    MASSERT(m_Helper->m_Type == T_INT64);
    return m_Helper->m_Value.i;
}
float  FancyPicker::FGetPick() const
{
    MASSERT(m_Helper->m_Type == T_FLOAT);
    return m_Helper->m_Value.f;
}

//
// Assignment operator overload
//
FancyPicker& FancyPicker::operator =(const FancyPicker &rhs)
{
    MASSERT(m_Helper);

    FpHelper* newHelper = rhs.m_Helper->Clone();
    delete m_Helper;
    this->m_Helper = newHelper;
    this->m_SetFromJs = rhs.m_SetFromJs;

    return *this;
}

//
// Get min/max values returned by Pick()
//
RC FancyPicker::GetPickRange(UINT32 *pMinPick, UINT32 *pMaxPick) const
{
    UINT64 minPick;
    UINT64 maxPick;
    RC rc;

    CHECK_RC(GetPickRange(&minPick, &maxPick));

    if (pMinPick)
        *pMinPick = static_cast<UINT32>(minPick);
    if (pMaxPick)
        *pMaxPick = static_cast<UINT32>(maxPick);
    return rc;
}
RC FancyPicker::GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const
{
    UINT64 minPick;
    UINT64 maxPick;
    RC rc;

    CHECK_RC(m_Helper->GetPickRange(&minPick, &maxPick));

    if (pMinPick)
        *pMinPick = minPick;
    if (pMaxPick)
        *pMaxPick = maxPick;
    return rc;
}

//
// Get num of added items
//
RC FancyPicker::GetNumItems(UINT32 *pNumItems) const
{
    return m_Helper->GetNumItems(pNumItems);
}

RC FancyPicker::GetRawItems(FancyPicker::PickerItems *pItems) const
{
    MASSERT((m_Helper->m_Type == T_INT64) || (m_Helper->m_Type == T_INT));

    return m_Helper->GetRawItems(pItems);
}

RC FancyPicker::GetFRawItems(PickerFItems *pItems) const
{
    MASSERT(m_Helper->m_Type == T_FLOAT);

    return m_Helper->GetFRawItems(pItems);
}

RC FancyPicker::StartOverride()
{
    if (m_OverridenHelper)
    {
        MASSERT(!"Picker override already active!");
        return RC::SOFTWARE_ERROR;
    }

    m_OverridenHelper = m_Helper;
    FpHelper *overrideHelper = m_Helper->Clone();
    overrideHelper->m_pFpCtx = new FpContext;
    m_Helper = overrideHelper;

    return RC::OK;
}

void FancyPicker::StopOverride()
{
    if (!m_OverridenHelper)
        return;

    delete m_Helper->m_pFpCtx;
    delete m_Helper;
    m_Helper = m_OverridenHelper;
    m_OverridenHelper = nullptr;
}

RC FancyPicker::CheckUsed() const
{
    if (!m_Helper)
    {
        return RC::OK;
    }
    return m_Helper->CheckUsed();
}

//------------------------------------------------------------------------------
//
// class FpHelper (base class)
//
//------------------------------------------------------------------------------

//
// Pick functions
//
void FpHelper::Pick()
{
    MASSERT(0);
}
void FpHelper::FPick()
{
    MASSERT(0);
}

//
// clone function
//
FpHelper* FpHelper::Clone() const
{
    MASSERT(0);
    return NULL;
}

void FpHelper::Compile()
{
    MASSERT(!"wrong FpHelper type");
}
void FpHelper::AddItem (UINT32 Count, UINT64 Min, UINT64 Max)
{
    MASSERT(!"wrong FpHelper type");
}
void FpHelper::FAddItem (UINT32 Count, float Min, float Max)
{
    MASSERT(!"wrong FpHelper type");
}
RC FpHelper::GetNumItems(UINT32 *pNumItems) const
{
    MASSERT(!"wrong FpHelper type");
    return RC::BAD_PICK_ITEMS;
}
RC FpHelper::GetRawItems(FancyPicker::PickerItems *pItems) const
{
    MASSERT(!"wrong FpHelper type");
    return RC::BAD_PICK_ITEMS;
}
RC FpHelper::GetFRawItems(FancyPicker::PickerFItems *pItems) const
{
    MASSERT(!"wrong FpHelper type");
    return RC::BAD_PICK_ITEMS;
}
//
// constructor, destructor
//
FpHelper::FpHelper(FancyPicker::FpContext * pFpCtx)
  : m_pFpCtx(pFpCtx),
    m_PickOnRestart(false),
    m_Type(FancyPicker::T_INT)
{
    m_Value.i = 0ULL;
}
FpHelper::FpHelper(const FpHelper * pFpHelper)
  : m_pFpCtx(pFpHelper->m_pFpCtx),
    m_PickOnRestart(false),
    m_Type(pFpHelper->m_Type),
    m_Name(pFpHelper->m_Name)
{
    m_Value.i = 0ULL;
}
FpHelper::~FpHelper()
{
}

void FpHelper::SetName(string name)
{
    if (!name.empty())
    {
        m_Name = move(name);
    }
}

//
// standard Helper functions
//
bool FpHelper::ConstructorOK() const
{
    return true;
}
void FpHelper::Restart()
{
    MASSERT(0);
}
bool FpHelper::Ready() const
{
    return false;
}
FancyPicker::eMode FpHelper::Mode() const
{
    return FancyPicker::UNINITIALIZED;
}
FancyPicker::eWrapMode FpHelper::WrapMode() const
{
    return FancyPicker::NA;
}
RC FpHelper::GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const
{
    MASSERT(!"wrong FpHelper type");
    return RC::UNSUPPORTED_FUNCTION;
}

RC FpHelper::CheckUsedPicks(const char* name, const vector<bool>& picks, UINT32 numItems)
{
    UINT32 numUsed = 0;

#ifdef NDEBUG
    constexpr auto pri = Tee::PriDebug;
#else
    constexpr INT32 pri = Tee::PriNormal;
#endif

    for (UINT32 i = 0; i < numItems; i++)
    {
        if (picks[i])
        {
            ++numUsed;
        }
    }

    // Some fancy pickers are unused, for example when a feature used by the
    // fancy picker is not present on the current GPU or when this is a debug
    // feature enabled in a special way.  Ignore such pickers.
    if (numUsed == 0)
    {
        return RC::OK;
    }

    if (numUsed < numItems)
    {
        Printf(pri, "%u out of %u items used (%u%%) in fancy picker %s\n",
               numUsed, numItems, numUsed * 100 / numItems, name);
    }

#ifdef DEBUG
    // Don't fail on simulators, we usually run with deliberately
    // lowered coverage there
    if (Platform::GetSimulationMode() != Platform::Hardware)
    {
        return RC::OK;
    }

    if (numUsed < numItems)
    {
        return RC::BAD_PICKER_INDEX;
    }
#endif

    return RC::OK;
}

//------------------------------------------------------------------------------
//
// class RandomFpHelper
//
//------------------------------------------------------------------------------

//
// Pick functions:
//
void RandomFpHelper::Pick()
{
    MASSERT(m_State == IntCompiled);
    MASSERT(m_Items);

    // Consume exactly 2 random numbers, for T_INT. A 3rd will be consumed
    // for T_INT64
    UINT32 rnd1 = m_pFpCtx->Rand.GetRandom();
    UINT32 rnd2 = m_pFpCtx->Rand.GetRandom();

    Item * pItem = m_Items;
    m_LastPick = 0;

    while (rnd1 > pItem->Prob)
    {
        ++pItem;
        ++m_LastPick;
    }

    if (m_Type == FancyPicker::T_INT)
        m_Value.i = pItem->Min.i + UINT64(pItem->Scaler * double(rnd2));
    else
    {
        UINT32 rnd3 = m_pFpCtx->Rand.GetRandom();
        UINT64 binNum = UINT64(pItem->BinScaler * double(rnd2));
        m_Value.i = pItem->Min.i + (pItem->BinSize * binNum);
        if (binNum < pItem->ExtraBins)
        {
            m_Value.i += binNum;
            m_Value.i += UINT64(pItem->ExtraScaler * double(rnd3));
        }
        else
        {
            m_Value.i += pItem->ExtraBins;
            m_Value.i += UINT64(pItem->Scaler * double(rnd3));
        }
    }
}
void RandomFpHelper::FPick()
{
    MASSERT(m_State == FloatCompiled);
    MASSERT(m_Items);

    // Consume exactly 3 random numbers, regardless of state.
    UINT32 rnd1 = m_pFpCtx->Rand.GetRandom();
    UINT32 rnd2 = m_pFpCtx->Rand.GetRandom();
    UINT32 rnd3 = m_pFpCtx->Rand.GetRandom();

    Item * pItem = m_Items;
    m_LastPick = 0;

    while (rnd1 > pItem->Prob)
    {
        ++pItem;
        ++m_LastPick;
    }

    // rnd2 gets us to the bottom of a "bin" and gives us flat distribution.
    // rnd3 spreads the number across the bin (get more precision near 0.0).
    m_Value.f = (float)(pItem->Min.f +
                        (pItem->Scaler * rnd2) +
                        (pItem->Scaler * rnd3 * TwoExp32Less1Ilw));
}

//
// Add Items:
//
void RandomFpHelper::AddItem  (UINT32 RelProb, UINT64 Min, UINT64 Max)
{
    MASSERT((m_State == Empty) || (m_State == IntRaw));
    MASSERT(m_Items);

    if (m_NumItems >= m_SizeItems)
    {
        Item * tmp = new Item[m_SizeItems+1];
        int i;
        for (i = 0; i < m_NumItems; i++)
            tmp[i] = m_Items[i];

        delete [] m_Items;
        m_Items = tmp;
        m_SizeItems++;

        m_Picks.resize(m_SizeItems);
    }

    m_Items[m_NumItems].Prob   = RelProb;
    m_Items[m_NumItems].Min.i  = Min;

    if (Min == Max)
    {
        m_Items[m_NumItems].Scaler = 0.0;
        m_Items[m_NumItems].BinScaler = 0.0;
        m_Items[m_NumItems].BinSize = 1;
        m_Items[m_NumItems].ExtraBins = 0;
    }
    else if (m_Type == FancyPicker::T_INT)
    {
        // The double item.Scaler will scale a random number, 0 to 0xffffffff.
        // Then, the result will be truncated to int.
        //
        // If Min==0, Max==0xffffffff, Scaler must be exactly 1.0
        // If Min==0, Max==1, Scaler must be exactly 1/(2**31)
        //
        m_Items[m_NumItems].Scaler = (Max - Min + 1.0) * TwoExp32Ilw;
    }
    else
    {
        // Since it is not possible for a native type to fully represent 2^64
        // aclwrately, it is necessary to divide UINT64 randoms into bins
        // similar to what is done with floating point numbers and then select
        // a random value from that bin.  Because these are integers it is not
        // possible for all bins to always be equally sized, therefore some
        // bins are exactly 1 wider than other bins
        //
        // The double item.BinScaler will scale a random UINT32 into a bin
        // selector [0, (numBins - 1)]
        //
        // The double item.Scaler will scale a random UINT32 into a value within
        // a normal sized bin [0, BinSize]
        //
        // The double item.ExtraScaler will scale a random UINT32 into a value
        // within a bin containing an extra range [0, (BinSize + 1)]
        //
        if ((Max - Min) == static_cast<UINT64>((numeric_limits<UINT64>::max)()))
        {
            m_Items[m_NumItems].BinScaler = 1.0;
            m_Items[m_NumItems].Scaler = 1.0;
            m_Items[m_NumItems].BinSize = static_cast<UINT64>(TwoExp32);
            m_Items[m_NumItems].ExtraBins = 0;
        }
        else if ((Max - Min) <= (numeric_limits<UINT32>::max)())
        {
            m_Items[m_NumItems].BinScaler = 0.0;
            m_Items[m_NumItems].Scaler = (Max - Min + 1.0) * TwoExp32Ilw;
            m_Items[m_NumItems].BinSize = (Max - Min + 1ULL);
            m_Items[m_NumItems].ExtraBins = 0;
        }
        else
        {
            UINT64 numBins = (Max - Min + 1) >> 32;
            if ((Max - Min + 1) & (numeric_limits<UINT32>::max)())
                numBins++;

            m_Items[m_NumItems].BinScaler = numBins * TwoExp32Ilw;
            m_Items[m_NumItems].BinSize = (Max - Min + 1) / numBins;
            m_Items[m_NumItems].ExtraBins = UNSIGNED_CAST(UINT32, (Max - Min + 1) % numBins);
            m_Items[m_NumItems].Scaler = m_Items[m_NumItems].BinSize * TwoExp32Ilw;
            m_Items[m_NumItems].ExtraScaler = (m_Items[m_NumItems].BinSize + 1) * TwoExp32Ilw;
        }
    }

    m_RelProbSum += RelProb;
    m_State = IntRaw;
    m_NumItems++;
}
void RandomFpHelper::FAddItem (UINT32 RelProb,  float  Min, float  Max)
{
    MASSERT((m_State == Empty) || (m_State == FloatRaw));
    MASSERT(m_Items);

    if (m_NumItems >= m_SizeItems)
    {
        Item * tmp = new Item[m_SizeItems+1];
        int i;
        for (i = 0; i < m_NumItems; i++)
            tmp[i] = m_Items[i];

        delete [] m_Items;
        m_Items = tmp;
        m_SizeItems++;

        m_Picks.resize(m_SizeItems);
    }

    m_Items[m_NumItems].Prob   = RelProb;
    m_Items[m_NumItems].Min.f  = Min;

    // The double item.Scaler will scale a random UINT32, 0 to 0xffffffff.
    //
    // When the range of Max-Min is large, the "step" between 2
    // nearest random numbers will be large also (up to 2**98).  We
    // could fix this by just stuffing 32 random bits into a float,
    // but that would skew the distribution (i.e. half of all numbers
    // would be between -1.0 and +1.0).
    //
    // Our compromise is to use 2 random numbers, one to choose one of
    // 2**32 "bins", and the other to spread the result across the chosen bin.
    //
    // So Scaler is the bin-size.
    //
    // If Min==0.0, Max==exp(2.0,32), Scaler must be exactly 1.0
    //
    m_Items[m_NumItems].Scaler = (Max - Min) * TwoExp32Ilw;

    m_RelProbSum += RelProb;
    m_State = FloatRaw;
    m_NumItems++;
}

//
// Colwert from relative prob to absolute prob, scaled to 0..ffffffff.
//
void RandomFpHelper::Compile()
{
    MASSERT((m_State == IntRaw) || (m_State == FloatRaw));
    MASSERT(m_Items);
    MASSERT(m_NumItems == m_SizeItems);

    if (!m_RelProbSum)
    {
        // In the pathological case of probsum being 0, we will just always
        // pick the last item.
        m_RelProbSum               = 1;
        m_Items[m_NumItems-1].Prob = 1;
    }

    // Get the colwersion factor to scale probability to the full 32-bit range.
    double ProbScaler = TwoExp32 / m_RelProbSum;

    // Fill in Prob for each Item.
    double ScaledProbSum = 0.0;

    Item * pItem    = m_Items;
    Item * pItemEnd = m_Items + m_SizeItems;

    while (pItem < pItemEnd)
    {
        ScaledProbSum += ProbScaler * pItem->Prob;

        if (ScaledProbSum >= 0.5)
        {
            // Watch for rollover here -- ScaledProbSum ends up
            // approx. = TwoExp32.
            pItem->Prob = UINT32(floor(ScaledProbSum + 0.5) - 1.0);
        }
        // Else, pathological case: item 0, with relprob 0.

        ++pItem;
    }

    // The last item MUST always have the scaled Prob of 2^32 - 1.
    MASSERT((pItem-1)->Prob == 0xFFFFFFFF);

    m_State = (m_State == IntRaw) ? IntCompiled : FloatCompiled;
}

//
// clone function
//
FpHelper* RandomFpHelper::Clone() const
{
    RandomFpHelper* clonedHelper = new RandomFpHelper(this);
    if (m_SizeItems > clonedHelper->m_SizeItems)
    {
        delete[] clonedHelper->m_Items;
        clonedHelper->m_Items = new Item[m_SizeItems];
    }

    clonedHelper->m_State = this->m_State;
    clonedHelper->m_RelProbSum = this->m_RelProbSum;
    clonedHelper->m_NumItems = this->m_NumItems;
    clonedHelper->m_SizeItems = this->m_SizeItems;
    for (int i = 0; i < this->m_NumItems; i++)
    {
        clonedHelper->m_Items[i] = this->m_Items[i];
    }

    clonedHelper->m_Picks = this->m_Picks;

    return clonedHelper;
}

//
// constructor, destructor
//
RandomFpHelper::RandomFpHelper(const FpHelper * pFpHelper)
  : FpHelper(pFpHelper)
{
    m_State        = Empty;
    m_RelProbSum   = 0;
    m_NumItems     = 0;
    m_SizeItems    = 1;

    m_Items = new Item[m_SizeItems];
    if (! m_Items)
        m_SizeItems = 0;

    m_Picks.resize(1);
}
RandomFpHelper::~RandomFpHelper()
{
    delete [] m_Items;
}

//
// standard Helper functions
//
bool RandomFpHelper::ConstructorOK() const
{
    return (m_Items != 0);
}
void RandomFpHelper::Restart()
{
}
bool RandomFpHelper::Ready() const
{
    return ((m_State == IntCompiled) || (m_State == FloatCompiled));
}
FancyPicker::eMode RandomFpHelper::Mode() const
{
    return FancyPicker::RANDOM;
}
FancyPicker::eWrapMode RandomFpHelper::WrapMode() const
{
    return FancyPicker::NA;
}
RC RandomFpHelper::GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const
{
    MASSERT(pMinPick);
    MASSERT(pMaxPick);
    MASSERT(m_State == IntCompiled);
    MASSERT(m_Items);
    MASSERT(m_NumItems > 0);

    *pMinPick = m_Items[0].Min.i;
    *pMaxPick = *pMinPick;
    for (int ii = 0; ii < m_NumItems; ++ii)
    {
        UINT64 itemMin = m_Items[ii].Min.i;
        UINT64 itemMax = itemMin;

        if (m_Items[ii].Scaler != 0.0)
        {
            if (FancyPicker::T_INT == m_Type)
            {
                itemMax = UINT32(m_Items[ii].Scaler * TwoExp32 - 1.0 + itemMin);
            }
            else if (FancyPicker::T_INT64 == m_Type)
            {
                itemMax = itemMin + m_Items[ii].ExtraBins +
                          UINT64(m_Items[ii].BinScaler * TwoExp32Less1) * m_Items[ii].BinSize +
                          UINT64(m_Items[ii].Scaler * TwoExp32) - 1;
            }
        }

        MASSERT(itemMin <= itemMax);
        *pMinPick = min(*pMinPick, itemMin);
        *pMaxPick = max(*pMaxPick, itemMax);
    }
    return OK;
}
RC RandomFpHelper::GetNumItems(UINT32 *pNumItems) const
{
    MASSERT(pNumItems != nullptr);

    *pNumItems = m_NumItems;
    return OK;
}

RC RandomFpHelper::GetRawItems(FancyPicker::PickerItems *pItems) const
{
    MASSERT(pItems != nullptr);

    pItems->clear();
    for (int ii = 0; ii< m_NumItems; ii++)
    {
        pItems->push_back(m_Items[ii].Min.i);
    }
    return OK;
}

RC RandomFpHelper::GetFRawItems(FancyPicker::PickerFItems *pItems) const
{
    MASSERT(pItems != nullptr);

    pItems->clear();
    for (int ii = 0; ii< m_NumItems; ii++)
    {
       pItems->push_back(m_Items[m_NumItems].Min.f);
    }
    return OK;
}

void RandomFpHelper::RecordPick()
{
    m_Picks[m_LastPick] = true;
}

//------------------------------------------------------------------------------
//
// class ShuffleFpHelper
//
//------------------------------------------------------------------------------

// If the Deck size gets too big, perf will suffer.
const UINT32 ShuffleFpHelper::s_MaxReasonableTotalCount = 64*1024;

//
// Pick functions:
//
void ShuffleFpHelper::Pick()
{
    MASSERT(m_State == IntCompiled);
    MASSERT(m_Deck);

    if (m_DeckNext >= m_Deck + m_TotalCount)
        Shuffle();

    m_Value.i = m_DeckNext->i;
    m_DeckNext++;
}
void ShuffleFpHelper::FPick()
{
    MASSERT(m_State == FloatCompiled);
    MASSERT(m_Deck);

    if (m_DeckNext >= m_Deck + m_TotalCount)
        Shuffle();

    m_Value.f = m_DeckNext->f;
    m_DeckNext++;
}

//
// Add Items:
//
void ShuffleFpHelper::GrowItems()
{
    UINT32 tmpSize = m_SizeItems+1;
    Item * tmp = new Item[tmpSize];

    int i;
    for (i = 0; i < m_NumItems; i++)
        tmp[i] = m_Items[i];

    delete [] m_Items;
    m_Items = tmp;
    m_SizeItems = tmpSize;

    m_Picks.resize(m_SizeItems);
}

void ShuffleFpHelper::AddItem  (UINT32 count, UINT64 Min, UINT64 Max)
{
    MASSERT((m_State == Empty) || (m_State == IntRaw));
    MASSERT(m_Items);
    MASSERT(Min == Max);

    if (m_NumItems >= m_SizeItems)
        GrowItems();

    m_Items[m_NumItems].Count = count;
    m_Items[m_NumItems].Val.i = Min;
    m_State = IntRaw;
    m_NumItems++;
    m_TotalCount += count;
    MASSERT(m_TotalCount < s_MaxReasonableTotalCount);
}
void ShuffleFpHelper::FAddItem (UINT32 count, float Min, float Max)
{
    MASSERT((m_State == Empty) || (m_State == FloatRaw));
    MASSERT(m_Items);
    MASSERT(Min == Max);

    if (m_NumItems >= m_SizeItems)
        GrowItems();

    m_Items[m_NumItems].Count = count;
    m_Items[m_NumItems].Val.f = Min;
    m_State = FloatRaw;
    m_NumItems++;
    m_TotalCount += count;
    MASSERT(m_TotalCount < s_MaxReasonableTotalCount);
}

void ShuffleFpHelper::Compile()
{
    MASSERT((m_State == IntRaw) || (m_State == FloatRaw));
    MASSERT(m_Items);
    MASSERT(m_NumItems == m_SizeItems);

    delete [] m_Deck;
    m_Deck = new IorF[m_TotalCount];
    MakeDeck();

    m_State = (m_State == IntRaw) ? IntCompiled : FloatCompiled;
}
void ShuffleFpHelper::MakeDeck()
{
    m_DeckNext = m_Deck;
    for (Item * pItem = m_Items; pItem < m_Items + m_NumItems; pItem++)
    {
        for (UINT32 i = 0; i < pItem->Count; i++)
            *m_DeckNext++ = pItem->Val;
    }
    MASSERT(m_DeckNext == m_Deck + m_TotalCount);
}
void ShuffleFpHelper::Shuffle()
{
    m_pFpCtx->Rand.Shuffle(m_TotalCount, &m_Deck[0].i);
    m_DeckNext = m_Deck;
}

void ShuffleFpHelper::RecordPick()
{
    Item*       pItem = m_Items;
    Item* const pEnd  = pItem + m_NumItems;

    for (UINT32 i = 0; pItem < pEnd; i++, pItem++)
    {
        if (m_Value.i == pItem->Val.i)
        {
            m_Picks[i] = true;
            break;
        }
    }
}

//
// clone function
//
FpHelper* ShuffleFpHelper::Clone() const
{
    ShuffleFpHelper* clonedHelper = new ShuffleFpHelper(this);
    if (m_SizeItems > clonedHelper->m_SizeItems)
    {
        delete[] clonedHelper->m_Items;
        clonedHelper->m_Items = new Item[m_SizeItems];
    }

    clonedHelper->m_State = this->m_State;
    clonedHelper->m_NumItems = this->m_NumItems;
    clonedHelper->m_SizeItems = this->m_SizeItems;
    clonedHelper->m_TotalCount = this->m_TotalCount;
    for (int i = 0; i < this->m_NumItems; i++)
    {
        clonedHelper->m_Items[i] = this->m_Items[i];
    }
    if (this->m_Deck)
    {
        clonedHelper->MakeDeck();
    }

    clonedHelper->m_Picks = this->m_Picks;

    return clonedHelper;
}

//
// constructor, destructor
//
ShuffleFpHelper::ShuffleFpHelper(const FpHelper * pFpHelper)
 : FpHelper(pFpHelper)
{
    m_State        = Empty;
    m_TotalCount   = 0;
    m_NumItems     = 0;
    m_SizeItems    = 1;
    m_Deck         = NULL;
    m_DeckNext     = NULL;

    m_Items = new Item[m_SizeItems];
    if (! m_Items)
        m_SizeItems = 0;

    m_Picks.resize(1);
}
ShuffleFpHelper::~ShuffleFpHelper()
{
    delete [] m_Items;
    delete [] m_Deck;
}

//
// standard Helper functions
//
bool ShuffleFpHelper::ConstructorOK() const
{
    return (m_Items != 0);
}
void ShuffleFpHelper::Restart()
{
    if (!m_PickOnRestart)
    {
        // Start from a sorted deck at restart, so frames are independent.
        MakeDeck();
        Shuffle();
    }
}
bool ShuffleFpHelper::Ready() const
{
    return ((m_State == IntCompiled) || (m_State == FloatCompiled));
}
FancyPicker::eMode ShuffleFpHelper::Mode() const
{
    return FancyPicker::SHUFFLE;
}
FancyPicker::eWrapMode ShuffleFpHelper::WrapMode() const
{
    return FancyPicker::NA;
}
RC ShuffleFpHelper::GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const
{
    MASSERT(pMinPick);
    MASSERT(pMaxPick);
    MASSERT(m_State == IntCompiled);
    MASSERT(m_Items);
    MASSERT(m_NumItems > 0);

    *pMinPick = m_Items[0].Val.i;
    *pMaxPick = *pMinPick;
    for (int ii = 0; ii < m_NumItems; ++ii)
    {
        UINT64 val = m_Items[ii].Val.i;
        *pMinPick = min(*pMinPick, val);
        *pMaxPick = max(*pMaxPick, val);
    }
    return OK;
}
RC ShuffleFpHelper::GetNumItems(UINT32 *pNumItems) const
{
    MASSERT(pNumItems != nullptr);

    *pNumItems = m_NumItems;
    return OK;
}

RC ShuffleFpHelper::GetRawItems(FancyPicker::PickerItems *pItems) const
{
    MASSERT(pItems != nullptr);

    pItems->clear();
    for (int ii = 0; ii< m_NumItems; ii++)
    {
       pItems->push_back(m_Items[ii].Val.i);
    }
    return OK;
}

RC ShuffleFpHelper::GetFRawItems(FancyPicker::PickerFItems *pItems) const
{
    MASSERT(pItems != nullptr);

    pItems->clear();
    for (int ii = 0; ii< m_NumItems; ii++)
    {
        pItems->push_back(m_Items[ii].Val.f);
    }
    return OK;
}

//------------------------------------------------------------------------------
//
// class StepFpHelper
//
//------------------------------------------------------------------------------

//
// Pick functions
//
void StepFpHelper::Pick()
{
    MASSERT(m_Type != FancyPicker::T_FLOAT);
    if (0 == m_Step.i)
    {
        m_Value.i = m_Begin.i;
        return;
    }

    UINT32 steps = m_PickOnRestart ? m_pFpCtx->RestartNum : m_pFpCtx->LoopNum;

    if (m_WrapMode == FancyPicker::WRAP)
    {
        if (0 != m_MaxStepNum + 1)
            steps %= (m_MaxStepNum + 1);

        m_Value.i = (UINT64)(m_Begin.i + (INT64)steps * m_Step.i);
    }
    else
    {
        if (steps > m_MaxStepNum)
        {
            m_Value.i = m_End.i;
        }
        else
        {
            m_Value.i = (UINT64)(m_Begin.i + (INT64)steps * m_Step.i);
        }
    }
}
void StepFpHelper::FPick()
{
    MASSERT(m_Type == FancyPicker::T_FLOAT);
    if (0.0F == m_Step.f)
    {
        m_Value.f = m_Begin.f;
        return;
    }

    UINT32 steps = m_PickOnRestart ? m_pFpCtx->RestartNum : m_pFpCtx->LoopNum;

    if (m_WrapMode == FancyPicker::WRAP)
    {
        if (0 != m_MaxStepNum + 1)
            steps %= (m_MaxStepNum + 1);

        m_Value.f = m_Begin.f + steps * m_Step.f;
    }
    else
    {
        if (steps > m_MaxStepNum)
        {
            m_Value.f = m_End.f;
        }
        else
        {
            m_Value.f = m_Begin.f + steps * m_Step.f;
        }
    }
}

//
// clone function
//
FpHelper* StepFpHelper::Clone() const
{
    StepFpHelper* clonedHelper;
    if((this->m_Type == FancyPicker::T_INT) || (this->m_Type == FancyPicker::T_INT64))
    {
        clonedHelper = new StepFpHelper(this, this->m_Begin.i, this->m_Step.i, this->m_End.i, this->m_WrapMode);
    }
    else
    {
        clonedHelper = new StepFpHelper(this, this->m_Begin.f, this->m_Step.f, this->m_End.f, this->m_WrapMode);
    }

    clonedHelper->m_MaxStepNum = this->m_MaxStepNum;

    return clonedHelper;
}

//
// constructor, destructor
//
StepFpHelper::StepFpHelper(const FpHelper * pFpHelper, UINT64 Begin, INT64 Step, UINT64 End, FancyPicker::eWrapMode wm)
 : FpHelper(pFpHelper)
{
    // Because of the signed/unsigned ambiguity, don't attempt to swap.
    m_Begin.i = (INT64)Begin;
    m_Step.i  = Step;
    m_End.i   = (INT64)End;
    m_WrapMode = wm;

    if (0 == m_Step.i)
    {
        m_End.i = m_Begin.i;
        m_MaxStepNum = 0xffffffff;
    }
    else
    {
        m_MaxStepNum = (UINT32)((m_End.i - m_Begin.i) / m_Step.i);
        if (0 == m_MaxStepNum)
        {
            // If m_MaxStepNum is 0, and m_Step is !0, we won't do
            // anything useful.
            //
            // Try unsigned.
            // (begin,step,end) == (0, 0x3fffffff, 0xffffffff) works this way.
            //
            // This is messy, the alternative to to have a separate
            // "step" class for unsigned vs. signed.  JavaScript
            // doesn't support an unsigned integer type directly...
            m_MaxStepNum = (UINT32)(((UINT64)(m_End.i - m_Begin.i)) / (UINT64)m_Step.i);
        }
        if (0 == m_MaxStepNum)
        {
            // Oh, well.  abs(step) is greater than abs(end-begin).
            // Treat this as the "const" case.
            Printf(Tee::PriLow,
                   "WARNING: FancyPicker [\"step\", %lld, %lld, %lld], step is > range!\n",
                   m_End.i, m_Step.i, m_End.i);
            m_Step.i = 0;
            m_End.i = m_Begin.i;
            m_MaxStepNum = (INT32)0xffffffff;
        }
    }
    m_Value.i = m_Begin.i;
}
StepFpHelper::StepFpHelper(const FpHelper * pFpHelper, float  Begin, float Step, float  End, FancyPicker::eWrapMode wm)
  : FpHelper(pFpHelper)
{
    // Swap Begin and End if necessary.

    if (((Step < 0.0f) && (Begin < End)) ||
        ((Step > 0.0f) && (Begin > End)))
    {
        m_Begin.f = End;
        m_End.f   = Begin;
    }
    else
    {
        m_Begin.f = Begin;
        m_End.f   = End;
    }
    m_Step.f  = Step;
    m_WrapMode = wm;

    if (0.0F == m_Step.f)
    {
        m_End.f = m_Begin.f;
        m_MaxStepNum = 0xffffffff;
    }
    else
    {
        double maxStep = (m_End.f - m_Begin.f) / (double)m_Step.f;
        if (maxStep < 1.0)
            m_MaxStepNum = 0;
        else if (maxStep >= (double)0xffffffff)
            m_MaxStepNum = 0xffffffff;
        else
            m_MaxStepNum = (UINT32)maxStep;

        if (0 == m_MaxStepNum)
        {
            // Oh, well.  abs(step) is greater than abs(end-begin).
            // Treat this as the "const" case.
            Printf(Tee::PriLow,
                   "WARNING: FancyPicker [\"step\", %f, %f, %f], step is > range!\n",
                   m_End.f, m_Step.f, m_End.f);
            m_Step.f = 0.0F;
            m_End.f = m_Begin.f;
            m_MaxStepNum = 0xffffffff;
        }
    }
    m_Value.f = m_Begin.f;
}
StepFpHelper::~StepFpHelper()
{
}

//
// standard Helper functions
//
bool StepFpHelper::ConstructorOK() const
{
    return true;
}
void StepFpHelper::Restart()
{
}
bool StepFpHelper::Ready() const
{
    return true;
}
FancyPicker::eMode StepFpHelper::Mode() const
{
    if (((m_Type == FancyPicker::T_FLOAT) && (m_Step.f == 0.0f)) ||
        (0ULL == m_Step.i))
    {
        return FancyPicker::CONSTANT;
    }

    return FancyPicker::STEP;
}
FancyPicker::eWrapMode StepFpHelper::WrapMode() const
{
    return m_WrapMode;
}
RC StepFpHelper::GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const
{
    MASSERT(pMinPick);
    MASSERT(pMaxPick);
    MASSERT(m_Type != FancyPicker::T_FLOAT);

    UINT64 val1 = static_cast<UINT64>(m_Begin.i);
    UINT64 val2 = static_cast<UINT64>(m_End.i);
    *pMinPick = min(val1, val2);
    *pMaxPick = max(val1, val2);
    return OK;
}

//------------------------------------------------------------------------------
//
// class ListFpHelper
//
//------------------------------------------------------------------------------

//
// Pick functions
//
void ListFpHelper::Pick()
{
    MASSERT(m_NumItems);
    MASSERT(m_Items);
    if (0 == m_RptSum)
    {
        // Whoops, all items have 0 repeats!
        // Force the first item to have a repeat count of 1.
        m_Items->Rpt = 1;
        m_RptSum     = 1;
    }

    UINT32 step = m_PickOnRestart ? m_pFpCtx->RestartNum : m_pFpCtx->LoopNum;

    if (m_WrapMode == FancyPicker::WRAP)
    {
        if (m_RptSum == 1)
            step = 0;
        else
            step %= m_RptSum;
    }
    else
    {
        if (step >= m_RptSum)
            step = m_RptSum - 1;
    }
    MASSERT(step < m_RptSum);

    Item * pItem = m_Items;
    while (step >= pItem->Rpt)
    {
        step -= pItem->Rpt;
        pItem++;
    }
    MASSERT(pItem < m_Items + m_NumItems);

    m_Value.i = pItem->Val.i;
}
void ListFpHelper::FPick()
{
    MASSERT(m_NumItems);
    MASSERT(m_Items);
    if (0 == m_RptSum)
    {
        // Whoops, all items have 0 repeats!
        // Force the first item to have a repeat count of 1.
        m_Items->Rpt = 1;
        m_RptSum     = 1;
    }

    UINT32 step = m_PickOnRestart ? m_pFpCtx->RestartNum : m_pFpCtx->LoopNum;

    if (m_WrapMode == FancyPicker::WRAP)
    {
        if (m_RptSum == 1)
            step = 0;
        else
            step %= m_RptSum;
    }
    else
    {
        if (step >= m_RptSum)
            step = m_RptSum - 1;
    }
    MASSERT(step < m_RptSum);

    Item * pItem = m_Items;
    while (step >= pItem->Rpt)
    {
        step -= pItem->Rpt;
        pItem++;
    }
    MASSERT(pItem < m_Items + m_NumItems);

    m_Value.f = pItem->Val.f;
}

//
// clone function
//
FpHelper* ListFpHelper::Clone() const
{
    ListFpHelper* clonedHelper = new ListFpHelper(this, this->m_WrapMode);
    if (m_SizeItems > clonedHelper->m_SizeItems)
    {
        delete[] clonedHelper->m_Items;
        clonedHelper->m_Items = new Item[m_SizeItems];
    }

    clonedHelper->m_RptSum = this->m_RptSum;
    clonedHelper->m_NumItems = this->m_NumItems;
    clonedHelper->m_SizeItems = this->m_SizeItems;
    for (int i = 0; i < this->m_NumItems; i++)
    {
        clonedHelper->m_Items[i] = this->m_Items[i];
    }

    return clonedHelper;
}

//
// constructor, destructor
//
ListFpHelper::ListFpHelper(const FpHelper * pFpHelper, FancyPicker::eWrapMode wm)
  : FpHelper(pFpHelper)
{
    m_SizeItems = 1;
    m_NumItems  = 0;
    m_WrapMode  = wm;
    m_RptSum    = 0;
    m_Items = new Item [m_SizeItems];
    if (! m_Items)
        m_SizeItems = 0;
}
ListFpHelper::~ListFpHelper()
{
    delete [] m_Items;
}

//
// standard Helper functions
//
bool ListFpHelper::ConstructorOK() const
{
    return (m_Items != 0);
}
void ListFpHelper::Restart()
{
}
bool ListFpHelper::Ready() const
{
    return (m_NumItems == m_SizeItems);
}
FancyPicker::eMode ListFpHelper::Mode() const
{
    return FancyPicker::LIST;
}
FancyPicker::eWrapMode ListFpHelper::WrapMode() const
{
    return m_WrapMode;
}
RC ListFpHelper::GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const
{
    MASSERT(pMinPick);
    MASSERT(pMaxPick);
    MASSERT(m_NumItems);
    MASSERT(m_Items);
    MASSERT(m_Type != FancyPicker::T_FLOAT);

    *pMinPick = m_Items[0].Val.i;
    *pMaxPick = *pMinPick;
    for (int ii = 0; ii < m_NumItems; ++ii)
    {
        UINT64 val = m_Items[ii].Val.i;
        *pMinPick = min(*pMinPick, val);
        *pMaxPick = max(*pMaxPick, val);
    }
    return OK;
}
RC ListFpHelper::GetNumItems(UINT32 *pNumItems) const
{
    MASSERT(pNumItems != nullptr);

    *pNumItems = m_NumItems;
    return OK;
}
RC ListFpHelper::GetRawItems(FancyPicker::PickerItems *pItems) const
{
    MASSERT(pItems != nullptr);

    pItems->clear();
    for (int ii = 0; ii< m_NumItems; ii++)
    {
        pItems->push_back(m_Items[ii].Val.i);
    }
    return OK;
}
RC ListFpHelper::GetFRawItems(FancyPicker::PickerFItems *pItems) const
{
    MASSERT(pItems != nullptr);

    pItems->clear();
    for (int ii = 0; ii< m_NumItems; ii++)
    {
        pItems->push_back(m_Items[ii].Val.f);
    }
    return OK;
}
//
// Add item functions
//
void ListFpHelper::AddItemRepeat(UINT64 Val, int RepeatCount)
{
    MASSERT(m_Items);
    if (m_NumItems >= m_SizeItems)
    {
        Item * tmp = new Item[m_SizeItems+1];
        int i;
        for (i = 0; i < m_NumItems; i++)
            tmp[i] = m_Items[i];

        delete [] m_Items;
        m_Items = tmp;
        m_SizeItems++;
    }

    m_Items[m_NumItems].Val.i = Val;
    m_Items[m_NumItems].Rpt   = RepeatCount;
    if (m_RptSum > (m_RptSum + RepeatCount))
    {
        m_RptSum = 0xffffffff;
        Printf(Tee::PriLow,
               "WARNING: FancyPicker \"list\" repeat sum overflow\n");
    }
    else
    {
        m_RptSum += RepeatCount;
    }
    m_NumItems++;

    if (m_NumItems == 1)
    {
        // Initialize the "current pick" to the first value, in case someone
        // calls GetPick() before calling Pick() the first time.
        m_Value.i = Val;
    }
}
void ListFpHelper::FAddItemRepeat(float Val, int RepeatCount)
{
    MASSERT(m_Items);
    if (m_NumItems >= m_SizeItems)
    {
        Item * tmp = new Item[m_SizeItems+1];
        int i;
        for (i = 0; i < m_NumItems; i++)
            tmp[i] = m_Items[i];

        delete [] m_Items;
        m_Items = tmp;
        m_SizeItems++;
    }

    m_Items[m_NumItems].Val.f = Val;
    m_Items[m_NumItems].Rpt   = RepeatCount;
    if (m_RptSum > (m_RptSum + RepeatCount))
    {
        m_RptSum = 0xffffffff;
        Printf(Tee::PriLow,
               "WARNING: FancyPicker \"list\" repeat sum overflow\n");
    }
    else
    {
        m_RptSum += RepeatCount;
    }
    m_NumItems++;

    if (m_NumItems == 1)
    {
        // Initialize the "current pick" to the first value, in case someone
        // calls GetPick() before calling Pick() the first time.
        m_Value.f = Val;
    }
}

//------------------------------------------------------------------------------
//
// class JsFpHelper
//
//------------------------------------------------------------------------------

//
// Pick functions
//
void JsFpHelper::Pick()
{
    JavaScriptPtr pJs;

    // Colwert m_Pick from jsval to a JSFunction*.
    JSFunction * jsFunc;
    MASSERT(m_Pick);
    if (OK != pJs->FromJsval(m_Pick, &jsFunc))
        return;

    // Build argument list for function.
    JsArray args(2, jsval(0));
    if (OK != pJs->ToJsval(m_pFpCtx->LoopNum, &args[0]))
        return;
    if (OK != pJs->ToJsval(m_pFpCtx->RestartNum, &args[1]))
        return;

    // Call into the javascript function.
    jsval jsResult;
    if (m_pFpCtx->pJSObj)
        pJs->CallMethod(m_pFpCtx->pJSObj, jsFunc, args, &jsResult);
    else
        pJs->CallMethod(jsFunc, args, &jsResult);

    // Colwert result into UINT64.
    pJs->FromJsval(jsResult, &m_Value.i);
}
void JsFpHelper::FPick()
{
    JavaScriptPtr pJs;

    // Colwert m_Pick from jsval to a JSFunction*.
    JSFunction * jsFunc;
    MASSERT(m_Pick);
    if (OK != pJs->FromJsval(m_Pick, &jsFunc))
        return;

    // Build argument list for function.
    JsArray args(2, jsval(0));
    if (OK != pJs->ToJsval(m_pFpCtx->LoopNum, &args[0]))
        return;
    if (OK != pJs->ToJsval(m_pFpCtx->RestartNum, &args[1]))
        return;

    // Call into the javascript function.
    jsval jsResult;
    if (m_pFpCtx->pJSObj)
        pJs->CallMethod(m_pFpCtx->pJSObj, jsFunc, args, &jsResult);
    else
        pJs->CallMethod(jsFunc, args, &jsResult);

    // Colwert result into float.
    double result;
    pJs->FromJsval(jsResult, &result);
    m_Value.f = (float)result;
}

//
// clone function
//
FpHelper* JsFpHelper::Clone() const
{
    JsFpHelper* clonedHelper =
        new JsFpHelper(this, this->m_Pick, this->m_Restart);

    return clonedHelper;
}

//
// constructor, destructor
//
JsFpHelper::JsFpHelper(const FpHelper * pFpHelper, jsval pick, jsval restart)
 : FpHelper(pFpHelper)
{
    m_Pick = pick;
    m_Restart = restart;
}
JsFpHelper::~JsFpHelper()
{
}

//
// standard Helper functions
//
bool JsFpHelper::ConstructorOK() const
{
    return true;
}
void JsFpHelper::Restart()
{
    if (m_Restart)
    {
        JavaScriptPtr pJs;

        // Colwert m_Restart from jsval to a JSFunction*.
        JSFunction * jsFunc;
        if (OK != pJs->FromJsval(m_Restart, &jsFunc))
            return;

        // Build argument list for function.
        JsArray args(2, jsval(0));
        if (OK != pJs->ToJsval(m_pFpCtx->LoopNum, &args[0]))
            return;
        if (OK != pJs->ToJsval(m_pFpCtx->RestartNum, &args[1]))
            return;

        // Call into the javascript function.
        jsval jsResult;
        if (m_pFpCtx->pJSObj)
            pJs->CallMethod(m_pFpCtx->pJSObj, jsFunc, args, &jsResult);
        else
            pJs->CallMethod(jsFunc, args, &jsResult);
    }
}
bool JsFpHelper::Ready() const
{
    return true;
}
FancyPicker::eMode JsFpHelper::Mode() const
{
    return FancyPicker::JS;
}
FancyPicker::eWrapMode JsFpHelper::WrapMode() const
{
    return FancyPicker::NA;
}
RC JsFpHelper::GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const
{
    // We cannot tell what the JS function will return
    return RC::UNSUPPORTED_FUNCTION;
}

//------------------------------------------------------------------------------
FancyPickerArray::FancyPickerArray(int numPickers)
{
    MASSERT(numPickers > 0);
    m_Size    = numPickers;
    m_Pickers = new FancyPicker[m_Size];

    for (int i = 0; i < numPickers; i++)
    {
        m_Pickers[i].SetName(Utility::StrPrintf("%d", i).c_str());
    }
}

FancyPickerArray::FancyPickerArray()
{
    m_Size   = 0;
    m_Pickers = 0;
}

/* virtual */
FancyPickerArray::~FancyPickerArray()
{
    delete [] m_Pickers;
}

RC FancyPickerArray::PickerFromJsval(UINT32 index, jsval value)
{
    MASSERT(m_Size > 0);
    if (index >= m_Size)
        return RC::BAD_PICKER_INDEX;

    return m_Pickers[index].FromJsval(value);
}

RC FancyPickerArray::PickerToJsval(UINT32 index, jsval * pvalue)
{
    MASSERT(m_Size > 0);
    if (index >= m_Size)
        return RC::BAD_PICKER_INDEX;

    if (! m_Pickers[index].IsInitialized())
    {
        MASSERT(0);
        m_Pickers[index].ConfigConst(0);
    }

    return m_Pickers[index].ToJsval(pvalue);
}

RC FancyPickerArray::PickToJsval(UINT32 index, jsval * pvalue)
{
    MASSERT(m_Size > 0);
    if (index >= m_Size)
        return RC::BAD_PICKER_INDEX;

    JavaScriptPtr pJs;

    if (m_Pickers[index].Type() == FancyPicker::T_INT)
    {
        return pJs->ToJsval(m_Pickers[index].GetPick(), pvalue);
    }
    else
    {
        double tmp = m_Pickers[index].FGetPick();
        return pJs->ToJsval(tmp, pvalue);
    }
}

void FancyPickerArray::SetNumPickers(UINT32 numPickers)
{
    // arbitrarily deciding that you cannot call this twice because
    // this used to only be set through a constructor.  If need
    // arrises, you may remove the second part of this assertion.
    MASSERT(numPickers > 0 && m_Size == 0);
    m_Size    = numPickers;
    m_Pickers = new FancyPicker[m_Size];

    for (UINT32 i = 0; i < numPickers; i++)
    {
        m_Pickers[i].SetName(Utility::StrPrintf("%u", i).c_str());
    }
}

void FancyPickerArray::CheckInitialized()
{
    MASSERT(m_Size > 0);
    UINT32 i;
    for (i = 0; i < m_Size; i++)
    {
        if (! m_Pickers[i].IsInitialized())
        {
            MASSERT(0);
            m_Pickers[i].ConfigConst(0xBADF00D);   // punt
        }
    }
}

void  FancyPickerArray::Restart()
{
    MASSERT(m_Size > 0);
    UINT32 i;
    for (i = 0; i < m_Size; i++)
        m_Pickers[i].Restart();
}

void  FancyPickerArray::RestartInitialized()
{
    MASSERT(m_Size > 0);
    UINT32 i;
    for (i = 0; i < m_Size; ++i)
    {
        if(m_Pickers[i].IsInitialized())
            m_Pickers[i].Restart();
    }
}

void FancyPickerArray::SetContext(FancyPicker::FpContext * pFpCtx)
{
    MASSERT(m_Size > 0);
    UINT32 i;
    for (i = 0; i < m_Size; i++)
        m_Pickers[i].SetContext(pFpCtx);
}

RC FancyPickerArray::CheckUsed() const
{
    StickyRC rc;

    for (UINT32 i = 0; i < m_Size; i++)
    {
        rc = m_Pickers[i].CheckUsed();
    }

    return rc;
}
