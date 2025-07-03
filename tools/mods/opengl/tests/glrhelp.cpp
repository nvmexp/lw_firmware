/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "glrandom.h"

using namespace GLRandom;

//------------------------------------------------------------------------------
GLRandomHelper::GLRandomHelper(GLRandomTest * pglr,int numPickers, const char * name)
: m_Pickers(numPickers),
  m_Name(name),
  m_pGLRandom(pglr)
{
   m_pFpCtx = NULL;
}

//------------------------------------------------------------------------------
GLRandomHelper::~GLRandomHelper()
{
}

//------------------------------------------------------------------------------
const char * GLRandomHelper::GetName() const
{
   return m_Name;
}

//------------------------------------------------------------------------------
RC GLRandomHelper::PickerFromJsval(int index, jsval value)
{
   return m_Pickers.PickerFromJsval(index, value);
}

//------------------------------------------------------------------------------
RC GLRandomHelper::PickerToJsval(int index, jsval * pvalue)
{
   return m_Pickers.PickerToJsval(index, pvalue);
}

//------------------------------------------------------------------------------
RC GLRandomHelper::PickToJsval(int index, jsval * pvalue)
{
   return m_Pickers.PickToJsval(index, pvalue);
}

//------------------------------------------------------------------------------
void GLRandomHelper::CheckInitialized()
{
   m_Pickers.CheckInitialized();
}

//------------------------------------------------------------------------------
void GLRandomHelper::SetContext(FancyPicker::FpContext * pFpCtx)
{
   m_pFpCtx = pFpCtx;
   m_Pickers.SetContext(pFpCtx);
}

//------------------------------------------------------------------------------
RC GLRandomHelper::InitOpenGL()
{
   return OK;
}

//------------------------------------------------------------------------------
RC GLRandomHelper::Restart()
{
   m_Pickers.Restart();
   return OK;
}

RC GLRandomHelper::Playback()
{
    return OK;
}

RC GLRandomHelper::UnPlayback()
{
    return OK;
}

RC GLRandomHelper::CleanupGLState()
{
    return OK;
}

