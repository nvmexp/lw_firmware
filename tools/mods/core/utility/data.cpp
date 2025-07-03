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

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/data.h"
#include "core/include/massert.h"

void VectorData::Print( vector<UINT08> * pData, Tee::Priority Pri, UINT08 BytePerLine )
{
   if (!pData)
   {
      Printf(Tee::PriHigh, "*** ERROR: VectorData::Print() Invalid pData.***\n");
      return;
   }

   Printf(Pri, " PrintVector(), size = 0x%x\n", (UINT32)pData->size());
   for (UINT32 i = 0; i<pData->size(); i++)
   {
      if (i%BytePerLine == 0)
      {
         if (i!=0)
            Printf(Pri, "\n");
         Printf(Pri, "0x%04x\t", i);
      }
      Printf(Pri, "%02x ", (*pData)[i]);
   }
   Printf(Pri, "\n");
}

void VectorData::Print( vector<UINT16> * pData, Tee::Priority Pri, UINT08 WordPerLine)
{
   if (!pData)
   {
      Printf(Tee::PriHigh, "*** ERROR: VectorData::Print() Invalid pData.***\n");
      return;
   }

   Printf(Pri, " PrintVector(), size = 0x%x\n", (UINT32)pData->size());
   for (UINT32 i = 0; i<pData->size(); i++)
   {
      if (i%WordPerLine == 0)
      {
         if (i!=0)
            Printf(Pri, "\n");
         Printf(Pri, "0x%04x\t", i);
      }
      Printf(Pri, "%04x ", (*pData)[i]);
   }
   Printf(Pri, "\n");
}

void VectorData::Print( vector<UINT32> * pData, Tee::Priority Pri, UINT08 DWordPerLine)
{
   if (!pData)
   {
      Printf(Tee::PriHigh, "*** ERROR: VectorData::Print() Invalid pData.***\n");
      return;
   }

   Printf(Pri, " PrintVector(), size = 0x%x\n", (UINT32)pData->size());
   for (UINT32 i = 0; i<pData->size(); i++)
   {
      if (i%DWordPerLine == 0)
      {
         if (i!=0)
            Printf(Pri, "\n");
         Printf(Pri, "0x%04x ", i);
      }
      Printf(Pri, "%08x ", (*pData)[i]);
   }
   Printf(Pri, "\n");
}

void VectorData::Print( vector<void *> * pData, Tee::Priority Pri, UINT08 DWordPerLine)
{
   if (!pData)
   {
      Printf(Tee::PriHigh, "*** ERROR: VectorData::Print() Invalid pData.***\n");
      return;
   }

   Printf(Pri, " PrintVector(), size = 0x%x\n", (UINT32)pData->size());
   for (UINT32 i = 0; i<pData->size(); i++)
   {
      if (i%DWordPerLine == 0)
      {
         if (i!=0)
            Printf(Pri, "\n");
         Printf(Pri, "0x%04x ", i);
      }
      Printf(Pri, "%16p ", (*pData)[i]);
   }
   Printf(Pri, "\n");
}

RC VectorData::Compare( vector<UINT08>* pData, vector<UINT08>* pExpData )
{
   size_t DataSize = pData->size();
   size_t ExpSize  = pExpData->size();

   if (DataSize != ExpSize)
   {
      Printf(Tee::PriHigh, "*** Error: Data mis compare: different size %ld, %ld.***\n",
             (long) DataSize, (long) ExpSize);
      return RC::VECTORDATA_SIZE_MISMATCH;
   }

   for (UINT32 i = 0; i < ExpSize; i++)
   {
      if ((*pData)[i] != (*pExpData)[i])
      {
         Printf(Tee::PriHigh, "*** Error: Data mis compare: Data1[%i] = 0x%02x, Exp[%i] = 0x%02x.***\n",
                i, (*pData)[i], i, (*pExpData)[i] );

         return RC::VECTORDATA_VALUE_MISMATCH;
      }
   }

   Printf(Tee::PriDebug, "---VectorData::Compare Pass---\n");
   return OK;
}

RC VectorData::Compare( vector<UINT16>* pData, vector<UINT16>* pExpData )
{
   size_t DataSize = pData->size();
   size_t ExpSize = pExpData->size();

   if (DataSize != ExpSize)
   {
      Printf(Tee::PriHigh, "*** Error: Data mis compare: size MISMATCH %ld, %ld.***\n",
             (long) DataSize, (long) ExpSize);
      return RC::VECTORDATA_SIZE_MISMATCH;
   }

   for (UINT32 i = 0; i < ExpSize; i++)
   {
      if ((*pData)[i] != (*pExpData)[i])
      {
         Printf(Tee::PriHigh, "*** Error: Data mis compare: Data1[%i] = 0x%04x, Exp[%i] = 0x%04x.***\n",
                i, (*pData)[i], i, (*pExpData)[i] );

         return RC::BUFFER_MISMATCH;
      }
   }

   Printf(Tee::PriDebug, "---VectorData::Compare Pass---\n");
   return OK;
}

RC VectorData::Compare( vector<UINT32>* pData, vector<UINT32>* pExpData )
{
   size_t DataSize = pData->size();
   size_t ExpSize = pExpData->size();

   if (DataSize != ExpSize)
   {
      Printf(Tee::PriHigh, "*** Error: Data mis compare:  size MISMATCH  %ld, %ld.***\n",
             (long) DataSize, (long) ExpSize);
      return RC::VECTORDATA_SIZE_MISMATCH;
   }

   for (UINT32 i = 0; i < ExpSize; i++)
   {
      if ((*pData)[i] != (*pExpData)[i])
      {
         Printf(Tee::PriHigh, "*** Error: Data mis compare: Data1[%i] = 0x%08x, Exp[%i] = 0x%08x.***\n",
                i, (*pData)[i], i, (*pExpData)[i] );

         return RC::VECTORDATA_VALUE_MISMATCH;
      }
   }

   Printf(Tee::PriDebug, "---VectorData::Compare Pass---\n");
   return OK;
}

RC VectorData::ComparePattern( vector<UINT08>* pData, vector<UINT08>* pPat )
{
   size_t PatSize = (*pPat).size();
   size_t DataSize = (*pData).size();

   for (UINT32 i = 0; i < DataSize; i++)
   {
      if ((*pData)[i] != (*pPat)[i%PatSize])
      {
         Printf(Tee::PriHigh, "*** Error: Data mis compare: Data[%i] = 0x%02x, Pat[%ld] = 0x%02x.***\n",
                i, (*pData)[i], (long) (i%PatSize), (*pPat)[i%PatSize] );

         return RC::VECTORDATA_VALUE_MISMATCH;
      }
   }
   Printf(Tee::PriDebug, "---VectorData::ComparePattern Pass---\n");
   return OK;
}

RC VectorData::ComparePattern( vector<UINT16>* pData, vector<UINT16>* pPat )
{
   size_t PatSize = (*pPat).size();
   size_t DataSize = (*pData).size();

   for (UINT32 i = 0; i < DataSize; i++)
   {
      if ((*pData)[i] != (*pPat)[i%PatSize])
      {
         Printf(Tee::PriHigh, "*** Error: Data mis compare: Data[%i] = 0x%04x, Pat[%ld] = 0x%04x.***\n",
                i, (*pData)[i], (long) (i%PatSize), (*pPat)[i%PatSize] );

         return RC::VECTORDATA_VALUE_MISMATCH;
      }
   }
   Printf(Tee::PriDebug, "---VectorData::ComparePattern Pass---\n");
   return OK;
}

RC VectorData::ComparePattern( vector<UINT32>* pData, vector<UINT32>* pPat )
{
   size_t PatSize = (*pPat).size();
   size_t DataSize = (*pData).size();

   for (UINT32 i = 0; i < DataSize; i++)
   {
      if ((*pData)[i] != (*pPat)[i%PatSize])
      {
         Printf(Tee::PriHigh, "*** Error: Data mis compare: Data[%i] = 0x%08x, Pat[%ld] = 0x%08x.***\n",
                i, (*pData)[i], (long) (i%PatSize), (*pPat)[i%PatSize] );

         return RC::VECTORDATA_VALUE_MISMATCH;
      }
   }
   Printf(Tee::PriDebug, "---VectorData::ComparePattern Pass---\n");
   return OK;
}

//------------------------------------------------------------------------------
// Memory object, methods, and tests.
//------------------------------------------------------------------------------

JS_CLASS(ArrayData);

static SObject ArrayData_Object
   (
   "ArrayData",
   ArrayDataClass,
   0,
   0,
   "Array Data read and write."
   );

// Methods and Tests
C_(ArrayData_Print);
static SMethod ArrayData_Print
   (
   ArrayData_Object,
   "Print",
   C_ArrayData_Print,
   1,
   "Print Js Array."
   );

C_(ArrayData_Compare);
static STest ArrayData_Compare
   (
   ArrayData_Object,
   "Compare",
   C_ArrayData_Compare,
   2,
   "Compare 2 Js Array."
   );

C_(ArrayData_Print)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 1)
   {
      JS_ReportError(pContext, "Usage: VectoreData.Print([Data])");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   JsArray  jsData;

   if (OK != pJavaScript->FromJsval(pArguments[0], &jsData))
   {
      JS_ReportError(pContext, "[Data] bad value.");
      return JS_FALSE;
   }

   // copy the data from array to vector
   vector<UINT32> vData(jsData.size());

   UINT32 data;
   for (UINT32 i = 0; i< jsData.size(); i++)
   {
      if (OK != pJavaScript->FromJsval((jsData[i]), &data))
      {
         JS_ReportError(pContext, "data element bad value.");
         return JS_FALSE;
      }
      vData[i] = data;
   }
   VectorData::Print(&vData);
   RETURN_RC(OK);
}

C_(ArrayData_Compare)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 2)
   {
      JS_ReportError(pContext, "Usage: VectoreData.Compare([Data1], [Data2])");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;
   JsArray  jsData1, jsData2;

   if (OK != pJavaScript->FromJsval(pArguments[0], &jsData1))
   {
      JS_ReportError(pContext, "[Data1] bad value.");
      return JS_FALSE;
   }
   if (OK != pJavaScript->FromJsval(pArguments[1], &jsData2))
   {
      JS_ReportError(pContext, "[Data2] bad value.");
      return JS_FALSE;
   }

   // copy the data from array to vector
   vector<UINT32> vData1(jsData1.size());
   vector<UINT32> vData2(jsData2.size());

   UINT32 data;
   UINT32 i;
   for (i = 0; i< jsData1.size(); i++)
   {
      if (OK != pJavaScript->FromJsval((jsData1[i]), &data))
      {
         JS_ReportError(pContext, "data element bad value.");
         return JS_FALSE;
      }
      vData1[i] = data;
   }

   for (i = 0; i< jsData2.size(); i++)
   {
      if (OK != pJavaScript->FromJsval((jsData2[i]), &data))
      {
         JS_ReportError(pContext, "data element bad value.");
         return JS_FALSE;
      }
      vData2[i] = data;
   }

   RETURN_RC( VectorData::Compare(&vData1, &vData2) );
}

