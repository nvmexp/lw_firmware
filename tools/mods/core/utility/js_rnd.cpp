/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2010, 2018-2019 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#include "random.h"
#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/utility.h"

//! \brief Create a JavaScript "class" to wrap the C++ Random class.

static JSBool C_Random_constructor
(
   JSContext *cx,
   JSObject *obj,
   uintN argc,
   jsval *argv,
   jsval *rval
)
{
    if (argc != 0)
    {
       JS_ReportError(cx, "Usage : new Random()");
       return JS_FALSE;
    }

    Random * pRandom = new Random;
    MASSERT(pRandom);
    return JS_SetPrivate(cx, obj, pRandom);
}

static void C_Random_finalize
(
   JSContext *cx,
   JSObject *obj
)
{
   void *pvRandom = JS_GetPrivate(cx, obj);
   if(pvRandom)
   {
      Random * pRandom = (Random *)pvRandom;
      delete pRandom;
   }
}

static JSClass Random_class =
{
   "Random",
   JSCLASS_HAS_PRIVATE,
   JS_PropertyStub,
   JS_PropertyStub,
   JS_PropertyStub,
   JS_PropertyStub,
   JS_EnumerateStub,
   JS_ResolveStub,
   JS_ColwertStub,
   C_Random_finalize
};

static SObject Random_Object
(
   "Random",
   Random_class,
   0,
   0,
   "JS wrapper for a Random object",
   C_Random_constructor
);

JS_SMETHOD_LWSTOM
(
    Random,
    SeedRandom,
    1,
    "Seed & reset the generator"
)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);

    JavaScriptPtr pJavaScript;
    Random * pRandom = JS_GET_PRIVATE(Random, pContext, pObject, "Random");
    if (pRandom == nullptr)
    {
        return JS_FALSE;
    }

    if (NumArguments != 1)
    {
        JS_ReportError(pContext, "Usage: Random.SeedRandom(seed)");
        return JS_FALSE;
    }

    RC rc;
    if (JSVAL_IS_NUMBER(pArguments[0]))
    {
        UINT32 seed = 0;
        rc = pJavaScript->FromJsval(pArguments[0], &seed);
        pRandom->SeedRandom(seed);
    }
    else
    {
        vector<UINT32> seedArray;
        rc = pJavaScript->FromJsval(pArguments[0], &seedArray);
        pRandom->SeedRandom(seedArray.data(), seedArray.size());
    }
    if (rc != RC::OK)
    {
        JS_ReportError(pContext,
            "Random.SeedRandom() arg must be a number or array of numbers");
        return JS_FALSE;
    }

    return JS_TRUE;
}

JS_SMETHOD_LWSTOM
(
    Random,
    GetRandom,
    2,
    "Return a random integer between low and high inclusive."
)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    UINT32 lo;
    UINT32 hi;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &lo)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &hi)))
    {
        JS_ReportError(pContext, "Usage: Random.GetRandom(lo, hi)");
        return JS_FALSE;
    }

    Random * pRandom = JS_GET_PRIVATE(Random, pContext, pObject, "Random");
    if (pRandom)
    {
        const UINT32 result = pRandom->GetRandom(lo, hi);

        if (OK == pJavaScript->ToJsval(result, pReturlwalue))
            return JS_TRUE;
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM
(
    Random,
    GetWeightedRandom,
    1,
    "Return a random integer with weighted probability."
)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    vector<Random::PickItem> Items;
    if ((NumArguments != 1) ||
        (OK != Utility::PickItemsFromJsval(pArguments[0], &Items)))
    {
        JS_ReportError(pContext, "Usage: GetWeightedRandom([[prob, min, max],[prob,min,max]...)");
        return JS_FALSE;
    }

    Random * pRandom = JS_GET_PRIVATE(Random, pContext, pObject, "Random");
    if (pRandom)
    {
        const UINT32 result = pRandom->Pick(&Items[0]);

        if (OK == pJavaScript->ToJsval(result, pReturlwalue))
            return JS_TRUE;
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM
(
    Random,
    GetRandomFloat,
    2,
    "Return a random float between low and high inclusive."
)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    double lo;
    double hi;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &lo)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &hi)))
    {
        JS_ReportError(pContext, "Usage: Random.GetRandomFloat(lo, hi)");
        return JS_FALSE;
    }

    Random * pRandom = JS_GET_PRIVATE(Random, pContext, pObject, "Random");
    if (pRandom)
    {
        const float result = pRandom->GetRandomFloat(lo, hi);
        if (OK == pJavaScript->ToJsval(result, pReturlwalue))
            return JS_TRUE;
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM
(
    Random,
    GetRandomFloatExp,
    2,
    "Return float with random mantissa and sign, with exp clamped to range."
)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    INT32 minPow2;
    INT32 maxPow2;

    if ((NumArguments != 2) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &minPow2)) ||
        (OK != pJavaScript->FromJsval(pArguments[1], &maxPow2)))
    {
        JS_ReportError(pContext, "Usage: Random.GetRandomFloatExp(minPow2, ,maxPow2)");
        return JS_FALSE;
    }

    Random * pRandom = JS_GET_PRIVATE(Random, pContext, pObject, "Random");
    if (pRandom)
    {
        const float result = pRandom->GetRandomFloatExp(minPow2, maxPow2);
        if (OK == pJavaScript->ToJsval(result, pReturlwalue))
            return JS_TRUE;
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM
(
    Random,
    GetNormalFloat,
    0,
    "Return float with normal distribution (mean 0, stdev +-1)."
)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    if (NumArguments != 0)
    {
        JS_ReportError(pContext, "Usage: Random.GetNormalFloat()");
        return JS_FALSE;
    }
    JavaScriptPtr pJavaScript;

    Random * pRandom = JS_GET_PRIVATE(Random, pContext, pObject, "Random");
    if (pRandom)
    {
        const float result = pRandom->GetNormalFloat();
        if (OK == pJavaScript->ToJsval(result, pReturlwalue))
            return JS_TRUE;
    }

    return JS_FALSE;
}

JS_SMETHOD_LWSTOM
(
    Random,
    Shuffle,
    2,
    "Create and return a new Array that is a shuffled copy of the input Array."
)
{
    MASSERT(pContext     !=0);
    MASSERT(pArguments   !=0);
    MASSERT(pReturlwalue !=0);

    JavaScriptPtr pJavaScript;
    JsArray inA;
    UINT32 outLen = 0;

    if ((NumArguments < 1) ||
        (OK != pJavaScript->FromJsval(pArguments[0], &inA)) ||
        (NumArguments > 1 &&
         OK != pJavaScript->FromJsval(pArguments[1], &outLen)) ||
        (NumArguments > 2)
        )
    {
        JS_ReportError(pContext,
                       "Usage: Random.Shuffle(inArray, outLen=inArray.length)");
        return JS_FALSE;
    }
    if (outLen < 1)
        outLen = static_cast<UINT32>(inA.size());
    if (outLen > inA.size())
        outLen = static_cast<UINT32>(inA.size());

    Random * pRandom = JS_GET_PRIVATE(Random, pContext, pObject, "Random");
    if (pRandom)
    {
        vector<UINT32> idxs(inA.size());
        for (UINT32 i = 0; i < inA.size(); i++)
            idxs[i] = i;

        pRandom->Shuffle(static_cast<UINT32>(inA.size()), &idxs[0], outLen);

        JsArray outA;
        for (UINT32 i = 0; i < outLen; i++)
            outA.push_back(inA[idxs[i]]);

        if (OK == pJavaScript->ToJsval(&outA, pReturlwalue))
            return JS_TRUE;
    }

    return JS_FALSE;
}
