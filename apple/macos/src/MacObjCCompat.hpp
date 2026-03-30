#ifndef LOKA_MAC_OBJC_COMPAT_HPP
#define LOKA_MAC_OBJC_COMPAT_HPP

#import <AvailabilityMacros.h>
#import <AppKit/AppKit.h>
#include <float.h>

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

#if !defined(MAC_OS_X_VERSION_MAX_ALLOWED) || (MAC_OS_X_VERSION_MAX_ALLOWED < 1060)
#ifndef NSApplicationActivationPolicyRegular
#define NSApplicationActivationPolicyRegular 0
#endif
#endif

#ifndef CGFLOAT_MAX
#define CGFLOAT_MAX FLT_MAX
#endif

#ifndef NSRunLoopCommonModes
#define NSRunLoopCommonModes ((NSString *)kCFRunLoopCommonModes)
#endif

#if !defined(MAC_OS_X_VERSION_MAX_ALLOWED) || (MAC_OS_X_VERSION_MAX_ALLOWED < 1050)
@interface NSString (LokaTigerStringDrawingCompat)
- (NSSize)sizeWithFont:(NSFont *)font;
- (NSSize)sizeWithFont:(NSFont *)font constrainedToSize:(NSSize)size;
- (NSSize)sizeWithFont:(NSFont *)font constrainedToSize:(NSSize)size lineBreakMode:(NSLineBreakMode)mode;
@end

@interface NSTextField (LokaTigerTextFieldCompat)
- (void)setUsesSingleLineMode:(BOOL)flag;
@end
#endif

#endif // LOKA_MAC_OBJC_COMPAT_HPP
