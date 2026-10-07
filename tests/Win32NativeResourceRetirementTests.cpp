#include "Win32NativeResourceRetirementTests.hpp"
#include "Win32PlatformContext.hpp"
#include "Win32Window.hpp"
#include "Win32ScenePlatformController.hpp"
#include "Win32BitmapCapture.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/nodes/Text.hpp"
#include "core/Operation.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "testing/app/NativeResourceRetirementTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "support/LokaAllocFailure.hpp"
#include "support/TestVerify.hpp"

namespace
{
  int captureDcCalls = 0;
  HDC WINAPI CountedGetWindowDC(HWND window)
  {
    ++captureDcCalls;
    return GetWindowDC(window);
  }
}

// Compile the same capture body with only the native acquisition door counted.
// The ordinary linked producer is exercised by the real Window/control pins.
// This copy makes even acquire-then-clean-up mutations observable, without
// adding test counters or hooks to the shipped rail.
#define GetWindowDC CountedGetWindowDC
#define CaptureWindowClientBitmap ObservedCaptureWindowClientBitmap
#include "../win32/src/Win32BitmapCapture.cpp"
#undef CaptureWindowClientBitmap
#undef GetWindowDC

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

  class CompletionApp : public Win32App
  {
  public:
    explicit CompletionApp(Config &config)
        : Win32App(&config, GetModuleHandleW(0), SW_HIDE) {}
    virtual ~CompletionApp()
    {
      // Stack fixture owns the window; withdraw the App's borrowed row first.
      if (this->group_)
        this->group_->build();
    }
    void install(Window &window)
    {
      this->group_ = new AppComponentGroup(std::vector<AppComponent *>(1, &window));
    }
    void complete()
    {
      loka::core::Operation turn;
      this->admitAndApplyWindows();
      turn.close();
      this->reclaimWindows();
    }
  };

  struct Fixture
  {
    Win32PlatformContext context;
    Config config;
    CompletionApp app;
    Fixture() : config(this->context), app(this->config) {}
  };

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
    blob.mutableBytes().assign(bytes, bytes + sizeof(bytes));
    return blob;
  }

  bool decode(Win32PlatformContext &context, const Blob &blob, Image &image)
  {
    return context.createImageFromBlob(blob, 0, blob.bytes().size(), image);
  }

  DWORD gdiCount()
  {
    GdiFlush();
    return GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
  }

  bool bitmapExists(HBITMAP bitmap)
  {
    BITMAP info;
    return GetObjectW(bitmap, static_cast<int>(sizeof(info)), &info) != 0;
  }

  void empty(const PlatformContext &context)
  {
    LOKA_VERIFY(Access::held(context) == 0);
    LOKA_VERIFY(Access::queued(context) == 0);
    LOKA_VERIFY(Access::inFlight(context) == 0);
  }

  void warmDecode(Fixture &fixture, const Blob &blob)
  {
    Image image;
    LOKA_VERIFY(decode(fixture.context, blob, image));
    image = Image();
    fixture.app.complete();
    empty(fixture.context);
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

void testWin32NativeRetirementDecode()
{
  // Mutation: restore decode's inline Image releaser, or omit the App drain.
  Fixture fixture;
  const Blob blob = bitmapBlob();
  warmDecode(fixture, blob);
  const DWORD baseline = gdiCount();
  Image image;
  LOKA_VERIFY(decode(fixture.context, blob, image));
  HBITMAP bitmap = static_cast<HBITMAP>(image.nativeHandle());
  Image copy = image;
  image = Image();
  LOKA_VERIFY(Access::held(fixture.context) == 1);
  copy = Image();
  LOKA_VERIFY(bitmapExists(bitmap));
  LOKA_VERIFY(Access::queued(fixture.context) == 1);
  fixture.app.complete();
  LOKA_VERIFY(!bitmapExists(bitmap));
  empty(fixture.context);
  LOKA_VERIFY(gdiCount() == baseline);
}

void testWin32NativeRetirementDecodeRefusal()
{
  // Mutation: leak a reservation on WIC refusal or acquire after ticket refusal.
  Fixture fixture;
  const Blob valid = bitmapBlob();
  warmDecode(fixture, valid);
  Blob malformed = Blob::Create();
  malformed.mutableBytes().assign(16, 0xff);
  Blob zeroDimension = bitmapBlob();
  zeroDimension.mutableBytes()[18] = 0; // BMP width = zero, rejected by WIC.
  const DWORD baseline = gdiCount();
  Image image;
  LOKA_VERIFY(!decode(fixture.context, Blob(), image));
  empty(fixture.context);
  LOKA_VERIFY(!fixture.context.createImageFromBlob(valid, 0, 0, image));
  empty(fixture.context);
  LOKA_VERIFY(!decode(fixture.context, malformed, image));
  empty(fixture.context);
  LOKA_VERIFY(!decode(fixture.context, zeroDimension, image));
  empty(fixture.context);
  LOKA_VERIFY(!image.isValid());
  LOKA_VERIFY(gdiCount() == baseline);
  alloc::failLokaAllocRaw("NativeResourceRetirement", "Ticket", 1);
  LOKA_VERIFY(!decode(fixture.context, valid, image));
  LOKA_VERIFY(!image.isValid());
  empty(fixture.context);
  LOKA_VERIFY(alloc::lokaAllocRawLive() == 0);
  LOKA_VERIFY(gdiCount() == baseline);
  alloc::allowLokaAllocRaw();
}

void testWin32NativeRetirementPublicationRefusal()
{
  // Mutations: inline FromNative refusal disposal; cancel consumed ticket;
  // leave a deleted ticket queued (second completion must be harmless).
  Fixture fixture;
  const Blob blob = bitmapBlob();
  warmDecode(fixture, blob);
  const char *owners[] = {"Image", "Managed"};
  const char *types[] = {"Record", "ControlBlock"};
  for (int index = 0; index != 2; ++index)
  {
    const DWORD baseline = gdiCount();
    alloc::failLokaAllocRaw(owners[index], types[index], 1);
    Image image;
    LOKA_VERIFY(!decode(fixture.context, blob, image));
    LOKA_VERIFY(!image.isValid());
    LOKA_VERIFY(Access::held(fixture.context) == 0);
    LOKA_VERIFY(Access::queued(fixture.context) == 1);
    HBITMAP bitmap = static_cast<HBITMAP>(Access::queuedHandle(fixture.context));
    LOKA_VERIFY(bitmapExists(bitmap));
    LOKA_VERIFY(gdiCount() == baseline + 1);
    fixture.app.complete();
    LOKA_VERIFY(!bitmapExists(bitmap));
    empty(fixture.context);
    fixture.app.complete();
    LOKA_VERIFY(gdiCount() == baseline);
    LOKA_VERIFY(alloc::lokaAllocRawLive() == 0);
    alloc::allowLokaAllocRaw();
  }
}

void testWin32NativeRetirementCapture()
{
  // Mutations: omit Window's ancestor borrow; leave either control inline;
  // bind to another context; retain a controller in the published ticket.
  Win32PlatformContext context;
  Config config(context);
  HBITMAP survivingBitmap = 0;
  {
    Win32Window window(&context, captureProps());
    CompletionApp app(config);
    app.install(window);
    app.complete();
    LOKA_VERIFY(window.hwnd());
    UpdateWindow(window.hwnd());
    loka::app::scene::BoundaryNode *root =
        loka::dsl::testing::SceneTestAccess::rootBoundary(*window.scene());
    LOKA_VERIFY(root && root->childrenHead() && root->childrenHead()->asNestable());
    loka::app::scene::Node *node = root->childrenHead()->asNestable()->childrenHead();
    const DWORD baseline = gdiCount();
    for (int index = 0; index != 2; ++index)
    {
      LOKA_VERIFY(node && node->getContext());
      const loka::app::scene::ICapturableBitmap *capture = node->getContext()->asCapturableBitmap();
      LOKA_VERIFY(capture);
      Image image;
      LOKA_VERIFY(capture->captureBitmap(image));
      HBITMAP bitmap = static_cast<HBITMAP>(image.nativeHandle());
      image = Image();
      LOKA_VERIFY(bitmapExists(bitmap));
      LOKA_VERIFY(Access::queued(context) == 1);
      app.complete();
      LOKA_VERIFY(!bitmapExists(bitmap));
      empty(context);
      LOKA_VERIFY(gdiCount() == baseline);
      node = node->nextInComposition;
    }
    // A live capture may outlast its originating Window and controller.
    node = root->childrenHead()->asNestable()->childrenHead();
    LOKA_VERIFY(node->getContext()->asCapturableBitmap()->captureBitmap(config.image));
    survivingBitmap = static_cast<HBITMAP>(config.image.nativeHandle());
  }
  LOKA_VERIFY(bitmapExists(survivingBitmap));
  config.image = Image();
  LOKA_VERIFY(bitmapExists(survivingBitmap));
  CompletionApp app(config);
  app.complete();
  LOKA_VERIFY(!bitmapExists(survivingBitmap));
  empty(context);
}

void testWin32NativeRetirementCaptureRefusal()
{
  // Mutation: null-owner fallback captures inline, or reservation follows DC work.
  Fixture fixture;
  HWND window = CreateWindowExW(0, L"STATIC", L"capture refusal", WS_OVERLAPPEDWINDOW,
                                0, 0, 160, 120, 0, 0, GetModuleHandleW(0), 0);
  LOKA_VERIFY(window);
  // BitBlt from a never-shown window's DC fails with ERROR_INVALID_HANDLE, so
  // the positive control needs a visible window.
  ShowWindow(window, SW_SHOWNOACTIVATE);
  UpdateWindow(window);
  {
    Win32ScenePlatformController controller(window,
        loka::win32::Win32DisplayScale(96, loka::app::RailMetrics()));
    const DWORD baseline = gdiCount();
    Image image;
    LOKA_VERIFY(!controller.captureWindowClientBitmap(window, image));
    LOKA_VERIFY(!image.isValid());
    LOKA_VERIFY(gdiCount() == baseline);
    captureDcCalls = 0;
    LOKA_VERIFY(!loka::win32::ObservedCaptureWindowClientBitmap(0, window, image));
    LOKA_VERIFY(captureDcCalls == 0);
    alloc::failLokaAllocRaw("NativeResourceRetirement", "Ticket", 1);
    LOKA_VERIFY(!loka::win32::ObservedCaptureWindowClientBitmap(&fixture.context, window, image));
    LOKA_VERIFY(captureDcCalls == 0);
    empty(fixture.context);
    LOKA_VERIFY(gdiCount() == baseline);
    LOKA_VERIFY(alloc::lokaAllocRawLive() == 0);
    alloc::allowLokaAllocRaw();
    // Positive control establishes that the counted native door really fires.
    LOKA_VERIFY(loka::win32::ObservedCaptureWindowClientBitmap(&fixture.context, window, image));
    LOKA_VERIFY(captureDcCalls == 1);
    image = Image();
    fixture.app.complete();
    empty(fixture.context);
    LOKA_VERIFY(gdiCount() == baseline);

    const char *owners[] = {"Image", "Managed"};
    const char *types[] = {"Record", "ControlBlock"};
    for (int index = 0; index != 2; ++index)
    {
      alloc::failLokaAllocRaw(owners[index], types[index], 1);
      LOKA_VERIFY(!loka::win32::CaptureWindowClientBitmap(&fixture.context, window, image));
      LOKA_VERIFY(!image.isValid());
      LOKA_VERIFY(Access::held(fixture.context) == 0 && Access::queued(fixture.context) == 1);
      HBITMAP bitmap = static_cast<HBITMAP>(Access::queuedHandle(fixture.context));
      LOKA_VERIFY(bitmapExists(bitmap));
      LOKA_VERIFY(gdiCount() == baseline + 1);
      fixture.app.complete();
      LOKA_VERIFY(!bitmapExists(bitmap));
      empty(fixture.context);
      LOKA_VERIFY(gdiCount() == baseline);
      LOKA_VERIFY(alloc::lokaAllocRawLive() == 0);
      alloc::allowLokaAllocRaw();
    }
  }
  LOKA_VERIFY(DestroyWindow(window));
}

void testWin32NativeRetirementActiveOperation()
{
  // Mutation: remove Operation exclusion or treat joined close as outer close.
  Fixture fixture;
  const Blob blob = bitmapBlob();
  Image image;
  LOKA_VERIFY(decode(fixture.context, blob, image));
  HBITMAP bitmap = static_cast<HBITMAP>(image.nativeHandle());
  {
    loka::core::Operation outer;
    image = Image();
    fixture.app.complete();
    LOKA_VERIFY(bitmapExists(bitmap));
    LOKA_VERIFY(Access::queued(fixture.context) == 1);
    outer.close();
  }
  LOKA_VERIFY(bitmapExists(bitmap));
  fixture.app.complete();
  LOKA_VERIFY(!bitmapExists(bitmap));
  empty(fixture.context);
}

void testWin32NativeRetirementFinalDrain()
{
  // Mutation: remove context final drain or put shutdown disposal only in App.
  const Blob blob = bitmapBlob();
  { Fixture warm; warmDecode(warm, blob); }
  const DWORD baseline = gdiCount();
  HBITMAP bitmap = 0;
  {
    Win32PlatformContext context;
    {
      Config config(context);
      {
        CompletionApp app(config);
        LOKA_VERIFY(decode(context, blob, config.image));
        bitmap = static_cast<HBITMAP>(config.image.nativeHandle());
      }
      LOKA_VERIFY(bitmapExists(bitmap));
    }
    LOKA_VERIFY(Access::held(context) == 0);
    LOKA_VERIFY(Access::queued(context) == 1);
    LOKA_VERIFY(bitmapExists(bitmap));
  }
  LOKA_VERIFY(!bitmapExists(bitmap));
  LOKA_VERIFY(gdiCount() == baseline);
}
