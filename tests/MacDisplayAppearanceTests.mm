#include "MacDisplayAppearanceTests.hpp"
#include "MacDisplayAppearance.hpp"
#include <Foundation/Foundation.h>
#include <assert.h>

@interface LokaLegacyAppearanceProbe : NSObject
@end

@implementation LokaLegacyAppearanceProbe
- (NSString *)name
{
  return @"NSAppearanceNameAqua";
}
@end

@interface LokaMatchingAppearanceProbe : NSObject
{
  NSString *match_;
}
- (id)initWithMatch:(NSString *)match;
- (NSString *)name;
- (NSString *)bestMatchFromAppearancesWithNames:(NSArray *)names;
@end

@implementation LokaMatchingAppearanceProbe
- (id)initWithMatch:(NSString *)match
{
  self = [super init];
  if (self)
  {
    match_ = [match retain];
  }
  return self;
}

- (void)dealloc
{
  [match_ release];
  [super dealloc];
}

- (NSString *)name
{
  // The mechanism must use AppKit's match result, not infer from this name.
  return @"CustomLightLookingName";
}

- (NSString *)bestMatchFromAppearancesWithNames:(NSArray *)names
{
  assert([names count] == 2);
  assert([names containsObject:@"NSAppearanceNameAqua"]);
  assert([names containsObject:@"NSAppearanceNameDarkAqua"]);
  return match_;
}
@end

void testMacDisplayAppearanceDeclinesWithoutMatchingCapability()
{
  LokaLegacyAppearanceProbe *appearance = [[LokaLegacyAppearanceProbe alloc] init];
  Window::DisplayAppearance value = Window::DISPLAY_APPEARANCE_DARK;
  assert(!loka::macos::TryReadDisplayAppearance((void *)appearance, value));
  assert(value == Window::DISPLAY_APPEARANCE_DARK);
  [appearance release];
}

void testMacDisplayAppearanceUsesNativeBestMatch()
{
  LokaMatchingAppearanceProbe *darkAppearance = [[LokaMatchingAppearanceProbe alloc]
      initWithMatch:@"NSAppearanceNameDarkAqua"];
  Window::DisplayAppearance value = Window::DISPLAY_APPEARANCE_LIGHT;
  assert(loka::macos::TryReadDisplayAppearance((void *)darkAppearance, value));
  assert(value == Window::DISPLAY_APPEARANCE_DARK);
  [darkAppearance release];

  LokaMatchingAppearanceProbe *lightAppearance = [[LokaMatchingAppearanceProbe alloc]
      initWithMatch:@"NSAppearanceNameAqua"];
  value = Window::DISPLAY_APPEARANCE_DARK;
  assert(loka::macos::TryReadDisplayAppearance((void *)lightAppearance, value));
  assert(value == Window::DISPLAY_APPEARANCE_LIGHT);
  [lightAppearance release];

  LokaMatchingAppearanceProbe *unknownAppearance = [[LokaMatchingAppearanceProbe alloc]
      initWithMatch:@"CustomAppearanceName"];
  value = Window::DISPLAY_APPEARANCE_DARK;
  assert(!loka::macos::TryReadDisplayAppearance((void *)unknownAppearance, value));
  assert(value == Window::DISPLAY_APPEARANCE_DARK);
  [unknownAppearance release];
}
