#ifndef __AMIGA_COMPILER_COMPAT
#define __AMIGA_COMPILER_COMPAT

/* Borrow from NDK compatibility  */
#include <clib/compiler-specific.h>

#ifndef __INLINE__
#ifdef __SASC
#define __INLINE__ __inline
#else
#define __INLINE__
#endif
#endif /* __INLINE__ */

#endif