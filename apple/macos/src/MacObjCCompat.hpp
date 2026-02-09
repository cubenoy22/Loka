#ifndef LOKA_MAC_OBJC_COMPAT_HPP
#define LOKA_MAC_OBJC_COMPAT_HPP

#import <AvailabilityMacros.h>

#if !defined(__OBJC2__) || !__OBJC2__
#ifndef NSINTEGER_DEFINED
typedef int NSInteger;
typedef unsigned int NSUInteger;
#define NSINTEGER_DEFINED 1
#endif

#ifndef CGFLOAT_TYPE
typedef float CGFloat;
#define CGFLOAT_TYPE 1
#endif
#endif

#if !defined(MAC_OS_X_VERSION_MAX_ALLOWED) || (MAC_OS_X_VERSION_MAX_ALLOWED < 1070)
#ifndef NSApplicationActivationPolicyRegular
#define NSApplicationActivationPolicyRegular 0
#endif
#endif

#endif // LOKA_MAC_OBJC_COMPAT_HPP
