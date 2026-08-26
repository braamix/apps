#ifndef _STDINT_H_
#define _STDINT_H_
#include <sys/types.h>

#define UINT8_MAX  0xFF
#define UINT16_MAX 0xFFFF
#define UINT32_MAX 0xFFFFFFFFU
#define UINT64_MAX 0xFFFFFFFFFFFFFFFFULL
#define INT32_MAX  0x7FFFFFFF
#define INT64_MAX  0x7FFFFFFFFFFFFFFFLL

#define UINT8_C(v)  (v)
#define UINT16_C(v) (v)
#define UINT32_C(v) (v##U)
#define UINT64_C(v) (v##ULL)
#define INT8_C(v)   (v)
#define INT16_C(v)  (v)
#define INT32_C(v)  (v)
#define INT64_C(v)  (v##LL)
#endif
