/**@@@+++@@@@******************************************************************
**
** Microsoft (r) PlayReady (r)
** Copyright (c) Microsoft Corporation. All rights reserved.
**
***@@@---@@@@******************************************************************
*/

#ifndef __OEMBYTEORDER_H__
#define __OEMBYTEORDER_H__

#include <drmmathsafe.h>
#include <drmpragmas.h>

/*
** Begin: Ideal implementations of DRM_BYTE array colwersions.
** The DRM_BYTE arrays are in big or little endian format.
** They are colwerted to/from DRM_WORD / DRM_DWORD / DRM_UINT64
** in native endian format.
*/

#define DRM_BIG_ENDIAN_BYTES_TO_NATIVE_WORD( to_native_word, from_big_endian_bytes ) DRM_DO {           \
    const DRM_BYTE *_pb = (const DRM_BYTE*)(from_big_endian_bytes);                                     \
    (to_native_word) = (DRM_WORD)                                                                       \
        ( (DRM_WORD)( ((DRM_WORD)(_pb[0])) << (DRM_WORD)8 )                                             \
        | (DRM_WORD)( (_pb[1]) ) );                                                                     \
} DRM_WHILE_FALSE

#define DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_WORD( to_native_word, from_little_endian_bytes ) DRM_DO {     \
    const DRM_BYTE *_pb = (const DRM_BYTE*)(from_little_endian_bytes);                                  \
    (to_native_word) = (DRM_WORD)                                                                       \
        ( (DRM_WORD)( (_pb[0]) )                                                                        \
        | (DRM_WORD)( ((DRM_WORD)(_pb[1])) << (DRM_WORD)8 ) );                                          \
} DRM_WHILE_FALSE

#define DRM_NATIVE_WORD_TO_BIG_ENDIAN_BYTES( to_big_endian_bytes, from_native_word ) DRM_DO {           \
    DRM_WORD   _w  = (from_native_word);                                                                \
    DRM_BYTE  *_pb = (DRM_BYTE*)(to_big_endian_bytes);                                                  \
    _pb[0] = (DRM_BYTE)( _w >> (DRM_WORD)8 );                                                           \
    _pb[1] = (DRM_BYTE)( _w                );                                                           \
} DRM_WHILE_FALSE

#define DRM_NATIVE_WORD_TO_LITTLE_ENDIAN_BYTES( to_little_endian_bytes, from_native_word ) DRM_DO {     \
    DRM_WORD   _w  = (from_native_word);                                                                \
    DRM_BYTE  *_pb = (DRM_BYTE*)(to_little_endian_bytes);                                               \
    _pb[0] = (DRM_BYTE)( _w                );                                                           \
    _pb[1] = (DRM_BYTE)( _w >> (DRM_WORD)8 );                                                           \
} DRM_WHILE_FALSE

#define DRM_BIG_ENDIAN_BYTES_TO_NATIVE_DWORD( to_native_dword, from_big_endian_bytes ) DRM_DO {         \
    const DRM_BYTE *_pb = (const DRM_BYTE*)(from_big_endian_bytes);                                     \
    (to_native_dword) = (DRM_DWORD)                                                                     \
        ( (DRM_DWORD)( ((DRM_DWORD)(_pb[0])) << (DRM_DWORD)24 )                                         \
        | (DRM_DWORD)( ((DRM_DWORD)(_pb[1])) << (DRM_DWORD)16 )                                         \
        | (DRM_DWORD)( ((DRM_DWORD)(_pb[2])) << (DRM_DWORD) 8 )                                         \
        | (DRM_DWORD)( (_pb[3]) ) );                                                                    \
} DRM_WHILE_FALSE

#define DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_DWORD( to_native_dword, from_little_endian_bytes ) DRM_DO {   \
    const DRM_BYTE *_pb = (const DRM_BYTE*)(from_little_endian_bytes);                                  \
    (to_native_dword) = (DRM_DWORD)                                                                     \
        ( (DRM_DWORD)( (_pb[0]) )                                                                       \
        | (DRM_DWORD)( ((DRM_DWORD)(_pb[1])) << (DRM_DWORD) 8 )                                         \
        | (DRM_DWORD)( ((DRM_DWORD)(_pb[2])) << (DRM_DWORD)16 )                                         \
        | (DRM_DWORD)( ((DRM_DWORD)(_pb[3])) << (DRM_DWORD)24 ) );                                      \
} DRM_WHILE_FALSE

#define DRM_NATIVE_DWORD_TO_BIG_ENDIAN_BYTES( to_big_endian_bytes, from_native_dword ) DRM_DO {         \
    DRM_DWORD  _dw = (from_native_dword);                                                               \
    DRM_BYTE  *_pb = (DRM_BYTE*)(to_big_endian_bytes);                                                  \
    _pb[0] = (DRM_BYTE)( _dw >> (DRM_DWORD)24 );                                                        \
    _pb[1] = (DRM_BYTE)( _dw >> (DRM_DWORD)16 );                                                        \
    _pb[2] = (DRM_BYTE)( _dw >> (DRM_DWORD) 8 );                                                        \
    _pb[3] = (DRM_BYTE)( _dw                  );                                                        \
} DRM_WHILE_FALSE

#define DRM_NATIVE_DWORD_TO_LITTLE_ENDIAN_BYTES( to_little_endian_bytes, from_native_dword ) DRM_DO {   \
    DRM_DWORD  _dw = (from_native_dword);                                                               \
    DRM_BYTE  *_pb = (DRM_BYTE*)(to_little_endian_bytes);                                               \
    _pb[0] = (DRM_BYTE)( _dw                  );                                                        \
    _pb[1] = (DRM_BYTE)( _dw >> (DRM_DWORD) 8 );                                                        \
    _pb[2] = (DRM_BYTE)( _dw >> (DRM_DWORD)16 );                                                        \
    _pb[3] = (DRM_BYTE)( _dw >> (DRM_DWORD)24 );                                                        \
} DRM_WHILE_FALSE

#if DRM_SUPPORT_NATIVE_64BIT_TYPES

#define DRM_BIG_ENDIAN_BYTES_TO_NATIVE_QWORD( to_native_qword, from_big_endian_bytes ) DRM_DO {         \
    const DRM_BYTE *_pb = (const DRM_BYTE*)(from_big_endian_bytes);                                     \
    (to_native_qword) = (DRM_UINT64)                                                                    \
        ( (DRM_UINT64)( ((DRM_UINT64)(_pb[0])) << (DRM_UINT64)56 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[1])) << (DRM_UINT64)48 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[2])) << (DRM_UINT64)40 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[3])) << (DRM_UINT64)32 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[4])) << (DRM_UINT64)24 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[5])) << (DRM_UINT64)16 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[6])) << (DRM_UINT64) 8 )                                      \
        | (DRM_UINT64)( (_pb[7]) ) );                                                                   \
} DRM_WHILE_FALSE

#define DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_QWORD( to_native_qword, from_little_endian_bytes ) DRM_DO {   \
    const DRM_BYTE *_pb = (const DRM_BYTE*)(from_little_endian_bytes);                                  \
    (to_native_qword) = (DRM_UINT64)                                                                    \
        ( (DRM_UINT64)( (_pb[0]) )                                                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[1])) << (DRM_UINT64) 8 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[2])) << (DRM_UINT64)16 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[3])) << (DRM_UINT64)24 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[4])) << (DRM_UINT64)32 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[5])) << (DRM_UINT64)40 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[6])) << (DRM_UINT64)48 )                                      \
        | (DRM_UINT64)( ((DRM_UINT64)(_pb[7])) << (DRM_UINT64)56 ) );                                   \
} DRM_WHILE_FALSE

#define DRM_NATIVE_QWORD_TO_BIG_ENDIAN_BYTES( to_big_endian_bytes, from_native_qword ) DRM_DO {         \
    DRM_UINT64 _qw = (from_native_qword);                                                               \
    DRM_BYTE  *_pb = (DRM_BYTE*)(to_big_endian_bytes);                                                  \
    _pb[0] = (DRM_BYTE)( _qw >> (DRM_UINT64)56 );                                                       \
    _pb[1] = (DRM_BYTE)( _qw >> (DRM_UINT64)48 );                                                       \
    _pb[2] = (DRM_BYTE)( _qw >> (DRM_UINT64)40 );                                                       \
    _pb[3] = (DRM_BYTE)( _qw >> (DRM_UINT64)32 );                                                       \
    _pb[4] = (DRM_BYTE)( _qw >> (DRM_UINT64)24 );                                                       \
    _pb[5] = (DRM_BYTE)( _qw >> (DRM_UINT64)16 );                                                       \
    _pb[6] = (DRM_BYTE)( _qw >> (DRM_UINT64) 8 );                                                       \
    _pb[7] = (DRM_BYTE)( _qw                   );                                                       \
} DRM_WHILE_FALSE

#define DRM_NATIVE_QWORD_TO_LITTLE_ENDIAN_BYTES( to_little_endian_bytes, from_native_qword ) DRM_DO {   \
    DRM_UINT64 _qw = (from_native_qword);                                                               \
    DRM_BYTE  *_pb = (DRM_BYTE*)(to_little_endian_bytes);                                               \
    _pb[0] = (DRM_BYTE)( _qw                   );                                                       \
    _pb[1] = (DRM_BYTE)( _qw >> (DRM_UINT64) 8 );                                                       \
    _pb[2] = (DRM_BYTE)( _qw >> (DRM_UINT64)16 );                                                       \
    _pb[3] = (DRM_BYTE)( _qw >> (DRM_UINT64)24 );                                                       \
    _pb[4] = (DRM_BYTE)( _qw >> (DRM_UINT64)32 );                                                       \
    _pb[5] = (DRM_BYTE)( _qw >> (DRM_UINT64)40 );                                                       \
    _pb[6] = (DRM_BYTE)( _qw >> (DRM_UINT64)48 );                                                       \
    _pb[7] = (DRM_BYTE)( _qw >> (DRM_UINT64)56 );                                                       \
} DRM_WHILE_FALSE

#else   /* DRM_SUPPORT_NATIVE_64BIT_TYPES */

#define DRM_BIG_ENDIAN_BYTES_TO_NATIVE_QWORD( to_native_qword, from_big_endian_bytes ) DRM_DO {         \
    const DRM_BYTE *_pb2 = (const DRM_BYTE*)(from_big_endian_bytes);                                    \
    DRM_DWORD _dwhigh;                                                                                  \
    DRM_DWORD _dwlow;                                                                                   \
    DRM_BIG_ENDIAN_BYTES_TO_NATIVE_DWORD( _dwhigh, &(_pb2[0]) );                                        \
    DRM_BIG_ENDIAN_BYTES_TO_NATIVE_DWORD( _dwlow,  &(_pb2[4]) );                                        \
    (to_native_qword) = DRM_UI64HL( _dwhigh, _dwlow );                                                  \
} DRM_WHILE_FALSE

#define DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_QWORD( to_native_qword, from_little_endian_bytes ) DRM_DO {   \
    const DRM_BYTE *_pb2 = (const DRM_BYTE*)(from_little_endian_bytes);                                 \
    DRM_DWORD _dwhigh;                                                                                  \
    DRM_DWORD _dwlow;                                                                                   \
    DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_DWORD( _dwlow,  &(_pb2[0]) );                                     \
    DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_DWORD( _dwhigh, &(_pb2[4]) );                                     \
    (to_native_qword) = DRM_UI64HL( _dwhigh, _dwlow );                                                  \
} DRM_WHILE_FALSE

#define DRM_NATIVE_QWORD_TO_BIG_ENDIAN_BYTES( to_big_endian_bytes, from_native_qword ) DRM_DO {         \
    DRM_UINT64 _qw = (from_native_qword);                                                               \
    DRM_BYTE  *_pb2 = (DRM_BYTE*)(to_big_endian_bytes);                                                 \
    DRM_NATIVE_DWORD_TO_BIG_ENDIAN_BYTES( &_pb2[0], DRM_UI64High32( _qw ) );                            \
    DRM_NATIVE_DWORD_TO_BIG_ENDIAN_BYTES( &_pb2[4], DRM_UI64Low32 ( _qw ) );                            \
} DRM_WHILE_FALSE

#define DRM_NATIVE_QWORD_TO_LITTLE_ENDIAN_BYTES( to_little_endian_bytes, from_native_qword ) DRM_DO {   \
    DRM_UINT64 _qw = (from_native_qword);                                                               \
    DRM_BYTE  *_pb2 = (DRM_BYTE*)(to_big_endian_bytes);                                                 \
    DRM_NATIVE_DWORD_TO_LITTLE_ENDIAN_BYTES( &_pb2[0], DRM_UI64Low32 ( _qw ) );                         \
    DRM_NATIVE_DWORD_TO_LITTLE_ENDIAN_BYTES( &_pb2[4], DRM_UI64High32( _qw ) );                         \
} DRM_WHILE_FALSE

#endif  /* DRM_SUPPORT_NATIVE_64BIT_TYPES */

/*
** End: Ideal implementations of byte array colwersions.
*/

/*
** Legacy macros that colwert DRM_BYTE arrays in little endian format to/from
** DRM_WORD / DRM_DWORD / DRM_UINT64 in native endian format.
*/
#define BYTES_TO_WORD(  word,  byte  )   DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_WORD(  (word ), (byte ) )
#define WORD_TO_BYTES(  byte,  word  )   DRM_NATIVE_WORD_TO_LITTLE_ENDIAN_BYTES(  (byte ), (word ) )
#define BYTES_TO_DWORD( dword, byte  )   DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_DWORD( (dword), (byte ) )
#define DWORD_TO_BYTES( byte,  dword )   DRM_NATIVE_DWORD_TO_LITTLE_ENDIAN_BYTES( (byte ), (dword) )
#define BYTES_TO_QWORD( qword, byte  )   DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_QWORD( (qword), (byte ) )
#define QWORD_TO_BYTES( byte,  qword )   DRM_NATIVE_QWORD_TO_LITTLE_ENDIAN_BYTES( (byte ), (qword) )

#define BITS_PER_STANDARD_BYTE 8

#define ROTATE_LEFT_WORD(x, n)   ((DRM_WORD)  (((DRM_WORD)  (((x) << (n)) & 0xFFFF)) | ((DRM_WORD)  (x) >> (16-(n)))))
#define ROTATE_RIGHT_WORD(x, n)  ((DRM_WORD)  (((DRM_WORD)  (x) >> (n)) | ((DRM_WORD)  (((x) << (16-(n))) & 0xFFFF))))
#define ROTATE_LEFT_DWORD(x, n)  ((DRM_DWORD) (((DRM_DWORD) (((x) << (n)) & 0xFFFFFFFF)) | ((DRM_DWORD) (x) >> (32-(n)))))
#define ROTATE_RIGHT_DWORD(x, n) ((DRM_DWORD) (((DRM_DWORD) (x) >> (n)) | ((DRM_DWORD) (((x) << (32-(n))) & 0xFFFFFFFF))))
#if DRM_SUPPORT_NATIVE_64BIT_TYPES
#define ROTATE_LEFT_QWORD(x, n)  ((DRM_UINT64)(((DRM_UINT64)(x) << (n)) | ((DRM_UINT64)(x) >> (64-(n)))))
#define ROTATE_RIGHT_QWORD(x, n) ((DRM_UINT64)(((DRM_UINT64)(x) >> (n)) | ((DRM_UINT64)(x) << (64-(n)))))
#endif /* DRM_SUPPORT_NATIVE_64BIT_TYPES */

#define ROTATE_LEFT(x, n)  (((x) << (n)) | ((x) >> (32-(n))))
#define ROTATE_RIGHT(x, n) (((x) >> (n)) | ((x) << (32-(n))))

#define FLIP_WORD_BYTES(w) ( ROTATE_LEFT_WORD((DRM_WORD)(w), 8) )

/* for initialization of constant values, we define special macro for flipping bytes in DRM_DWORD */
#define FLIP_DWORD_BYTES_CONST(dw) ((ROTATE_RIGHT((dw), 24) & 0x00ff00ff) | (ROTATE_RIGHT((dw), 8) & 0xff00ff00))

#if DRM_SUPPORT_NATIVE_64BIT_TYPES
#if DRM_BUILD_PROFILE == DRM_BUILD_PROFILE_IOS
#define FLIP_DWORD_BYTES(dw) __builtin_bswap32(dw)
#define FLIP_QWORD_BYTES(qw) __builtin_bswap64(qw)
#elif (defined(_M_IX86) || defined(_M_AMD64) || defined(_XBOX)) && !defined(_MANAGED)
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
__checkReturn unsigned long  __cdecl _byteswap_ulong (__in unsigned long _Long);
__checkReturn unsigned __int64 __cdecl _byteswap_uint64(__in unsigned __int64 _Int64);
#ifdef __cplusplus
} /* extern "C"*/
#endif /* __cplusplus */
PRAGMA_INTRINSIC(_byteswap_ulong)
PRAGMA_INTRINSIC(_byteswap_uint64)
#define FLIP_DWORD_BYTES(dw) ((DRM_DWORD)_byteswap_ulong((unsigned long)(dw)))
#define FLIP_QWORD_BYTES(qw) ((DRM_UINT64)_byteswap_uint64((unsigned __int64)(qw)))
#else
#define FLIP_DWORD_BYTES(dw) FLIP_DWORD_BYTES_CONST(dw)
#define FLIP_QWORD_BYTES(qw) ( FLIP_DWORD_BYTES(qw>>32) | ( ( (DRM_UINT64) FLIP_DWORD_BYTES((DRM_DWORD)qw) ) << 32 ) )
#endif
#else /* DRM_SUPPORT_NATIVE_64BIT_TYPES */
#define FLIP_DWORD_BYTES(dw) FLIP_DWORD_BYTES_CONST(dw)
#define FLIP_QWORD_BYTES(qw) DRM_UI64HL(FLIP_DWORD_BYTES(DRM_UI64Low32(qw)),FLIP_DWORD_BYTES(DRM_UI64High32(qw)))
#endif /* DRM_SUPPORT_NATIVE_64BIT_TYPES */

#define WORDSWAP(d) ROTATE_LEFT(d, 16)

/*
** Legacy macros that colwert DRM_BYTE arrays in little endian format to/from
** DRM_WORD / DRM_DWORD / DRM_UINT64 in native endian format.
*/
#define BYTES2WORD_LE(  word,  byte  )      DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_WORD(  (word ), (byte ) )
#define WORD2BYTES_LE(  byte,  word  )      DRM_NATIVE_WORD_TO_LITTLE_ENDIAN_BYTES(  (byte ), (word ) )
#define BYTES2DWORD_LE( dword, byte  )      DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_DWORD( (dword), (byte ) )
#define DWORD2BYTES_LE( byte,  dword )      DRM_NATIVE_DWORD_TO_LITTLE_ENDIAN_BYTES( (byte ), (dword) )
#define BYTES2QWORD_LE( qword, byte  )      DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_QWORD( (qword), (byte ) )
#define QWORD2BYTES_LE( byte,  qword )      DRM_NATIVE_QWORD_TO_LITTLE_ENDIAN_BYTES( (byte ), (qword) )

#define REVERSE_BYTES_WORD(w)               DRM_DO { (w)  = FLIP_WORD_BYTES(w);   } DRM_WHILE_FALSE
#define REVERSE_BYTES_DWORD(dw)             DRM_DO { (dw) = FLIP_DWORD_BYTES(dw); } DRM_WHILE_FALSE
#define REVERSE_BYTES_LONG(l)               DRM_DO { (l) = (DRM_LONG)FLIP_DWORD_BYTES((DRM_DWORD)l); } DRM_WHILE_FALSE
#define REVERSE_BYTES_QWORD(qw)             DRM_DO { (qw) = FLIP_QWORD_BYTES(qw); } DRM_WHILE_FALSE

/*
** Legacy macros that colwert DRM_BYTE arrays in little endian format to
** DRM_WORD / DRM_DWORD / DRM_UINT64 in native endian format.
*/
#define LITTLEENDIAN_BYTES_TO_WORD(  word,  byte, index )   DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_WORD(  (word) , &((const DRM_BYTE*)(byte))[(index)] )
#define LITTLEENDIAN_BYTES_TO_DWORD( dword, byte, index )   DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_DWORD( (dword), &((const DRM_BYTE*)(byte))[(index)] )
#define LITTLEENDIAN_BYTES_TO_QWORD( qword, byte, index )   DRM_LITTLE_ENDIAN_BYTES_TO_NATIVE_QWORD( (qword), &((const DRM_BYTE*)(byte))[(index)] )

/*
** Legacy macros that colwert DRM_BYTE arrays in big endian format to
** DRM_WORD / DRM_DWORD / DRM_UINT64 in native endian format.
*/
#define NETWORKBYTES_TO_WORD(word, byte, index)     DRM_BIG_ENDIAN_BYTES_TO_NATIVE_WORD(  (word ), &((DRM_BYTE*)(byte))[(index)] )
#define NETWORKBYTES_TO_DWORD(dword, byte, index)   DRM_BIG_ENDIAN_BYTES_TO_NATIVE_DWORD( (dword), &((DRM_BYTE*)(byte))[(index)] )
#define NETWORKBYTES_TO_QWORD(qword, byte, index)   DRM_BIG_ENDIAN_BYTES_TO_NATIVE_QWORD( (qword), &((DRM_BYTE*)(byte))[(index)] )

#define NETWORKBYTES_FROMBUFFER_TO_WORD(word, byte, index, buffersize)   DRM_DO {       \
    DRM_DWORD __dwSpaceRequired=0;                                                      \
    ChkDR( DRM_DWordAdd( index, sizeof(DRM_WORD), &__dwSpaceRequired ) );               \
    ChkBOOL( __dwSpaceRequired  <= (buffersize), DRM_E_BUFFERTOOSMALL );                \
    __analysis_assume( index + sizeof( DRM_WORD ) <= buffersize );                      \
    NETWORKBYTES_TO_WORD( ( word ), ( byte ), ( index ) );                              \
    (index) = (__dwSpaceRequired);                                                      \
} DRM_WHILE_FALSE

#define NETWORKBYTES_FROMBUFFER_TO_DWORD(dword, byte, index, buffersize) DRM_DO {       \
    DRM_DWORD __dwSpaceRequired=0;                                                      \
    ChkDR( DRM_DWordAdd( index, sizeof(DRM_DWORD), &__dwSpaceRequired ) );              \
    ChkBOOL( __dwSpaceRequired <= (buffersize), DRM_E_BUFFERTOOSMALL );                 \
    __analysis_assume( index + sizeof( DRM_DWORD ) <= buffersize );                     \
    NETWORKBYTES_TO_DWORD( (dword), (byte), (index) );                                  \
    (index) = (__dwSpaceRequired);                                                      \
} DRM_WHILE_FALSE

/*
** Legacy macros that colwert DRM_BYTE arrays in big endian format *from*
** DRM_WORD / DRM_DWORD / DRM_UINT64 in native endian format.
*/
#define WORD_TO_NETWORKBYTES(byte_ptr, index, word)     DRM_NATIVE_WORD_TO_BIG_ENDIAN_BYTES(  &((DRM_BYTE*)(byte_ptr))[(index)], word  )
#define DWORD_TO_NETWORKBYTES(byte_ptr, index, dword)   DRM_NATIVE_DWORD_TO_BIG_ENDIAN_BYTES( &((DRM_BYTE*)(byte_ptr))[(index)], dword )
#define QWORD_TO_NETWORKBYTES(byte_ptr, index, qword)   DRM_NATIVE_QWORD_TO_BIG_ENDIAN_BYTES( &((DRM_BYTE*)(byte_ptr))[(index)], qword )

/*
** Legacy macros that colwert DRM_BYTE arrays in little endian format from
** DRM_WORD / DRM_DWORD / DRM_UINT64 in native endian format.
*/
#define WORD_TO_LITTLEENDIAN_BYTES(byte, index, word)   DRM_NATIVE_WORD_TO_LITTLE_ENDIAN_BYTES(  &((DRM_BYTE*)(byte))[(index)], word  )
#define DWORD_TO_LITTLEENDIAN_BYTES(byte, index, dword) DRM_NATIVE_DWORD_TO_LITTLE_ENDIAN_BYTES( &((DRM_BYTE*)(byte))[(index)], dword )

#if TARGET_LITTLE_ENDIAN

#define MAKE_WORD_BIG_ENDIAN(w)         FLIP_WORD_BYTES(w)
#define MAKE_DWORD_BIG_ENDIAN(dw)       FLIP_DWORD_BYTES(dw)
#define MAKE_DWORD_BIG_ENDIAN_CONST(dw) FLIP_DWORD_BYTES_CONST(dw)
#define MAKE_GUID_BIG_ENDIAN(guid)      DRM_DO {REVERSE_BYTES_DWORD((guid).Data1);REVERSE_BYTES_WORD((guid).Data2);REVERSE_BYTES_WORD((guid).Data3);} DRM_WHILE_FALSE

/*
** Note: Do *NOT* define MAKE_QWORD_BIG_ENDIAN.
** Due to non-native 64-bit type support, its semantics are confusing and it should not be defined/used.
*/

#define FIX_ENDIAN_WORD(w)
#define FIX_ENDIAN_DWORD(dw)
#define FIX_ENDIAN_LONG(l)
#define FIX_ENDIAN_QWORD(qw)
#define FIX_ENDIAN_GUID(guid)

#else /* TARGET_LITTLE_ENDIAN */

#define MAKE_WORD_BIG_ENDIAN(w)         (w)
#define MAKE_DWORD_BIG_ENDIAN(dw)       (dw)
#define MAKE_DWORD_BIG_ENDIAN_CONST(dw) (dw)
#define MAKE_GUID_BIG_ENDIAN(guid)      (guid)

/*
** Note: Do *NOT* define MAKE_QWORD_BIG_ENDIAN.  Refer to comments near earlier reference to MAKE_QWORD_BIG_ENDIAN.
*/

#define FIX_ENDIAN_WORD(w)   REVERSE_BYTES_WORD(w)
#define FIX_ENDIAN_DWORD(dw) REVERSE_BYTES_DWORD(dw)
#define FIX_ENDIAN_LONG(l)   REVERSE_BYTES_LONG(l)
#define FIX_ENDIAN_QWORD(qw) DRM_DO {REVERSE_BYTES_QWORD(qw); NONNATIVE64_TO_NATIVE64((qw));} DRM_WHILE_FALSE

#define FIX_ENDIAN_GUID(guid)                       DRM_DO {FIX_ENDIAN_DWORD((guid).Data1);FIX_ENDIAN_WORD((guid).Data2);FIX_ENDIAN_WORD((guid).Data3);} DRM_WHILE_FALSE

#endif /* TARGET_LITTLE_ENDIAN */

#if TARGET_LITTLE_ENDIAN
#define DRM_CHANGE_CASTED_DWORD_ENDIANNESS_TO_NATIVE_INPLACE(dw,isSrcLE) DRM_DO { if(!isSrcLE) { REVERSE_BYTES_DWORD(dw); } } DRM_WHILE_FALSE
#define DRM_CHANGE_CASTED_QWORD_ENDIANNESS_TO_NATIVE_INPLACE(qw,isSrcLE) DRM_DO { if(!isSrcLE) { REVERSE_BYTES_QWORD(qw); } } DRM_WHILE_FALSE
#define DRM_CHANGE_NATIVE_DWORD_ENDIANNESS_TO_CASTED_BIG_ENDIAN_INPLACE(dw) DRM_DO { REVERSE_BYTES_DWORD(dw); } DRM_WHILE_FALSE
#define DRM_CHANGE_NATIVE_QWORD_ENDIANNESS_TO_CASTED_BIG_ENDIAN_INPLACE(qw) DRM_DO { REVERSE_BYTES_QWORD(qw); } DRM_WHILE_FALSE
#else /* TARGET_LITTLE_ENDIAN */
#define DRM_CHANGE_CASTED_DWORD_ENDIANNESS_TO_NATIVE_INPLACE(dw,isSrcLE) DRM_DO { if(isSrcLE) { REVERSE_BYTES_DWORD(dw); } } DRM_WHILE_FALSE
#define DRM_CHANGE_CASTED_QWORD_ENDIANNESS_TO_NATIVE_INPLACE(qw,isSrcLE) DRM_DO { if(isSrcLE) { REVERSE_BYTES_QWORD(qw); } NONNATIVE64_TO_NATIVE64(qw); } DRM_WHILE_FALSE
#define DRM_CHANGE_NATIVE_DWORD_ENDIANNESS_TO_CASTED_BIG_ENDIAN_INPLACE(dw)
#define DRM_CHANGE_NATIVE_QWORD_ENDIANNESS_TO_CASTED_BIG_ENDIAN_INPLACE(qw) DRM_DO { NATIVE64_TO_NONNATIVE64(qw); } DRM_WHILE_FALSE
#endif /* TARGET_LITTLE_ENDIAN */

#define DRM_IS_INTEGER_POWER_OF_TW0( x )    (((x)&((x)-1))==0)

#define DRM_XOR( __pbLeft, __pbRight, __cb ) DRM_DO {   \
    DRM_DWORD __cbxor = (__cb);                         \
    DRM_BYTE *__pbxorL = ((DRM_BYTE*)(__pbLeft));       \
    DRM_BYTE *__pbxorR = ((DRM_BYTE*)(__pbRight));      \
    DRMASSERT( __cbxor > 0 );                           \
    DRMASSERT( __pbxorL != NULL );                      \
    DRMASSERT( __pbxorR != NULL );                      \
    while( __cbxor > 0 )                                \
    {                                                   \
        __cbxor--;                                      \
        __pbxorL[ __cbxor ] ^= __pbxorR[ __cbxor ];     \
    }                                                   \
} DRM_WHILE_FALSE

#endif /* __OEMBYTEORDER_H__ */

