#import <UIKit/UIKit.h>

@interface LokaUIKitSceneDelegate : NSObject <UIWindowSceneDelegate> {
  UIWindow *window_;
}

@property(nonatomic, retain) UIWindow *window;

@end

@implementation LokaUIKitSceneDelegate

@synthesize window = window_;

- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
                 options:(UISceneConnectionOptions *)connectionOptions {
  (void)session;
  (void)connectionOptions;

  NSAssert([scene isKindOfClass:[UIWindowScene class]],
           @"The application scene must be a UIWindowScene.");
  UIWindowScene *windowScene = (UIWindowScene *)scene;
  UIWindow *window = [[UIWindow alloc] initWithWindowScene:windowScene];
  UIViewController *controller = [[UIViewController alloc] init];
  UIView *view = [[UIView alloc] initWithFrame:[window bounds]];
  UILabel *label = [[UILabel alloc] initWithFrame:[view bounds]];

  [view setBackgroundColor:[UIColor whiteColor]];
  [label setAutoresizingMask:(UIViewAutoresizingFlexibleWidth |
                              UIViewAutoresizingFlexibleHeight)];
  [label setText:@"Hello, iPhone and iPad!"];
  [label setTextAlignment:NSTextAlignmentCenter];
  [label setTextColor:[UIColor blackColor]];
  [view addSubview:label];
  [controller setView:view];
  [window setRootViewController:controller];

  [self setWindow:window];
  [[self window] makeKeyAndVisible];

  [label release];
  [view release];
  [controller release];
  [window release];
}

- (void)dealloc {
  [self setWindow:nil];
  [super dealloc];
}

@end

@interface LokaUIKitAppDelegate : NSObject <UIApplicationDelegate>
@end

@implementation LokaUIKitAppDelegate
@end

int main(int argc, char *argv[]) {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  int result = UIApplicationMain(
      argc, argv, nil, NSStringFromClass([LokaUIKitAppDelegate class]));
  [pool release];
  return result;
}
