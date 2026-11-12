/* Riffhouse - the settings LAME's own configure script would have found on the systems
   Riffhouse is built for (Windows with MinGW or MSVC, Linux, macOS). Only the encoder is
   built: no decoder, no front end. */
#ifndef RIFFHOUSE_LAME_CONFIG_H
#define RIFFHOUSE_LAME_CONFIG_H

#define STDC_HEADERS 1
#define HAVE_STDINT_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define HAVE_STRCHR 1
#define HAVE_MEMCPY 1
#define PROTOTYPES 1
#define PACKAGE "lame"
#define LAME_LIBRARY_BUILD 1

#if defined(__SSE__) || defined(_M_X64) || defined(__x86_64__)
#define HAVE_XMMINTRIN_H 1
#define MIN_ARCH_SSE 1
#endif

/* the 32-bit float LAME's fast logarithm works on */
typedef float ieee754_float32_t;
typedef double ieee754_float64_t;

#endif
