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

// AppKit's modern constant names are enum values, not preprocessor macros.
// Select by SDK version so current SDKs stay warning-clean while the legacy
// 10.5 SDK used for the 10.4 deployment target keeps the original spellings.
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= 101400)
#define LOKA_MAC_BUTTON_BEZEL_STYLE NSBezelStylePush
#define LOKA_MAC_BUTTON_TYPE_MOMENTARY_PUSH_IN NSButtonTypeMomentaryPushIn
#else
#define LOKA_MAC_BUTTON_BEZEL_STYLE NSRoundedBezelStyle
#define LOKA_MAC_BUTTON_TYPE_MOMENTARY_PUSH_IN NSMomentaryPushInButton
#endif

#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= 101200)
#define LOKA_MAC_WINDOW_STYLE_TITLED NSWindowStyleMaskTitled
#define LOKA_MAC_WINDOW_STYLE_CLOSABLE NSWindowStyleMaskClosable
#define LOKA_MAC_WINDOW_STYLE_RESIZABLE NSWindowStyleMaskResizable
#define LOKA_MAC_WINDOW_STYLE_MINIATURIZABLE NSWindowStyleMaskMiniaturizable
#define LOKA_MAC_COMPOSITING_SOURCE_OVER NSCompositingOperationSourceOver
#else
#define LOKA_MAC_WINDOW_STYLE_TITLED NSTitledWindowMask
#define LOKA_MAC_WINDOW_STYLE_CLOSABLE NSClosableWindowMask
#define LOKA_MAC_WINDOW_STYLE_RESIZABLE NSResizableWindowMask
#define LOKA_MAC_WINDOW_STYLE_MINIATURIZABLE NSMiniaturizableWindowMask
#define LOKA_MAC_COMPOSITING_SOURCE_OVER NSCompositeSourceOver
#endif

#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) && (MAC_OS_X_VERSION_MAX_ALLOWED >= 101000)
#define LOKA_MAC_MODAL_RESPONSE_OK NSModalResponseOK
#else
#define LOKA_MAC_MODAL_RESPONSE_OK NSOKButton
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
