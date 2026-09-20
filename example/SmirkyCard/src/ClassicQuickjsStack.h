#ifndef SMIRKYCARD_CLASSIC_QUICKJS_STACK_H
#define SMIRKYCARD_CLASSIC_QUICKJS_STACK_H

#include <alloca.h>

/* RetroPPC GCC 16.1 places fixed locals on an 8-byte boundary but rounds
   ordinary alloca's result to 16 bytes. The extra offset can overlap the
   fixed frame. Reserve one extra 16-byte ABI stack unit so the requested
   payload ends before fixed locals. Keep ordinary alloca's function
   lifetime: some QuickJS argument arrays escape the block allocating them,
   so __builtin_alloca_with_align's block lifetime is insufficient. */
#if defined(_ARCH_PPC) && defined(__GNUC__)
#define SMIRKYCARD_JS_STACK_SIZE(size) ((size) + 16u)
#else
#define SMIRKYCARD_JS_STACK_SIZE(size) (size)
#endif
#define SMIRKYCARD_JS_ALLOCA(size) alloca(SMIRKYCARD_JS_STACK_SIZE(size))

#endif
