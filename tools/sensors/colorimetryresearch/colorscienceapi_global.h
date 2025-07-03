#ifndef COLORSCIENCEAPI_GLOBAL_H
#define COLORSCIENCEAPI_GLOBAL_H


#if defined(COLORSCIENCE_API_LIBRARY)
#ifdef _MSC_VER
#  define COLORSCIENCEAPI_EXPORT(T) __declspec (dllexport) T
#else // !_MSC_VER
#  define COLORSCIENCEAPI_EXPORT(T) T
#endif
#else // !defined(COLORSCIENCE_API_LIBRARY)
#ifdef _MSC_VER
#  define COLORSCIENCEAPI_EXPORT(T) __declspec (dllimport) T
#else // !_MSC_VER
#  define COLORSCIENCEAPI_EXPORT(T) T
#endif
#endif // defined(COLORSCIENCE_API_LIBRARY)


#endif // COLORSCIENCEAPI_GLOBAL_H
