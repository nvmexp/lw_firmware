/*
 * Copyright © 2014 Colorimetry Research, Inc. <engineering@colorimetryresearch.com>
 *
 */

#ifndef COLOSCIENCEAPI_FLICKERAPI_H
#define COLOSCIENCEAPI_FLICKERAPI_H

#ifndef _MSC_VER
#include <stdint.h>
#include <sys/time.h>
#else
#include "ms_stdint.h"
#include <time.h>
#endif

#include "colorscienceapi_global.h"
//#include "colorscienceapi_version.h"

#ifdef  __cplusplus
# define COLOSCIENCEAPI_BEGIN_DECLS  extern "C" {
# define COLOSCIENCEAPI_END_DECLS    }
#else
# define COLOSCIENCEAPI_BEGIN_DECLS
# define COLOSCIENCEAPI_END_DECLS
#endif


COLOSCIENCEAPI_BEGIN_DECLS

/* Random number to avoid errno conflicts */
#define CS_ERRNOBASE 1122334455

/* Protocol exceptions */
typedef enum {
    CS_EXCEPTION_ILLEGAL_FUNCTION = 0x01,
    CS_EXCEPTION_ILLEGAL_PARAMETER,
    CS_EXCEPTION_ILLEGAL_DATA,
    CS_EXCEPTION_NOT_DEFINED,
    CS_EXCEPTION_MAX
} cs_exceptions;

#define ECSILFUNC  (CS_ERRNOBASE + CS_EXCEPTION_ILLEGAL_FUNCTION)
#define ECSILPARAM  (CS_ERRNOBASE + CS_EXCEPTION_ILLEGAL_PARAMETER)
#define ECSILDATA  (CS_ERRNOBASE + CS_EXCEPTION_ILLEGAL_DATA)
#define ECSNOTDEFINED  (CS_ERRNOBASE + CS_EXCEPTION_NOT_DEFINED)


typedef struct _flicker flicker_t;


typedef struct {
    double sampling_rate;
    uint32_t samples;

    double percent_flicker;
    double flicker_index;
    double flicker_modulation_amplitude;

    double maximum_flicker_search_frequency;
    double flicker_frequency;
    double flicker_level;

    double minimum_frequency_index;
    double maximum_frequency_index;
    double minimum_frequency;
    double maximum_frequency;

} cs_flicker_data_t;

typedef enum
{
    FILTER_FAMILY_NONE          = 0,
    FILTER_FAMILY_BUTTERWORTH   = 1
} flicker_filter_family;


typedef enum
{
    FILTER_TYPE_NONE          = 0,
    FILTER_TYPE_LOWPASS       = 1,
    FILTER_TYPE_HIGHPASS      = 2,
    FILTER_TYPE_BANDPASS      = 3,
    FILTER_TYPE_BANDSTOP      = 4
} flicker_filter_type;


typedef struct {
    uint8_t filter_type;
    uint8_t filter_family; //reserved for future use
    uint8_t order;
    double frequency;
    double bandwidth;

} cs_flicker_filter_t;


/* filter */
COLORSCIENCEAPI_EXPORT(int32_t) cs_flicker_filter(cs_flicker_filter_t* filter, double sampling_rate, double *data, uint32_t count, double *filtered_data);

/* create/destroy */
COLORSCIENCEAPI_EXPORT(flicker_t*) cs_flicker_create(double sampling_rate, double *data, uint32_t count, double maximumSearchFrequency);
COLORSCIENCEAPI_EXPORT(void) cs_flicker_free(flicker_t *ctx);

/* flicker parameters */
COLORSCIENCEAPI_EXPORT(int32_t) cs_flicker_fft(flicker_t *ctx, double *fft_data, uint32_t *count);
COLORSCIENCEAPI_EXPORT(int32_t) cs_flicker_data_ex(cs_flicker_data_t *&flicker_data, double sampling_rate, double *data, uint32_t count, double maximumSearchFrequency);
COLORSCIENCEAPI_EXPORT(int32_t) cs_flicker_data(flicker_t *ctx, cs_flicker_data_t *flicker_data);


COLORSCIENCEAPI_EXPORT(const char *)cs_flicker_strerror(uint32_t errnum);

COLOSCIENCEAPI_END_DECLS

#endif // !COLOSCIENCEAPI_FLICKERAPI_H
