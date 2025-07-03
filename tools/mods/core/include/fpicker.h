/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2009,2010,2015-2017,2019 by LWPU Corporation.  All rights
 * reserved.  All information contained herein is proprietary and confidential
 * to LWPU Corporation.  Any use, reproduction, or disclosure without the
 * written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   fpicker.h
 * @brief  Declare class FancyPicker.
 */

#pragma once

#include "script.h"
#include "random.h"
#include "massert.h"
#include <string>

class FpHelper;   // fwd declarations, see fpicker.cpp

//---------------------------------------------------------------------------
/**
 * @brief flexibly generate a sequence of UINT32 or float values.
 *
 * Use instead of a simple call to Utility::GetRandom() or GetRandomFloat().
 * Has many modes:
 *
 * - Random
 *    - within range, pick from list, or any combination
 *    - probabilities may be flat or weighted
 * - Shuffle
 *    - like a Random, but all items in list guaranteed to get used eventually
 * - Step
 *    - first, step, last
 *    - stick-at-end or wraparound
 * - List
 *    - iterate through list of values
 *    - stick-at-end or wraparound
 * - JS
 *    - call a JavaScript function to get the next value
 *    - slow, but very flexible
 *
 * Except for JS mode, calling Pick() or FPick() is very fast.
 *
 * Also, the number of calls to GetRandom() doesn't depend on
 * the current set of items/ranges.  This allows you to "force"
 * a particular pick without changing the random sequence for the
 * rest of the test.
 *
 * Several javascript tests use FancyPicker, and allow the configuration
 * of the pickers to be overridden from javascript (GLRandom, Class05f,
 * MemRefresh, ...)
 *
 * There is a description of the javascript syntax for overriding FancyPicker
 * configs <A HREF="glr_comm_h-source.html#Indexes">here</A> in glr_comm.h.
 */

class FancyPicker
{
public:
    FancyPicker();
    FancyPicker(const FancyPicker &fPicker);
    ~FancyPicker();

    void SetName(const char* name);

    //------------------------------------------------------------------------
    // Constants and types:

    enum eType
    {
        T_UNKNOWN,     //!< No Config function has been called yet.
        T_INT,         //!< Use Pick() to get UINT32 values.
        T_INT64,       //!< Use Pick() to get UINT64 values.
        T_FLOAT        //!< Use FPick() to get float32 values.
    };
    enum eMode        //!< how we are generating the next value
    {
        UNINITIALIZED, //!< No Config* function has been called yet.
        PARTIAL,       //!< Some Config function has been called, but all the Add* calls aren't done.
        CONSTANT,      //!< same value every time
        RANDOM,        //!< random picks
        STEP,          //!< steady stepping through a range of values
        LIST,          //!< iterate through a list of values
        JS,            //!< call a JavaScript function to do the picking
        SHUFFLE        //!< Like Random, but all items get used eventually
    };
    enum eWrapMode    //!< what to do after hitting last value in sequence
    {
        NA,            //!< not applicable (Mode is not STEP or LIST)
        CLAMP,         //!< always return last value
        WRAP           //!< jump back to first value, then continue
    };
    struct FpContext  //!< Context used at Pick time (also Restart in PickOnRestart mode)
    {
        Random      Rand;       //!< psuedo-random number generator
        UINT32      LoopNum;    //!< loop number (for list, step, js)
        UINT32      RestartNum; //!< frame number (instead of loop in PickOnRestart mode)
        JSObject *  pJSObj;     //!< "this" pointer, passed to js methods
    };

    //------------------------------------------------------------------------
    // Set configuration:

    /**
     * @brief Set the FpContext to be used during calls to Pick().
     */
    void SetContext(FpContext * pFpCtx);

    /**
     * @{
     * @brief ReConfig this object to always return the same value.
     */
    RC ConfigConst  (UINT32 value);
    RC ConfigConst64(UINT64 value);
    RC FConfigConst (float value);
    //@}

    /**
     * @{
     * @brief ReConfig this object for random picking.
     *
     * First, call ConfigRandom.
     * Then,  call AddRandItem or AddRandRange N times.
     * Finally, call Compile to scale the probability tables once the sum is known.
     */
    RC   ConfigRandom  (const char* name = "");
    void AddRandItem   (UINT32 RelativeProbability, UINT32 Item);
    void AddRandRange  (UINT32 RelativeProbability, UINT32 Min, UINT32 Max);
    RC   ConfigRandom64(const char* name = "");
    void AddRandItem64 (UINT32 RelativeProbability, UINT64 Item);
    void AddRandRange64(UINT32 RelativeProbability, UINT64 Min, UINT64 Max);
    RC   FConfigRandom (const char* name = "");
    void FAddRandItem  (UINT32 RelativeProbability, float  Item);
    void FAddRandRange (UINT32 RelativeProbability, float  Min, float  Max);
    void CompileRandom ();
    /** @} */

    /**
     * @{
     * @brief ReConfig this object for shuffle picking.
     *
     * First, call ConfigShuffle.
     * Then,  call AddRandItem N times.
     * Finally, call Compile to scale the probability tables once the sum is known.
     */
    RC ConfigShuffle  (const char* name = "");
    RC ConfigShuffle64(const char* name = "");
    RC FConfigShuffle (const char* name = "");
    void CompileShuffle () { return CompileRandom(); }
    /** @} */

    /**
     * @{
     * @brief ReConfig this object for stepping across a range.
     *
     * If (sign of Step) != (sign of End-Begin), Step = -Step.
     */
    RC ConfigStep  (UINT32 Begin, int   Step, UINT32 End, eWrapMode wm = WRAP);
    RC ConfigStep64(UINT64 Begin, INT64 Step, UINT64 End, eWrapMode wm = WRAP);
    RC FConfigStep (float  Begin, float Step, float  End, eWrapMode wm = WRAP);
      /** @} */

    /**
     * @{
     * @brief ReConfig this object for stepping through a list.
     *
     * First, call ConfigList.
     * Then,  call AddListItem N times.
     * If RepeatCount is > 1, the item will be used RepeatCount times.
     */
    RC   ConfigList   (eWrapMode wm = WRAP);
    void AddListItem  (UINT32 Item, int RepeatCount=1);
    RC   ConfigList64 (eWrapMode wm = WRAP);
    void AddListItem64(UINT64 Item, int RepeatCount=1);
    RC   FConfigList  (eWrapMode wm = WRAP);
    void FAddListItem (float  Item, int RepeatCount=1);
    /** @} */

    /**
     * @{
     * @brief ReConfig this object for calling out to JavaScript.
     *
     * If restartPtr is set, that JS function will be called at Restart() time.
     * The JS functions will be called with no arguments.
     */
    RC   ConfigJs  (jsval funcPtr, jsval restartPtr=0);
    RC   Config64Js(jsval funcPtr, jsval restartPtr=0);
    RC   FConfigJs (jsval funcPtr, jsval restartPtr=0);
    /** @} */

    /**
     * @brief Enable or disable PickOnRestart mode.
     *
     * If this mode is enabled, we generate a new value in the sequence only
     * on the first Pick after each Restart, then always return that value
     * on all subsequent Pick calls.
     */
    void SetPickOnRestart(bool enable);

    /**
     * @{
     * @brief Read back current configuration.
     */
    eType      Type() const;
    eMode      Mode() const;
    eWrapMode  WrapMode() const;
    bool       PickOnRestart() const;
    bool       IsInitialized() const;
    /** @} */

    /**
     * @brief Restart the random sequence.
     *
     * @verbatim
     * If !PickOnRestart,
     *   if STEP or LIST, go to beginning of sequence,
     *   if JS, call restart function.
     * If PickOnRestart,
     *   choose a new value to use for all calls to Pick.
     * @endverbatim
     */
    void Restart();

    /**
     * @brief Advance to and return the next value in the sequence.
     */
    UINT32 Pick();
    UINT64 Pick64();
    /**
     * @brief Same as Pick, but for T_FLOAT.
     */
    float  FPick();

    /**
     * @brief Return current value (result of last Pick or FPick).
     */
    UINT32 GetPick() const;
    UINT64 GetPick64() const;
    /**
     * @brief Same as GetPick, but for T_FLOAT.
     */
    float  FGetPick() const;

    /**
     * @brief UnConfig(), then configure the picker from a jsval array.
     *
     * If type is T_UNKNOWN, guesses from JS, else retains old T_INT or T_FLOAT.
     * @return error if the jsval is invalid or type mismatch.
     *
     * For a description of the javascript syntax to specify FancyPicker configs,
     * see <A HREF="glr_comm_h-source.html#Indexes">glr_comm.h</A>.
     */
    RC FromJsval(jsval js);

    /**
     * @brief Get a jsval containing this object's config.
     *
     * For a description of the javascript structure returned,
     * see <A HREF="glr_comm_h-source.html#Indexes">glr_comm.h</A>.
     */
    RC ToJsval(jsval * pjs) const;

    bool WasSetFromJs() {return m_SetFromJs;}

    FancyPicker & operator=(const FancyPicker &rhs);

    /**
     * @brief Get the minimum and/or maximum value that Pick() can return
     *
     * Unused arguments can be set to nullptr.
     */
    RC GetPickRange(UINT32 *pMinPick, UINT32 *pMaxPick) const;
    RC GetPickRange(UINT64 *pMinPick, UINT64 *pMaxPick) const;

    RC GetNumItems(UINT32 *pNumItems) const;

    typedef vector<UINT64> PickerItems;
    RC GetRawItems(PickerItems *pItems) const;

    typedef vector<float> PickerFItems;
    RC GetFRawItems(PickerFItems *pItems) const;

    // The Override below allows to modify a picker (including replacing it
    // with completely different type - for example const) and not disturb
    // the other pickers. It is even possible upon calling StopOverride to
    // bring the original picker back with the randomization still in sync.
    // For all this to work the picker has to be still Picked the same number
    // of times as before.
    // Usage example:
    //     CHECK_RC(m_Pickers[RND_TX_SPARSE_TEX_INT_FMT_CLASS].StartOverride());
    //     m_Pickers[RND_TX_SPARSE_TEX_INT_FMT_CLASS].ConfigConst(GL_VIEW_CLASS_32_BITS);
    //     m_Pickers[RND_TX_SPARSE_TEX_INT_FMT_CLASS].Pick();
    //     m_Pickers[RND_TX_SPARSE_TEX_INT_FMT_CLASS].StopOverride();
    RC StartOverride();
    void StopOverride();

    // Check if all fancy picker values have been used during the test
    RC CheckUsed() const;

private:
    void SetHelperName(const char* name);

    FpHelper *  m_Helper;
    FpHelper *  m_OverridenHelper;
    string      m_DefaultName;

    static FpContext s_DefaultContext;

    bool m_SetFromJs;
};

//------------------------------------------------------------------------------
// A class for managing an array of FancyPicker objects.
//
// Mods tests like GLRandom and Random2d use dozens of FancyPicker objects.
//
// This class contains an array of FancyPicker objects and has some handy
// functions for managing them.
//
class FancyPickerArray
{
public:
    FancyPickerArray(int numPickers);
    FancyPickerArray();
    ~FancyPickerArray();

    // JS interface functions

    RC PickerFromJsval(UINT32 index, jsval value);
    RC PickerToJsval(UINT32 index, jsval * pvalue);
    RC PickToJsval(UINT32 index, jsval * pvalue);

    // Set the number of pickers if default constructor was used.
    void SetNumPickers(UINT32 numPickers);
    UINT32 GetNumPickers() {return m_Size;}

    // Assert (debug) or print warning (release) if any FancyPicker is not
    // initialized and ready for use.
    void CheckInitialized();

    // Call Restart() on each FancyPicker.
    void Restart();

    // Call Restart on each FancyPicker that is initialized
    void RestartInitialized();

    // Call SetContext() on each FancyPicker.
    void SetContext(FancyPicker::FpContext * pFpCtx);

    // Access to the contained FancyPickers.
    FancyPicker & operator[] (int index);
    const FancyPicker & operator[] (int index) const;

    // Check if all fancy picker values have been used during the test
    RC CheckUsed() const;

private:
    void operator=(FancyPickerArray &); // not implemented

    UINT32        m_Size;
    FancyPicker * m_Pickers;
};

inline FancyPicker & FancyPickerArray::operator[] (int index)
{
    MASSERT(index >=0);
    MASSERT((UINT32)index < m_Size);
    return m_Pickers[index];
}
inline const FancyPicker & FancyPickerArray::operator[] (int index) const
{
    MASSERT(index >=0);
    MASSERT((UINT32)index < m_Size);
    return m_Pickers[index];
}
