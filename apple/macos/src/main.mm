#include <AppKit/AppKit.h>
#ifdef TEST_BUILD
#include "Tests.hpp"
#endif

#ifdef TEST_BUILD
static void runTests()
{
  testDependencyPropagationCases();
  testTrackerPropagation();
  testDeferredSideEffect();
  testBatchTransaction();
  testRAIITransaction();
  testTextInputOnChange();
  testDerivedStruct();
  testSceneManagerTransaction();
  testComponentContext();
  testNodeCompositionTree();
  testStaticNodeManagerRun();
  testLokaCoreString();
  testLokaCoreCollections();
}
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;

  @autoreleasepool {
    #ifdef TEST_BUILD
    runTests();
    #endif

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

    NSRect frame = NSMakeRect(100.0, 100.0, 640.0, 360.0);
    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                       NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable;
    NSWindow *window =
        [[NSWindow alloc] initWithContentRect:frame
                                    styleMask:style
                                      backing:NSBackingStoreBuffered
                                        defer:NO];
    [window setTitle:@"Declara macOS"];
    [window makeKeyAndOrderFront:nil];

    [NSApp activateIgnoringOtherApps:YES];
    [NSApp run];
  }

  return 0;
}
