// newlib's <string.h> includes the BSD <strings.h> under __BSD_VISIBLE, which
// g++ always enables. On a case-insensitive host filesystem (macOS APFS/HFS+)
// that lookup resolves to the Toolbox interfaces' Strings.h instead, which
// pulls TextUtils.h and Events.h into every translation unit that touches
// <cstring>; the global Toolbox Button() then makes an unqualified Button in
// application code ambiguous with loka::app::Button. This directory sits
// first on the Classic include path, so <strings.h> lands here on every host.
//
// The same case-folding means a deliberate #include <Strings.h> lands here
// too, so the shim must not simply be empty. It tells the two apart by when
// it is entered: the first entry that happens while newlib's string.h is
// being processed (its guard _STRING_H_ is already defined at that point) is
// the BSD include and yields nothing (Loka uses none of bcopy, bzero, index,
// rindex, ffs, strcasecmp); every other entry is a Toolbox request and is
// forwarded to the real header with #include_next. Deliberately no include
// guard on this file: it must be re-entered for that second case.
#if !defined(LOKA_TOOLBOX_STRINGS_SHIM_ENTERED)
#define LOKA_TOOLBOX_STRINGS_SHIM_ENTERED 1
#if !defined(_STRING_H_)
#include_next <Strings.h>
#endif
#else
#include_next <Strings.h>
#endif
