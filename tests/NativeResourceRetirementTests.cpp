#include "NativeResourceRetirementTests.hpp"
#include "app/internal/NativeResourceReservation.hpp"
#include "app/core/App.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/nodes/nestable/Fragment.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "core/Operation.hpp"
#include "core/resource/Image.hpp"
#include "dsl/flow/Flow.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullWindow.hpp"
#include "testing/app/NativeResourceRetirementTestAccess.hpp"
#include "support/LokaAllocFailure.hpp"
#include "support/TestVerify.hpp"
#include <cassert>
#include <cstdio>
#if defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace
{
  using loka::app::internal::Reservation;
  using loka::core::resource::Image;
  typedef loka::app::testing::NativeResourceRetirementTestAccess Access;
  namespace alloc = loka::core::testing;

  struct Config : AppConfigurable
  {
    explicit Config(PlatformContext *context)
        : AppConfigurable(context)
    {
    }
    virtual void compose(AppComposition &) {}
    Image image;
  };

  struct TestApp : App
  {
    explicit TestApp(AppConfigurable *config)
        : App(config)
    {
    }
    virtual void quit() {}
    using App::admitAndApplyWindows;
    using App::reclaimWindows;
    void complete()
    {
      loka::core::Operation turn;
      this->admitAndApplyWindows();
      turn.close();
      this->reclaimWindows();
    }
    void install(Window *window)
    {
      this->group_ = new AppComponentGroup(std::vector<AppComponent *>(1, window));
    }
  };

  struct Handle
  {
    Handle()
        : calls(0),
          drop(0),
          reenter(0),
          context(0),
          observedInFlight(0)
    {
    }
    int calls;
    Image *drop;
    TestApp *reenter;
    PlatformContext *context;
    std::size_t observedInFlight;
    static void dispose(void *value)
    {
      Handle &handle = *static_cast<Handle *>(value);
      ++handle.calls;
      if (handle.context)
        handle.observedInFlight = Access::inFlight(*handle.context);
      if (handle.drop)
        *handle.drop = Image();
      if (handle.reenter)
        handle.reenter->complete();
    }
  };

  Image makeImage(PlatformContext &context, Handle &handle)
  {
    Reservation reservation(context, &Handle::dispose);
    Image image;
    LOKA_VERIFY(reservation.isValid());
    LOKA_VERIFY(reservation.publishImage(&handle, 4, 3, image));
    LOKA_VERIFY(!reservation.isValid());
    return image;
  }

  void empty(const PlatformContext &context)
  {
    LOKA_VERIFY(Access::held(context) == 0);
    LOKA_VERIFY(Access::queued(context) == 0);
    LOKA_VERIFY(Access::inFlight(context) == 0);
  }

#if defined(LOKA_LIFECYCLE_AUDIT) && defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  void destroyContext(void *value)
  {
    delete static_cast<PlatformContext *>(value);
  }
  void reserveDuringDispose(void *value)
  {
    Reservation forbidden(*static_cast<PlatformContext *>(value), &Handle::dispose);
  }
#endif

  struct Fixture
  {
    NullPlatformContext context;
    Config config;
    TestApp app;
    Fixture()
        : config(&context),
          app(&config)
    {
    }
  };

  struct DropInFlow
  {
    typedef int In;
    typedef int Out;
    Image *image;
    Handle *handle;
    PlatformContext *context;
    DropInFlow(Image &i, Handle &h, PlatformContext &c)
        : image(&i),
          handle(&h),
          context(&c)
    {
    }
    loka::dsl::StepRunStatus run(const int &input, int &output, loka::dsl::FlowError &) const
    {
      *this->image = Image();
      LOKA_VERIFY(this->handle->calls == 0);
      LOKA_VERIFY(Access::queued(*this->context) == 1);
      output = input;
      return loka::dsl::FLOW_STEP_SUCCEEDED;
    }
  };

  class ImageWindow : public NullWindow
  {
  public:
    ImageWindow(PlatformContext &context, const WindowProps &props, Handle &handle)
        : NullWindow(&context, props), image_(makeImage(context, handle)) {}
  private:
    Image image_;
  };

  PlatformContext *boundaryContext = 0;
  Handle *boundaryHandle = 0;
  class ImageBoundary;
  typedef loka::app::scene::BoundaryPropsFor<ImageBoundary> ImageBoundaryProps;
  class ImageBoundary : public loka::app::scene::BoundaryNodeFor<ImageBoundary>
  {
  public:
    explicit ImageBoundary(const ImageBoundaryProps &props)
        : loka::app::scene::BoundaryNodeFor<ImageBoundary>(props),
          image_(makeImage(*boundaryContext, *boundaryHandle))
    {
    }
    virtual void composeNode(loka::app::scene::NodeComposition &composition)
    {
      composition.declare(loka::app::Fragment());
    }

  private:
    Image image_;
  };
} // namespace

void testNativeRetirementLastCopyAndReplacement()
{
  Fixture f;
  Handle a, b;
  alloc::failLokaAllocRaw("unused", "unused", 0);
  {
    Image image = makeImage(f.context, a);
    Image copy = image;
    image = Image();
    LOKA_VERIFY(Access::held(f.context) == 1 && Access::queued(f.context) == 0);
    Image replacement = makeImage(f.context, b);
    const int attempts = alloc::lokaAllocRawAttempts();
    copy = replacement;
    LOKA_VERIFY(a.calls == 0 && b.calls == 0);
    LOKA_VERIFY(Access::held(f.context) == 1 && Access::queued(f.context) == 1);
    LOKA_VERIFY(alloc::lokaAllocRawAttempts() == attempts);
    f.app.reclaimWindows(); // No open admission: not a completion.
    LOKA_VERIFY(a.calls == 0);
    f.app.complete();
    LOKA_VERIFY(a.calls == 1 && b.calls == 0);
  }
  LOKA_VERIFY(Access::queued(f.context) == 1);
  f.app.complete();
  LOKA_VERIFY(a.calls == 1 && b.calls == 1);
  empty(f.context);
  LOKA_VERIFY(alloc::lokaAllocRawLive() == 0);
  alloc::allowLokaAllocRaw();
}

void testNativeRetirementFlowDrop()
{
  Fixture f;
  Handle handle;
  Image image = makeImage(f.context, handle);
  int input = 1;
  loka::dsl::FlowChain<int, int> flow =
      loka::dsl::Flow() | loka::dsl::Step(1, DropInFlow(image, handle, f.context)).input(&input);
  LOKA_VERIFY(flow.run());
  LOKA_VERIFY(handle.calls == 0);
  f.app.complete();
  LOKA_VERIFY(handle.calls == 1);
  empty(f.context);
}

void testNativeRetirementBoundaryReclaim()
{
  for (int active = 0; active != 2; ++active)
  {
    Fixture f;
    Handle handle, windowHandle;
    boundaryContext = &f.context;
    boundaryHandle = &handle;
    WindowProps props;
    props.scene(
        new loka::app::scene::Scene(new loka::app::scene::BoundaryDefinition<ImageBoundaryProps, ImageBoundary>()));
    NullWindow *window = new ImageWindow(f.context, props, windowHandle);
    f.app.install(window);
    f.app.complete();
    LOKA_VERIFY(Access::held(f.context) == 2);
    f.app.requestWindowClose(window);
    if (active)
    {
      loka::core::Operation outer;
      f.app.admitAndApplyWindows();
      LOKA_VERIFY(Access::held(f.context) == 1 && Access::queued(f.context) == 1);
      f.app.reclaimWindows();
      LOKA_VERIFY(handle.calls == 0 && windowHandle.calls == 0);
      LOKA_VERIFY(Access::held(f.context) == 0 && Access::queued(f.context) == 2);
    }
    else
    {
      f.app.admitAndApplyWindows();
      LOKA_VERIFY(Access::held(f.context) == 1 && Access::queued(f.context) == 1);
      LOKA_VERIFY(handle.calls == 0 && windowHandle.calls == 0);
      f.app.reclaimWindows();
      LOKA_VERIFY(handle.calls == 1 && windowHandle.calls == 1); // same tail, after Boundary reclamation
    }
    f.app.complete();
    LOKA_VERIFY(handle.calls == 1 && windowHandle.calls == 1);
    empty(f.context);
    boundaryContext = 0;
    boundaryHandle = 0;
  }
}

void testNativeRetirementJoinedOperation()
{
  Fixture f;
  Handle handle;
  Image image = makeImage(f.context, handle);
  loka::core::Operation outer;
  image = Image();
  loka::core::Operation joined;
  f.app.admitAndApplyWindows();
  LOKA_VERIFY(joined.close().status == loka::core::OPERATION_JOINED);
  f.app.reclaimWindows();
  LOKA_VERIFY(handle.calls == 0 && Access::queued(f.context) == 1);
  outer.close();
  f.app.complete();
  LOKA_VERIFY(handle.calls == 1);
  empty(f.context);
}

void testNativeRetirementReentrantSnapshot()
{
  Fixture f;
  Handle a, b;
  Image second = makeImage(f.context, b);
  a.drop = &second;
  a.reenter = &f.app;
  a.context = &f.context;
  Image first = makeImage(f.context, a);
  first = Image();
  f.app.complete();
  LOKA_VERIFY(a.calls == 1 && b.calls == 0);
  LOKA_VERIFY(a.observedInFlight == 1);
  LOKA_VERIFY(Access::queued(f.context) == 1 && Access::inFlight(f.context) == 0);
  f.app.complete();
  LOKA_VERIFY(a.calls == 1 && b.calls == 1);
  empty(f.context);
}

void testNativeRetirementReservationPaths()
{
  Fixture f;
  Handle handle;
  int acquisitions = 0;
  alloc::failLokaAllocRaw("NativeResourceRetirement", "Ticket", 1);
  {
    Reservation reservation(f.context, &Handle::dispose);
    if (reservation.isValid())
      ++acquisitions;
    LOKA_VERIFY(!reservation.isValid());
  }
  LOKA_VERIFY(acquisitions == 0 && alloc::lokaAllocRawAttempts() == 1);
  empty(f.context);
  alloc::allowLokaAllocRaw();
  alloc::failLokaAllocRaw("unused", "unused", 0);
  {
    Reservation earlyReturn(f.context, &Handle::dispose);
    LOKA_VERIFY(earlyReturn.isValid());
    LOKA_VERIFY(Access::held(f.context) == 1);
  }
  empty(f.context);
  {
    Reservation nullHandle(f.context, &Handle::dispose);
    Image image;
    LOKA_VERIFY(!nullHandle.publishImage(0, 4, 3, image));
    LOKA_VERIFY(nullHandle.isValid());
    LOKA_VERIFY(Access::held(f.context) == 1 && Access::queued(f.context) == 0);
  }
  empty(f.context);
  LOKA_VERIFY(alloc::lokaAllocRawAttempts() == 2 && alloc::lokaAllocRawLive() == 0);
  LOKA_VERIFY(handle.calls == 0);
  alloc::allowLokaAllocRaw();
}

void testNativeRetirementImageRefusal()
{
  Fixture f;
  Handle handles[2];
  const char *owners[] = {"Image", "Managed"};
  const char *types[] = {"Record", "ControlBlock"};
  for (int i = 0; i != 2; ++i)
  {
    alloc::failLokaAllocRaw(owners[i], types[i], 1);
    {
      Reservation reservation(f.context, &Handle::dispose);
      LOKA_VERIFY(reservation.isValid());
      Image image;
      LOKA_VERIFY(!reservation.publishImage(&handles[i], 4, 3, image));
      LOKA_VERIFY(!reservation.isValid());
      LOKA_VERIFY(!image.isValid());
      LOKA_VERIFY(handles[i].calls == 0);
      // Exactly one synchronous releaser transfer; no allocation in that transfer.
      LOKA_VERIFY(Access::held(f.context) == 0 && Access::queued(f.context) == 1);
      LOKA_VERIFY(alloc::lokaAllocRawAttempts() == i + 2);
      LOKA_VERIFY(alloc::lokaAllocRawLive() == 1);
    }
    f.app.complete();
    LOKA_VERIFY(handles[i].calls == 1);
    empty(f.context);
    LOKA_VERIFY(alloc::lokaAllocRawLive() == 0);
    alloc::allowLokaAllocRaw();
  }
}

void testNativeRetirementExitCascade()
{
  Handle a, b, c;
  Image second, third;
  alloc::failLokaAllocRaw("unused", "unused", 0);
  {
    NullPlatformContext context;
    second = makeImage(context, b);
    third = makeImage(context, c);
    a.drop = &second;
    b.drop = &third;
    {
      Config config(&context);
      config.image = makeImage(context, a);
      {
        TestApp app(&config);
        app.complete();
      }
      LOKA_VERIFY(a.calls == 0 && Access::held(context) == 3);
    }
    LOKA_VERIFY(a.calls == 0 && Access::queued(context) == 1 && Access::held(context) == 2);
  }
  LOKA_VERIFY(a.calls == 1 && b.calls == 1 && c.calls == 1);
  LOKA_VERIFY(!second.isValid() && !third.isValid());
  LOKA_VERIFY(alloc::lokaAllocRawLive() == 0); // all three lists reclaimed
  alloc::allowLokaAllocRaw();
}

void testNativeRetirementIndependentContexts()
{
  Fixture first, second;
  Handle a, b;
  {
    Image image = makeImage(first.context, a);
  }
  {
    Image image = makeImage(second.context, b);
  }
  first.app.complete();
  LOKA_VERIFY(a.calls == 1 && b.calls == 0);
  empty(first.context);
  LOKA_VERIFY(Access::queued(second.context) == 1);
  second.app.complete();
  LOKA_VERIFY(b.calls == 1);
  empty(second.context);
  // Optional config and optional context are both legitimate no-service Apps.
  TestApp noConfig(0);
  noConfig.complete();
  Config noContext(0);
  TestApp noService(&noContext);
  noService.complete();
}

void testNativeRetirementLateOwner()
{
#if !defined(LOKA_LIFECYCLE_AUDIT)
  Handle handle;
  Image image;
  alloc::failLokaAllocRaw("unused", "unused", 0);
  {
    NullPlatformContext context;
    image = makeImage(context, handle);
  }
  LOKA_VERIFY(handle.calls == 0 && alloc::lokaAllocRawLive() == 3);
  image = Image();
  LOKA_VERIFY(handle.calls == 0 && alloc::lokaAllocRawLive() == 0);
  alloc::allowLokaAllocRaw();
#elif defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  const pid_t child = fork();
  LOKA_VERIFY(child >= 0);
  if (child == 0)
  {
    Handle handle;
    Image image;
    {
      NullPlatformContext context;
      image = makeImage(context, handle);
    }
    _exit(0);
  }
  int status = 0;
  LOKA_VERIFY(waitpid(child, &status, 0) == child);
  LOKA_VERIFY(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
#else
  std::printf("[skip] late owner audit death pin requires Linux debug without ASan; containment requires audit off.\n");
#endif
}

void testNativeRetirementShutdownPreconditions()
{
#if defined(LOKA_LIFECYCLE_AUDIT) && defined(__linux__) && !defined(__SANITIZE_ADDRESS__) && !defined(NDEBUG)
  for (int misuse = 0; misuse != 4; ++misuse)
  {
    const pid_t child = fork();
    LOKA_VERIFY(child >= 0);
    if (child == 0)
    {
      NullPlatformContext *context = new NullPlatformContext();
      if (misuse == 0)
      {
        loka::core::Operation active;
        delete context;
      }
      else if (misuse == 1)
      {
        Reservation reservation(*context, &Handle::dispose);
        delete context;
      }
      else
      {
        Reservation reservation(*context, misuse == 2 ? &destroyContext : &reserveDuringDispose);
        Image image;
        LOKA_VERIFY(reservation.publishImage(context, 1, 1, image));
        image = Image();
        Access::drain(*context);
      }
      _exit(0);
    }
    int status = 0;
    LOKA_VERIFY(waitpid(child, &status, 0) == child);
    LOKA_VERIFY(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
  }
#else
  std::printf("[skip] shutdown precondition death pins require lifecycle audit and Linux debug without ASan.\n");
#endif
}
