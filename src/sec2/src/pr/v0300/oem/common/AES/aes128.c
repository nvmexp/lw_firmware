/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

/*
** Module Name: aes128.c
**
** Abstract:
**    This module contains the low-level AES encryption routines
**    for 128-bit AES
**    Derived from public domain sources
*/

#define DRM_BUILDING_AES128_C
#include "aes128.h"
#include "aesbox.h"
#include <oemteecrypto.h>
#include <oemcommon.h>
#include <drmbytemanip.h>
#include <drmlastinclude.h>

#include "sec2_hs.h"
#include "sec2_objpr.h"
#include "lwosselwreovly.h"


// PDI got renamed in Turing.
#ifndef LW_FUSE_OPT_PRIV_PDI_0
#define LW_FUSE_OPT_PRIV_PDI_0                                  LW_FUSE_OPT_PDI_0
#endif

#ifndef LW_FUSE_OPT_PRIV_PDI_1
#define LW_FUSE_OPT_PRIV_PDI_1                                  LW_FUSE_OPT_PDI_1
#endif

ENTER_PK_NAMESPACE_CODE;

#if defined(USE_MSFT_SW_AES_IMPLEMENTATION)
/*********************************************************************************************
** Function:  Aes_CreateKey128
**
** Synopsis:  Set up an AES Key Table for a 128-bit AES key
**
** Arguments: [f_pKeyTable ]: The key table to set up ( output )
**            [f_rgbKey]    : The key data
**
** Returns:   None
**********************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Aes_CreateKey128(
    __out_ecount( 1 )                        DRM_AESTable *f_pKeyTable,
    __in_bcount( DRM_AES_KEYSIZE_128 ) const DRM_BYTE      f_rgbKey[ DRM_AES_KEYSIZE_128 ] )
{
    CLAW_AUTO_RANDOM_CIPHER
    /*
    ** Callwlate the necessary round keys
    ** The number of callwlations depends on keyBits and blockBits
    */
    DRM_DWORD  j           = 0;
    DRM_DWORD  r           = 0;
    DRM_DWORD  t           = 0;
    DRM_DWORD  rconpointer = 0;
    DRM_BYTE   bTmp        = 0;
    DRM_DWORD *w           = NULL;
    DRM_DWORD  tk[DRM_AES_ROUNDS_128 - 6];
    const DRM_DWORD  KC    = DRM_AES_ROUNDS_128 - 6;

    /*
    ** These assertions check that DWORD access of T and U arrays is valid
    */
    DRMASSERT( DRM_IS_DWORD_ALIGNED( T1[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( T2[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( T3[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( T4[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( T5[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( T6[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( T7[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( T8[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( U1[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( U2[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( U3[0] ) );
    DRMASSERT( DRM_IS_DWORD_ALIGNED( U4[0] ) );


    /*
    ** Compute keytabenc
    */

    OEM_SELWRE_MEMCPY( ( DRM_BYTE * )&tk, &f_rgbKey[0], KC*sizeof( DRM_DWORD ) );

    /*
    ** copy values into round key array
    */
    for( j = 0; ( j < KC ) && ( r < DRM_AES_ROUNDS_128 + 1 ); )
    {
        for ( ; ( j < KC ) && ( t < 4 ); j++, t++ )
        {
            f_pKeyTable->keytabenc[r][t] = tk[j];
        }
        if ( t == 4 )
        {
            r++;
            t = 0;
        }
    }
    /*
    ** while not enough round key material callwlated
    ** callwlate new values
    */
    while ( r < DRM_AES_ROUNDS_128 + 1 )
    {
        bTmp =  ( ( DRM_BYTE * )( tk + ( KC-1 ) ) )[ 1 ];
        bTmp =  S[ bTmp ];
        bTmp ^= ( ( DRM_BYTE * )( tk ) )[ 0 ];
        ( ( DRM_BYTE * )( tk ) )[ 0 ] = bTmp;

        bTmp =  ( ( DRM_BYTE * )( tk + ( KC-1 ) ) )[ 2 ];
        bTmp =  S[ bTmp ];
        bTmp ^= ( ( DRM_BYTE * )( tk ) )[ 1 ];
        ( ( DRM_BYTE * )( tk ) )[ 1 ] = bTmp;

        bTmp =  ( ( DRM_BYTE * )( tk + ( KC-1 ) ) )[ 3 ];
        bTmp =  S[ bTmp ];
        bTmp ^= ( ( DRM_BYTE * )( tk ) )[ 2 ];
        ( ( DRM_BYTE * )( tk ) )[ 2 ] = bTmp;

        bTmp =  ( ( DRM_BYTE * )( tk + ( KC-1 ) ) )[ 0 ];
        bTmp =  S[ bTmp ];
        bTmp ^= ( ( DRM_BYTE * )( tk ) )[ 3 ];
        ( ( DRM_BYTE * )( tk ) )[ 3 ] = bTmp;

        bTmp =  ( DRM_BYTE ) rcon[rconpointer++];
        bTmp ^= ( ( DRM_BYTE * )( tk ) )[ 0 ];
        ( ( DRM_BYTE * )( tk ) )[ 0 ] = bTmp;

        for ( j = 1; j < KC; j++ )
        {
            tk[j] ^= tk[j-1];
        }

        /*
        ** Copy values into round key array
        */
        for ( j = 0; ( j < KC ) && ( r < DRM_AES_ROUNDS_128 + 1 ); )
        {
            for ( ; ( j < KC ) && ( t < 4 ); j++, t++ )
            {
                f_pKeyTable->keytabenc[r][t] = tk[j];
            }
            if ( t == 4 )
            {
                r++;
                t = 0;
            }
        }
    }

    OEM_SELWRE_MEMCPY( ( DRM_BYTE * )( f_pKeyTable->keytabdec[0] ),
            ( DRM_BYTE * )( f_pKeyTable->keytabenc[0] ),
            sizeof( f_pKeyTable->keytabenc ) );

    /*
    ** Compute keytabdec
    */
    for ( r = 1; r < DRM_AES_ROUNDS_128; r++ )
    {
        w = &f_pKeyTable->keytabdec[r][0];
        *w =
              *( ( DRM_DWORD* )U1[(( DRM_BYTE * )w)[ 0 ]] )
            ^ *( ( DRM_DWORD* )U2[(( DRM_BYTE * )w)[ 1 ]] )
            ^ *( ( DRM_DWORD* )U3[(( DRM_BYTE * )w)[ 2 ]] )
            ^ *( ( DRM_DWORD* )U4[(( DRM_BYTE * )w)[ 3 ]] );

        w = &f_pKeyTable->keytabdec[r][1];
        *w =
              *( ( DRM_DWORD* )U1[(( DRM_BYTE * )w)[ 0 ]] )
            ^ *( ( DRM_DWORD* )U2[(( DRM_BYTE * )w)[ 1 ]] )
            ^ *( ( DRM_DWORD* )U3[(( DRM_BYTE * )w)[ 2 ]] )
            ^ *( ( DRM_DWORD* )U4[(( DRM_BYTE * )w)[ 3 ]] );

        w = &f_pKeyTable->keytabdec[r][2];
        *w =
              *( ( DRM_DWORD* )U1[(( DRM_BYTE * )w)[ 0 ]] )
            ^ *( ( DRM_DWORD* )U2[(( DRM_BYTE * )w)[ 1 ]] )
            ^ *( ( DRM_DWORD* )U3[(( DRM_BYTE * )w)[ 2 ]] )
            ^ *( ( DRM_DWORD* )U4[(( DRM_BYTE * )w)[ 3 ]] );

        w = &f_pKeyTable->keytabdec[r][3];
        *w =
              *( ( DRM_DWORD* )U1[(( DRM_BYTE * )w)[ 0 ]] )
            ^ *( ( DRM_DWORD* )U2[(( DRM_BYTE * )w)[ 1 ]] )
            ^ *( ( DRM_DWORD* )U3[(( DRM_BYTE * )w)[ 2 ]] )
            ^ *( ( DRM_DWORD* )U4[(( DRM_BYTE * )w)[ 3 ]] );
    }
}
#endif

/*********************************************************************************************
** Function:  Aes_Encrypt128
**
** Synopsis:  Encrypt a single block
**
** Arguments:
**            [f_rgbEncrypted] : Returns the encrypted data
**            [f_rgbClear]     : Specifies the data to encrypt
**            [f_rgbKeys]      : The key table to use for decryption
**
** Returns:   None
**********************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Aes_Encrypt128(
    __out_bcount( DRM_AES_KEYSIZE_128 )       DRM_BYTE   f_rgbEncrypted[ DRM_AES_KEYSIZE_128 ],
    __in_bcount( DRM_AES_KEYSIZE_128 )  const DRM_BYTE   f_rgbClear[ DRM_AES_KEYSIZE_128 ],
    __in_ecount( DRM_AES_ROUNDS_128+1 ) const DRM_DWORD  f_rgbKeys[DRM_AES_ROUNDS_128+1][4] )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_DWORD r       = 0;
    DRM_DWORD c[4];
    DRM_BYTE *d       = ( DRM_BYTE * )c;
    DRM_DWORD temp[4];
    DRM_BYTE  bTmp    = 0;

    OEM_SELWRE_MEMCPY( ( DRM_BYTE * )temp, f_rgbClear, DRM_AES_KEYSIZE_128 );

    temp[0] ^= f_rgbKeys[0][0];
    temp[1] ^= f_rgbKeys[0][1];
    temp[2] ^= f_rgbKeys[0][2];
    temp[3] ^= f_rgbKeys[0][3];

    c[0] = *( ( DRM_DWORD* )T1[( ( DRM_BYTE * )temp )[ 0 ]] )
         ^ *( ( DRM_DWORD* )T2[( ( DRM_BYTE * )( temp + 1 ) )[ 1 ]] )
         ^ *( ( DRM_DWORD* )T3[( ( DRM_BYTE * )( temp + 2 ) )[ 2 ]] )
         ^ *( ( DRM_DWORD* )T4[( ( DRM_BYTE * )( temp + 3 ) )[ 3 ]] );

    c[1] = *( ( DRM_DWORD* )T1[( ( DRM_BYTE * )( temp + 1 ) )[ 0 ]] )
         ^ *( ( DRM_DWORD* )T2[( ( DRM_BYTE * )( temp + 2 ) )[ 1 ]] )
         ^ *( ( DRM_DWORD* )T3[( ( DRM_BYTE * )( temp + 3 ) )[ 2 ]] )
         ^ *( ( DRM_DWORD* )T4[( ( DRM_BYTE * )temp )[ 3 ]] );

    c[2] = *( ( DRM_DWORD* )T1[( ( DRM_BYTE * )( temp + 2 ) )[ 0 ]] )
         ^ *( ( DRM_DWORD* )T2[( ( DRM_BYTE * )( temp + 3 ) )[ 1 ]] )
         ^ *( ( DRM_DWORD* )T3[( ( DRM_BYTE * )temp )[ 2 ]] )
         ^ *( ( DRM_DWORD* )T4[( ( DRM_BYTE * )( temp + 1 ) )[ 3 ]] );

    c[3] = *( ( DRM_DWORD* )T1[( ( DRM_BYTE * )( temp + 3 ) )[ 0 ]] )
         ^ *( ( DRM_DWORD* )T2[( ( DRM_BYTE * )temp )[ 1 ]] )
         ^ *( ( DRM_DWORD* )T3[( ( DRM_BYTE * )( temp + 1 ) )[ 2 ]] )
         ^ *( ( DRM_DWORD* )T4[( ( DRM_BYTE * )( temp + 2 ) )[ 3 ]] );

    for ( r = 1; r < DRM_AES_ROUNDS_128 - 1; r++ )
    {
        temp[0] = c[0] ^ f_rgbKeys[r][0];
        temp[1] = c[1] ^ f_rgbKeys[r][1];
        temp[2] = c[2] ^ f_rgbKeys[r][2];
        temp[3] = c[3] ^ f_rgbKeys[r][3];

        c[0] = *( ( DRM_DWORD* )T1[( ( DRM_BYTE * )temp )[ 0 ]] )
             ^ *( ( DRM_DWORD* )T2[( ( DRM_BYTE * )( temp + 1 ) )[ 1 ]] )
             ^ *( ( DRM_DWORD* )T3[( ( DRM_BYTE * )( temp + 2 ) )[ 2 ]] )
             ^ *( ( DRM_DWORD* )T4[( ( DRM_BYTE * )( temp + 3 ) )[ 3 ]] );

        c[1] = *( ( DRM_DWORD* )T1[( ( DRM_BYTE * )( temp + 1 ) )[ 0 ]] )
             ^ *( ( DRM_DWORD* )T2[( ( DRM_BYTE * )( temp + 2 ) )[ 1 ]] )
             ^ *( ( DRM_DWORD* )T3[( ( DRM_BYTE * )( temp + 3 ) )[ 2 ]] )
             ^ *( ( DRM_DWORD* )T4[( ( DRM_BYTE * )temp )[ 3 ]] );

        c[2] = *( ( DRM_DWORD* )T1[( ( DRM_BYTE * )( temp + 2 ) )[ 0 ]] )
             ^ *( ( DRM_DWORD* )T2[( ( DRM_BYTE * )( temp + 3 ) )[ 1 ]] )
             ^ *( ( DRM_DWORD* )T3[( ( DRM_BYTE * )temp )[ 2 ]] )
             ^ *( ( DRM_DWORD* )T4[( ( DRM_BYTE * )( temp + 1 ) )[ 3 ]] );

        c[3] = *( ( DRM_DWORD* )T1[( ( DRM_BYTE * )( temp + 3 ) )[ 0 ]] )
             ^ *( ( DRM_DWORD* )T2[( ( DRM_BYTE * )temp )[ 1 ]] )
             ^ *( ( DRM_DWORD* )T3[( ( DRM_BYTE * )( temp + 1 ) )[ 2 ]] )
             ^ *( ( DRM_DWORD* )T4[( ( DRM_BYTE * )( temp + 2 ) )[ 3 ]] );
    }

    /*
    ** last round is special
    */
    temp[0] = c[0] ^ f_rgbKeys[DRM_AES_ROUNDS_128-1][0];
    temp[1] = c[1] ^ f_rgbKeys[DRM_AES_ROUNDS_128-1][1];
    temp[2] = c[2] ^ f_rgbKeys[DRM_AES_ROUNDS_128-1][2];
    temp[3] = c[3] ^ f_rgbKeys[DRM_AES_ROUNDS_128-1][3];

    bTmp = ( ( DRM_BYTE * )temp )[ 0 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 0 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 1 ) )[ 1 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 1 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 2 ) )[ 2 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 2 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 3 ) )[ 3 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 3 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 1 ) )[ 0 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 4 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 2 ) )[ 1 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 5 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 3 ) )[ 2 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 6 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )temp )[ 3 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 7 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 2 ) )[ 0 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 8 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 3 ) )[ 1 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 9 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )temp )[ 2 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 10 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 1 ) )[ 3 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 11 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 3 ) )[ 0 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 12 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )temp )[ 1 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 13 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 1 ) )[ 2 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 14 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 2 ) )[ 3 ];
    bTmp = T1[bTmp][ 1 ];
    d[ 15 ] = bTmp;

    c[0] ^= f_rgbKeys[DRM_AES_ROUNDS_128][0];
    c[1] ^= f_rgbKeys[DRM_AES_ROUNDS_128][1];
    c[2] ^= f_rgbKeys[DRM_AES_ROUNDS_128][2];
    c[3] ^= f_rgbKeys[DRM_AES_ROUNDS_128][3];
    OEM_SELWRE_MEMCPY( f_rgbEncrypted, c, sizeof( c ) );

    return;
}
#endif
/*********************************************************************************************
** Function:  Aes_Decrypt128
**
** Synopsis:  Decrypt a single block
**
** Arguments:
**            [f_rgbClear]     : Returns the decrypted data
**            [f_rgbEncrypted] : Specifies the data to decrypt
**            [f_rgbKeys]      : The table to use for encryption
**
** Returns:   None
**********************************************************************************************/
#if defined(SEC_COMPILE)
DRM_NO_INLINE DRM_API_VOID DRM_VOID DRM_CALL Aes_Decrypt128(
    __out_bcount( DRM_AES_KEYSIZE_128 )       DRM_BYTE   f_rgbClear[ DRM_AES_KEYSIZE_128 ],
    __in_bcount( DRM_AES_KEYSIZE_128 )  const DRM_BYTE   f_rgbEncrypted[ DRM_AES_KEYSIZE_128 ],
    __in_ecount( DRM_AES_ROUNDS_128+1 ) const DRM_DWORD  f_rgbKeys[DRM_AES_ROUNDS_128+1][4] )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_DWORD r       = 0;
    DRM_DWORD temp[4];
    DRM_DWORD c[4];
    DRM_BYTE  bTmp    = 0;

    OEM_SELWRE_MEMCPY( ( DRM_BYTE * )temp, f_rgbEncrypted, DRM_AES_KEYSIZE_128 );
    temp[0] ^= f_rgbKeys[DRM_AES_ROUNDS_128][0];
    temp[1] ^= f_rgbKeys[DRM_AES_ROUNDS_128][1];
    temp[2] ^= f_rgbKeys[DRM_AES_ROUNDS_128][2];
    temp[3] ^= f_rgbKeys[DRM_AES_ROUNDS_128][3];

    c[0] =  *( ( DRM_DWORD* )T5[( ( DRM_BYTE * )temp )[ 0 ]] )
          ^ *( ( DRM_DWORD* )T6[( ( DRM_BYTE * )( temp + 3 ) )[ 1 ]] )
          ^ *( ( DRM_DWORD* )T7[( ( DRM_BYTE * )( temp + 2 ) )[ 2 ]] )
          ^ *( ( DRM_DWORD* )T8[( ( DRM_BYTE * )( temp + 1 ) )[ 3 ]] );
    c[1] = *( ( DRM_DWORD* )T5[( ( DRM_BYTE * )( temp + 1 ) )[ 0 ]] )
         ^ *( ( DRM_DWORD* )T6[( ( DRM_BYTE * )temp )[ 1 ]] )
         ^ *( ( DRM_DWORD* )T7[( ( DRM_BYTE * )( temp + 3 ) )[ 2 ]] )
         ^ *( ( DRM_DWORD* )T8[( ( DRM_BYTE * )( temp + 2 ) )[ 3 ]] );
    c[2] = *( ( DRM_DWORD* )T5[( ( DRM_BYTE * )( temp + 2 ) )[ 0 ]] )
         ^ *( ( DRM_DWORD* )T6[( ( DRM_BYTE * )( temp + 1 ) )[ 1 ]] )
         ^ *( ( DRM_DWORD* )T7[( ( DRM_BYTE * )temp )[ 2 ]] )
         ^ *( ( DRM_DWORD* )T8[( ( DRM_BYTE * )( temp + 3 ) )[ 3 ]] );
    c[3] = *( ( DRM_DWORD* )T5[( ( DRM_BYTE * )( temp + 3 ) )[ 0 ]] )
         ^ *( ( DRM_DWORD* )T6[( ( DRM_BYTE * )( temp + 2 ) )[ 1 ]] )
         ^ *( ( DRM_DWORD* )T7[( ( DRM_BYTE * )( temp + 1 ) )[ 2 ]] )
         ^ *( ( DRM_DWORD* )T8[( ( DRM_BYTE * )temp )[ 3 ]] );

    for ( r = DRM_AES_ROUNDS_128 - 1; r > 1; r-- )
    {
        temp[0] = c[0] ^ f_rgbKeys[r][0];
        temp[1] = c[1] ^ f_rgbKeys[r][1];
        temp[2] = c[2] ^ f_rgbKeys[r][2];
        temp[3] = c[3] ^ f_rgbKeys[r][3];

        c[0] = *( ( DRM_DWORD* )T5[( ( DRM_BYTE * )temp )[ 0 ]] )
             ^ *( ( DRM_DWORD* )T6[( ( DRM_BYTE * )( temp + 3 ) )[ 1 ]] )
             ^ *( ( DRM_DWORD* )T7[( ( DRM_BYTE * )( temp + 2 ) )[ 2 ]] )
             ^ *( ( DRM_DWORD* )T8[( ( DRM_BYTE * )( temp + 1 ) )[ 3 ]] );

        c[1] = *( ( DRM_DWORD* )T5[( ( DRM_BYTE * )( temp + 1 ) )[ 0 ]] )
             ^ *( ( DRM_DWORD* )T6[( ( DRM_BYTE * )temp )[ 1 ]] )
             ^ *( ( DRM_DWORD* )T7[( ( DRM_BYTE * )( temp + 3 ) )[ 2 ]] )
             ^ *( ( DRM_DWORD* )T8[( ( DRM_BYTE * )( temp + 2 ) )[ 3 ]] );

        c[2] = *( ( DRM_DWORD* )T5[( ( DRM_BYTE * )( temp + 2 ) )[ 0 ]] )
             ^ *( ( DRM_DWORD* )T6[( ( DRM_BYTE * )( temp + 1 ) )[ 1 ]] )
             ^ *( ( DRM_DWORD* )T7[( ( DRM_BYTE * )temp )[ 2 ]] )
             ^ *( ( DRM_DWORD* )T8[( ( DRM_BYTE * )( temp + 3 ) )[ 3 ]] );

        c[3] = *( ( DRM_DWORD* )T5[( ( DRM_BYTE * )( temp + 3 ) )[ 0 ]] )
             ^ *( ( DRM_DWORD* )T6[( ( DRM_BYTE * )( temp + 2 ) )[ 1 ]] )
             ^ *( ( DRM_DWORD* )T7[( ( DRM_BYTE * )( temp + 1 ) )[ 2 ]] )
             ^ *( ( DRM_DWORD* )T8[( ( DRM_BYTE * )temp )[ 3 ]] );
    }

    /*
    ** last round is special
    */
    OEM_SELWRE_MEMCPY( ( DRM_BYTE * )temp, ( DRM_BYTE * )c, DRM_AES_KEYSIZE_128 );
    temp[0] ^= f_rgbKeys[1][0];
    temp[1] ^= f_rgbKeys[1][1];
    temp[2] ^= f_rgbKeys[1][2];
    temp[3] ^= f_rgbKeys[1][3];

    bTmp = ( ( DRM_BYTE * )temp )[ 0 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 0 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 3 ) )[ 1 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 1 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 2 ) )[ 2 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 2 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 1 ) )[ 3 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 3 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 1 ) )[ 0 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 4 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )temp )[ 1 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 5 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 3 ) )[ 2 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 6 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 2 ) )[ 3 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 7 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 2 ) )[ 0 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 8 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 1 ) )[ 1 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 9 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )temp )[ 2 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 10 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 3 ) )[ 3 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 11 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 3 ) )[ 0 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 12 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 2 ) )[ 1 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 13 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )( temp + 1 ) )[ 2 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 14 ] = bTmp;

    bTmp = ( ( DRM_BYTE * )temp )[ 3 ];
    bTmp = S5[ bTmp ];
    (( DRM_BYTE * )c)[ 15 ] = bTmp;

    c[0] ^= f_rgbKeys[0][0];
    c[1] ^= f_rgbKeys[0][1];
    c[2] ^= f_rgbKeys[0][2];
    c[3] ^= f_rgbKeys[0][3];

    OEM_SELWRE_MEMCPY( f_rgbClear, ( DRM_BYTE * )c, DRM_AES_KEYSIZE_128 );

    return;
}
#endif

#else           // else part of if defined(USE_MSFT_SW_AES_IMPLEMENTATION)

/*********************************************************************************************
** Function:  _Oem_Aes_EncryptDecryptOneBlockWithSuppliedKey_LWIDIA_LS
**
** Synopsis:  This LS helper function encrypts/decrypts the input buffer with the supplied key.
**
**            WARNING: Lwrrently this helper function is only called by Oem_Aes_EncryptDecryptOneBlock_LWIDIA_LS,
**            and only the 16byte aligned global arrays are passed to this function.  So it's fine to not
**            sanitize the input value.  We will need to add some sanity checks if this function will be
**            used by other callers.
**
** Arguments:
**            [f_rgbBuffer]   : Buffer storing the data for encryption/descryption and returning the encrypted/decrypted data
**            [f_rgbKey]      : The supplied key to be used for encryption/descryption
**            [f_fCryptoMode] : Specifies if encryption or decryption is requested
**
** Returns:   DRM_SUCCESS on success
**********************************************************************************************/
DRM_API DRM_RESULT DRM_CALL _Oem_Aes_EncryptDecryptOneBlockWithSuppliedKey_LWIDIA_LS(
    __inout_bcount ( DRM_AES_KEYSIZE_128 )       DRM_BYTE   f_rgbBuffer [ DRM_AES_KEYSIZE_128 ],
    __in_ecount    ( DRM_AES_KEYSIZE_128 ) const DRM_BYTE   f_rgbKey    [ DRM_AES_KEYSIZE_128 ],
    __in                                   const DRM_DWORD  f_fCryptoMode )
{
    DRM_RESULT dr = DRM_SUCCESS;

    falc_scp_trap(TC_INFINITY);

    // Load the input data into SCP
    falc_trapped_dmwrite(falc_sethi_i((LwU32)f_rgbBuffer, SCP_R2));
    falc_dmwait();

    // Load the SW key into SCP
    falc_trapped_dmwrite(falc_sethi_i((LwU32)f_rgbKey, SCP_R3));
    falc_dmwait();

    // Does the encryption/decryption based on caller's request
    if (FLD_TEST_DRF(_PR_AES128, _OPERATION_LS, _ENC, _YES, f_fCryptoMode))
    {
        falc_scp_key(SCP_R3);
        falc_scp_encrypt(SCP_R2, SCP_R1);
    }
    else
    {
        falc_scp_rkey10(SCP_R3, SCP_R3);
        falc_scp_key(SCP_R3);
        falc_scp_decrypt(SCP_R2, SCP_R1);
    }

    // Fetch the result from SCP
    falc_trapped_dmread(falc_sethi_i((LwU32)f_rgbBuffer, SCP_R1));
    falc_dmwait();

    // Zero out SCP registers we used for encrypt/decrypt operation
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);

    // Reset scp trap
    falc_scp_trap(0);

    return dr;
}

/*********************************************************************************************
** Function:  _Oem_Aes_EncryptOneBlockWithKeyDerivedFromSalt_LWIDIA_HS
**
** Synopsis:  This HS helper function encrypts the SALT with the HW key to generate the derived key.
**            Then encrypts the input buffer with the derived key for certain KDF modes.
**
**            Note that we have HS entry function for .prHsCommon overlay, so this function will never
**            be at offset 0 of .prHsCommon overlay to ensure no LS code can call into this function.
**            Besides, any HS code calling this function must make sure it's not simply passing an input
**            from LS code into this function and instead should generate the SALT in HS code.
**
** Arguments:
**            [f_rgbBuffer]   : Buffer storing the data for encryption and returning the encrypted data
**            [f_rgbSalt]     : The SALT used to generate the derived key in conjunction with the HW key
**            [f_fCryptoMode] : Specifies what KDF mode is requested
**
** Returns:   DRM_SUCCESS on success
**********************************************************************************************/
DRM_API DRM_RESULT DRM_CALL _Oem_Aes_EncryptOneBlockWithKeyDerivedFromSalt_LWIDIA_HS(
    __inout_bcount ( DRM_AES_KEYSIZE_128 )       DRM_BYTE   f_rgbBuffer [ DRM_AES_KEYSIZE_128 ],
    __in_ecount    ( DRM_AES_KEYSIZE_128 ) const DRM_BYTE   f_rgbSalt   [ DRM_AES_KEYSIZE_128 ],
    __in                                   const DRM_DWORD  f_fCryptoMode )
{
    DRM_RESULT dr = DRM_SUCCESS;

    falc_scp_trap(TC_INFINITY);

    // Load the SALT into SCP
    falc_trapped_dmwrite(falc_sethi_i((LwU32)f_rgbSalt, SCP_R2));
    falc_dmwait();

    // Load the HW secret into SCP
    falc_scp_secret(LW_PR_AES128_HW_SECRET_INDEX, SCP_R3);

    // Encrypt the SALT with HW key to generate the derived key
    falc_scp_key(SCP_R3);
    falc_scp_encrypt(SCP_R2, SCP_R1);

    //
    // When KDF mode is _GEN_OEM_BLOB_XXX_KEY, we need to encrypt the input data
    // with the derived key generated above.  In other cases, the derived key will be
    // returned directly.
    //
    if (FLD_TEST_DRF(_PR_AES128, _KDF, _MODE, _GEN_OEM_BLOB_ENC_KEY, f_fCryptoMode) ||
        FLD_TEST_DRF(_PR_AES128, _KDF, _MODE, _GEN_OEM_BLOB_SIGN_KEY, f_fCryptoMode))
    {
        falc_scp_mov(SCP_R1, SCP_R3);
        falc_trapped_dmwrite(falc_sethi_i((LwU32)f_rgbBuffer, SCP_R2));
        falc_dmwait();

        falc_scp_key(SCP_R3);
        falc_scp_encrypt(SCP_R2, SCP_R1);
    }

    // Fetch the result from SCP
    falc_trapped_dmread(falc_sethi_i((LwU32)f_rgbBuffer, SCP_R1));
    falc_dmwait();

    // Zero out SCP registers we used for encrypt/decrypt operation
    falc_scp_xor(SCP_R1, SCP_R1);
    falc_scp_xor(SCP_R2, SCP_R2);
    falc_scp_xor(SCP_R3, SCP_R3);

    // Reset scp trap
    falc_scp_trap(0);

    return dr;
}

/*********************************************************************************************
** Function:  Oem_Aes_EncryptDecryptOneBlock_LWIDIA_LS
**
** Synopsis:  Encrypt/Decrypt a single block with the supplied key in LS mode
**
** Arguments:
**            [f_rgbOutput]   : Returns the encrypted/decrypted data
**            [f_rgbInput]    : Specifies the data to encrypt/decrypt
**            [f_rgbKey]      : The supplied key to be used for encryption/descryption
**            [f_fCryptoMode] : Specifies if encryption or decryption is requested and what KDF mode is requested
**
** Returns:   DRM_SUCCESS on success
**            or DRM_E_OEM_ILWALID_AES_CRYPTO_MODE if KDF mode is requested which shouldn't happen with SW key
**            or other failure code based on the return value of the called functions.
**********************************************************************************************/
DRM_BYTE g_prAesBuffer[DRM_AES_KEYSIZE_128] GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT) = {0};
DRM_BYTE g_prAesKey   [DRM_AES_KEYSIZE_128] GCC_ATTRIB_ALIGN(SCP_BUF_ALIGNMENT) = {0};
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_EncryptDecryptOneBlock_LWIDIA_LS(
    __out_bcount( DRM_AES_KEYSIZE_128 )       DRM_BYTE   f_rgbOutput[ DRM_AES_KEYSIZE_128 ],
    __in_bcount ( DRM_AES_KEYSIZE_128 ) const DRM_BYTE   f_rgbInput [ DRM_AES_KEYSIZE_128 ],
    __in_ecount ( DRM_AES_KEYSIZE_128 ) const DRM_BYTE   f_rgbKey   [ DRM_AES_KEYSIZE_128 ],
    __in                                const DRM_DWORD  f_fCryptoMode )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr = DRM_SUCCESS;

    ChkArg( f_rgbOutput != NULL );
    ChkArg( f_rgbInput  != NULL );
    ChkArg( f_rgbKey    != NULL );

    // KDF mode can only happen with HW key which will not work in LS mode
    if (!FLD_TEST_DRF(_PR_AES128, _KDF, _MODE, _NONE, f_fCryptoMode))
    {
        ChkDR(DRM_E_OEM_ILWALID_AES_CRYPTO_MODE);
    }

    // Copy key into the local aligned buffer
    ChkVOID( OEM_SELWRE_MEMCPY( g_prAesKey, f_rgbKey, DRM_AES_KEYSIZE_128 ) );

    // Copy input data into the local aligned buffer
    ChkVOID( OEM_SELWRE_MEMCPY( g_prAesBuffer, f_rgbInput, DRM_AES_KEYSIZE_128 ) );

    // Provide the keys for low level HW AES operation
    ChkDR( _Oem_Aes_EncryptDecryptOneBlockWithSuppliedKey_LWIDIA_LS( g_prAesBuffer, g_prAesKey, f_fCryptoMode ) );

    // Copy result to the output parameter
    ChkVOID( OEM_SELWRE_MEMCPY( f_rgbOutput, g_prAesBuffer, DRM_AES_KEYSIZE_128 ) );

ErrorExit:
    // Zero out the buffers
    ChkVOID( OEM_SELWRE_MEMSET( g_prAesBuffer, 0, DRM_AES_KEYSIZE_128 ) );
    ChkVOID( OEM_SELWRE_MEMSET( g_prAesKey, 0, DRM_AES_KEYSIZE_128 ) );

    return dr;
}

/*********************************************************************************************
** Function:  Oem_Aes_EncryptOneBlockWithKeyForKDFMode_LWIDIA_HS
**
** Synopsis:  Encrypt a single block with the derived key in HS mode based on the KDF mode requested
**            (HW key will be used)
**
** Arguments:
**            [f_rgbBuffer]   : Buffer storing the data for encryption and returning the encrypted data
**            [f_fCryptoMode] : Specifies what KDF mode is requested
**
** Returns:   DRM_SUCCESS on success
**            or DRM_E_OEM_ILWALID_AES_CRYPTO_MODE if KDF mode is not requested which shouldn't happen in HS mode
**            or DRM_E_OEM_UNSUPPORTED_KDF if the requested KDF mode is not supported
**            or other failure code based on the return value of the called functions.
**********************************************************************************************/
DRM_NO_INLINE DRM_API DRM_RESULT DRM_CALL Oem_Aes_EncryptOneBlockWithKeyForKDFMode_LWIDIA_HS(
    __inout_bcount( DRM_AES_KEYSIZE_128 )       DRM_BYTE   f_rgbBuffer[ DRM_AES_KEYSIZE_128 ],
    __in                                  const DRM_DWORD  f_fCryptoMode )
{
    CLAW_AUTO_RANDOM_CIPHER
    DRM_RESULT dr      = DRM_SUCCESS;
    DRM_BYTE   kdfMode = DRF_VAL(_PR_AES128, _KDF, _MODE, f_fCryptoMode);

    ChkArg( f_rgbBuffer != NULL );

    // KDF mode should always be requested in HS mode to protect the HW key
    if (FLD_TEST_DRF(_PR_AES128, _KDF, _MODE, _NONE, f_fCryptoMode))
    {
        ChkDR(DRM_E_OEM_ILWALID_AES_CRYPTO_MODE);
    }

    // Validate the signature of libMemHs before calling functions on this overlay
    OS_SEC_HS_LIB_VALIDATE(libMemHs, HASH_SAVING_DISABLE);

    //
    // Derived key will be generated by Enc(HW key, SALT),
    // where SALT = (64bits PDI | 32bits KDF constant | 32bits TK ID)
    //
    ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PRIV_PDI_0, ((DRM_DWORD*)g_prAesKey))));
    ChkDR(OEM_TEE_FlcnStatusToDrmStatus_HS(BAR0_REG_RD32_HS_ERRCHK(LW_FUSE_OPT_PRIV_PDI_1, ((DRM_DWORD*)g_prAesKey + 1))));

    // Sanitize the read PDI value
    if ((*((DRM_DWORD*)g_prAesKey) == 0) && (*((DRM_DWORD*)g_prAesKey + 1) == 0))
    {
        ChkDR( DRM_E_OEM_ILWALID_PDI );
    }

    *( (DRM_DWORD*)g_prAesKey + 2 ) = (LW_PR_AES128_KDF_SALT_2ND_DWORD ^ kdfMode);

    switch (kdfMode)
    {
        case LW_PR_AES128_KDF_MODE_GEN_TK:
        {
            *( (DRM_DWORD*)g_prAesKey + 3 ) = DRF_VAL(_PR_AES128, _TK, _ID, f_fCryptoMode);
            break;
        }
        case LW_PR_AES128_KDF_MODE_GEN_OEM_BLOB_ENC_KEY:
        case LW_PR_AES128_KDF_MODE_GEN_OEM_BLOB_SIGN_KEY:
        {
            *( (DRM_DWORD*)g_prAesKey + 3 ) = DRM_TEE_LWRRENT_TK_ID;

            // Copy input data into the local aligned buffer
            memcpy_hs( g_prAesBuffer, f_rgbBuffer, DRM_AES_KEYSIZE_128 );
            break;
        }
        default:
        {
            ChkDR( DRM_E_OEM_UNSUPPORTED_KDF );
            break;
        }
    }

    // Provide the input buffer and the SALT for low level HW AES operation to generate the derived key
    ChkDR( _Oem_Aes_EncryptOneBlockWithKeyDerivedFromSalt_LWIDIA_HS( g_prAesBuffer, g_prAesKey, f_fCryptoMode ) );

    // Copy result to the output parameter
    memcpy_hs( f_rgbBuffer, g_prAesBuffer, DRM_AES_KEYSIZE_128 );

ErrorExit:
    // Zero out the buffers
    memset_hs( g_prAesBuffer, 0, DRM_AES_KEYSIZE_128 );
    memset_hs( g_prAesKey, 0, DRM_AES_KEYSIZE_128 );

    return dr;
}
#endif
EXIT_PK_NAMESPACE_CODE;
