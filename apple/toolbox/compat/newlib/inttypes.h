#ifndef LOKA_TOOLBOX_COMPAT_INTTYPES_H
#define LOKA_TOOLBOX_COMPAT_INTTYPES_H

#include_next <inttypes.h>

// RetroPPC can pair GCC's stdint.h with newlib's inttypes.h. GCC declares
// int64_t but not newlib's __int64_t_defined, so newlib omits PRId64.
// Keep an existing definition; otherwise use newlib's own type-aware prefix
// rather than assuming that int64_t is long or long long on every target.
#if !defined(PRId64) && defined(__NEWLIB__) && defined(__INT64_TYPE__) && defined(__PRI64)
#define PRId64 __PRI64(d)
#endif

#endif
