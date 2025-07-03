/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//------------------------------------------------------------------------------

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/fft.h"
#include <stdlib.h>
#include <vector>
#include <list>
#include "core/include/massert.h"
#include <math.h>
#include <vector>
#include "core/include/tee.h"

JS_CLASS(Fourier);

static SObject Fft_Object
(
   "Fourier",
   FourierClass,
   0,
   0,
   "Fast Fourier Transform."
);

C_(Fft_RealTransform);
static STest Fft_RealTransform
(
   Fft_Object,
   "Fft",
   C_Fft_RealTransform,
   2,
   "Fast Fourier Transform."
);

C_(Fft_RealTransformMag);
static STest Fft_RealTransformMag
(
   Fft_Object,
   "FftMag",
   C_Fft_RealTransformMag,
   2,
   "Fast Fourier Transform (magnitude of each f component only)."
);

C_(Fft_WMaxMag);
static STest Fft_WMaxMag
(
   Fft_Object,
   "WMaxMag",
   C_Fft_WMaxMag,
   2,
   "Callwlate the weighted maximum frequency."
);

C_(Fft_PrintPeaks);
static STest Fft_PrintPeaks
(
   Fft_Object,
   "PrintPeaks",
   C_Fft_PrintPeaks,
   3,
   "Print signal peaks."
);

C_(Fft_IlwerseRealTransform);
static STest Fft_IlwerseRealTransform
(
   Fft_Object,
   "IFft",
   C_Fft_IlwerseRealTransform,
   2,
   "Ilwerse Fast Fourier Transform."
);

C_(Fft_RealTransform)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // check for the correct number of arguments
   if(NumArguments != 2)
   {
      JS_ReportError(pContext,"Usage: Fourier.Fft(input array, output array)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;

   JsArray jsData;
   if (OK != pJavaScript->FromJsval(pArguments[0], &jsData))
   {
      JS_ReportError(pContext, "Failed to get input array.");
      return JS_FALSE;
   }
   JSObject *jsResult;
   if (OK != pJavaScript->FromJsval(pArguments[1], &jsResult))
   {
      JS_ReportError(pContext, "Failed to get output array.");
      return JS_FALSE;
   }

   size_t samples = jsData.size();

   // colwert to an STL vector of doubles
   vector<FLOAT64> data(samples);

   for(unsigned int i=0; i<samples; i++) {
      if (OK != pJavaScript->FromJsval(jsData[i], &data[i]))
      {
         JS_ReportError(pContext, "Non-numeric element in array.");
         return JS_FALSE;
      }
      // correct for sign bit
      if (data[i]>32768) data[i] = data[i] - 65536;
   }

   // do the FFT
   Fft calc((int)samples);
   vector<double> result(samples);
   RC rc(OK);

   C_CHECK_RC( calc.RealFourierTransform(data, result));

   // colwert the result back to javascript
   for(unsigned int j=0; j<samples; j++)
      if (OK != (rc = pJavaScript->SetElement(jsResult, j, result[j])))
         RETURN_RC( rc );

   RETURN_RC(OK);
}

C_(Fft_RealTransformMag)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // check for the correct number of arguments
   if(NumArguments != 2)
   {
      JS_ReportError(pContext,
         "Usage: Fourier.FftMag(input array, output array)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;

   JsArray jsData;
   if (OK != pJavaScript->FromJsval(pArguments[0], &jsData))
   {
      JS_ReportError(pContext, "Failed to get input array.");
      return JS_FALSE;
   }
   JSObject *jsResult;
   if (OK != pJavaScript->FromJsval(pArguments[1], &jsResult))
   {
      JS_ReportError(pContext, "Failed to get output array.");
      return JS_FALSE;
   }

   size_t samples = jsData.size();

   // colwert to an STL vector
   vector<FLOAT64> data(samples);

   for(unsigned int i=0; i<samples; i++) {
      if (OK != pJavaScript->FromJsval(jsData[i], &data[i]))
      {
         JS_ReportError(pContext, "Non-numeric element in array.");
         return JS_FALSE;
      }
      // correct for sign bit
      if (data[i] >= 32768) data[i] = data[i] - 65536;
   }

   // do the FFT
   Fft calc((int)samples);
   vector<double> result(samples);

   RC rc = OK;

   C_CHECK_RC(calc.RealFourierTransformMagnitude(data, result));

   // colwert the result back to javascript
   for(unsigned int j=0; j<samples/2; j++)
      if (OK != (rc = pJavaScript->SetElement(jsResult, j, result[j])))
         RETURN_RC( rc );

   RETURN_RC(OK);
}

C_(Fft_IlwerseRealTransform)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // check for the correct number of arguments
   if(NumArguments != 2)
   {
      JS_ReportError(pContext,
         "Usage: Fourier.IFft(input array, output array)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;

   JsArray jsData;
   if (OK != pJavaScript->FromJsval(pArguments[0], &jsData))
   {
      JS_ReportError(pContext, "Failed to get input array.");
      return JS_FALSE;
   }
   JSObject *jsResult;
   if (OK != pJavaScript->FromJsval(pArguments[1], &jsResult))
   {
      JS_ReportError(pContext, "Failed to get output array.");
      return JS_FALSE;
   }

   size_t samples = jsData.size();

   // colwert to an STL vector
   vector<FLOAT64> data(samples);

   for(unsigned int i=0; i<samples; i++)
      if (OK != pJavaScript->FromJsval(jsData[i], &data[i]))
      {
         JS_ReportError(pContext, "Non-numeric element in array.");
         return JS_FALSE;
      }

   // do the FFT
   Fft calc((int)samples);
   vector<double> result(samples);
   RC rc = OK;

   C_CHECK_RC(calc.IlwerseRealFourierTransform(data, result));

   // colwert the result back to javascript
   for(unsigned int j=0; j<samples; j++)
      if (OK != (rc = pJavaScript->SetElement(jsResult, j, result[j])))
         RETURN_RC( rc );

   return JS_TRUE;
}

C_(Fft_WMaxMag)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // check for the correct number of arguments
   if(NumArguments != 2)
   {
      JS_ReportError(pContext,
         "Usage: Fourier.WMaxMag(magnitude array, [max weighed freq])");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;

   JsArray jsData;
   if (OK != pJavaScript->FromJsval(pArguments[0], &jsData))
   {
      JS_ReportError(pContext, "Failed to get input array.");
      return JS_FALSE;
   }
   JSObject *jsResult;
   if (OK != pJavaScript->FromJsval(pArguments[1], &jsResult))
   {
      JS_ReportError(pContext, "Failed to get output array.");
      return JS_FALSE;
   }

   FLOAT64  sum = 0;
   FLOAT64  weightedMax = 0;
   FLOAT64  current;

   for(unsigned int i=1; i<jsData.size(); i++)
   {
     if (OK != pJavaScript->FromJsval(jsData[i], &current))
      {
         JS_ReportError(pContext, "Non-numeric element in array.");
         return JS_FALSE;
      }
      sum += current;
      weightedMax += current*i;
   }

   RC rc(OK);

   // colwert the result back to javascript
   if (OK != (rc = pJavaScript->SetElement(jsResult, 0,
         weightedMax/sum)))
      RETURN_RC( rc );

   return JS_TRUE;
}

C_(Fft_PrintPeaks)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // check for the correct number of arguments
   if(NumArguments != 4)
   {
      JS_ReportError(pContext,
         "Usage: Fourier.PrintPeaks(magnitude array, cutoff, seperation, sampling rate)");
      return JS_FALSE;
   }

   JavaScriptPtr pJavaScript;

   JsArray jsData;
   if (OK != pJavaScript->FromJsval(pArguments[0], &jsData))
   {
      JS_ReportError(pContext, "Failed to get input array.");
      return JS_FALSE;
   }
   UINT32   cutoff, separation;
   FLOAT64 samplingRate;
   if (OK != pJavaScript->FromJsval(pArguments[1], &cutoff))
   {
      JS_ReportError(pContext, "Failed to get lwoff.");
      return JS_FALSE;
   }
   if (OK != pJavaScript->FromJsval(pArguments[2], &separation))
   {
      JS_ReportError(pContext, "Failed to get separation.");
      return JS_FALSE;
   }
   if (OK != pJavaScript->FromJsval(pArguments[3], &samplingRate))
   {
      JS_ReportError(pContext, "Failed to get samplingRate.");
      return JS_FALSE;
   }

   vector<FLOAT64> data(jsData.size());

   for(unsigned int i=0; i<jsData.size(); i++)
      if (OK != pJavaScript->FromJsval(jsData[i], &data[i]))
      {
         JS_ReportError(pContext, "Non-numeric element in array.");
         return JS_FALSE;
      }

   FLOAT64 average = 0;

   Printf(Tee::ScreenOnly, "Peaks in data:\n");

   for(unsigned int j=0; j<data.size(); j++)
   {
      average += data[j];
   }
   average = average / data.size();

   Printf(Tee::PriNormal, "    (average: %f )\n",average);

   for (unsigned int k=1; k < data.size()-1; k++)
   {
      FLOAT64 max = 0;
      for(int j=-int(separation); j < int(separation); j++)
         if ( (k+j)<data.size() )
            if (data[k+j]>max)
               max = data[k+j];

      if (data[k] == max && max >= (average * cutoff) &&
         k*2 <= data.size())  // Nyquest frequency
      {
         // use 3 points and lagrange interpolation to find the
         // quadratically approximated maximum

         FLOAT64 height = 0.0;
         FLOAT64 maximum;
         maximum = quadraticInterpolate(data[k-1], data[k], data[k+1], height); // modifies height!

         Printf(Tee::PriNormal, "    frequency: %f amplitude: %f\n",
                samplingRate/data.size()*maximum+k, height);
      }
   }

   return JS_TRUE;
}

FLOAT64 quadraticInterpolate(FLOAT64 y1, FLOAT64 y2, FLOAT64 y3, FLOAT64 & height)
// takes three equally spaced points (at x=-1, x=0, x=1) and their heights
{
   FLOAT64 lcoef1, lcoef2, lcoef3;
   FLOAT64 maxima;

   // coeffecients of lagrage polynomial
   lcoef1 = y1/2;
   lcoef2 = y2/(-1);
   lcoef3 = y3/2;

   // root of the derivate of polynomial
   maxima = (lcoef1 - lcoef3) / (2*lcoef1 + 2*lcoef2 + 2*lcoef3);

   // height at the local maxima
   height = lcoef1*(maxima*maxima-maxima) + lcoef2*(maxima*maxima - 1) +
               lcoef3*(maxima*maxima + maxima);

   return maxima;
}

//----------------------------------------------------

Fft::Fft(int n)
{
   workArea = (int *)new char[(sizeof(double) * (2+n/2+1))];
   trigTable = (double *)new double[n/2];

   workArea[0] = 0;
}

Fft::~Fft()
{
   delete[] workArea;
   delete[] trigTable;
}

RC Fft::RealFourierTransform(vector<double> & data, vector<double> & result)
{
   size_t n = data.size();
   vector<double> a(n);

   if (( n < 2 ) || ( n & (n-1) ))
   {
      Printf(Tee::PriNormal, "The number of samples must be a power of two.\n");
      return RC::BAD_PARAMETER;
   }

   for (unsigned int i=0; i<n; i++)
   {
      a[i] = data[i];
   }

   rdft((int)n, 1, &a[0], workArea, trigTable);

   for (unsigned int j=0; j<n; j++)
   {
      result[j] = a[j];
   }

   return OK;
}

RC Fft::RealFourierTransformMagnitude(vector<double> & data, vector<double> &result)
{
   size_t n = data.size();
   vector<double> a(n);

   if (( n < 2 ) || ( n & (n-1) ))
   {
      Printf(Tee::PriNormal, "The number of samples must be a power of two.\n");
      return RC::BAD_PARAMETER;
   }

   for (unsigned int i=0; i<n; i++)
   {
      a[i] = data[i];
   }

   rdft((int)n, 1, &a[0], workArea, trigTable);

   result[0] = a[0] / n;
   for (unsigned int j=1; j<(n/2); j++)
   {
      result[j] = sqrt(a[2*j]*a[j*2] + a[j*2+1]*a[j*2+1])*2/n;
   }

   return OK;
}

RC Fft::IlwerseRealFourierTransform(vector<double> & data, vector<double> &result)
{
   size_t n = data.size();
   vector<double> a(n);

   if (( n < 2 ) || ( n & (n-1) ))
   {
      Printf(Tee::PriNormal, "The number of samples must be a power of two.\n");
      return RC::BAD_PARAMETER;
   }

   for (unsigned int i=0; i<n; i++)
   {
      a[i] = data[i];
   }

   rdft((int)n, -1, &a[0], workArea, trigTable);

   for (unsigned int j=0; j<n; j++)
   {
      result[j] = a[j];
   }

   return OK;
}

//----------------------------------------------------

void Fft::cdft(int n, int isgn, double *a, int *ip, double *w)
{
    int j;

    if (n > (ip[0] << 2)) {
        Fft::makewt(n >> 2, ip, w);
    }
    if (n > 4) {
        bitrv2(n, ip + 2, a);
    }
    if (n > 4 && isgn < 0) {
        for (j = 1; j <= n - 1; j += 2) {
            a[j] = -a[j];
        }
        cftsub(n, a, w);
        for (j = 1; j <= n - 1; j += 2) {
            a[j] = -a[j];
        }
    } else {
        cftsub(n, a, w);
    }
}

void Fft::rdft(int n, int isgn, double *a, int *ip, double *w)
{
    int j, nw, nc;
    double xi;

    nw = ip[0];
    if (n > (nw << 2)) {
        nw = n >> 2;
        Fft::makewt(nw, ip, w);
    }
    nc = ip[1];
    if (n > (nc << 2)) {
        nc = n >> 2;
        makect(nc, ip, w + nw);
    }
    if (isgn < 0) {
        a[1] = 0.5 * (a[1] - a[0]);
        a[0] += a[1];
        for (j = 3; j <= n - 1; j += 2) {
            a[j] = -a[j];
        }
        if (n > 4) {
            rftsub(n, a, nc, w + nw);
            bitrv2(n, ip + 2, a);
        }
        cftsub(n, a, w);
        for (j = 1; j <= n - 1; j += 2) {
            a[j] = -a[j];
        }
    } else {
        if (n > 4) {
            bitrv2(n, ip + 2, a);
        }
        cftsub(n, a, w);
        if (n > 4) {
            rftsub(n, a, nc, w + nw);
        }
        xi = a[0] - a[1];
        a[0] += a[1];
        a[1] = xi;
    }
}

void Fft::ddct(int n, int isgn, double *a, int *ip, double *w)
{
    int j, nw, nc;
    double xr;

    nw = ip[0];
    if (n > (nw << 2)) {
        nw = n >> 2;
        Fft::makewt(nw, ip, w);
    }
    nc = ip[1];
    if (n > nc) {
        nc = n;
        makect(nc, ip, w + nw);
    }
    if (isgn < 0) {
        xr = a[n - 1];
        for (j = n - 2; j >= 2; j -= 2) {
            a[j + 1] = a[j - 1] - a[j];
            a[j] += a[j - 1];
        }
        a[1] = xr - a[0];
        a[0] += xr;
        if (n > 4) {
            rftsub(n, a, nc, w + nw);
            bitrv2(n, ip + 2, a);
        }
        cftsub(n, a, w);
        for (j = 1; j <= n - 1; j += 2) {
            a[j] = -a[j];
        }
    }
    dctsub(n, a, nc, w + nw);
    if (isgn >= 0) {
        if (n > 4) {
            bitrv2(n, ip + 2, a);
        }
        cftsub(n, a, w);
        if (n > 4) {
            rftsub(n, a, nc, w + nw);
        }
        xr = a[0] - a[1];
        a[0] += a[1];
        for (j = 2; j <= n - 2; j += 2) {
            a[j - 1] = a[j] - a[j + 1];
            a[j] += a[j + 1];
        }
        a[n - 1] = xr;
    }
}

void Fft::ddst(int n, int isgn, double *a, int *ip, double *w)
{
    int j, nw, nc;
    double xr;

    nw = ip[0];
    if (n > (nw << 2)) {
        nw = n >> 2;
        Fft::makewt(nw, ip, w);
    }
    nc = ip[1];
    if (n > nc) {
        nc = n;
        makect(nc, ip, w + nw);
    }
    if (isgn < 0) {
        xr = a[n - 1];
        for (j = n - 2; j >= 2; j -= 2) {
            a[j + 1] = a[j - 1] + a[j];
            a[j] -= a[j - 1];
        }
        a[1] = -xr - a[0];
        a[0] -= xr;
        if (n > 4) {
            rftsub(n, a, nc, w + nw);
            bitrv2(n, ip + 2, a);
        }
        cftsub(n, a, w);
        for (j = 1; j <= n - 1; j += 2) {
            a[j] = -a[j];
        }
    }
    Fft::dstsub(n, a, nc, w + nw);
    if (isgn >= 0) {
        if (n > 4) {
            bitrv2(n, ip + 2, a);
        }
        cftsub(n, a, w);
        if (n > 4) {
            rftsub(n, a, nc, w + nw);
        }
        xr = a[0] - a[1];
        a[0] += a[1];
        for (j = 2; j <= n - 2; j += 2) {
            a[j - 1] = -a[j] - a[j + 1];
            a[j] -= a[j + 1];
        }
        a[n - 1] = -xr;
    }
}

void Fft::dfct(int n, double *a, double *t, int *ip, double *w)
{
    int j, k, l, m, mh, nw, nc;
    double xr, xi;

    nw = ip[0];
    if (n > (nw << 3)) {
        nw = n >> 3;
        Fft::makewt(nw, ip, w);
    }
    nc = ip[1];
    if (n > (nc << 1)) {
        nc = n >> 1;
        makect(nc, ip, w + nw);
    }
    m = n >> 1;
    xr = a[0] + a[n];
    a[0] -= a[n];
    t[0] = xr - a[m];
    t[m] = xr + a[m];
    if (n > 2) {
        mh = m >> 1;
        for (j = 1; j <= mh - 1; j++) {
            k = m - j;
            xr = a[j] + a[n - j];
            a[j] -= a[n - j];
            xi = a[k] + a[n - k];
            a[k] -= a[n - k];
            t[j] = xr - xi;
            t[k] = xr + xi;
        }
        t[mh] = a[mh] + a[n - mh];
        a[mh] -= a[n - mh];
        dctsub(m, a, nc, w + nw);
        if (m > 4) {
            bitrv2(m, ip + 2, a);
        }
        cftsub(m, a, w);
        if (m > 4) {
            rftsub(m, a, nc, w + nw);
        }
        xr = a[0] + a[1];
        a[n - 1] = a[0] - a[1];
        for (j = m - 2; j >= 2; j -= 2) {
            a[(j << 1) + 1] = a[j] + a[j + 1];
            a[(j << 1) - 1] = a[j] - a[j + 1];
        }
        a[1] = xr;
        l = 2;
        m = mh;
        while (m >= 2) {
            dctsub(m, t, nc, w + nw);
            if (m > 4) {
                bitrv2(m, ip + 2, t);
            }
            cftsub(m, t, w);
            if (m > 4) {
                rftsub(m, t, nc, w + nw);
            }
            a[n - l] = t[0] - t[1];
            a[l] = t[0] + t[1];
            k = 0;
            for (j = 2; j <= m - 2; j += 2) {
                k += l << 2;
                a[k - l] = t[j] - t[j + 1];
                a[k + l] = t[j] + t[j + 1];
            }
            l <<= 1;
            mh = m >> 1;
            for (j = 0; j <= mh - 1; j++) {
                k = m - j;
                t[j] = t[m + k] - t[m + j];
                t[k] = t[m + k] + t[m + j];
            }
            t[mh] = t[m + mh];
            m = mh;
        }
        a[l] = t[0];
        a[n] = t[2] - t[1];
        a[0] = t[2] + t[1];
    } else {
        a[1] = a[0];
        a[2] = t[0];
        a[0] = t[1];
    }
}

void Fft::dfst(int n, double *a, double *t, int *ip, double *w)
{
    int j, k, l, m, mh, nw, nc;
    double xr, xi;

    nw = ip[0];
    if (n > (nw << 3)) {
        nw = n >> 3;
        makewt(nw, ip, w);
    }
    nc = ip[1];
    if (n > (nc << 1)) {
        nc = n >> 1;
        makect(nc, ip, w + nw);
    }
    if (n > 2) {
        m = n >> 1;
        mh = m >> 1;
        for (j = 1; j <= mh - 1; j++) {
            k = m - j;
            xr = a[j] - a[n - j];
            a[j] += a[n - j];
            xi = a[k] - a[n - k];
            a[k] += a[n - k];
            t[j] = xr + xi;
            t[k] = xr - xi;
        }
        t[0] = a[mh] - a[n - mh];
        a[mh] += a[n - mh];
        a[0] = a[m];
        Fft::dstsub(m, a, nc, w + nw);
        if (m > 4) {
            bitrv2(m, ip + 2, a);
        }
        cftsub(m, a, w);
        if (m > 4) {
            rftsub(m, a, nc, w + nw);
        }
        xr = a[0] + a[1];
        a[n - 1] = a[1] - a[0];
        for (j = m - 2; j >= 2; j -= 2) {
            a[(j << 1) + 1] = a[j] - a[j + 1];
            a[(j << 1) - 1] = -a[j] - a[j + 1];
        }
        a[1] = xr;
        l = 2;
        m = mh;
        while (m >= 2) {
            Fft::dstsub(m, t, nc, w + nw);
            if (m > 4) {
                bitrv2(m, ip + 2, t);
            }
            cftsub(m, t, w);
            if (m > 4) {
                rftsub(m, t, nc, w + nw);
            }
            a[n - l] = t[1] - t[0];
            a[l] = t[0] + t[1];
            k = 0;
            for (j = 2; j <= m - 2; j += 2) {
                k += l << 2;
                a[k - l] = -t[j] - t[j + 1];
                a[k + l] = t[j] - t[j + 1];
            }
            l <<= 1;
            mh = m >> 1;
            for (j = 1; j <= mh - 1; j++) {
                k = m - j;
                t[j] = t[m + k] + t[m + j];
                t[k] = t[m + k] - t[m + j];
            }
            t[0] = t[m + mh];
            m = mh;
        }
        a[l] = t[0];
    }
    a[0] = 0;
}

/* -------- initializing routines -------- */

#include <math.h>

void Fft::makewt(int nw, int *ip, double *w)
{
    int nwh, j;
    double delta, x, y;

    ip[0] = nw;
    ip[1] = 1;
    if (nw > 2) {
        nwh = nw >> 1;
        delta = atan(1.0) / nwh;
        w[0] = 1;
        w[1] = 0;
        w[nwh] = cos(delta * nwh);
        w[nwh + 1] = w[nwh];
        for (j = 2; j <= nwh - 2; j += 2) {
            x = cos(delta * j);
            y = sin(delta * j);
            w[j] = x;
            w[j + 1] = y;
            w[nw - j] = y;
            w[nw - j + 1] = x;
        }
        bitrv2(nw, ip + 2, w);
    }
}

void Fft::makect(int nc, int *ip, double *c)
{
    int nch, j;
    double delta;

    ip[1] = nc;
    if (nc > 1) {
        nch = nc >> 1;
        delta = atan(1.0) / nch;
        c[0] = 0.5;
        c[nch] = 0.5 * cos(delta * nch);
        for (j = 1; j <= nch - 1; j++) {
            c[j] = 0.5 * cos(delta * j);
            c[nc - j] = 0.5 * sin(delta * j);
        }
    }
}

/* -------- child routines -------- */

void Fft::bitrv2(int n, int *ip, double *a)
{
    int j, j1, k, k1, l, m, m2;
    double xr, xi;

    ip[0] = 0;
    l = n;
    m = 1;
    while ((m << 2) < l) {
        l >>= 1;
        for (j = 0; j <= m - 1; j++) {
            ip[m + j] = ip[j] + l;
        }
        m <<= 1;
    }
    if ((m << 2) > l) {
        for (k = 1; k <= m - 1; k++) {
            for (j = 0; j <= k - 1; j++) {
                j1 = (j << 1) + ip[k];
                k1 = (k << 1) + ip[j];
                xr = a[j1];
                xi = a[j1 + 1];
                a[j1] = a[k1];
                a[j1 + 1] = a[k1 + 1];
                a[k1] = xr;
                a[k1 + 1] = xi;
            }
        }
    } else {
        m2 = m << 1;
        for (k = 1; k <= m - 1; k++) {
            for (j = 0; j <= k - 1; j++) {
                j1 = (j << 1) + ip[k];
                k1 = (k << 1) + ip[j];
                xr = a[j1];
                xi = a[j1 + 1];
                a[j1] = a[k1];
                a[j1 + 1] = a[k1 + 1];
                a[k1] = xr;
                a[k1 + 1] = xi;
                j1 += m2;
                k1 += m2;
                xr = a[j1];
                xi = a[j1 + 1];
                a[j1] = a[k1];
                a[j1 + 1] = a[k1 + 1];
                a[k1] = xr;
                a[k1 + 1] = xi;
            }
        }
    }
}

void Fft::cftsub(int n, double *a, double *w)
{
    int j, j1, j2, j3, k, k1, ks, l, m;
    double wk1r, wk1i, wk2r, wk2i, wk3r, wk3i;
    double x0r, x0i, x1r, x1i, x2r, x2i, x3r, x3i;

    l = 2;
    while ((l << 1) < n) {
        m = l << 2;
        for (j = 0; j <= l - 2; j += 2) {
            j1 = j + l;
            j2 = j1 + l;
            j3 = j2 + l;
            x0r = a[j] + a[j1];
            x0i = a[j + 1] + a[j1 + 1];
            x1r = a[j] - a[j1];
            x1i = a[j + 1] - a[j1 + 1];
            x2r = a[j2] + a[j3];
            x2i = a[j2 + 1] + a[j3 + 1];
            x3r = a[j2] - a[j3];
            x3i = a[j2 + 1] - a[j3 + 1];
            a[j] = x0r + x2r;
            a[j + 1] = x0i + x2i;
            a[j2] = x0r - x2r;
            a[j2 + 1] = x0i - x2i;
            a[j1] = x1r - x3i;
            a[j1 + 1] = x1i + x3r;
            a[j3] = x1r + x3i;
            a[j3 + 1] = x1i - x3r;
        }
        if (m < n) {
            wk1r = w[2];
            for (j = m; j <= l + m - 2; j += 2) {
                j1 = j + l;
                j2 = j1 + l;
                j3 = j2 + l;
                x0r = a[j] + a[j1];
                x0i = a[j + 1] + a[j1 + 1];
                x1r = a[j] - a[j1];
                x1i = a[j + 1] - a[j1 + 1];
                x2r = a[j2] + a[j3];
                x2i = a[j2 + 1] + a[j3 + 1];
                x3r = a[j2] - a[j3];
                x3i = a[j2 + 1] - a[j3 + 1];
                a[j] = x0r + x2r;
                a[j + 1] = x0i + x2i;
                a[j2] = x2i - x0i;
                a[j2 + 1] = x0r - x2r;
                x0r = x1r - x3i;
                x0i = x1i + x3r;
                a[j1] = wk1r * (x0r - x0i);
                a[j1 + 1] = wk1r * (x0r + x0i);
                x0r = x3i + x1r;
                x0i = x3r - x1i;
                a[j3] = wk1r * (x0i - x0r);
                a[j3 + 1] = wk1r * (x0i + x0r);
            }
            k1 = 1;
            ks = -1;
            for (k = (m << 1); k <= n - m; k += m) {
                k1++;
                ks = -ks;
                wk1r = w[k1 << 1];
                wk1i = w[(k1 << 1) + 1];
                wk2r = ks * w[k1];
                wk2i = w[k1 + ks];
                wk3r = wk1r - 2 * wk2i * wk1i;
                wk3i = 2 * wk2i * wk1r - wk1i;
                for (j = k; j <= l + k - 2; j += 2) {
                    j1 = j + l;
                    j2 = j1 + l;
                    j3 = j2 + l;
                    x0r = a[j] + a[j1];
                    x0i = a[j + 1] + a[j1 + 1];
                    x1r = a[j] - a[j1];
                    x1i = a[j + 1] - a[j1 + 1];
                    x2r = a[j2] + a[j3];
                    x2i = a[j2 + 1] + a[j3 + 1];
                    x3r = a[j2] - a[j3];
                    x3i = a[j2 + 1] - a[j3 + 1];
                    a[j] = x0r + x2r;
                    a[j + 1] = x0i + x2i;
                    x0r -= x2r;
                    x0i -= x2i;
                    a[j2] = wk2r * x0r - wk2i * x0i;
                    a[j2 + 1] = wk2r * x0i + wk2i * x0r;
                    x0r = x1r - x3i;
                    x0i = x1i + x3r;
                    a[j1] = wk1r * x0r - wk1i * x0i;
                    a[j1 + 1] = wk1r * x0i + wk1i * x0r;
                    x0r = x1r + x3i;
                    x0i = x1i - x3r;
                    a[j3] = wk3r * x0r - wk3i * x0i;
                    a[j3 + 1] = wk3r * x0i + wk3i * x0r;
                }
            }
        }
        l = m;
    }
    if (l < n) {
        for (j = 0; j <= l - 2; j += 2) {
            j1 = j + l;
            x0r = a[j] - a[j1];
            x0i = a[j + 1] - a[j1 + 1];
            a[j] += a[j1];
            a[j + 1] += a[j1 + 1];
            a[j1] = x0r;
            a[j1 + 1] = x0i;
        }
    }
}

void Fft::rftsub(int n, double *a, int nc, double *c)
{
    int j, k, kk, ks;
    double wkr, wki, xr, xi, yr, yi;

    ks = (nc << 2) / n;
    kk = 0;
    for (k = (n >> 1) - 2; k >= 2; k -= 2) {
        j = n - k;
        kk += ks;
        wkr = 0.5 - c[kk];
        wki = c[nc - kk];
        xr = a[k] - a[j];
        xi = a[k + 1] + a[j + 1];
        yr = wkr * xr - wki * xi;
        yi = wkr * xi + wki * xr;
        a[k] -= yr;
        a[k + 1] -= yi;
        a[j] += yr;
        a[j + 1] -= yi;
    }
}

void Fft::dctsub(int n, double *a, int nc, double *c)
{
    int j, k, kk, ks, m;
    double wkr, wki, xr;

    ks = nc / n;
    kk = ks;
    m = n >> 1;
    for (k = 1; k <= m - 1; k++) {
        j = n - k;
        wkr = c[kk] - c[nc - kk];
        wki = c[kk] + c[nc - kk];
        kk += ks;
        xr = wki * a[k] - wkr * a[j];
        a[k] = wkr * a[k] + wki * a[j];
        a[j] = xr;
    }
    a[m] *= 2 * c[kk];
}

void Fft::dstsub(int n, double *a, int nc, double *c)
{
    int j, k, kk, ks, m;
    double wkr, wki, xr;

    ks = nc / n;
    kk = ks;
    m = n >> 1;
    for (k = 1; k <= m - 1; k++) {
        j = n - k;
        wkr = c[kk] - c[nc - kk];
        wki = c[kk] + c[nc - kk];
        kk += ks;
        xr = wki * a[j] - wkr * a[k];
        a[j] = wkr * a[j] + wki * a[k];
        a[k] = xr;
    }
    a[m] *= 2 * c[kk];
}

