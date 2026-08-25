#ifdef BYTE_ORDER
#  undef BYTE_ORDER
#endif
#ifdef LITTLE_ENDIAN
#  undef LITTLE_ENDIAN
#endif
#ifdef BIG_ENDIAN
#  undef BIG_ENDIAN
#endif

#define BIG_ENDIAN	4321
#define LITTLE_ENDIAN	1234
#define BYTE_ORDER LITTLE_ENDIAN
#ifdef ACTUAL_SIZE_OF_C_LONG
#  undef ACTUAL_SIZE_OF_C_LONG
#endif
#define ACTUAL_SIZE_OF_C_LONG	4
#ifdef NECESSARY_SIZE_OF_C_LONG
#  undef NECESSARY_SIZE_OF_C_LONG
#endif
#define NECESSARY_SIZE_OF_C_LONG	8
#define AGENA_BUILDDATE  "September 06, 2025"
#define AGENA_BUILDTIME  "18:58 h"

