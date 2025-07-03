/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/
#define DRM_BUILDING_ECPPQ_C
#include "bignum.h"
#include "elwrve.h"
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

#if defined(SEC_COMPILE)
/*
**     Let P and Q be points on an elliptic lwrve
**
**            y^2 + xy = x^3 + ax^2 + b        (characteristic 2)
**  or
**            y^2 = x^3 + ax + b               (field GF(p), p odd)
**
**  using affine coordinates.  We want R = 2*P + Q (or 2P - Q)
**
**      We optimize for the frequent case where P and Q are distinct and
**  neither of these nor any intermediate result is the point at infinity,
**  while designing the code to work properly in all cases.
**
**      Computing P + (P + Q) rather than (2*P) + Q saves a squaring when using
**  GF(p) arithmetic, where a doubling is more expensive than an addition.
**  In summer, 2001, Microsoft Research intern Kirsten Eisentrager observed that
**  we can save another multiplication (both GF(p) and characteristic 2 cases)
**  by not explicitly computing y(P + Q).  See the `Think' lines below.
**
**
**
**  Algorithm for 2*P + Q
**
**      if (P = infinity) then
**          return Q;
**      else if (Q = infinity)
**          Use colwentional algorithm for 2*P;
**      else
**          xP = x(P);  yP = y(P);
**          xQ = x(Q);  yQ = y(Q);
**
**          char2 = (TRUE if characteristic 2, else FALSE);
**          yQplus = yQ;
**          yQminus = (char2 ? xQ + yQ : -yQ);
**
**          if (pm1 == -1) then                                         2*P - Q wanted
**              exchange yQplus, yQminus
**          }
**
**          if (xP <> xQ) then
**              m_1 = (yP - yQplus) / (xP - xQ);                        slope of line through P, Q
**          else if (yP == yQminus) then
**              return P;                                               P and Q are negatives, so 2P + Q = P
**          else if (char2) then
**              m_1 = xP + yP/xP;                                       P = Q, tangent slope
**          else
**              m_1 = (3*xP^2 + a) / (2*yP);                            P = Q.  tangent slope
**          end if;
**
**          if (char2) then
**              xPQ = m_1^2 + m_1 - a - xP - xQ;                        Think yPQ = xPQ + yP + m_1 * (xPQ - xP);
**
**              if (xPQ = xP) then
**                  return infinity;                                    P + Q and P cannot be same point, must be negatives
**              end if;
**                                                                      Think m_2 = (yPQ - yP) / (xPQ - xP);
**              m_2 + 1 = m_1 + xP / (xPQ - xP);
**              xPPQ = m_2^2 + m_2 - a - xPQ - xP;
**              yPPQ = xP + yP + (m_2 + 1) * (xPPQ - xP);
**          else
**              xPQ = m_1^2 - xP - xQ;
**                                                                      Think yPQ = - [yP + m_1 * (xPQ - xP)];
**              if (xPQ = xP) then
**                  return infinity;                                    P + Q and P must be negatives
**              end if;
**                                                                      Think m_2 = (yPQ - yP) / (xPQ - xP);
**              -m_2 = (2*yP) / (xPQ - xP) + m_1;
**              xPPQ = m_2^2 - xPQ - xP;
**              yPPQ = -[yP + m_2*(xPPQ - xP)];
**          end if;
**          return (xPPQ, yPPQ);
**      end if;
*/

/*
**  Compute PPQ = 2*P + Q if pm1 = +1
**  PPQ = 2*P - Q if pm1 = -1
*/
DRM_NO_INLINE DRM_API DRM_BOOL DRM_CALL ecaffine_PPQ(
    __in_ecount( 2 * E->fdesc->elng )                                                   const digit_t           *P,
    __in_ecount( 2 * E->fdesc->elng )                                                   const digit_t           *Q,
    __out_ecount( 2 * E->fdesc->elng )                                                        digit_t           *PPQ,
    __in                                                                                const DRM_LONG           pm1,     /* +- 1 */
    __in_ecount( 1 )                                                                    const elwrve_t          *E,
    __inout_ecount( DRM_MAX( 3 * E->fdesc->elng + E->ndigtemps,
                             4 * E->fdesc->elng + DRM_MAX( E->fdesc->ndigtemps_ilwert1,
                                                           E->fdesc->ndigtemps_mul ) ) )      digit_t           *supplied_temps,
    __inout                                                                                   struct bigctx_t   *f_pBigCtx )
{
    DRM_BOOL OK = TRUE;
    DRM_BOOL done = FALSE;
    const field_desc_t *fdesc = E->fdesc;
    const DRM_DWORD elng = fdesc->elng;
    const digit_t *xP = P;
    const digit_t *yP = P + elng;
    const digit_t *xQ = Q;
    const digit_t *yQ = Q + elng;
    digit_t *xPPQ = PPQ;
    digit_t *yPPQ = PPQ + elng;

    digit_t *m1 = NULL;
    digit_t *m2 = NULL;
    digit_t *xtemp = NULL;
    digit_t *ytemp = NULL;
    digit_t *ftemps = NULL;

    if( CHARACTERISTIC_2( fdesc ) )
    {
        return FALSE;
    }

#if MAX_ECTEMPS < 4
#error -- Increase MAX_ECTEMPS
#endif

    if( supplied_temps == NULL )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_NULL_POINTER, "ecaffine_PPQ" );
    }
    else
    {
        m1 = supplied_temps;
        m2 = m1 + elng;
        xtemp = m2 + elng;
        ytemp = xtemp + elng;
        ftemps = ytemp + elng;
    }

    OK = OK && ( fdesc->modulo->length <= elng );     /* Buffer overflow check */

    /* Check whether either supplied point is infinite. */

    if( !OK )
    {
    }
    else if( ecaffine_is_infinite( P, E, f_pBigCtx ) )
    {
        /* P = identity so 2*P +- Q = +-Q */
        OK = OK && ecaffine_multiply_pm1( Q, PPQ, pm1, E, f_pBigCtx );
        done = TRUE;
    }
    else if( ecaffine_is_infinite( Q, E, f_pBigCtx ) )
    {
        /* Q = identity so 2*P +- Q = 2*P = P + P */
        OK = OK && ecaffine_addition( P, P, PPQ, +1, E, supplied_temps, f_pBigCtx );
        done = TRUE;
    }
    else
    {
        const digit_t *yQplus, *yQminus;
        OK = OK && Ksub( xP, xQ, xtemp, fdesc, f_pBigCtx );   /* xP - xQ */
        {
            OK = OK && Knegate( yQ, m2, fdesc, f_pBigCtx );   /* m2 = y(-Q) */
        }

        if( pm1 == +1 )
        {
            /* 2*P + Q wanted */
            yQplus = yQ;           /* y(pm1*Q) */
            yQminus = m2;          /* y(-pm1*Q) */
        }
        else if( pm1 == -1 )
        {
            /* 2*P - Q wanted */
            yQplus = m2;           /* y(pm1*Q) */
            yQminus = yQ;          /* y(-pm1*Q) */
        }
        else
        {
            OK = FALSE;
            SetMpErrno_clue( MP_ERRNO_ILWALID_DATA, "ecaffine_PPQ pm1" );
            yQplus = yQminus = NULL;
        }

        OK = OK && Ksub( xP, xQ, xtemp, fdesc, f_pBigCtx );       /* xtemp = xP - xQ */

        if( !Kiszero( xtemp, fdesc, f_pBigCtx ) )
        {
            OK = OK && Kilwert( xtemp, m1, fdesc, ftemps, f_pBigCtx );
            OK = OK && Ksub( yP, yQplus, ytemp, fdesc, f_pBigCtx );       /* ytemp = y(P) - y(pm1*Q) */
            OK = OK && Kmul( m1, ytemp, m1, fdesc, ftemps, f_pBigCtx );   /* m1 = ytemp/xtemp */
        }
        else if( Kequal( yP, yQminus, fdesc, f_pBigCtx ) )
        {
            /*
            ** Same x, y(P) = y(-pm1*Q). so
            ** P and pm1*Q must be negatives
            ** Therefore 2P + pm1*Q = P.
            */
            OEM_SELWRE_DIGITTCPY( PPQ, P, 2 * E->fdesc->elng );
            done = TRUE;
        }
        else if( !Kequal( yP, yQplus, fdesc, f_pBigCtx ) )
        {
            OK = FALSE;
            SetMpErrno_clue( MP_ERRNO_INTERNAL_ERROR, "ecaffine_PPQ expected P = pm1*Q" );
        }
        else
        {
            OK = OK && Kmul( xP, xP, m1, fdesc, ftemps, f_pBigCtx );
            OK = OK && Kadd( E->a, m1, xtemp, fdesc, f_pBigCtx );
            OK = OK && Kadd( xtemp, m1, xtemp, fdesc, f_pBigCtx );
            OK = OK && Kadd( xtemp, m1, xtemp, fdesc, f_pBigCtx );    /* xtemp = a + 3*xP^2 */

            OK = OK && Kadd( yP, yP, ytemp, fdesc, f_pBigCtx );
            OK = OK && Kilwert( ytemp, m1, fdesc, ftemps, f_pBigCtx );  /* 1/(2*yP) */
            OK = OK && Kmul( xtemp, m1, m1, fdesc, ftemps, f_pBigCtx );
        }
    }
    if( !done )
    {
        OK = OK && Kmul( m1, m1, xtemp, fdesc, ftemps, f_pBigCtx );
        {
            /* Not characteristic 2 */
            OK = OK && Ksub( xtemp, xP, xtemp, fdesc, f_pBigCtx );
            OK = OK && Ksub( xtemp, xQ, xtemp, fdesc, f_pBigCtx );
            /* xPQ = m1^2 - xP - xQ */
            if( Kequal( xtemp, xP, fdesc, f_pBigCtx ) )
            {
                /* P + Q and P must be negatives */
                OK = OK && ecaffine_set_infinite( PPQ, E, f_pBigCtx );
                done = TRUE;
            }
            else
            {
                digit_t *m2neg = m2;   /* -m2 */
                OK = OK && Ksub( xtemp, xP, ytemp, fdesc, f_pBigCtx );                /* xPQ - xP */
                OK = OK && Kilwert( ytemp, m2neg, fdesc, ftemps, f_pBigCtx );         /* 1/(xPQ - xP) */
                OK = OK && Kmul( yP, m2neg, m2neg, fdesc, ftemps, f_pBigCtx );        /* Think m_2 = (yPQ - yP) / (xPQ - xP) */
                OK = OK && Kadd( m2neg, m2neg, m2neg, fdesc, f_pBigCtx );
                OK = OK && Kadd( m2neg, m1, m2neg, fdesc, f_pBigCtx );                /* -m2 = 2*yP / (xPQ - xP) + m1 */
                OK = OK && Kmul( m2neg, m2neg, m1, fdesc, ftemps, f_pBigCtx );        /* m2^2 */
                OK = OK && Ksub( m1, xtemp, m1, fdesc, f_pBigCtx );
                OK = OK && Ksub( m1, xP, xtemp, fdesc, f_pBigCtx );                   /*  xtemp = m2^2 - xPQ - xP (new xPPQ) */
                OK = OK && Ksub( xtemp, xP, ytemp, fdesc, f_pBigCtx );                /* xPPQ - xP */
                OK = OK && Kmul( ytemp, m2neg, ytemp, fdesc, ftemps, f_pBigCtx );     /* -m2 * (xPPQ - xP) */
                OK = OK && Ksub( ytemp, yP, yPPQ, fdesc, f_pBigCtx );
                if( fdesc->elng > 0 && xtemp != NULL && xPPQ != NULL )
                {
                    OEM_SELWRE_DIGITTCPY( xPPQ, xtemp, fdesc->elng );
                }
                done = TRUE;
            }
        }
    }

    if( OK && !done )
    {
        OK = FALSE;
        SetMpErrno_clue( MP_ERRNO_INTERNAL_ERROR, "ecaffine_PPQ -- OK = T, done = F" );
    }

    return OK;
}
#endif
EXIT_PK_NAMESPACE_CODE;

