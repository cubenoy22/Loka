#include "MacNativeResourceRetirementTests.hpp"
#include "MacPlatformContext.hpp"
#include "MacApp.hpp"
#include "MacWindow.hpp"
#include "MacScenePlatformController.hpp"
#include "MacBitmapCapture.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/Text.hpp"
#include "app/scene/ability/CapturableBitmap.hpp"
#include "core/Operation.hpp"
#include "core/resource/Blob.hpp"
#include "testing/app/NativeResourceRetirementTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/LokaAllocFailure.hpp"
#include "support/TestVerify.hpp"
#include <cstring>
#include "support/MacRetirementProbe.hpp"
#include <AppKit/AppKit.h>
#include <objc/runtime.h>

@interface LokaRetirementSentinel : NSObject
{
@private
  int *count_;
}
- (id)initWithCount:(int *)count;
@end
@implementation LokaRetirementSentinel
- (id)initWithCount:(int *)count
{
  self = [super init];
  if (self) count_ = count;
  return self;
}
- (void)dealloc
{
  ++*count_;
  [super dealloc];
}
@end

void observeMacNativeDeallocation(void *object, int &count)
{
  static char key;
  LOKA_VERIFY(object);
  LokaRetirementSentinel *sentinel = [[LokaRetirementSentinel alloc] initWithCount:&count];
  LOKA_VERIFY(sentinel);
  objc_setAssociatedObject((id)object, &key, sentinel, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
  [sentinel release];
}

static int captureCalls = 0;
@interface LokaRetirementCountedView : NSView
@end
@implementation LokaRetirementCountedView
- (NSBitmapImageRep *)bitmapImageRepForCachingDisplayInRect:(NSRect)rect
{
  ++captureCalls;
  return [super bitmapImageRepForCachingDisplayInRect:rect];
}
@end

namespace
{
  using loka::core::resource::Blob;
  using loka::core::resource::Image;
  typedef loka::app::testing::NativeResourceRetirementTestAccess Access;
  namespace alloc = loka::core::testing;
  struct Config : AppConfigurable
  {
    explicit Config(PlatformContext &context) : AppConfigurable(&context) {}
    virtual void compose(AppComposition &) {}
    Image image;
  };
  class CompletionApp : public MacApp
  {
  public:
    explicit CompletionApp(Config &config) : MacApp(&config) {}
    virtual ~CompletionApp()
    {
      if (this->group_) this->group_->build();
    }
    void install(Window &window)
    {
      this->group_ = new AppComponentGroup(std::vector<AppComponent *>(1, &window));
    }
    void complete() { this->flushInvalidationsTick(); }
  };
  struct Fixture
  {
    MacPlatformContext context;
    Config config;
    CompletionApp app;
    Fixture() : config(this->context), app(this->config) {}
  };
  void empty(const PlatformContext &context)
  {
    LOKA_VERIFY(Access::held(context) == 0);
    LOKA_VERIFY(Access::queued(context) == 0);
    LOKA_VERIFY(Access::inFlight(context) == 0);
  }
  // Original hand-authored 1x1 24-bit BMP, including its padded pixel row.
  Blob bitmapBlob()
  {
    const unsigned char bytes[] = {
      0x42,0x4d,58,0,0,0,0,0,0,0,54,0,0,0,
      40,0,0,0,1,0,0,0,1,0,0,0,1,0,24,0,
      0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,
      0,0,0,0,0,0,0,0,0x20,0x40,0x80,0
    };
    Blob blob = Blob::Create();
    LOKA_VERIFY(blob.tryAssign(bytes, sizeof(bytes)));
    return blob;
  }

  bool decode(MacPlatformContext &context, const Blob &blob, Image &image)
  {
    return context.createImageFromBlob(blob, 0, blob.size(), image);
  }

  class CaptureRoot : public loka::app::scene::BoundaryNodeFor<CaptureRoot>
  {
  public:
    explicit CaptureRoot(const loka::app::scene::BoundaryPropsFor<CaptureRoot> &props)
        : loka::app::scene::BoundaryNodeFor<CaptureRoot>(props) {}
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::Column() << loka::app::Button() << loka::app::Text("capture"));
    }
  };

  WindowProps captureProps()
  {
    WindowProps props;
    props.frame(40, 40, 240, 160).visible(true).scene(
        new loka::app::scene::Scene(loka::app::scene::Boundary<CaptureRoot>()));
    return props;
  }
}

void testMacNativeRetirementDecode()
{
  // Mutation: restore the inline decode releaser, or omit completion drain.
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  int released = 0;
  {
    Fixture fixture;
    const Blob blob = bitmapBlob();
    NSAutoreleasePool *inner = [[NSAutoreleasePool alloc] init];
    Image image;
    LOKA_VERIFY(decode(fixture.context, blob, image));
    observeMacNativeDeallocation(image.nativeHandle(), released);
    Image copy = image;
    [inner drain];
    image = Image();
    fixture.app.complete();
    LOKA_VERIFY(released == 0 && Access::held(fixture.context) == 1);
    copy = Image();
    LOKA_VERIFY(released == 0 && Access::queued(fixture.context) == 1);
    fixture.app.complete();
    LOKA_VERIFY(released == 1);
    empty(fixture.context);
    fixture.app.complete();
    LOKA_VERIFY(released == 1);
  }
  [pool drain];
  LOKA_VERIFY(released == 1);
}

void testMacNativeRetirementDecodeRefusal()
{
  // Mutation: ignore invalid reservation or fail to cancel on native refusal.
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  {
    Fixture fixture;
    Image image;
    Blob invalid;
    LOKA_VERIFY(!decode(fixture.context, invalid, image));
    Blob malformed = Blob::Create();
    LOKA_VERIFY(malformed.tryResize(8));
    std::memset(malformed.mutableData(), 0, 8);
    LOKA_VERIFY(!decode(fixture.context, malformed, image));
    empty(fixture.context);
    const Blob blob = bitmapBlob();
    LOKA_VERIFY(!fixture.context.createImageFromBlob(blob, 0, 0, image));
    alloc::failLokaAllocRaw("NativeResourceRetirement", "Ticket", 1);
    LOKA_VERIFY(!decode(fixture.context, blob, image));
    LOKA_VERIFY(!image.isValid());
    empty(fixture.context);
    LOKA_VERIFY(alloc::lokaAllocRawLive() == 0);
    alloc::allowLokaAllocRaw();
    LOKA_VERIFY(decode(fixture.context, blob, image));
    image = Image();
    fixture.app.complete();
    empty(fixture.context);
  }
  [pool drain];
}

void testMacNativeRetirementPublicationRefusal()
{
  // Mutation: inline FromNative refusal release, double cancel, or stale queue.
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  {
    Fixture fixture;
    const Blob blob = bitmapBlob();
    const char *owners[] = {"Image", "Managed"};
    const char *types[] = {"Record", "ControlBlock"};
    for (int i = 0; i != 2; ++i)
    {
      int released = 0;
      NSAutoreleasePool *inner = [[NSAutoreleasePool alloc] init];
      alloc::failLokaAllocRaw(owners[i], types[i], 1);
      Image image;
      LOKA_VERIFY(!decode(fixture.context, blob, image));
      LOKA_VERIFY(!image.isValid());
      LOKA_VERIFY(Access::held(fixture.context) == 0 && Access::queued(fixture.context) == 1);
      observeMacNativeDeallocation(Access::queuedHandle(fixture.context), released);
      [inner drain];
      LOKA_VERIFY(released == 0);
      fixture.app.complete();
      LOKA_VERIFY(released == 1);
      empty(fixture.context);
      fixture.app.complete();
      LOKA_VERIFY(released == 1);
      LOKA_VERIFY(alloc::lokaAllocRawLive() == 0);
      alloc::allowLokaAllocRaw();
    }
  }
  [pool drain];
}

void testMacNativeRetirementCapture()
{
  // Mutation: omit Window ancestor wiring, leave either control inline, or
  // make the ticket depend on its originating controller's lifetime.
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  int survivingReleased = 0;
  {
    MacPlatformContext context;
    Config config(context);
    {
      MacWindow window(&context, captureProps());
      CompletionApp app(config);
      app.install(window);
      app.complete();
      loka::app::scene::BoundaryNode *root =
          loka::dsl::testing::SceneTestAccess::rootBoundary(*window.scene());
      LOKA_VERIFY(root && root->childrenHead() && root->childrenHead()->asNestable());
      loka::app::scene::Node *node = root->childrenHead()->asNestable()->childrenHead();
      for (int i = 0; i != 2; ++i)
      {
        int released = 0;
        LOKA_VERIFY(node && node->getContext());
        const loka::app::scene::ICapturableBitmap *capture = node->getContext()->asCapturableBitmap();
        LOKA_VERIFY(capture);
        NSAutoreleasePool *inner = [[NSAutoreleasePool alloc] init];
        Image image;
        LOKA_VERIFY(capture->captureBitmap(image));
        observeMacNativeDeallocation(image.nativeHandle(), released);
        [inner drain];
        image = Image();
        LOKA_VERIFY(released == 0 && Access::queued(context) == 1);
        app.complete();
        LOKA_VERIFY(released == 1);
        empty(context);
        node = node->nextInComposition;
      }
      NSAutoreleasePool *inner = [[NSAutoreleasePool alloc] init];
      node = root->childrenHead()->asNestable()->childrenHead();
      LOKA_VERIFY(node->getContext()->asCapturableBitmap()->captureBitmap(config.image));
      observeMacNativeDeallocation(config.image.nativeHandle(), survivingReleased);
      [inner drain];
    }
    LOKA_VERIFY(survivingReleased == 0);
    config.image = Image();
    LOKA_VERIFY(survivingReleased == 0 && Access::queued(context) == 1);
    CompletionApp app(config);
    app.complete();
    LOKA_VERIFY(survivingReleased == 1);
    empty(context);
  }
  [pool drain];
  LOKA_VERIFY(survivingReleased == 1);
}

void testMacNativeRetirementCaptureRefusal()
{
  // Mutation: acquire before owner/reservation checks, or inline fallback.
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  [NSApplication sharedApplication];
  NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 160, 120)
      styleMask:0 backing:NSBackingStoreBuffered defer:NO];
  [window setReleasedWhenClosed:NO];
  NSView *view = [[LokaRetirementCountedView alloc] initWithFrame:NSMakeRect(0, 0, 160, 120)];
  [window setContentView:view];
  [window orderFront:nil];
  {
    Fixture fixture;
    MacScenePlatformController ownerless(view, loka::app::RailMetrics());
    Image image;
    captureCalls = 0;
    LOKA_VERIFY(!ownerless.captureViewBitmap(view, image));
    LOKA_VERIFY(!image.isValid() && captureCalls == 0);
    alloc::failLokaAllocRaw("NativeResourceRetirement", "Ticket", 1);
    LOKA_VERIFY(!loka::macos::CaptureViewBitmap(&fixture.context, view, image));
    LOKA_VERIFY(!image.isValid() && captureCalls == 0);
    empty(fixture.context);
    LOKA_VERIFY(alloc::lokaAllocRawLive() == 0);
    alloc::allowLokaAllocRaw();
    // Positive control proves the counted native selector actually fires.
    LOKA_VERIFY(loka::macos::CaptureViewBitmap(&fixture.context, view, image));
    LOKA_VERIFY(captureCalls == 1);
    image = Image();
    fixture.app.complete();
    empty(fixture.context);
  }
  [view release];
  [window close];
  [window release];
  [pool drain];
}

void testMacNativeRetirementActiveOperation()
{
  // Mutation: remove Operation exclusion or treat a joined tick as outer close.
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  int released = 0;
  {
    Fixture fixture;
    const Blob blob = bitmapBlob();
    NSAutoreleasePool *inner = [[NSAutoreleasePool alloc] init];
    Image image;
    LOKA_VERIFY(decode(fixture.context, blob, image));
    observeMacNativeDeallocation(image.nativeHandle(), released);
    [inner drain];
    {
      loka::core::Operation outer;
      image = Image();
      fixture.app.complete();
      LOKA_VERIFY(released == 0 && Access::queued(fixture.context) == 1);
      outer.close();
    }
    LOKA_VERIFY(released == 0);
    fixture.app.complete();
    LOKA_VERIFY(released == 1);
    empty(fixture.context);
  }
  [pool drain];
  LOKA_VERIFY(released == 1);
}

void testMacNativeRetirementFinalDrain()
{
  // Mutation: omit context final drain or attach it only to App destruction.
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  int released = 0;
  {
    MacPlatformContext context;
    {
      Config config(context);
      {
        CompletionApp app(config);
        NSAutoreleasePool *inner = [[NSAutoreleasePool alloc] init];
        const Blob blob = bitmapBlob();
        LOKA_VERIFY(decode(context, blob, config.image));
        observeMacNativeDeallocation(config.image.nativeHandle(), released);
        [inner drain];
      }
      LOKA_VERIFY(released == 0);
    }
    LOKA_VERIFY(released == 0 && Access::held(context) == 0 && Access::queued(context) == 1);
  }
  LOKA_VERIFY(released == 1);
  [pool drain];
  LOKA_VERIFY(released == 1);
}
