/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#define CK_PTR *
#define CK_DECLARE_FUNCTION(returnType, name) returnType name
#define CK_DECLARE_FUNCTION_POINTER(returnType, name) returnType (* name)
#define CK_CALLBACK_FUNCTION(returnType, name) returnType (* name)

#include "ccsl/inc/pkcs11.h"

#include "aes.h"
#include "filters.h"
#include "gcm.h"
#include "hmac.h"
#include "sha.h"

#include <vector>

struct ccslContext 
{
    CryptoPP::FilterWithBufferedInput *pipe;
    unsigned char *keys[2];
    unsigned long keyLen;
    unsigned long numKeys;
};

static std::vector<ccslContext> ctxPool;
static std::vector<bool> ctxPoolUsed;

static bool initDone = 0;
static CK_FUNCTION_LIST functionList;

static CK_RV CCSL_MODS_Initialize(CK_VOID_PTR pInitArgs)
{
    return CKR_OK;
}

static CK_RV CCSL_MODS_Finalize(CK_VOID_PTR pIgnored)
{
    return CKR_OK;
}

static CK_RV CCSL_MODS_GetSlotList(
        CK_BBOOL       tokenPresent,  /* only slots with tokens? */
        CK_SLOT_ID_PTR pSlotList,     /* receives array of slot IDs */
        CK_ULONG_PTR   pulCount       /* receives number of slots */
        )
{
    // Nothing to do, we ignore all slot info anyway
    return CKR_OK;
}

static CK_RV CCSL_MODS_OpenSession(
        CK_SLOT_ID            slotID,        /* the slot's ID */
        CK_FLAGS              flags,         /* from CK_SESSION_INFO */
        CK_VOID_PTR           pApplication,  /* passed to callback */
        CK_NOTIFY             Notify,        /* callback function */
        CK_SESSION_HANDLE_PTR phSession      /* gets session handle */
        )
{
    // We happily ignore all the flags
    ccslContext newCtx;

    // Do we have unused allocated slot?
    size_t idx;
    for (idx = 0; idx < ctxPool.size(); ++idx)
    {
        if (ctxPoolUsed[idx] == false)
        {
            ctxPoolUsed[idx] = true;
            memset(&ctxPool[idx], 0, sizeof(ctxPool[idx]));
            *phSession = idx;
            return CKR_OK;
        }
    }

    // We don't, allocate new one
    memset(&newCtx, 0, sizeof(newCtx));
    ctxPool.push_back(newCtx);
    ctxPoolUsed.push_back(true);
    *phSession = idx;

    return CKR_OK;
}

static CK_RV CCSL_MODS_CloseSession(
  CK_SESSION_HANDLE hSession  /* the session's handle */
)
{
    ctxPoolUsed[hSession] = false;
    return CKR_OK;
}


/* C_CreateObject creates a new object. */
static CK_RV CCSL_MODS_CreateObject
(
  CK_SESSION_HANDLE hSession,    /* the session's handle */
  CK_ATTRIBUTE_PTR  pTemplate,   /* the object's template */
  CK_ULONG          ulCount,     /* attributes in template */
  CK_OBJECT_HANDLE_PTR phObject  /* gets new object's handle. */
)
{
    ccslContext *ctx = &ctxPool[hSession];
    uint32_t attr;

    for (attr=0; attr < ulCount; ++attr)
    {
        switch (pTemplate[attr].type)
        {
            case CKA_VALUE:
                if (ctx->numKeys == 0)
                {
                    ctx->keys[0] = (unsigned char *) pTemplate[attr].pValue;
                    ctx->keyLen = pTemplate[attr].ulValueLen;
                    ctx->numKeys = 1;
                    *phObject = 0;
                }
                else if (ctx->numKeys == 1)
                {
                    ctx->keys[1] = (unsigned char *) pTemplate[attr].pValue;
                    ctx->numKeys = 2;
                    *phObject = 1;
                }
                else
                {
                    return CKR_FUNCTION_FAILED; 
                }
                break;

            case CKA_CLASS: // Should be CKO_SECRET_KEY 
            case CKA_KEY_TYPE: // Should be CKK_AES
            default:
                break;
        }
    }
    return CKR_OK;
}

/* C_DestroyObject destroys an object. */
static CK_RV CCSL_MODS_DestroyObject
(
  CK_SESSION_HANDLE hSession,  /* the session's handle */
  CK_OBJECT_HANDLE  hObject    /* the object's handle */
)
{
    return CKR_OK;
}


/* C_EncryptInit initializes an encryption operation. */
static CK_RV CCSL_MODS_EncryptInit
(
 CK_SESSION_HANDLE hSession,    /* the session's handle */
 CK_MECHANISM_PTR  pMechanism,  /* the encryption mechanism */
 CK_OBJECT_HANDLE  hKey         /* handle of encryption key */
 )
{
    ccslContext *ctx = &ctxPool[hSession];
    unsigned char const *keyStr = ctx->keys[hKey];
    
    if(pMechanism->mechanism == CKM_AES_GCM)
    {
        CK_GCM_PARAMS *params = reinterpret_cast<CK_GCM_PARAMS *> (pMechanism->pParameter);
        unsigned char *iv = params->pIv;

        CryptoPP::SecByteBlock key(keyStr, 32);

        CryptoPP::GCM<CryptoPP::AES, CryptoPP::GCM_64K_Tables>::Encryption *gcmEnc;
        gcmEnc = new CryptoPP::GCM<CryptoPP::AES, CryptoPP::GCM_64K_Tables>::Encryption;
        if(gcmEnc == NULL)
            return CKR_GENERAL_ERROR;
        gcmEnc->SetKeyWithIV(key, key.size(), iv);
        ctx->pipe = new CryptoPP::AuthenticatedEncryptionFilter(*gcmEnc, new CryptoPP::ByteQueue(), false, 16);
    }
    else
    {
        CK_AES_CTR_PARAMS *params = reinterpret_cast<CK_AES_CTR_PARAMS *> (pMechanism->pParameter);
        unsigned char *iv = params->cb;

        CryptoPP::SecByteBlock key(keyStr, CryptoPP::AES::DEFAULT_KEYLENGTH);
        static_assert(CryptoPP::AES::DEFAULT_KEYLENGTH == 16, "Unexpected key length");

        CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption *ctrEnc;
        ctrEnc = new CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption;
        if(ctrEnc == NULL)
            return CKR_GENERAL_ERROR;
        ctrEnc->SetKeyWithIV(key, key.size(), iv);
        ctx->pipe = new CryptoPP::StreamTransformationFilter(*ctrEnc, new CryptoPP::ByteQueue());
    }

    return CKR_OK;
}


/* C_EncryptUpdate continues a multiple-part encryption operation. */
static CK_RV CCSL_MODS_EncryptUpdate
(
 CK_SESSION_HANDLE hSession,           /* session's handle */
 CK_BYTE_PTR       pPart,              /* the plaintext data */
 CK_ULONG          ulPartLen,          /* plaintext data len */
 CK_BYTE_PTR       pEncryptedPart,     /* gets ciphertext */
 CK_ULONG_PTR      pulEncryptedPartLen /* gets c-text size */
 )
{
    ccslContext *ctx = &ctxPool[hSession];
    ctx->pipe->Put(pPart, ulPartLen);
    *pulEncryptedPartLen = ctx->pipe->Get(pEncryptedPart, ulPartLen);
    return CKR_OK;
}

/* C_EncryptFinal finishes a multiple-part encryption operation */
static CK_RV CCSL_MODS_EncryptFinal
(
 CK_SESSION_HANDLE hSession,                /* session handle */
 CK_BYTE_PTR       pLastEncryptedPart,      /* last c-text */
 CK_ULONG_PTR      pulLastEncryptedPartLen  /* gets last size */
 )
{
    ccslContext *ctx = &ctxPool[hSession];
    ctx->pipe->MessageEnd();
    *pulLastEncryptedPartLen = ctx->pipe->Get(pLastEncryptedPart, 1024);
    delete ctx->pipe;
    return CKR_OK;
}


/* C_DecryptInit initializes a decryption operation. */
static CK_RV CCSL_MODS_DecryptInit
(
 CK_SESSION_HANDLE hSession,    /* the session's handle */
 CK_MECHANISM_PTR  pMechanism,  /* the decryption mechanism */
 CK_OBJECT_HANDLE  hKey         /* handle of decryption key */
 )
{
    ccslContext *ctx = &ctxPool[hSession];
    unsigned char const *keyStr = ctx->keys[hKey];

    if(pMechanism->mechanism == CKM_AES_GCM)
    {
        CK_GCM_PARAMS *params = reinterpret_cast<CK_GCM_PARAMS *> (pMechanism->pParameter);
        unsigned char *iv = params->pIv;

        CryptoPP::SecByteBlock key(keyStr, 32);

        CryptoPP::GCM<CryptoPP::AES, CryptoPP::GCM_64K_Tables>::Decryption *gcmDec;
        gcmDec = new CryptoPP::GCM<CryptoPP::AES, CryptoPP::GCM_64K_Tables>::Decryption;
        if(gcmDec == NULL)
            return CKR_GENERAL_ERROR;
        gcmDec->SetKeyWithIV(key, key.size(), iv);
        ctx->pipe = new CryptoPP::AuthenticatedDecryptionFilter(*gcmDec, new CryptoPP::ByteQueue(),
                    CryptoPP::AuthenticatedDecryptionFilter::MAC_AT_BEGIN |
                    CryptoPP::AuthenticatedDecryptionFilter::THROW_EXCEPTION, 16);
    }
    else
    {
        CK_AES_CTR_PARAMS *params = reinterpret_cast<CK_AES_CTR_PARAMS *> (pMechanism->pParameter);
        unsigned char *iv = params->cb;

        CryptoPP::SecByteBlock key(keyStr, CryptoPP::AES::DEFAULT_KEYLENGTH);
        static_assert(CryptoPP::AES::DEFAULT_KEYLENGTH == 16, "Unexpected key length");

        CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption *ctrDec;
        ctrDec = new CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption;
        if(ctrDec == NULL)
            return CKR_GENERAL_ERROR;
        ctrDec->SetKeyWithIV(key, key.size(), iv);
        ctx->pipe = new CryptoPP::StreamTransformationFilter(*ctrDec, new CryptoPP::ByteQueue());
    }

    return CKR_OK;
}

/* C_DecryptUpdate continues a multiple-part decryption operation. */
static CK_RV CCSL_MODS_DecryptUpdate
(
 CK_SESSION_HANDLE hSession,            /* session's handle */
 CK_BYTE_PTR       pEncryptedPart,      /* encrypted data */
 CK_ULONG          ulEncryptedPartLen,  /* input length */
 CK_BYTE_PTR       pPart,               /* gets plaintext */
 CK_ULONG_PTR      pulPartLen           /* p-text size */
 )
{
    ccslContext *ctx = &ctxPool[hSession];
    ctx->pipe->Put(pEncryptedPart, ulEncryptedPartLen);
    *pulPartLen = ctx->pipe->Get(pPart, ulEncryptedPartLen);
    return CKR_OK;
}


/* C_DecryptFinal finishes a multiple-part decryption operation. */
static CK_RV CCSL_MODS_DecryptFinal
(
 CK_SESSION_HANDLE hSession,       /* the session's handle */
 CK_BYTE_PTR       pLastPart,      /* gets plaintext */
 CK_ULONG_PTR      pulLastPartLen  /* p-text size */
 )
{
    ccslContext *ctx = &ctxPool[hSession];
    try
    {
        ctx->pipe->MessageEnd();
    }
    catch (CryptoPP::Exception& e)
    {
        (void) e;
        return CKR_SIGNATURE_ILWALID;
    }
    *pulLastPartLen = ctx->pipe->Get(pLastPart, 1024);
    delete ctx->pipe;
    return CKR_OK;
}

/* Signing and MACing */

/* C_SignInit initializes a signature (private key encryption)
 * operation, where the signature is (will be) an appendix to
 * the data, and plaintext cannot be recovered from the
 * signature.
 */
static CK_RV CCSL_MODS_SignInit
(
  CK_SESSION_HANDLE hSession,    /* the session's handle */
  CK_MECHANISM_PTR  pMechanism,  /* the signature mechanism */
  CK_OBJECT_HANDLE  hKey         /* handle of signature key */
)
{
    // XXX: we can ignore init as long as we use Sign and not SignUpdate
    return CKR_OK;
}

/* C_Sign signs (encrypts with private key) data in a single
 * part, where the signature is (will be) an appendix to the
 * data, and plaintext cannot be recovered from the signature.
 */
static CK_RV CCSL_MODS_Sign
(
  CK_SESSION_HANDLE hSession,        /* the session's handle */
  CK_BYTE_PTR       pData,           /* the data to sign */
  CK_ULONG          ulDataLen,       /* count of bytes to sign */
  CK_BYTE_PTR       pSignature,      /* gets the signature */
  CK_ULONG_PTR      pulSignatureLen  /* gets signature length */
)
{
    ccslContext *ctx = &ctxPool[hSession];
    unsigned char *key = ctx->keys[1];
    int keySize = ctx->keyLen;
    CryptoPP::HMAC<CryptoPP::SHA256> hmac(key, keySize);
    hmac.Update(pData, ulDataLen);
    hmac.Final(pSignature);
    *pulSignatureLen = 256 / 8;
    return CKR_OK;
}

/* Verifying signatures and MACs */

/* C_VerifyInit initializes a verification operation, where the
 * signature is an appendix to the data, and plaintext cannot
 * cannot be recovered from the signature (e.g. DSA).
 */
static CK_RV CCSL_MODS_VerifyInit
(
  CK_SESSION_HANDLE hSession,    /* the session's handle */
  CK_MECHANISM_PTR  pMechanism,  /* the verification mechanism */
  CK_OBJECT_HANDLE  hKey         /* verification key */
)
{
    return CKR_OK;
}

/* C_VerifyFinal finishes a multiple-part verification
 * operation, checking the signature.
 */
static CK_RV CCSL_MODS_VerifyFinal
(
  CK_SESSION_HANDLE hSession,       /* the session's handle */
  CK_BYTE_PTR       pSignature,     /* signature to verify */
  CK_ULONG          ulSignatureLen  /* signature length */
)
{
    ccslContext *ctx = &ctxPool[hSession];
    ctx->pipe->Put(pSignature, ulSignatureLen);
    return CKR_OK; 
}

CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFunctionList)
{
    if(initDone == 0)
    {
        // Export functions
        functionList.C_Initialize          = &CCSL_MODS_Initialize;
        functionList.C_Finalize            = &CCSL_MODS_Finalize;
        functionList.C_GetSlotList         = &CCSL_MODS_GetSlotList;
        functionList.C_OpenSession         = &CCSL_MODS_OpenSession;
        functionList.C_CloseSession        = &CCSL_MODS_CloseSession;
        functionList.C_CreateObject        = &CCSL_MODS_CreateObject;
        functionList.C_DestroyObject       = &CCSL_MODS_DestroyObject;
        functionList.C_EncryptInit         = &CCSL_MODS_EncryptInit;
        functionList.C_EncryptUpdate       = &CCSL_MODS_EncryptUpdate;
        functionList.C_EncryptFinal        = &CCSL_MODS_EncryptFinal;
        functionList.C_DecryptInit         = &CCSL_MODS_DecryptInit;
        functionList.C_DecryptUpdate       = &CCSL_MODS_DecryptUpdate;
        functionList.C_DecryptFinal        = &CCSL_MODS_DecryptFinal;
        functionList.C_SignInit            = &CCSL_MODS_SignInit;
        functionList.C_Sign                = &CCSL_MODS_Sign;
        functionList.C_VerifyInit          = &CCSL_MODS_VerifyInit;
        functionList.C_VerifyFinal         = &CCSL_MODS_VerifyFinal;
        functionList.C_DecryptVerifyUpdate = &CCSL_MODS_DecryptUpdate;

        initDone = 1;
    }

    *ppFunctionList = &functionList;
    return CKR_OK;
}
