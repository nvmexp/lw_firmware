/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_TEESTRM_H
#define INCLUDED_TEESTRM_H

class Tee;

/**
 * @class(TeeStream)
 *
 * Output string stream which uses Tee.
 */

class TeeStream
{
   public:

      TeeStream(Tee * pTee);
      ~TeeStream();

      Tee * GetTee() const;

      int Write(const char *in);
   private:

      Tee * m_pTee;

      // Do not allow copying.
      TeeStream(const TeeStream &);
      TeeStream & operator=(const TeeStream &);
};

// Nifty counter to initialize the TeeStream before it is used.
class TeeStreamInit
{
   public:
      TeeStreamInit();
      ~TeeStreamInit();
};

static TeeStreamInit s_TeeStreamInit;

#endif // ! INCLUDED_TEESTRM_H
