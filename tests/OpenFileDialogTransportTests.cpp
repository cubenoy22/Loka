#include "OpenFileDialogTransportTests.hpp"
#include "support/TestVerify.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "platform/null/NullWindow.hpp"
#include "app/core/DialogResultTransport.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "app/nodes/nestable/Show.hpp"
#include "app/scene/projection/RetainedNodeHandler.hpp"
#include "testing/core/StateTrackerTestAccess.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include <cassert>
#include <vector>

namespace loka
{
  namespace app
  {
    namespace testing
    {
      class DialogResultTestAccess
      {
      public:
        static size_t census(const DialogResultTransport &transport)
        {
          const DialogResultTransport::Chain *chains[] = {
              &transport.reserved_, &transport.pending_, &transport.active_, &transport.retired_};
          size_t count = 0;
          for (size_t i = 0; i != 4; ++i)
            for (DialogResultTransport::Entry *entry = chains[i]->head; entry; entry = entry->next)
              ++count;
          return count;
        }
      };
    } // namespace testing
  } // namespace app
} // namespace loka

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  typedef DialogResultTransport Transport;
  typedef loka::app::testing::DialogResultTestAccess Access;
  Window *g_window = 0;
  bool g_destroyOnDetach = false;
  class DialogOwner;
  DialogOwner *g_owner = 0;
  class DialogContext;
  DialogContext *g_context = 0;
  std::vector<int> *g_events = 0;

  /** Null native-equivalent producer: actual dialog node, shared transport. */
  class DialogContext : public NodeContext
  {
  public:
    explicit DialogContext(OpenFileDialogNode *node)
        : node_(node),
          registration_(0)
    {
      g_context = this;
    }
    virtual ~DialogContext()
    {
      this->cancel();
      g_context = 0;
      g_events->push_back(3);
    }
    void readLifecycleFactOnAttach()
    {
      if (this->node_->lifecycleFact() == NODE_FACT_ATTACHED)
        this->attach();
    }
    virtual void onFactChanged(NodeLifecycleFact, NodeLifecycleFact next)
    {
      g_events->push_back(4);
      if (next == NODE_FACT_ATTACHED)
        this->attach();
      else
        this->cancel();
    }
    virtual void onPropsApplied()
    {
      if (this->registration_ && !this->registration_->matches(this->node_->props))
        this->cancel();
    }
    void attach()
    {
      if (!this->registration_)
        this->registration_ = g_window->dialogResults().reserve(this->node_->props);
    }
    void cancel()
    {
      Transport::Registration *registration = this->registration_;
      this->registration_ = 0;
      delete registration;
    }
    void produce()
    {
      Transport::ReturnPort port(this->registration_);
      LOKA_VERIFY(port.seal(FileChooserResult::File(loka::file::File("accepted-dialog-result.png"))) == g_window);
    }
    OpenFileDialogNode *node_;
    Transport::Registration *registration_;
  };

  class DialogHandler : public RetainedNodeHandler<DialogHandler, OpenFileDialogNode, DialogContext>
  {
  public:
    static OpenFileDialogNode *cast(Node *node)
    {
      return node->asOpenFileDialogNode();
    }
    static DialogContext *create(OpenFileDialogNode *node, IPlatformController *, const LayoutState &)
    {
      return new DialogContext(node);
    }
  };

  typedef BoundaryPropsFor<DialogOwner> DialogOwnerProps;
  class DialogOwner : public StdCompositionBoundaryNodeBase<DialogOwnerProps>
  {
  public:
    explicit DialogOwner(const DialogOwnerProps &props)
        : StdCompositionBoundaryNodeBase<DialogOwnerProps>(props)
    {
      this->state(this->shown_, true);
      this->state(this->result_, FileChooserResult());
      g_owner = this;
    }
    virtual ~DialogOwner()
    {
      g_owner = 0;
    }
    virtual void composeNode(NodeComposition &composition)
    {
      ShowDefinition seat = Show(*this->shown_.state());
      if (g_destroyOnDetach)
        seat.destroyOnDetach();
      composition.declare(seat << OpenFileDialog().result(this->result_).onResult(&this->emitter_));
    }
    NodeState<bool> shown_;
    NodeState<FileChooserResult> result_;
    loka::core::EmitterState emitter_;
  };

  void countEvent(void *data)
  {
    ++*static_cast<int *>(data);
  }

  struct Fixture
  {
    explicit Fixture(bool destroyOnDetach = false)
        : window(0, WindowProps()),
          app(window)
    {
      g_destroyOnDetach = destroyOnDetach;
      g_events = &this->events;
      g_window = &this->window;
      LOKA_VERIFY(this->window.scenePlatformController()->registerNodeHandler(&this->handler));
      LOKA_VERIFY(this->window.sceneManager()->commitTransaction(
          0, new Scene(new NodeDefinition<DialogOwnerProps, DialogOwner>(DialogOwnerProps()))));
      this->app.flush();
      assert(g_owner && g_context);
    }
    ~Fixture()
    {
      this->window.destroyScenePlatform();
      g_window = 0;
      g_events = 0;
    }
    std::vector<int> events;
    DialogHandler handler;
    NullWindow window;
    WindowAdmissionTestApp app;
  };

  void cancelObserver(void *)
  {
    g_context->cancel();
  }

  class OwnedApp : public App
  {
  public:
    OwnedApp()
        : App(0)
    {
      this->group_ = new AppComponentGroup(std::vector<AppComponent *>());
    }
    void adopt(Window *window)
    {
      this->group_->adopt(window);
    }
    virtual void quit() {}
    void flush()
    {
      this->flushWindowInvalidations();
    }
    bool pending() const
    {
      return this->hasPendingWindowAdmission();
    }
  };

  class DeathWindow : public NullWindow
  {
  public:
    explicit DeathWindow(int &deaths)
        : NullWindow(0, WindowProps()),
          deaths_(deaths)
    {
    }
    virtual ~DeathWindow()
    {
      ++this->deaths_;
    }

  private:
    int &deaths_;
  };

  struct CloseProbe
  {
    OwnedApp *app;
    Window *window;
    int *deaths;
    static void close(void *data)
    {
      CloseProbe &probe = *static_cast<CloseProbe *>(data);
      probe.app->requestWindowClose(probe.window);
      probe.app->flush();
      assert(*probe.deaths == 0);
    }
  };
} // namespace

void testOpenFileDialogTransportRetiredNodeDropsProducedResult()
{
  Fixture fixture(true);
  int writes = 0, emits = 0;
  g_owner->result_.bind(&countEvent, &writes, false);
  g_owner->emitter_.bind(&countEvent, &emits, false);
  g_context->produce();
  assert(Access::census(fixture.window.dialogResults()) == 1);
  // Retire the actual node via its lifecycle door while ancestor targets survive.
  g_owner->shown_.set(false);
  fixture.window.flushSceneInvalidation();
  assert(!g_context);
  const size_t contextEvents = fixture.events.size();
  fixture.app.flush();
  assert(writes == 0 && emits == 0);
  assert(fixture.events.size() == contextEvents);
  assert(Access::census(fixture.window.dialogResults()) == 0);
  g_owner->result_.unbind(&countEvent, &writes);
  g_owner->emitter_.unbind(&countEvent, &emits);
}

void testOpenFileDialogTransportRetainedDetachAndFreshReattach()
{
  Fixture fixture;
  int writes = 0, emits = 0;
  g_owner->result_.bind(&countEvent, &writes, false);
  g_owner->emitter_.bind(&countEvent, &emits, false);
  DialogContext *context = g_context;
  g_context->produce();
  g_owner->shown_.set(false);
  fixture.window.flushSceneInvalidation();
  assert(g_context == context && !context->registration_);
  fixture.app.flush();
  assert(writes == 0 && emits == 0 && Access::census(fixture.window.dialogResults()) == 0);
  g_owner->shown_.set(true);
  fixture.app.flush();
  assert(g_context == context && context->registration_);
  g_context->produce();
  fixture.app.flush();
  assert(writes == 1 && emits == 1);
  fixture.app.flush();
  assert(writes == 1 && emits == 1 && Access::census(fixture.window.dialogResults()) == 0);
  g_owner->result_.unbind(&countEvent, &writes);
  g_owner->emitter_.unbind(&countEvent, &emits);
}

void testOpenFileDialogTransportOwnerReclaimDropsProducedResult()
{
  Fixture fixture;
  g_context->produce();
  fixture.window.teardownScene();
  assert(!g_owner && !g_context);
  fixture.app.flush();
  assert(Access::census(fixture.window.dialogResults()) == 0);
}

void testOpenFileDialogTransportObserverCancellationSuppressesEmitter()
{
  Fixture fixture;
  int emits = 0;
  g_owner->result_.bind(&cancelObserver, 0, false);
  g_owner->emitter_.bind(&countEvent, &emits, false);
  g_context->produce();
  fixture.app.flush();
  assert(g_owner->result_.get().kind == FileChooserResult::RESULT_FILE && emits == 0);
  fixture.app.flush();
  assert(Access::census(fixture.window.dialogResults()) == 0);
  g_owner->result_.unbind(&cancelObserver, 0);
  g_owner->emitter_.unbind(&countEvent, &emits);
}

void testOpenFileDialogTransportCloseMakesProgressWithoutInput()
{
  int deaths = 0, emits = 0;
  OwnedApp app;
  DeathWindow *window = new DeathWindow(deaths);
  app.adopt(window);
  loka::core::MutableState<FileChooserResult> result;
  loka::core::PushStateTracker tracker;
  tracker.addState(&result);
  loka::core::EmitterState emitter;
  emitter.bind(&countEvent, &emits, false);
  NodeState<FileChooserResult> channel(&result, &tracker);
  CloseProbe probe = {&app, window, &deaths};
  channel.bind(&CloseProbe::close, &probe, false);
  Transport::Registration *registration =
      window->dialogResults().reserve(OpenFileDialogProps().result(channel).onResult(&emitter));
  Transport::ReturnPort port(registration);
  LOKA_VERIFY(port.seal(FileChooserResult::Canceled()) == window);
  Transport::Registration *later = window->dialogResults().reserve(OpenFileDialogProps().onResult(&emitter));
  Transport::ReturnPort laterPort(later);
  LOKA_VERIFY(laterPort.seal(FileChooserResult::Canceled()) == window);
  app.flush();
  assert(deaths == 0 && emits == 0 && app.pending());
  app.flush();
  assert(deaths == 1 && !app.pending());
  delete registration;
  delete later;
  channel.unbind(&CloseProbe::close, &probe);
  emitter.unbind(&countEvent, &emits);
}

void testOpenFileDialogTransportReservedReturnIsRevoked()
{
  NullWindow window(0, WindowProps());
  WindowAdmissionTestApp app(window);
  loka::core::EmitterState emitter;
  Transport::Registration *registration = window.dialogResults().reserve(OpenFileDialogProps().onResult(&emitter));
  Transport::ReturnPort port(registration);
  assert(!window.dialogResults().hasRunnableWork());
  window.dialogResults().close();
  app.flush();
  assert(Access::census(window.dialogResults()) == 0);
  LOKA_VERIFY(port.seal(FileChooserResult::Canceled()) == 0);
  delete registration;

  Fixture fixture;
  Transport::ReturnPort contextPort(g_context->registration_);
  g_owner->shown_.set(false);
  fixture.window.flushSceneInvalidation();
  fixture.app.flush();
  assert(Access::census(fixture.window.dialogResults()) == 0);
  LOKA_VERIFY(contextPort.seal(FileChooserResult::Canceled()) == 0);
}

namespace
{
  struct CommitProbe
  {
    WindowAdmissionTestApp *app;
    loka::core::PushStateTracker *tracker;
    loka::core::StateBase *result;
    std::vector<int> *commits;
    static void admit(void *data)
    {
      CommitProbe &probe = *static_cast<CommitProbe *>(data);
      assert(probe.tracker->phase() == loka::core::TRACKER_COMMIT);
      const loka::core::PushStateTracker::StateList &dirty = probe.tracker->committedDirtyStates();
      if (probe.commits->empty())
      {
        probe.commits->push_back(1);
        probe.app->flush();
        LOKA_VERIFY(loka::core::testing::PushStateTrackerTestAccess::nextDirtyCount(*probe.tracker) == 1);
        assert(dirty.size() == 1 && dirty[0] != probe.result);
      }
      else
      {
        probe.commits->push_back(2);
        assert(dirty.size() == 1 && dirty[0] == probe.result);
      }
    }
  };

  struct ReentrantProducer
  {
    Transport *transport;
    Transport::Registration *registration;
    loka::core::EmitterState *emitter;
    WindowAdmissionTestApp *app;
    static void produce(void *data)
    {
      ReentrantProducer &probe = *static_cast<ReentrantProducer *>(data);
      probe.registration = probe.transport->reserve(OpenFileDialogProps().onResult(probe.emitter));
      Transport::ReturnPort port(probe.registration);
      LOKA_VERIFY(port.seal(FileChooserResult::Canceled()) != 0);
      probe.app->flush(); // Nested admission returns, leaving the new batch pending.
    }
  };
} // namespace

void testOpenFileDialogTransportCommitIntakeUsesNext()
{
  NullWindow window(0, WindowProps());
  WindowAdmissionTestApp app(window);
  loka::core::MutableState<int> trigger(0);
  loka::core::MutableState<FileChooserResult> result;
  loka::core::PushStateTracker tracker;
  tracker.addState(&trigger);
  tracker.addState(&result);
  NodeState<FileChooserResult> channel(&result, &tracker);
  loka::core::EmitterState emitter;
  int emits = 0;
  emitter.bind(&countEvent, &emits, false);
  std::vector<int> commits;
  CommitProbe probe = {&app, &tracker, &result, &commits};
  tracker.setInvalidateCallback(&CommitProbe::admit, &probe);
  Transport::Registration *registration =
      window.dialogResults().reserve(OpenFileDialogProps().result(channel).onResult(&emitter));
  Transport::ReturnPort port(registration);
  LOKA_VERIFY(port.seal(FileChooserResult::Error(665)) == &window);
  {
    loka::core::StateTrackerGuard guard(&tracker);
    trigger.set(1);
  }
  assert(commits.size() == 2 && commits[0] == 1 && commits[1] == 2 && emits == 1);
  tracker.setInvalidateCallback(0, 0);
  app.flush();
  delete registration;
  emitter.unbind(&countEvent, &emits);
}

void testOpenFileDialogTransportRetargetAndFiniteBatch()
{
  Fixture fixture;
  int emits = 0;
  loka::core::EmitterState replacement;
  replacement.bind(&countEvent, &emits, false);
  g_context->produce();
  OpenFileDialogDefinition same;
  same.result(g_owner->result_).onResult(&g_owner->emitter_);
  LOKA_VERIFY(same.applyPropsToNode(g_context->node_));
  assert(g_context->registration_);
  OpenFileDialogDefinition changed;
  changed.onResult(&replacement);
  LOKA_VERIFY(changed.applyPropsToNode(g_context->node_));
  assert(!g_context->registration_);
  fixture.app.flush();
  assert(g_owner->result_.get().kind == FileChooserResult::RESULT_NONE && emits == 0);

  loka::core::EmitterState first;
  ReentrantProducer probe = {&fixture.window.dialogResults(), 0, &replacement, &fixture.app};
  first.bind(&ReentrantProducer::produce, &probe, false);
  Transport::Registration *registration =
      fixture.window.dialogResults().reserve(OpenFileDialogProps().onResult(&first));
  Transport::ReturnPort port(registration);
  LOKA_VERIFY(port.seal(FileChooserResult::Canceled()) == &fixture.window);
  fixture.app.flush();
  assert(emits == 0);
  fixture.app.flush();
  assert(emits == 1);
  fixture.app.flush();
  assert(Access::census(fixture.window.dialogResults()) == 0);
  delete registration;
  delete probe.registration;
  first.unbind(&ReentrantProducer::produce, &probe);
  replacement.unbind(&countEvent, &emits);
}

void testOpenFileDialogTransportShutdownRevokesBothPopulations()
{
  int deaths = 0, emits = 0;
  loka::core::EmitterState emitter;
  emitter.bind(&countEvent, &emits, false);
  Transport::Registration *firstRegistration = 0, *secondRegistration = 0;
  {
    OwnedApp app;
    DeathWindow *first = new DeathWindow(deaths), *second = new DeathWindow(deaths);
    app.adopt(first);
    app.adopt(second);
    firstRegistration = first->dialogResults().reserve(OpenFileDialogProps().onResult(&emitter));
    secondRegistration = second->dialogResults().reserve(OpenFileDialogProps().onResult(&emitter));
    Transport::ReturnPort firstPort(firstRegistration), secondPort(secondRegistration);
    LOKA_VERIFY(firstPort.seal(FileChooserResult::Canceled()) == first);
    LOKA_VERIFY(secondPort.seal(FileChooserResult::Canceled()) == second);
    app.requestWindowClose(first);
  }
  assert(deaths == 2 && emits == 0);
  delete firstRegistration;
  delete secondRegistration;
  emitter.unbind(&countEvent, &emits);
}

namespace
{
  class RunningController : public NullScenePlatformController
  {
  public:
    RunningController()
        : app(0),
          window(0),
          emits(0)
    {
    }
    virtual void beginApplyCycle()
    {
      if (!this->app)
        return;
      assert(this->window->scene()->isRunInProgress()); // loka-assert-ok: read-only run exclusion query
      this->app->flush();
      assert(*this->emits == 0);
    }
    WindowAdmissionTestApp *app;
    Window *window;
    int *emits;
  };
  struct RetargetObserver
  {
    Transport::Registration *replacement;
    loka::core::EmitterState *emitter;
    static void retarget(void *data)
    {
      RetargetObserver &probe = *static_cast<RetargetObserver *>(data);
      OpenFileDialogDefinition changed;
      changed.onResult(probe.emitter);
      LOKA_VERIFY(changed.applyPropsToNode(g_context->node_));
      g_context->attach();
      probe.replacement = g_context->registration_;
      g_context->produce();
    }
  };
  void destroyEmitter(void *data)
  {
    loka::core::EmitterState **emitter = static_cast<loka::core::EmitterState **>(data);
    delete *emitter;
    *emitter = 0;
  }
} // namespace

void testOpenFileDialogTransportSceneRunExcludesAdmission()
{
  RunningController controller;
  WindowProps props;
  props.scene(new Scene(loka::app::Button("run").clone()));
  NullWindow window(0, props, &controller);
  WindowAdmissionTestApp app(window);
  int emits = 0;
  loka::core::EmitterState emitter;
  emitter.bind(&countEvent, &emits, false);
  Transport::Registration *registration = window.dialogResults().reserve(OpenFileDialogProps().onResult(&emitter));
  Transport::ReturnPort port(registration);
  LOKA_VERIFY(port.seal(FileChooserResult::Canceled()) == &window);
  controller.app = &app;
  controller.window = &window;
  controller.emits = &emits;
  window.scene()->invalidate();
  assert(emits == 0);
  controller.app = 0;
  app.flush();
  assert(emits == 1);
  app.flush();
  delete registration;
  emitter.unbind(&countEvent, &emits);
}

void testOpenFileDialogTransportRetargetDuringWriteCannotUnlinkReplacement()
{
  Fixture fixture;
  int oldEmits = 0, newEmits = 0;
  loka::core::EmitterState replacement;
  replacement.bind(&countEvent, &newEmits, false);
  g_owner->emitter_.bind(&countEvent, &oldEmits, false);
  RetargetObserver probe = {0, &replacement};
  g_owner->result_.bind(&RetargetObserver::retarget, &probe, false);
  g_context->produce();
  fixture.app.flush();
  assert(oldEmits == 0 && newEmits == 0 && g_context->registration_ == probe.replacement);
  fixture.app.flush();
  assert(oldEmits == 0 && newEmits == 1);
  fixture.app.flush();
  assert(Access::census(fixture.window.dialogResults()) == 0);
  g_owner->result_.unbind(&RetargetObserver::retarget, &probe);
  g_owner->emitter_.unbind(&countEvent, &oldEmits);
  replacement.unbind(&countEvent, &newEmits);
}

void testOpenFileDialogTransportEmitterTokenChecksDeliveryLifetime()
{
  NullWindow window(0, WindowProps());
  WindowAdmissionTestApp app(window);
  loka::core::MutableState<FileChooserResult> result;
  loka::core::PushStateTracker tracker;
  tracker.addState(&result);
  NodeState<FileChooserResult> channel(&result, &tracker);
  loka::core::EmitterState *emitter = new loka::core::EmitterState();
  channel.bind(&destroyEmitter, &emitter, false);
  Transport::Registration *registration =
      window.dialogResults().reserve(OpenFileDialogProps().result(channel).onResult(emitter));
  Transport::ReturnPort port(registration);
  LOKA_VERIFY(port.seal(FileChooserResult::Canceled()) == &window);
  app.flush();
  assert(!emitter && channel.get().kind == FileChooserResult::RESULT_CANCELED);
  app.flush();
  assert(Access::census(window.dialogResults()) == 0);
  delete registration;
  channel.unbind(&destroyEmitter, &emitter);
}

void testOpenFileDialogTransportCrossWindowCloseCancelsLaterBatch()
{
  int deaths = 0, emits = 0;
  OwnedApp app;
  DeathWindow *first = new DeathWindow(deaths), *second = new DeathWindow(deaths);
  app.adopt(first);
  app.adopt(second);
  loka::core::EmitterState closer, observer;
  CloseProbe probe = {&app, second, &deaths};
  closer.bind(&CloseProbe::close, &probe, false);
  observer.bind(&countEvent, &emits, false);
  Transport::Registration *a = first->dialogResults().reserve(OpenFileDialogProps().onResult(&closer));
  Transport::Registration *b = second->dialogResults().reserve(OpenFileDialogProps().onResult(&observer));
  Transport::ReturnPort portA(a), portB(b);
  LOKA_VERIFY(portA.seal(FileChooserResult::Canceled()) == first);
  LOKA_VERIFY(portB.seal(FileChooserResult::Canceled()) == second);
  app.flush();
  assert(deaths == 0 && emits == 0 && app.pending());
  app.flush();
  assert(deaths == 1 && emits == 0);
  delete a;
  delete b;
  closer.unbind(&CloseProbe::close, &probe);
  observer.unbind(&countEvent, &emits);
}
