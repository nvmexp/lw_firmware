/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#define DRM_BUILDING_OEMSHA256_C
#include <drmcompiler.h>

#if defined(DRM_MSC_VER) && DRM_64BIT_TARGET
/*
** Include intrinsics for _rotr64 and _byteswap_uint64.
** Must be included after drmcompiler.h (for DRM_MSC_VER and DRM_64BIT_TARGET)
** but before any other PK headers that might include drmsal.h.
*/
#include <intrin.h>
#endif

#include <oemsha256.h>
#include <oemcommon.h>
#include <drmerr.h>
#include <drmprofile.h>
#include <drmmathsafe.h>
#include <drmlastinclude.h>

ENTER_PK_NAMESPACE_CODE;

/*
** Globals
*/
#if defined(SEC_COMPILE)
/*
** Magic numbers use in SHA256
*/
static const DRM_DWORD g_rgdwSHA256Magic[ 64 ] PR_ATTR_DATA_OVLY(_g_rgdwSHA256Magic) =
{
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

/*
** Defines
*/

/*
** Define the shift and rotate operators
*/
#define SHA256_SHIFT_RIGHT( b, x )    ( ( x ) >> ( b ) )
#define SHA256_SHIFT_LEFT( b, x )     ( ( x ) << ( b ) )
#define SHA256_ROTATE_LEFT( x, n )   ( ( ( x ) << ( n ) ) | ( ( x ) >> ( 32 - ( n ) ) ) )
#define SHA256_ROTATE_RIGHT( x, n )  ( ( ( x ) >> ( n ) ) | ( ( x ) << ( 32 - ( n ) ) ) )

/*
** logical functions used in SHA-256
*/
#define __F( x, y, z ) ( ( ( x) & ( y ) ) ^ ( (~( x ) ) & ( z ) ) )
#define __G( x, y, z ) ( ( ( x) & ( y ) ) ^ ( ( x) & ( z ) ) ^ ( ( y ) & ( z ) ) )

#define __H( x ) (  SHA256_ROTATE_RIGHT( ( x ),  2 )    \
                  ^ SHA256_ROTATE_RIGHT( ( x ), 13 )    \
                  ^ SHA256_ROTATE_RIGHT( ( x ), 22 ) )
#define __I( x ) (  SHA256_ROTATE_RIGHT( ( x ),  6 )    \
                  ^ SHA256_ROTATE_RIGHT( ( x ), 11 )    \
                  ^ SHA256_ROTATE_RIGHT( ( x ), 25 ) )
#define __J( x ) (  SHA256_ROTATE_RIGHT( ( x ),  7 )    \
                  ^ SHA256_ROTATE_RIGHT( ( x ), 18 )    \
                  ^ SHA256_SHIFT_RIGHT( 3, ( x ) ) )
#define __K( x ) (  SHA256_ROTATE_RIGHT( ( x ), 17 )    \
                  ^ SHA256_ROTATE_RIGHT( ( x ), 19 )    \
                  ^ SHA256_SHIFT_RIGHT( 10, ( x ) ) )


#define SHA256_ROUNDINIT( a, b, c, d, e, f, g, h )              \
    dwTemp =   ( h ) + __I( e ) + __F( ( e ), ( f ), ( g ) )    \
           + g_rgdwSHA256Magic[ dwRound ] + pdwW256[ dwRound ]; \
    ( d ) += dwTemp;                                            \
    ( h ) = dwTemp + __H( a ) + __G( ( a ), ( b ), ( c ) );     \
    dwRound++

#define SHA256_ROUND( a, b, c, d, e, f, g, h )                                                  \
    dwS0 = pdwW256[ ( dwRound + 1 ) & 0x0f ];                                                   \
    dwS0 = __J( dwS0 );                                                                         \
    dwS1 = pdwW256[ ( dwRound + 14) & 0x0f ];                                                   \
    dwS1 = __K( dwS1 );                                                                         \
    dwTemp =   ( h ) + __I( e ) + __F( ( e ), ( f ), ( g ) )                                    \
           +  g_rgdwSHA256Magic[ dwRound ]                                                      \
           + ( pdwW256[ dwRound & 0x0f ] += dwS1 + pdwW256[ ( dwRound + 9 ) & 0x0f ] + dwS0 );  \
    ( d ) += dwTemp;                                                                            \
    ( h ) = dwTemp + __H( a ) + __G( ( a ), ( b ), ( c ) );                                     \
    dwRound++
#endif

#if defined(SEC_COMPILE)
#define SHA256_REVERSE32( w, x )                                               \
{                                                                              \
    DRM_DWORD tmp = ( w );                                                     \
    tmp = ( tmp >> 16 ) | ( tmp << 16 );                                       \
    ( x ) = ( ( tmp & 0xff00ff00UL ) >> 8 ) | ( ( tmp & 0x00ff00ffUL ) << 8 ); \
}
#endif

#ifdef NONE
#if !defined(DRM_MSC_VER) && DRM_64BIT_TARGET

/* Define _rotr64 and _byteswap_uint64 since no intrinsics are available */

DRM_API DRM_UINT64 DRM_CALL _byteswap_uint64(DRM_UINT64 i)
{
    DRM_UINT64 j;
    j =  (i << 56);
    j += (i << 40)&0x00FF000000000000LL;
    j += (i << 24)&0x0000FF0000000000LL;
    j += (i <<  8)&0x000000FF00000000LL;
    j += (i >>  8)&0x00000000FF000000LL;
    j += (i >> 24)&0x0000000000FF0000LL;
    j += (i >> 40)&0x000000000000FF00LL;
    j += (i >> 56);
    return j;

}

DRM_ALWAYS_INLINE DRM_API DRM_UINT64 DRM_CALL _rotr64( DRM_UINT64 inp, DRM_DWORD shift )
{
    return ( (inp >> (shift&63)) | (inp << ((64-shift) & 63) ) );
}

#endif /* !defined(DRM_MSC_VER) && DRM_64BIT_TARGET */

#define SHA256_REVERSE32_64( w, x )  ( x ) = ( DRM_UINT64 )_rotr64( _byteswap_uint64( w ), 32 )
#endif

#if defined(SEC_COMPILE)
#if TARGET_LITTLE_ENDIAN

#define SHA256_DWORDTOBIGENDIAN( __rgbBytes, __rgdwDwords, __cdwDwords )                    \
{                                                                                           \
    DRM_DWORD  __i;                                                                         \
    DRM_DWORD  __temp;                                                                      \
    for ( __i = 0 ; __i < __cdwDwords; ++__i )                                              \
    {                                                                                       \
        SHA256_REVERSE32( __rgdwDwords[ __i ], __temp );                                    \
        OEM_SELWRE_DWORDCPY( __rgbBytes + ( __i * sizeof( DRM_DWORD ) ), &__temp, 1 );      \
    } /* end for */                                                                         \
}
#else /* must be big endian */
#define SHA256_DWORDTOBIGENDIAN( __rgbBytes, __rgdwDwords, __cdwDwords )                    \
{                                                                                           \
    OEM_SELWRE_DWORDCPY( __rgbBytes, ( DRM_BYTE* )__rgdwDwords, __cdwDwords  );             \
}
#endif
#endif

#if defined(SEC_COMPILE)
/*
** Function Prototypes
*/

static DRM_RESULT _SHA256_Transform(
    __inout_ecount( SHA256_STATE_SIZE_IN_DWORDS )       DRM_DWORD f_rgdwState[],
    __in_ecount( SHA256_BLOCK_SIZE_IN_DWORDS )    const DRM_DWORD f_rgdwBlock[] )
GCC_ATTRIB_NO_STACK_PROTECT();

#endif
/*
** Function Definitions
*/

/*************************************************************************************************
**
** Function: DRM_SHA256_Init
**
** Description: Initialize the sha256 context structure. To do full SHA256 hashing run funct in
**              this order: 1x DRM_SHA256_Init, (0-inf)x DRM_SHA256_Update, 1x DRM_SHA256_Finalize
**
** Args:
**    [f_pShaContext]: Current running sha256 context
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:
**
*************************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_SHA256_Init(
    __out_ecount( 1 ) DRM_SHA256_CONTEXT *f_pShaContext )
{
    /*
    ** Decls
    */
    DRM_RESULT dr = DRM_SUCCESS;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMSHA256, PERF_FUNC_DRM_SHA256_Init );

    /*
    ** Arg Check
    */
    ChkArg( NULL != f_pShaContext );

    f_pShaContext->m_rgUnion.m_rgdwState[ 0 ] = 0x6a09e667;
    f_pShaContext->m_rgUnion.m_rgdwState[ 1 ] = 0xbb67ae85;
    f_pShaContext->m_rgUnion.m_rgdwState[ 2 ] = 0x3c6ef372;
    f_pShaContext->m_rgUnion.m_rgdwState[ 3 ] = 0xa54ff53a;
    f_pShaContext->m_rgUnion.m_rgdwState[ 4 ] = 0x510e527f;
    f_pShaContext->m_rgUnion.m_rgdwState[ 5 ] = 0x9b05688c;
    f_pShaContext->m_rgUnion.m_rgdwState[ 6 ] = 0x1f83d9ab;
    f_pShaContext->m_rgUnion.m_rgdwState[ 7 ] = 0x5be0cd19;

    f_pShaContext->m_rgdwCount[ 0 ] = 0;
    f_pShaContext->m_rgdwCount[ 1 ] = 0;

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
} /* end DRM_SHA256Init */
#endif


/*************************************************************************************************
**
** Function: DRM_SHA256_Update
**
** Description: Insert data to be hashed in the current sha 256 context. Some hashing takes place
**              in this function out of place.
**
** Args:
**    [f_pShaContext]: Current running sha context
**    [f_rgbBuffer]:   Buffer to perform sha256 on
**    [f_cbBuffer]:    Length of the [f_rgbBuffer] buffer
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:
**
*************************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_SHA256_Update(
    __inout_ecount( 1 )             DRM_SHA256_CONTEXT *f_pShaContext,
    __in_ecount( f_cbBuffer ) const DRM_BYTE            f_rgbBuffer[],
    __in                            DRM_DWORD           f_cbBuffer )
{
    return DRM_SHA256_UpdateOffset( f_pShaContext, f_rgbBuffer, f_cbBuffer, 0 );
} /* end DRM_SHA256_Update */
#endif


/*************************************************************************************************
**
** Function: DRM_SHA256_UpdateOffset
**
** Description: Insert data to be hashed in the current sha 256 context. Some hashing takes place
**              in this function out of place.
**
** Args:
**    [f_pShaContext]:    Current running sha context
**    [f_rgbBuffer]:      Buffer to perform sha256 on
**    [f_cbBuffer]:       Length of [f_rgbBuffer] buffer to hash (after the offset)
**    [f_ibBufferOffset]: Offset in the buffer where we will begin hashing and continue up to,
**                        but not including f_rgbBuffer[f_cbBuffer + f_ibBufferOffset]
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:
**
*************************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_SHA256_UpdateOffset(
    __inout_ecount( 1 )                                         DRM_SHA256_CONTEXT *f_pShaContext,
    __in_ecount( f_cbBufferRemaining + f_ibBufferOffset ) const DRM_BYTE            f_rgbBuffer[],
    __in                                                        DRM_DWORD           f_cbBufferRemaining,
    __in                                                        DRM_DWORD           f_ibBufferOffset )
{
    /*
    ** Inits
    */
    DRM_DWORD  ibOffset     = f_ibBufferOffset;
    DRM_DWORD  dwBufferLen  = 0;
    DRM_DWORD  dwResultTemp = 0;
    DRM_RESULT dr           = DRM_SUCCESS;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMSHA256, PERF_FUNC_DRM_SHA256_UpdateOffset );

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_pShaContext );
    ChkArg( NULL != f_rgbBuffer );

    /*
    ** Copy the size of left over bytes in the context, this size should not
    ** have exceeded 64 (ie it has to be lower than 0x40)
    */
    dwBufferLen = ( DRM_DWORD )( f_pShaContext->m_rgdwCount[ 1 ] & 0x3f );

    /*
    ** Update number of bytes, this will give the number of bytes left over
    ** if you mod it by 64 (or alternativly AND it with 0x3f (63) )
    */
    if ( ( f_pShaContext->m_rgdwCount[ 1 ] += f_cbBufferRemaining ) < f_cbBufferRemaining )
    {
        f_pShaContext->m_rgdwCount[ 0 ]++;
    } /* end if */

    /*
    ** If previous input in buffer, buffer new input and transform if possible.
    */
    if (     0 < dwBufferLen
        && ( SHA256_BLOCK_SIZE_IN_BYTES - dwBufferLen ) <= f_cbBufferRemaining )
    {
        OEM_SELWRE_MEMCPY( ( DRM_BYTE * )f_pShaContext->m_rgdwBuffer + dwBufferLen,
                           f_rgbBuffer + ibOffset,
                           SHA256_BLOCK_SIZE_IN_BYTES - dwBufferLen );
        ChkDR( _SHA256_Transform( f_pShaContext->m_rgUnion.m_rgdwState,
                                  f_pShaContext->m_rgdwBuffer ) );

        ChkDR( DRM_DWordSub( SHA256_BLOCK_SIZE_IN_BYTES, dwBufferLen, &dwResultTemp ) );
        ChkDR( DRM_DWordAdd( ibOffset, dwResultTemp, &ibOffset ) );
        f_cbBufferRemaining  -= dwResultTemp;
        dwBufferLen = 0;
    } /* end if */

    if ( !DRM_IS_DWORD_ALIGNED( f_rgbBuffer + ibOffset ) )
    {
        /*
        ** Copy input to aligned temporary buffer and then transform
        */
        while ( f_cbBufferRemaining >= SHA256_BLOCK_SIZE_IN_BYTES )
        {
            OEM_SELWRE_MEMCPY( ( DRM_BYTE * )f_pShaContext->m_rgdwBuffer,
                               f_rgbBuffer + ibOffset,
                               SHA256_BLOCK_SIZE_IN_BYTES );

            ChkDR( _SHA256_Transform( f_pShaContext->m_rgUnion.m_rgdwState,
                                      f_pShaContext->m_rgdwBuffer ) );

            ChkDR( DRM_DWordAdd( ibOffset, SHA256_BLOCK_SIZE_IN_BYTES, &ibOffset ) );
            ChkDR( DRM_DWordSub( f_cbBufferRemaining, SHA256_BLOCK_SIZE_IN_BYTES, &f_cbBufferRemaining ) );
        } /* end while */
    } /* end if */
    else
    {
        /*
        ** Transform directly from input.
        */
        while ( f_cbBufferRemaining >= SHA256_BLOCK_SIZE_IN_BYTES )
        {
            ChkDR( _SHA256_Transform( f_pShaContext->m_rgUnion.m_rgdwState,
                                      ( DRM_DWORD * )( f_rgbBuffer + ibOffset ) ) );

            ChkDR( DRM_DWordAdd( ibOffset, SHA256_BLOCK_SIZE_IN_BYTES, &ibOffset ) );
            f_cbBufferRemaining  -= SHA256_BLOCK_SIZE_IN_BYTES;
        } /* end while */
    } /* end if */

    /*
    ** Save remaining input buffer if necessary. We have already store the count of
    ** bytes left over earlier in this functions.
    */
    if ( 0 != f_cbBufferRemaining )
    {
        OEM_SELWRE_MEMCPY( ( DRM_BYTE * )f_pShaContext->m_rgdwBuffer + dwBufferLen,
                           f_rgbBuffer + ibOffset,
                           f_cbBufferRemaining );
    } /* end if */

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
} /* end DRM_SHA256_UpdateOffset */
#endif

/*************************************************************************************************
**
** Function: DRM_SHA256_Finalize
**
** Description: Perform all the SHA256 operations on the perviously inputted data and return the
**              the digest value.
**
** Args:
**    [f_pContext]: current running sha context
**    [f_pDigest]:  resultant sha digest from sha operation
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:
**
*************************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_SHA256_Finalize(
    __inout_ecount( 1 ) DRM_SHA256_CONTEXT *f_pContext,
    __out_ecount( 1 )   DRM_SHA256_Digest  *f_pDigest )
{
    /*
    ** Decls
    */
    DRM_DWORD  rgdwBitCount[ 2 ];         /* not initialized for perf */
    DRM_BYTE   rgbPad[ 72 ]; /* not initialized for perf */
    DRM_DWORD  dwPadLen;                  /* not initialized for perf */
    DRM_RESULT dr       = DRM_SUCCESS;

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMSHA256, PERF_FUNC_DRM_SHA256_Finalize );

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_pContext );
    ChkArg( NULL != f_pDigest );

    /*
    ** Compute padding: 80 00 00 ... 00 00 <bit count>
    */
    dwPadLen = SHA256_BLOCK_SIZE_IN_BYTES - ( DRM_DWORD )( f_pContext->m_rgdwCount[ 1 ] & 0x3f );
    if ( dwPadLen <= 8 )
    {
        dwPadLen += SHA256_BLOCK_SIZE_IN_BYTES;
    } /* end if */

    /* 72 is the size of rgbPad[] defined at the beginning of the function. */
    ChkArg( dwPadLen >= 8 && dwPadLen - 8 <= 72 );

    /*
    ** Create the padding for the last block.
    ** sfield: avoid DLW split by aligning fill on DWORD boundry.
    */
    OEM_SELWRE_ZERO_MEMORY( rgbPad, dwPadLen - 8 );
    rgbPad[ 0 ] = 0x80;

    rgdwBitCount[ 0 ] =   ( f_pContext->m_rgdwCount[ 0 ] << 3 )
                        | ( f_pContext->m_rgdwCount[ 1 ] >> 29 );
    rgdwBitCount[ 1 ] = f_pContext->m_rgdwCount[ 1 ] << 3;
    SHA256_DWORDTOBIGENDIAN( rgbPad + dwPadLen - 8, rgdwBitCount, 2 );

    /*
    ** digest padding.
    */
    ChkDR( DRM_SHA256_Update( f_pContext, rgbPad, dwPadLen ) );

    /*
    ** store digest.
    */
    SHA256_DWORDTOBIGENDIAN( f_pDigest->m_rgbDigest,
                             f_pContext->m_rgUnion.m_rgdwState,
                             SHA256_STATE_SIZE_IN_DWORDS );

    /*
    ** restart/scrub the context.
    */
    ChkDR( DRM_SHA256_Init( f_pContext ) );
    OEM_SELWRE_ZERO_MEMORY( ( DRM_BYTE * )f_pContext->m_rgdwBuffer, sizeof( f_pContext->m_rgdwBuffer ) );

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
} /* end DRM_SHA256_Finalize */
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_SHA256_Finalize_With_SHA_1_Size(
    __inout_ecount( 1 )                 DRM_SHA256_CONTEXT  *f_pContext,
    __out_bcount( DRM_SHA1_DIGEST_LEN ) DRM_BYTE             f_rgbDigest[DRM_SHA1_DIGEST_LEN] )
{
    DRM_RESULT dr  = DRM_SUCCESS;
    DRM_DWORD  idx = 0;
    DRM_BYTE   rgbHash[SHA256_DIGEST_SIZE_IN_BYTES];

    ChkArg( f_pContext != NULL );

    ChkDR( DRM_SHA256_Finalize( f_pContext, ( DRM_SHA256_Digest * )rgbHash ) );

    /*
    ** XOR the last 12 bytes with the begining 12 bytes of hash to fold in
    ** the 32 byte hash to a 20 byte hash.
    */
    OEM_SELWRE_MEMCPY( f_rgbDigest, rgbHash, DRM_SHA1_DIGEST_LEN );

    for( idx = 0; idx < ( SHA256_DIGEST_SIZE_IN_BYTES - DRM_SHA1_DIGEST_LEN ); idx++ )
    {
        f_rgbDigest[idx] ^= rgbHash[DRM_SHA1_DIGEST_LEN + idx];
    }

ErrorExit:
    return dr;
}
#endif

/*************************************************************************************************
**
** Function: _SHA256_Transform
**
** Description: Perform the SHA 256 internal transforms for a single block
**
** Args:
**   [f_rgdwState]: The current input state of the previous block hash, this value is modified
**   [f_rgdwBlock]: The next block to be hashed
**
** Returns: DRM_RESULT indicating success or the correct error code. Both parameters will be set
**          if function exelwtes successfully.
**
** Notes:
**
*************************************************************************************************/
#if defined(SEC_COMPILE)
static DRM_NO_INLINE DRM_RESULT _SHA256_Transform(
    __inout_ecount( SHA256_STATE_SIZE_IN_DWORDS )       DRM_DWORD f_rgdwState[],
    __in_ecount( SHA256_BLOCK_SIZE_IN_DWORDS )    const DRM_DWORD f_rgdwBlock[] )
{
    #define __a rgdwABCDEFGH[ 0 ]
    #define __b rgdwABCDEFGH[ 1 ]
    #define __c rgdwABCDEFGH[ 2 ]
    #define __d rgdwABCDEFGH[ 3 ]
    #define __e rgdwABCDEFGH[ 4 ]
    #define __f rgdwABCDEFGH[ 5 ]
    #define __g rgdwABCDEFGH[ 6 ]
    #define __h rgdwABCDEFGH[ 7 ]

    /*
    ** Decls
    */
    DRM_UINT64  rguulW256_64[ SHA256_STATE_SIZE_IN_DWORDS ]; /* not initialized for perf, room for 64 byte block */
    DRM_DWORD  *pdwW256 = DRM_REINTERPRET_CAST( DRM_DWORD, rguulW256_64 ); /* Safe casting, rguulW256_64 is in multiples of DWORDs */
    DRM_DWORD   rgdwABCDEFGH[ SHA256_STATE_SIZE_IN_DWORDS ]; /* not initialized for perf */
    DRM_DWORD   dwS0    /* not initialized for perf */;
    DRM_DWORD   dwS1    /* not initialized for perf */;
    DRM_DWORD   dwTemp  /* not initialized for perf */;
    DRM_DWORD   dwRound /* not initialized for perf */;
    DRM_RESULT  dr = DRM_SUCCESS;

    /*
    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMSHA256, PERF_FUNC_SHA256_Transform );
    */

    /*
    ** Arg Checks
    */
    ChkArg( NULL != f_rgdwState );
    ChkArg( NULL != f_rgdwBlock );

#if TARGET_LITTLE_ENDIAN

#if DRM_64BIT_TARGET
    {
        const DRM_UINT64 *puulIn = ( const DRM_UINT64* )f_rgdwBlock;
        DRM_UINT64 *puulOut = rguulW256_64;

        for ( dwRound = 0 ; dwRound < 2 ; dwRound++, puulIn += 4, puulOut += 4 )
        {
            SHA256_REVERSE32_64( puulIn[ 0 ], puulOut[ 0 ] );
            SHA256_REVERSE32_64( puulIn[ 1 ], puulOut[ 1 ] );
            SHA256_REVERSE32_64( puulIn[ 2 ], puulOut[ 2 ] );
            SHA256_REVERSE32_64( puulIn[ 3 ], puulOut[ 3 ] );
        } /* end for */
    } /* end local block for bigendian and !32-bit */
#else /* 32 bit */
    {
        const DRM_DWORD *pdwIn  = f_rgdwBlock;
        DRM_DWORD       *pdwOut = pdwW256;

        for ( dwRound = 0 ; dwRound < 4 ; dwRound++, pdwIn += 4, pdwOut += 4 )
        {
            SHA256_REVERSE32( pdwIn[ 0 ], pdwOut[ 0 ] );
            SHA256_REVERSE32( pdwIn[ 1 ], pdwOut[ 1 ] );
            SHA256_REVERSE32( pdwIn[ 2 ], pdwOut[ 2 ] );
            SHA256_REVERSE32( pdwIn[ 3 ], pdwOut[ 3 ] );
        } /* end for */
    } /* end local block for bigendian and !64-bit */

#endif /* end 32 bit */

#else
    OEM_SELWRE_DWORDCPY( pdwW256, ( DRM_BYTE * )f_rgdwBlock, SHA256_BLOCK_SIZE_IN_DWORDS );
#endif  /* BIGENDIAN */

PREFAST_PUSH_DISABLE_EXPLAINED( __WARNING_READ_OVERRUN_6385, "Cant overflow rgdwABCDEFGH[i] = f_rgdwState and both arrays are of size SHA256_STATE_SIZE_IN_DWORDS." )
    __a = f_rgdwState[ 0 ];
    __b = f_rgdwState[ 1 ];
    __c = f_rgdwState[ 2 ];
    __d = f_rgdwState[ 3 ];
    __e = f_rgdwState[ 4 ];
    __f = f_rgdwState[ 5 ];
    __g = f_rgdwState[ 6 ];
    __h = f_rgdwState[ 7 ];
PREFAST_POP /* __WARNING_READ_OVERRUN_6385 */

    /*
    ** Do the initial rounds (1 to 16)
    */
    for ( dwRound = 0 ; dwRound < SHA256_NUM_FRONT_ITERATIONS ; )
    {
        SHA256_ROUNDINIT( __a, __b, __c, __d, __e, __f, __g, __h );
        SHA256_ROUNDINIT( __h, __a, __b, __c, __d, __e, __f, __g );
        SHA256_ROUNDINIT( __g, __h, __a, __b, __c, __d, __e, __f );
        SHA256_ROUNDINIT( __f, __g, __h, __a, __b, __c, __d, __e );
        SHA256_ROUNDINIT( __e, __f, __g, __h, __a, __b, __c, __d );
        SHA256_ROUNDINIT( __d, __e, __f, __g, __h, __a, __b, __c );
        SHA256_ROUNDINIT( __c, __d, __e, __f, __g, __h, __a, __b );
        SHA256_ROUNDINIT( __b, __c, __d, __e, __f, __g, __h, __a );
    } /* end while */

    /*
    ** Do rounds 16 to 64.
    */
    while ( dwRound < SHA256_NUM_ITERATIONS )
    {
        SHA256_ROUND( __a, __b, __c, __d, __e, __f, __g, __h );
        SHA256_ROUND( __h, __a, __b, __c, __d, __e, __f, __g );
        SHA256_ROUND( __g, __h, __a, __b, __c, __d, __e, __f );
        SHA256_ROUND( __f, __g, __h, __a, __b, __c, __d, __e );
        SHA256_ROUND( __e, __f, __g, __h, __a, __b, __c, __d );
        SHA256_ROUND( __d, __e, __f, __g, __h, __a, __b, __c );
        SHA256_ROUND( __c, __d, __e, __f, __g, __h, __a, __b );
        SHA256_ROUND( __b, __c, __d, __e, __f, __g, __h, __a );
    } /* end for */

    /*
    ** Compute the current intermediate hash value
    */
    f_rgdwState[ 0 ] += __a;
    f_rgdwState[ 1 ] += __b;
    f_rgdwState[ 2 ] += __c;
    f_rgdwState[ 3 ] += __d;
    f_rgdwState[ 4 ] += __e;
    f_rgdwState[ 5 ] += __f;
    f_rgdwState[ 6 ] += __g;
    f_rgdwState[ 7 ] += __h;

ErrorExit:
    /*
    DRM_PROFILING_LEAVE_SCOPE;
    */
    return dr;

    #undef __a
    #undef __b
    #undef __c
    #undef __d
    #undef __e
    #undef __f
    #undef __g
    #undef __h
} /* end _SHA256_Transform */
#endif

#ifdef NONE
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_SHA256_HMAC_Init(
    __inout_ecount( 1 )          DRM_SHA256_HMAC_CONTEXT       *f_pContextHMAC,
    __in_bcount( f_cbKey ) const DRM_BYTE                      *f_pbKey,
    __in                         DRM_DWORD                      f_cbKey )
{
    DRM_RESULT dr             = DRM_SUCCESS;
    DRM_DWORD  cbShaBlockSize = SHA256_BLOCK_SIZE_IN_BYTES;
    DRM_DWORD  iCount         = 0;

    ChkArg( f_pContextHMAC != NULL );
    ChkArg( f_cbKey        != 0 );
    ChkArg( f_pbKey        != NULL);

    f_pContextHMAC->fInitialized = FALSE;

    /*
    ** Only use the first cbShaBlockSize bytes
    */
    if( f_cbKey > cbShaBlockSize )
    {
        f_cbKey = cbShaBlockSize;
    }

    OEM_SELWRE_ZERO_MEMORY( f_pContextHMAC, sizeof( *f_pContextHMAC ) );

    for( iCount = 0; iCount < f_cbKey; iCount++ )
    {
        f_pContextHMAC->rgbBuffer[iCount] = (DRM_BYTE)( f_pbKey[iCount] ^ 0x36 );
    }
    for( ; iCount < cbShaBlockSize ; iCount++ )
    {
        f_pContextHMAC->rgbBuffer[iCount] = 0 ^ 0x36;
    }

    ChkDR( DRM_SHA256_Init( &( f_pContextHMAC->shaContext ) ) );
    ChkDR( DRM_SHA256_Update( &( f_pContextHMAC->shaContext ), f_pContextHMAC->rgbBuffer, cbShaBlockSize ) );

    /*
    ** Update the internal context buffer so we don't have to hold onto the key.
    */
    for( iCount = 0; iCount < f_cbKey; iCount++ )
    {
        f_pContextHMAC->rgbBuffer[iCount] = (DRM_BYTE)( f_pbKey[iCount] ^ 0x5C );
    }
    for( ; iCount < cbShaBlockSize ; iCount++ )
    {
        f_pContextHMAC->rgbBuffer[iCount] = 0 ^ 0x5C;
    }

    f_pContextHMAC->fInitialized = TRUE;

ErrorExit:
    return dr;
}

#if DRM_INLINING_SUPPORTED
static DRM_ALWAYS_INLINE DRM_RESULT DRM_CALL DRM_SHA256_HMAC_UpdateOffset(
#else
static DRM_RESULT DRM_CALL DRM_SHA256_HMAC_UpdateOffset(
#endif
    __inout_ecount( 1 )                                DRM_SHA256_HMAC_CONTEXT  *f_pContextHMAC,
    __in_bcount( f_cbDataRemaining + f_ibData )  const DRM_BYTE                 *f_pbData,
    __in                                               DRM_DWORD                 f_ibData,
    __in                                               DRM_DWORD                 f_cbDataRemaining  )
{
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_pContextHMAC != NULL );
    ChkArg( f_pContextHMAC->fInitialized );

    if( f_cbDataRemaining > 0 )
    {
        ChkArg( f_pbData != NULL );
        ChkDR( DRM_SHA256_UpdateOffset( &( f_pContextHMAC->shaContext ), f_pbData, f_cbDataRemaining, f_ibData ) );
    }

ErrorExit:
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_SHA256_HMAC_Update(
    __inout_ecount( 1 )           DRM_SHA256_HMAC_CONTEXT  *f_pContextHMAC,
    __in_bcount( f_cbData ) const DRM_BYTE                 *f_pbData,
    __in                          DRM_DWORD                 f_cbData )
{
    return DRM_SHA256_HMAC_UpdateOffset( f_pContextHMAC, f_pbData, 0, f_cbData );
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_SHA256_HMAC_Finalize(
    __inout_ecount( 1 )           DRM_SHA256_HMAC_CONTEXT  *f_pContextHMAC,
    __out_bcount( f_cbKeyedHash ) DRM_BYTE                 *f_pbKeyedHash,
    __in                          DRM_DWORD                 f_cbKeyedHash )
{
    return DRM_SHA256_HMAC_FinalizeOffset( f_pContextHMAC, f_pbKeyedHash, 0, f_cbKeyedHash );
}

PREFAST_PUSH_DISABLE_EXPLAINED(__WARNING_COUNT_REQUIRED_FOR_WRITABLE_BUFFER, "f_pbKeyedHash length defined by f_cbKeyedHashRemaining and f_ibKeyedHash" )
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_SHA256_HMAC_FinalizeOffset(
    __inout_ecount( 1 )                                     DRM_SHA256_HMAC_CONTEXT  *f_pContextHMAC,
    __out_bcount( f_cbKeyedHashRemaining + f_ibKeyedHash )  DRM_BYTE                 *f_pbKeyedHash,
    __in                                                    DRM_DWORD                 f_ibKeyedHash,
    __in                                                    DRM_DWORD                 f_cbKeyedHashRemaining )
PREFAST_POP
{
    DRM_RESULT dr             = DRM_SUCCESS;
    DRM_DWORD  cbShaDigestLen = SHA256_DIGEST_SIZE_IN_BYTES;
    DRM_DWORD  cbShaBlockSize = SHA256_BLOCK_SIZE_IN_BYTES;

    ChkArg( f_pContextHMAC != NULL );
    ChkArg( f_pContextHMAC->fInitialized );
    ChkArg( f_pbKeyedHash  != NULL );
    ChkArg( f_cbKeyedHashRemaining  != 0 );

    if( f_cbKeyedHashRemaining > SHA256_DIGEST_SIZE_IN_BYTES )
    {
        f_cbKeyedHashRemaining = SHA256_DIGEST_SIZE_IN_BYTES;
    }

    /*
    ** Finish the first SHA operation.
    */
    ChkDR( DRM_SHA256_Finalize( &( f_pContextHMAC->shaContext ), ( DRM_SHA256_Digest * )f_pContextHMAC->shaDigest ) );

    /*
    ** Start second SHA operation
    */
    ChkDR( DRM_SHA256_Init( &( f_pContextHMAC->shaContext ) ) );
    ChkDR( DRM_SHA256_Update( &( f_pContextHMAC->shaContext ), f_pContextHMAC->rgbBuffer, cbShaBlockSize ) );
    ChkDR( DRM_SHA256_Update( &( f_pContextHMAC->shaContext ), f_pContextHMAC->shaDigest, cbShaDigestLen ) );
    ChkDR( DRM_SHA256_Finalize( &( f_pContextHMAC->shaContext ), ( DRM_SHA256_Digest * )f_pContextHMAC->shaDigest ) );

    OEM_SELWRE_MEMCPY( f_pbKeyedHash + f_ibKeyedHash, f_pContextHMAC->shaDigest, f_cbKeyedHashRemaining );

ErrorExit:
    if ( f_pContextHMAC != NULL )
    {
        OEM_SELWRE_ZERO_MEMORY( f_pContextHMAC->rgbBuffer, sizeof(f_pContextHMAC->rgbBuffer) );
    }
    return dr;
}

DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL DRM_SHA256_HMAC_CreateMAC(
    __in_bcount( f_cbKey )                       const DRM_BYTE                      *f_pbKey,
    __in                                               DRM_DWORD                      f_cbKey,
    __in_bcount( f_cbDataRemaining + f_ibData )  const DRM_BYTE                      *f_pbData,
    __in                                               DRM_DWORD                      f_ibData,
    __in                                               DRM_DWORD                      f_cbDataRemaining,
    __out_bcount( f_cbMACRemaining + f_ibMAC )         DRM_BYTE                      *f_pbMAC,
    __in                                               DRM_DWORD                      f_ibMAC,
    __in                                               DRM_DWORD                      f_cbMACRemaining )
{
    DRM_RESULT              dr             = DRM_SUCCESS;
    DRM_DWORD               dwResultTemp   = 0;
    DRM_SHA256_HMAC_CONTEXT hmac           = {0};

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMHMAC, PERF_FUNC_DRM_HMAC_CreateMAC );

    ChkArg( f_cbDataRemaining > 0 );
    ChkDR( DRM_DWordAdd( f_cbDataRemaining, f_ibData, &dwResultTemp ) );     /* Verifies no overflow */

    ChkDR( DRM_SHA256_HMAC_Init( &hmac, f_pbKey, f_cbKey ) );
    ChkDR( DRM_SHA256_HMAC_UpdateOffset( &hmac, f_pbData, f_ibData, f_cbDataRemaining ) );
    ChkDR( DRM_SHA256_HMAC_FinalizeOffset( &hmac, f_pbMAC, f_ibMAC, f_cbMACRemaining ) );

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}

DRM_API DRM_RESULT DRM_CALL DRM_SHA256_HMAC_VerifyMAC(
    __in_bcount( f_cbKey )                       const DRM_BYTE                      *f_pbKey,
    __in                                               DRM_DWORD                      f_cbKey,
    __in_bcount( f_cbDataRemaining + f_ibData )  const DRM_BYTE                      *f_pbData,
    __in                                               DRM_DWORD                      f_ibData,
    __in                                               DRM_DWORD                      f_cbDataRemaining,
    __in_bcount( f_cbMACRemaining + f_ibMAC )    const DRM_BYTE                      *f_pbMAC,
    __in                                               DRM_DWORD                      f_ibMAC,
    __in                                               DRM_DWORD                      f_cbMACRemaining )
{
    DRM_RESULT       dr   = DRM_SUCCESS;
    DRM_BYTE         rgbMAC[SHA256_DIGEST_SIZE_IN_BYTES] = { 0 };

    DRM_PROFILING_ENTER_SCOPE( PERF_MOD_DRMHMAC, PERF_FUNC_DRM_HMAC_VerifyMAC );

    ChkBOOL( f_cbMACRemaining <= SHA256_DIGEST_SIZE_IN_BYTES, DRM_E_ILWALID_SIGNATURE );
    ChkArg( f_cbDataRemaining > 0 );

    ChkDR( DRM_SHA256_HMAC_CreateMAC( f_pbKey,
                                      f_cbKey,
                                      f_pbData,
                                      f_ibData,
                                      f_cbDataRemaining,
                                      rgbMAC,
                                      0,
                                      f_cbMACRemaining ) );

    ChkBOOL( OEM_SELWRE_ARE_EQUAL( rgbMAC, &f_pbMAC[f_ibMAC], f_cbMACRemaining ), DRM_E_ILWALID_SIGNATURE );

ErrorExit:
    DRM_PROFILING_LEAVE_SCOPE;
    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;

