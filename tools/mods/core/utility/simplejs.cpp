/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/**
 * @file   simplejs.cpp
 * @brief  Example JavaScript class.
 *
 * This is an implementation of a JavaScript class called Simple to help
 * new MODS programmers get started.
 *
 *  @internal
 *
 *  Please note the location of the doxygen comments in this file.  Placement
 *  is important, since JavaScript classes are parsed by a fairly rigid perl
 *  script in order to make them palatable to doxygen.
 *
 *  In this file, many things are dolwmented with both a short, one-line
 *  comment and a longer, more detailed comment block.  This is ideal, but
 *  not required.  If the thing you're dolwmenting only needs one type of
 *  comment, doxygen will handle this correctly.
 */

/*
 * doxygen comments can look like:
 *       //!     [single-line comment]
 *       ///     [single-line comment]
 */
#if 0
       /**
          [multi-line comment]
        */

       /*!
          [multi-line comment]
        */
#endif

#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/script.h"
#include "core/include/gpu.h"
#include "core/include/massert.h"
#include "core/include/types.h"
#include "math.h"

namespace Simple
{

}

//------------------------------------------------------------------------------
// object, methods, and tests.
//------------------------------------------------------------------------------

// This declares a JavaScript class
JS_CLASS(Simple);

//! Short description of the Simple object.
/**
 * A more detailed description of the simple object.  This could be quite long
 * if your object is responsible for a complicated test or series of tests.
 */
static SObject Simple_Object
(
   // The first parameter is the name of the class as seen in JavaScript
   "Simple",

   // This is the name of the class in C++.  It must match the name of the
   // class you declared above in JS_CLASS.  If you look at the JS_CLASS
   // macro, you'll see that it appends "Class" to whatever you set up the
   // class name.  So, when I declared my class with JS_CLASS(Simple), the
   // actual C++ name is SimpleClass.
   SimpleClass,

   // set to zero for most purposes
   0,

   // set to zero for most purposes
   0,

   // this is the help message the user will get if they run Help() on your
   // object
   "This is the help message for the Simple class."
);

//
// Javascript properties
//

//! Short description of Number1
/**
 * Longer description of Number1.
 */
static SProperty Simple_Number1
(

   // This property is part of the Simple object
   Simple_Object,

   // this is the name that the Javascript user will see.  Note that it doesn't
   // have to be the same as the C++ name.  In this case, the JavaScript name
   // is "Num1" and the C++ name is "Number1"
   "Num1",

   // set to zero for most purposes
   0,

   // this is the default value for our object.
   3,

   // get and set overrides... more on this later in the file.  This property
   // doesn't use get or set overrides, so set these two
   0,
   0,

   // Flags.  There are a few special flags we could use.  The most useful one
   // is JSPROP_READONLY.  We want the Javascript user to be able to change
   // this property, though... so set this to zero for now
   0,

   // help message
   "This is the Number1 property."
);

// Here's another property

// prototypes Set override.  We're going to use this to disallow
// the user from setting this value to less than 3 or more than 15.
P_( Simple_Set_Number2 );

//! Short description of Number2.
/**
 * Number2 should never be allowed to be less than 3 or more than 15.
 * If this ever happened, your computer would explode.
 */
static SProperty Simple_Number2
(
   Simple_Object, // object it belongs to
   "Num2",        // name seen in JavaScript
   0,             // set to zero
   5,             // default value.

   0,

   // We're going to use a set override here.  The Set override will be called
   // when the user tries to change this property.
   Simple_Set_Number2,
   0,
   "This is the Number2 property.  Must be >= 3 and <= 15."
);

P_( Simple_Set_Number3 );

//! Short description of Number3
/**
 * Number3 can never be a negative number. Your computer wont explode, but if you did this,
 * you would be hunted down.
 */
static SProperty Simple_Number3
(
   Simple_Object,       // Object this belongs to
   "Num3",              // name seen in JavaScript
   0,
   7,                   // default value
   0,                   // get override
   Simple_Set_Number3,  // set override
   0,                   // flags

   // help message
   "This is the Number3 property. Must be >= 0."
);

// Another property called Sum.  This will contain the sum of Number1 and
// Number2.  We need to define a Get method that will get called every time
// this property is evaluated.

// Prototypes Get override.
// The Get override will be called when the user evaluates the property.
P_( Simple_Get_Sum );

//! Contains the sum of Number1 and Number2.
/**
 * Long description of Sum.
 */
static SProperty Simple_Sum
(
   Simple_Object, // object it belongs to
   "Sum",         // name seen in JavaScript
   0,             // set to zero
   0,             // default value.

   Simple_Get_Sum, // Get override
   0,

   // We don't want to allow the user to write this value... since it will
   // always depend on Number1 and Number2
   JSPROP_READONLY,

   "Sum of Num1 and Num2" // help message
);

P_(Simple_Set_Number3)
{
   // make sure that we have a valid context and value pointer.  I've never
   // seen this assertion fail...but extra asserts never hurt!
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument from the JavaScript environment and put it in a
   // local variable.
   INT32 arg3;
   JavaScriptPtr pJs;

   if (OK != pJs->FromJsval(*pValue, &arg3))
   {
      JS_ReportError(pContext, "Failed to set Simple.Num3.");
      return JS_FALSE;
   }

   // if the value is less than 0, reset to 0
   if (arg3 < 0 ) arg3 = 0;

   // write the new value back out to JavaScript.
   pJs->ToJsval(arg3, pValue);

   // normally return JS_TRUE.  If you return JS_FALSE, then the exelwtion
   // of the user's script will stop.  You should only use JS_FALSE for
   // severe errors.
   return JS_TRUE;
}

P_(Simple_Set_Number2)
{
   // make sure that we have a valid context and value pointer.  I've never
   // seen this assertion fail...but extra asserts never hurt!
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   // Colwert the argument from the JavaScript environment and put it in a
   // local variable.
   UINT32 LocalValue;
   JavaScriptPtr pJs;

   if (OK != pJs->FromJsval(*pValue, &LocalValue))
   {
      JS_ReportError(pContext, "Failed to set Simple.Number2.");
      return JS_FALSE;
   }

   // if the value the user is trying to set is outside the range we want,
   // restrict it
   if (LocalValue < 3 ) LocalValue = 3;
   if (LocalValue > 15) LocalValue = 15;

   // write the new value back out to JavaScript.
   pJs->ToJsval(LocalValue, pValue);

   // normally return JS_TRUE.  If you return JS_FALSE, then the exelwtion
   // of the user's script will stop.  You should only use JS_FALSE for
   // severe errors.
   return JS_TRUE;
}

P_(Simple_Get_Sum)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   UINT32 Arg1;
   UINT32 Arg2;
   UINT32 Sum;

   // we need a JavaScript object to get other properties from the JavaScript
   // space.
   JavaScriptPtr pJs;

   // go get the values in the Number1 and Number2 properties
   pJs->GetProperty(Simple_Object, Simple_Number1, &Arg1);
   pJs->GetProperty(Simple_Object, Simple_Number2, &Arg2);

   // add them together
   Sum = Arg1 + Arg2;

   // send the sum back to JavaScript
   if (OK != pJs->ToJsval(Sum, pValue))
   {
      JS_ReportError(pContext, "Failed to get Simple.Sum.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }
   return JS_TRUE;
}

//
// Methods and Tests
//

// There are two types of methods...
//
// An SMethod should be used for a method that can never fail.  A method that
// just returns the time of day, for instance, can never fail... it should
// always be able to return the time of day.

// An STest should be used for a method that CAN fail.  An example of this is
// a method that reads a sector from a hard drive.  Some of the errors that
// can occur are an invalid sector argument, the access times out, the
// drive doesn't respond, there is a protocol violation, etc.

// Let's declare an SMethod that will multiply its argument, Number1 and Number2

C_(Simple_Multiply); // forward declaration
//! Multiplies three numbers.
/**
 * @param arg1 The number to multiply by Number1 and Number2.
 *
 * @return \a arg1 * Number1 * Number2.
 */
static SMethod Simple_Multiply
(
   // the object it belongs to
   Simple_Object,

   // its name in JavaScript
   "Multiply",

   // the C++ function that will be called when this method is ilwoked
   C_Simple_Multiply,

   // the number of arguments the method takes
   1,

   // help message
   "Returns the product of its argument, Number1 and Number2"
);

// this is the actual C++ code that implments Multiply
C_(Simple_Multiply)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;

   UINT32 Num1;
   UINT32 Num2;
   UINT32 Arg;
   UINT32 Product;

   // Make sure the user passed us exactly 1 argument.
   if(NumArguments !=1)
   {
      JS_ReportError(pContext, "Usage: Simple.Multiply(multiplier)");
      return JS_FALSE;
   }

   // make sure we can read the parameter.  Store the argument in Arg.
   // If we get
   if (OK != pJavaScript->FromJsval(pArguments[0], &Arg))
   {
      JS_ReportError(pContext, "Error in Simple.Multiply()");
      return JS_FALSE;
   }

   // We'll also need to fetch the Number1 and Number2 property values.
   pJavaScript->GetProperty(Simple_Object, Simple_Number1, &Num1);
   pJavaScript->GetProperty(Simple_Object, Simple_Number2, &Num2);

   // compute the product
   Product = Num1 * Num2 * Arg;

   // send the final result back to Javascript.
   if (OK != pJavaScript->ToJsval(Product, pReturlwalue))
   {
      JS_ReportError(pContext, "Error oclwrred in Simple.Multiply()");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }
   return JS_TRUE;
}

P_(Simple_Get_Sub);

//! Contains the subtraction of Number1 from Number3.
/**
 * Long description of Sub.
 */
static SProperty Simple_Sub
(
   Simple_Object, // object it belongs to
   "Subtract",         // name seen in JavaScript
   0,             // set to zero
   0,             // default value.

   Simple_Get_Sub, // Get override
   0,

   // We don't want to allow the user to write this value... since it will
   // always depend on Number1 and Number3
   JSPROP_READONLY,

   "Subtraction of Num1 from Num3" // help message
);

P_(Simple_Get_Sub)
{
   MASSERT(pContext != 0);
   MASSERT(pValue   != 0);

   INT32 Arg1;
   INT32 Arg3;
   INT32 Sub;

   // we need a JavaScript object to get other properties from the JavaScript
   // space.
   JavaScriptPtr pJs;

   // go get the values in the Number1 and Number3 properties
   pJs->GetProperty(Simple_Object, Simple_Number1, &Arg1);
   pJs->GetProperty(Simple_Object, Simple_Number3, &Arg3);

   // subtract Arg1 from Arg3
   Sub = Arg3 - Arg1;

   // send the value back to JavaScript
   if (OK != pJs->ToJsval(Sub, pValue))
   {
      JS_ReportError(pContext, "Failed to get Simple.Sub.");
      *pValue = JSVAL_NULL;
      return JS_FALSE;
   }
   return JS_TRUE;
}

C_(Simple_Pow); // forward declaration
//! Takes Num1 to the power of arg1
/**
 * @param arg1 The number to raise Num1 by
 *
 * @return \a Num1 ^ arg1
 */
static SMethod Simple_Pow
(
   // the object it belongs to
   Simple_Object,

   // its name in JavaScript
   "Pow",

   // the C++ function that will be called when this method is ilwoked
   C_Simple_Pow,

   // the number of arguments the method takes
   1,

   // help message
   "Returns Num1 raised to the power of arg1."
);

// this is the actual C++ code that implments Pow
C_(Simple_Pow)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;
   *pReturlwalue = JSVAL_NULL;

   INT32 Num1;
   INT32 Arg;
   UINT64 power;

   // Make sure the user passed us exactly 1 argument.
   if(NumArguments !=1)
   {
      JS_ReportError(pContext, "Usage: Simple.Pow(power)");
      return JS_FALSE;
   }

   // make sure we can read the parameter.  Store the argument in Arg.
   // If we get
   if (OK != pJavaScript->FromJsval(pArguments[0], &Arg))
   {
      JS_ReportError(pContext, "Error in Simple.Pow()");
      return JS_FALSE;
   }

   // We'll also need to fetch the Number1 property value.
   pJavaScript->GetProperty(Simple_Object, Simple_Number1, &Num1);

   // compute the product
   power = (UINT64)pow((double)Num1, Arg);

   // send the final result back to Javascript.
   if (OK != pJavaScript->ToJsval(power, pReturlwalue))
   {
      JS_ReportError(pContext, "Error oclwrred in Simple.power()");
      *pReturlwalue = JSVAL_NULL;
      return JS_FALSE;
   }
   return JS_TRUE;
}
