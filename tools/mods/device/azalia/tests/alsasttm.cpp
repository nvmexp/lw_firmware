/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012,2016-2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "alsasttm.h"
#include "device/azalia/alsastrm.h"
#include "core/include/xp.h"

const UINT32 SND_PCM_STREAM_CAPTURE = 1;
const UINT32 SND_PCM_NONBLOCK = 1;
const UINT32 SND_PCM_ACCESS_RW_INTERLEAVED = 3;
const UINT32 SND_PCM_FORMAT_S16_LE = 2;

typedef const char * (*fn_snd_asoundlib_version)();
typedef const char * (*fn_snd_strerror)(INT32 errnum);
typedef INT32 (*fn_snd_pcm_open)(snd_pcm_t **pcm, const char *name,
                                 INT32 stream, INT32 mode);
typedef INT32 (*fn_snd_pcm_close)(snd_pcm_t *pcm);
typedef INT32 (*fn_snd_pcm_set_params)(snd_pcm_t *pcm,
                                       UINT32 format,
                                       UINT32 access,
                                       UINT32 channels,
                                       UINT32 rate,
                                       INT32 soft_resample,
                                       UINT32 latency);
typedef INT32 (*fn_snd_pcm_prepare)(snd_pcm_t *pcm);
typedef INT32 (*fn_snd_pcm_readi)(snd_pcm_t *pcm, void *buffer, UINT32 size);
typedef INT32 (*fn_snd_pcm_start)(snd_pcm_t *pcm);

fn_snd_asoundlib_version alsa_snd_asoundlib_version;
fn_snd_strerror alsa_snd_strerror;
fn_snd_pcm_open alsa_snd_pcm_open;
fn_snd_pcm_close alsa_snd_pcm_close;
fn_snd_pcm_set_params alsa_snd_pcm_set_params;
fn_snd_pcm_prepare alsa_snd_pcm_prepare;
fn_snd_pcm_start alsa_snd_pcm_start;
fn_snd_pcm_readi alsa_snd_pcm_readi;

bool AlsaLibraryInitialized = false;

#define GET_ALSA_FUNCTION(function_name)                                      \
do {                                                                          \
    alsa_##function_name =                                                    \
        (fn_##function_name) Xp::GetDLLProc(alsa_lib_handle, #function_name); \
    if (!alsa_##function_name)                                                \
    {                                                                         \
        Printf(Tee::PriHigh, "Cannot load ALSA function (%s)\n",              \
               #function_name);                                               \
        return RC::DLL_LOAD_FAILED;                                           \
    }                                                                         \
} while (0)

RC InitializeALSALibrary()
{
    RC rc;

    if (AlsaLibraryInitialized)
        return OK;

    void *alsa_lib_handle;
    string alsa_lib_name = "libasound" + Xp::GetDLLSuffix();
    rc = Xp::LoadDLL(alsa_lib_name, &alsa_lib_handle, Xp::UnloadDLLOnExit);
    if (OK != rc)
    {
        Printf(Tee::PriHigh, "Cannot load ALSA library %s, rc = %d\n",
            alsa_lib_name.c_str(), (UINT32)rc);
        return rc;
    }

    GET_ALSA_FUNCTION(snd_asoundlib_version);

    const char *version = alsa_snd_asoundlib_version();
    Printf(Tee::PriLow, "ALSA: snd_asoundlib_version() = %s\n", version);

    GET_ALSA_FUNCTION(snd_strerror);
    GET_ALSA_FUNCTION(snd_pcm_open);
    GET_ALSA_FUNCTION(snd_pcm_close);
    GET_ALSA_FUNCTION(snd_pcm_set_params);
    GET_ALSA_FUNCTION(snd_pcm_prepare);
    GET_ALSA_FUNCTION(snd_pcm_start);
    GET_ALSA_FUNCTION(snd_pcm_readi);

    AlsaLibraryInitialized = true;

    return rc;
}

AlsaStreamTestModule::AlsaStreamTestModule()
{
   m_pStream = new AlsaStream();
   m_CaptureHandle = nullptr;
}

AlsaStreamTestModule::~AlsaStreamTestModule()
{
   delete m_pStream;
   m_pStream = NULL;
}

RC AlsaStreamTestModule::Start()
{
    RC rc;

    if ((m_pStream->input.channels != AzaliaFormat::CHANNELS_2) ||
        (m_pStream->input.size != AzaliaFormat::SIZE_16))
    {
        Printf(Tee::PriNormal,
            "ALSA error: unsupported stream configuration.\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(InitializeALSALibrary());

    INT32 alsa_rc;
    const char *device_name = "hw";

    alsa_rc = alsa_snd_pcm_open(&m_CaptureHandle, device_name,
        SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);

    if (alsa_rc < 0)
    {
        Printf(Tee::PriNormal,"ALSA error: snd_pcm_open: %s(%i)\n",
            alsa_snd_strerror(alsa_rc), alsa_rc);
        return RC::SOFTWARE_ERROR;
    }

    alsa_rc = alsa_snd_pcm_set_params(m_CaptureHandle,
        SND_PCM_FORMAT_S16_LE,
        SND_PCM_ACCESS_RW_INTERLEAVED,
        2,                     // number of channels
        m_pStream->input.rate,
        1,                     // resample if needed
        500000);               // 500ms - from examples

    if (alsa_rc < 0)
    {
        Printf(Tee::PriNormal,"ALSA error: snd_pcm_set_params: %s(%i)\n",
            alsa_snd_strerror(alsa_rc), alsa_rc);
        return RC::SOFTWARE_ERROR;
    }

    m_pStream->InitParams();
    m_pStream->InitBuffer(m_pStream->input.minBlocks *
                          m_pStream->GetFormat()->GetSizeInBytes() *
                          m_pStream->GetFormat()->GetChannelsAsInt());

    alsa_rc = alsa_snd_pcm_prepare(m_CaptureHandle);
    if (alsa_rc < 0)
    {
        Printf(Tee::PriNormal,"ALSA error: snd_pcm_prepare: %s(%i)\n",
            alsa_snd_strerror(alsa_rc), alsa_rc);
        return RC::SOFTWARE_ERROR;
    }

    alsa_rc = alsa_snd_pcm_start(m_CaptureHandle);
    if (alsa_rc < 0)
    {
        Printf(Tee::PriNormal,"ALSA error: snd_pcm_start: %s(%i)\n",
            alsa_snd_strerror(alsa_rc), alsa_rc);
        return RC::SOFTWARE_ERROR;
    }

    m_IsFinished = false;

    return OK;
}

RC AlsaStreamTestModule::Continue()
{
    if (m_pStream->IsFinished())
    {
        m_IsFinished = true;
        return OK;
    }

    UINT08 capture_buffer[8192]; // Normally should fit 2048 frames
    const UINT32 frame_size = m_pStream->GetFormat()->GetSizeInBytes() *
                              m_pStream->GetFormat()->GetChannelsAsInt();

    INT32 frames_captured = alsa_snd_pcm_readi(m_CaptureHandle, capture_buffer,
        sizeof(capture_buffer)/frame_size);
    if (frames_captured == -11) // -EAGAIN - no samples were captured
        return OK;
    if (frames_captured < 0)
    {
        Printf(Tee::PriNormal, "ALSA error: snd_pcm_readi = %i\n",
            frames_captured);
        return RC::SOFTWARE_ERROR;
    }

    m_pStream->AddSamples(capture_buffer, frames_captured*frame_size);

    return OK;
}

RC AlsaStreamTestModule::Stop()
{
    alsa_snd_pcm_close(m_CaptureHandle);
    m_CaptureHandle = NULL;
    return OK;
}

RC AlsaStreamTestModule::GetStream(UINT32 index, AzaStream **pStrm)
{
    *pStrm = m_pStream;

    return OK;
}
