/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2011,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
//!
//! \file ptrnclss.h
//! \brief Header file for the PatternClass class

#ifndef INCLUDED_PATTERNCLASS_H
#define INCLUDED_PATTERNCLASS_H

#ifndef MATS_STANDALONE
#ifndef INCLUDED_RANDOM_H
   #include "random.h"
#endif
#ifndef INCLUDED_RC_H
   #include "core/include/rc.h"
#endif
#endif

class PatternClass
{
public:
    PatternClass();
    ~PatternClass();

    enum PatternSets
    {
        PATTERNSET_NORMAL = (1 << 0 ),
        PATTERNSET_STRESSFUL = (1 << 1),
        PATTERNSET_USER = (1 << 2),
        PATTERNSET_ALL = ((1 << 3) - 1)  // DO NOT forget to change this if
                                         // you add new enum to this list
    };

    RC SelectPatternSet(PatternSets Set);
    RC SelectPatterns( vector<UINT32> *Patterns);
    RC DisableRandomPattern();
    RC GetSelectedPatterns( vector<UINT32> *Patterns);
    RC GetPatterns( PatternSets Set, vector<UINT32> *Patterns );
    RC GetPatternName( UINT32 pattern, string *pName) const;
    RC GetPatternNumber(UINT32 *pPatternNum, string szName) const;
    UINT32 GetPatternLen( UINT32 Pattern ) const;

    RC FillMemoryWithPattern(volatile void *pBuffer,
                             UINT32 PitchInBytes,
                             UINT32 Lines,
                             UINT32 WidthInBytes,
                             UINT32 Depth,
                             UINT32 PatternNum,
                             UINT32 *RepeatInterval);

    RC FillMemoryWithPattern(volatile void *pBuffer,
                            UINT32 pitchInBytes,
                            UINT32 lines,
                            UINT32 widthInBytes,
                            UINT32 depth,
                            string patternName,
                            UINT32 *repeatInterval);

    RC FillMemoryIncrementing(volatile void *pBuffer,
                              UINT32 PitchInBytes,
                              UINT32 Lines,
                              UINT32 WidthInBytes,
                              UINT32 Depth);

    RC FillWithRandomPattern(volatile void *pBuffer,
                             UINT32 PitchInBytes,
                             UINT32 Lines,
                             UINT32 WidthInBytes,
                             UINT32 Depth);

    typedef RC CheckMemoryCallback(void *identifier, UINT32 offset,
                                   UINT32 expected, UINT32 actual,
                                   UINT32 patternOffset,
                                   const string &patternName);
    RC CheckMemory(volatile void *pBuffer,
                     UINT32 pitchInBytes,
                     UINT32 lines,
                     UINT32 widthInBytes,
                     UINT32 depth,
                     UINT32 patternNum,
                     CheckMemoryCallback *pCallback,
                     void *pCaller
                     ) const;

    RC CheckMemoryIncrementing(volatile void *pBuffer,
                     UINT32 pitchInBytes,
                     UINT32 lines,
                     UINT32 widthInBytes,
                     UINT32 depth,
                     CheckMemoryCallback *pCallback,
                     void *caller
                     );

    RC DumpSelectedPatterns();

    UINT32 GetLwrrentNumOfSelectedPatterns();
    UINT32 GetTotalNumOfPatterns();

    // must be called at least once before random pattern could be used
    void FillRandomPattern(UINT32 seed);

    void IncrementLwrrentCheckPatternIndex(); // used to skip checking the empty box
    void DecrementLwrrentCheckPatternIndex(); //Adjustment needed for retries

    void ResetFillIndex();
    void ResetCheckIndex();

    // this is a token to end the pattern array.  There isn't anything
    // special about the constant 0x123
    static const UINT32 END_PATTERN = 0x123;

    struct PTTRN
    {
        string patternName;
        UINT32 patternAttributes;
        UINT32 *thePattern;
    };

    typedef vector<const PTTRN*> PatternContainer;
    //! Returns all patterns known to the pattern class.
    static PatternContainer GetAllPatternsList();

private:
    UINT32 m_LwrrentFillPatternIndex;
    UINT32 m_LwrrentCheckPatternIndex;
    vector<UINT32> SelectedPatternSet; // keeps the IDs of the selected patterns
    vector<PTTRN*> & m_RefAllPatternsList;
};
#endif //INCLUDED_PATTERNCLASS_H

