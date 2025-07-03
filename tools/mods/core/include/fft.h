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

// Fast Fourier Transform (FFT).

#ifndef INCLUDED_FFT_H
#define INCLUDED_FFT_H

#ifndef INCLUDED_STL_VECTOR
#include <vector>
#define INCLUDED_STL_VECTOR
#endif

using namespace std;

class Fft
{
   public:

      Fft(int n);
      ~Fft();

      // transform routines
      RC RealFourierTransform(vector<double> & data, vector<double> & result);
      RC RealFourierTransformMagnitude(vector<double> & data, vector<double> & result);
      RC IlwerseRealFourierTransform(vector<double> & data, vector<double> & result);

   private:

      // transform routines
      void cdft(int, int, double *, int *, double *);
      void rdft(int, int, double *, int *, double *);
      void ddct(int, int, double *, int *, double *);
      void ddst(int, int, double *, int *, double *);
      void dfct(int, double *, double *, int *, double *);
      void dfst(int, double *, double *, int *, double *);

      // child routines
      void dstsub(int n, double *a, int nc, double *c);
      void dctsub(int n, double *a, int nc, double *c);
      void rftsub(int n, double *a, int nc, double *c);
      void cftsub(int n, double *a, double *w);
      void bitrv2(int n, int *ip, double *a);
      void makect(int nc, int *ip, double *c);
      void makewt(int nw, int *ip, double *w);

      double * trigTable;
      int * workArea;
};

FLOAT64 quadraticInterpolate(FLOAT64 y1, FLOAT64 y2, FLOAT64 y3,
                             FLOAT64 & height);

#endif // !INCLUDE_FFT_H
