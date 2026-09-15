#ifndef LOKA_TOOLBOX_NEWLIB_STRINGS_H
#define LOKA_TOOLBOX_NEWLIB_STRINGS_H

// newlib's <string.h> includes the BSD <strings.h> under __BSD_VISIBLE, which
// g++ always enables. On a case-insensitive host filesystem (macOS APFS/HFS+)
// that lookup resolves to the Toolbox interfaces' Strings.h instead, which
// pulls TextUtils.h and Events.h into every translation unit that touches
// <cstring>; the global Toolbox Button() then makes an unqualified Button in
// application code ambiguous with loka::app::Button. This directory sits
// first on the Classic include path, so <strings.h> lands here on every host.
// Loka uses none of the BSD strings.h declarations (bcopy, bzero, index,
// rindex, ffs, strcasecmp), so the shim declares nothing and defines newlib's
// own guard to keep the real header out as well.
#ifndef _STRINGS_H_
#define _STRINGS_H_
#endif

#endif
