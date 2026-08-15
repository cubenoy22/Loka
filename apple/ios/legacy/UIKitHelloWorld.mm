#import <UIKit/UIKit.h>

@interface LokaLegacyAppDelegate : NSObject <UIApplicationDelegate> {
  UIWindow *window_;
}

@property(nonatomic, retain) UIWindow *window;

@end


@implementation LokaLegacyAppDelegate

@synthesize window = window_;

- (void)applicationDidFinishLaunching:(UIApplication *)application {
  (void)application;

  UIWindow *window = [[UIWindow alloc]
      initWithFrame:[[UIScreen mainScreen] bounds]];
  UIView *view = [[UIView alloc] initWithFrame:[window bounds]];
  UILabel *label = [[UILabel alloc] initWithFrame:[view bounds]];

  [view setBackgroundColor:[UIColor whiteColor]];
  [label setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                              UIViewAutoresizingFlexibleHeight)];
  [label setText:@"Hello, iPhone OS 3.1.3!"];
  [label setTextAlignment:UITextAlignmentCenter];
  [label setTextColor:[UIColor blackColor]];
  [view addSubview:label];

  // The legacy host deliberately owns a direct UIWindow view tree. Do not
  // copy the modern UIScene/root-view-controller lifecycle into this profile.
  [window addSubview:view];
  [self setWindow:window];
  [[self window] makeKeyAndVisible];

  [label release];
  [view release];
  [window release];
}

- (void)dealloc {
  [self setWindow:nil];
  [super dealloc];
}

@end


int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  int result = UIApplicationMain(
      argc, argv, nil, NSStringFromClass([LokaLegacyAppDelegate class]));
  [pool release];
  return result;
}
