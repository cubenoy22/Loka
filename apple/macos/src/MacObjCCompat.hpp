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

// backingScaleFactor arrived in 10.7, so the legacy SDK does not declare it and
// Objective-C++ would infer an id return and reject the conversion to a number.
// Declared and never implemented: this supplies a return type so the call
// type-checks, while respondsToSelector: still decides at runtime whether the
// call happens at all -- and on anything this SDK can target, it does not. The
// CGFloat typedef above matches the 32-bit ABI the declaration would need.
//
// Note the difference from the Tiger blocks above: those methods do exist on the
// old system and are only missing from current SDKs, so their declarations
// enable a real call. This one enables compilation of a call that the runtime
// guard will decline.
#if !defined(MAC_OS_X_VERSION_MAX_ALLOWED) || (MAC_OS_X_VERSION_MAX_ALLOWED < 1070)
@interface NSWindow (LokaBackingScaleCompat)
- (CGFloat)backingScaleFactor;
@end
#endif

#endif // LOKA_MAC_OBJC_COMPAT_HPP
