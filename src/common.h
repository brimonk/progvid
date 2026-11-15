#ifndef COMMON_H_
#define COMMON_H_

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>
#include <float.h>
#include <wchar.h>
#include <locale.h>
#include <math.h>
#include <assert.h>

typedef int8_t     i8;
typedef int16_t    i16;
typedef int32_t    i32;
typedef int64_t    i64;
typedef uint8_t    u8;
typedef uint16_t   u16;
typedef uint32_t   u32;
typedef uint64_t   u64;
typedef float      f32;
typedef double     f64;

#define BUFLARGE 1 << 18

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define ARRSIZE(x) (sizeof((x))/sizeof((x)[0]))
#define SWAP(a, b) do { typeof((a)) z_ = (b); (b) = (a); (a) = z_; } while (0);

#define streq(a, b) (strcmp((a), (b)) == 0 && strlen((a)) == strlen((b)))
#define strneq(a, b) (strncmp((a), (b), strlen((b))) == 0)
#define strdup_null(s) (s) == NULL ? NULL : strdup((s))

#define autofree __attribute__((cleanup(cleanup_free)))
#define autofreearr __attribute__((cleanup(autofreearr_fn)))

void cleanup_free(void *p);
void autofreearr_fn(void *p);

#ifdef COMMON_IMPLEMENTATION
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"

// TODO is there a less awful way to do this?

void cleanup_free(void *p)
{
	free(*(void **)p);
}

void autofreearr_fn(void *p)
{
	arrfree(*(void **)p);
}

#undef  STB_DS_IMPLEMENTATION
#else
#include "stb_ds.h"
#endif // COMMON_IMPLEMENTATION

#endif // COMMON_H_
