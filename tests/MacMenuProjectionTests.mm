#include "MacMenuProjectionTests.hpp"

#include "support/TestVerify.hpp"

#include <AppKit/AppKit.h>
#include <cstdio>

#include "MacMenuProjection.hpp"

@interface LokaMenuProjectionTestTarget : NSObject
{
  bool *released_;
  bool *detachedBeforeRelease_;
}
- (id)initWithReleased:(bool *)released detachedBeforeRelease:(bool *)detachedBeforeRelease;
@end

@implementation LokaMenuProjectionTestTarget
- (id)initWithReleased:(bool *)released detachedBeforeRelease:(bool *)detachedBeforeRelease
{
  self = [super init];
  if (self)
  {
    released_ = released;
    detachedBeforeRelease_ = detachedBeforeRelease;
  }
  return self;
}

- (void)dealloc
{
  *released_ = true;
  *detachedBeforeRelease_ = [NSApp mainMenu] == nil;
  [super dealloc];
}
@end

void testMacMenuProjectionDetachesMainMenuBeforeReleasingTarget()
{
  [NSApplication sharedApplication];
  [NSApp setMainMenu:nil];

  bool targetReleased = false;
  bool detachedBeforeTargetRelease = false;
  LokaMenuProjectionTestTarget *target =
      [[LokaMenuProjectionTestTarget alloc] initWithReleased:&targetReleased
                                       detachedBeforeRelease:&detachedBeforeTargetRelease];
  {
    MacMenuProjection projection((void *)target);
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Loka test menu"];
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:@"Action" action:nil keyEquivalent:@""];
    [item setTarget:target];
    [menu addItem:item];
    [item release];

    projection.install((void *)menu);
    [menu release];
    LOKA_VERIFY([NSApp mainMenu] != nil);
  }

  LOKA_VERIFY([NSApp mainMenu] == nil);
  LOKA_VERIFY(targetReleased);
  LOKA_VERIFY(detachedBeforeTargetRelease);

  std::printf("testMacMenuProjectionDetachesMainMenuBeforeReleasingTarget passed\n");
}
