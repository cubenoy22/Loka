#include "CommandSetTests.hpp"

#include "app/CommandSet.hpp"
#include "app/nodes/boundary/StdComposition.hpp"
#include "platform/null/NullPlatformContext.hpp"
#include "platform/null/NullWindow.hpp"
#include "support/WindowAdmissionTestApp.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace CommandSetTests
{
  using namespace loka::app;
  using namespace loka::app::scene;

  enum TestCommand
  {
    TEST_INVALID_COMMAND = -1,
    TEST_FIRST_COMMAND = 0,
    TEST_SECOND_COMMAND,
    TEST_THIRD_COMMAND,
    TEST_LAST_COMMAND,
    TEST_COMMAND_COUNT
  };
  typedef CommandSet<TestCommand, TEST_COMMAND_COUNT> Commands;

  enum BindingOrder { COMMANDS_ONLY, ACTION_FIRST, COMMAND_FIRST, NULL_ACTION_FIRST, NULL_COMMAND_FIRST };

  struct Fixture
  {
    Fixture() : external(false), order(COMMANDS_ONLY), actionCalls(0) {}
    Commands commands;
    bool external;
    BindingOrder order;
    unsigned actionCalls;
    std::vector<TestCommand> calls;
  };
  static Fixture *fixture = 0;

  class Root : public BoundaryNodeFor<Root>
  {
  public:
    explicit Root(const BoundaryPropsFor<Root> &p) : BoundaryNodeFor<Root>(p) {}
    Commands &commands() { return fixture->external ? fixture->commands : this->commands_; }
    virtual void declareBindings(BindingToken &token)
    {
      if (fixture->order == NULL_ACTION_FIRST || fixture->order == NULL_COMMAND_FIRST)
      {
        void (Root::*action)() = 0;
        void (Root::*command)(TestCommand) = 0;
        if (fixture->order == NULL_ACTION_FIRST)
          token.action(*this->commands().slot<TEST_FIRST_COMMAND>(), this, action);
        token.actions(this->commands(), this, command);
        token.action(*this->commands().slot<TEST_FIRST_COMMAND>(), this, action);
        token.actions(this->commands(), this, command);
        token.action(*this->commands().slot<TEST_FIRST_COMMAND>(), this, action);
        LOKA_VERIFY(this->callbacks_.size() == Commands::count() + 1);
        return;
      }
      if (fixture->order == ACTION_FIRST)
        token.action(*this->commands().slot<TEST_FIRST_COMMAND>(), this, &Root::onAction);
      token.actions(this->commands(), this, &Root::onCommand);
      if (fixture->order != COMMANDS_ONLY)
      {
        token.action(*this->commands().slot<TEST_FIRST_COMMAND>(), this, &Root::onAction);
        token.actions(this->commands(), this, &Root::onCommand);
        token.action(*this->commands().slot<TEST_FIRST_COMMAND>(), this, &Root::onAction);
      }
    }
    virtual void composeNode(NodeComposition &c)
    {
      c.menuBar(MenuBarDefinition()
          << (MenuDefinition("File")
              << MenuItemDefinition("First").onClick(this->commands().slot<TEST_FIRST_COMMAND>())
              << MenuItemDefinition("Second").onClick(this->commands().slot<TEST_SECOND_COMMAND>())
              << MenuItemDefinition("Third").onClick(this->commands().slot<TEST_THIRD_COMMAND>())
              << MenuItemDefinition("Last").onClick(this->commands().slot<TEST_LAST_COMMAND>())));
      c.declare(FragmentDefinition());
    }
  private:
    void onAction() { ++fixture->actionCalls; }
    void onCommand(TestCommand id) { fixture->calls.push_back(id); }
    Commands commands_;
  };

  inline WindowProps props()
  {
    WindowProps result;
    result.scene(new Scene(Boundary<Root>()));
    return result;
  }

  inline void verifyDispatch(NullWindow &window)
  {
    Root *root = static_cast<Root *>(loka::dsl::testing::SceneTestAccess::rootBoundary(*window.scene()));
    const MenuBarDefinition *bar = window.scene()->menuBar();
    LOKA_VERIFY(root && bar && bar->menusCount() == 1);
    loka::core::EmitterState *slots[] = {
        root->commands().slot<TEST_FIRST_COMMAND>(), root->commands().slot<TEST_SECOND_COMMAND>(),
        root->commands().slot<TEST_THIRD_COMMAND>(), root->commands().slot<TEST_LAST_COMMAND>()};
    NullMenuAttachment &attachment = window.scenePlatformController()->menuAttachment();
    const bool opened = attachment.open(*bar);
    LOKA_VERIFY(opened);
    const MenuItemDefinition *item = bar->menusHead()->itemsHead();
    for (unsigned i = 0; i < Commands::count(); ++i)
    {
      LOKA_VERIFY(item && item->onClickState == slots[i]);
      attachment.dispatch(i + 1);
      LOKA_VERIFY(fixture->calls.size() == i + 1 && fixture->calls[i] == TestCommand(i));
      item = item->nextInComposition;
    }
    LOKA_VERIFY(!item);
  }
}

void testCommandSetMenuDispatch()
{
  using namespace CommandSetTests;
  Fixture f;
  fixture = &f;
  NullPlatformContext context;
  NullWindow window(&context, props());
  WindowAdmissionTestApp app(window);
  app.operationLoop();
  verifyDispatch(window);
}

void testCommandSetMenuDisconnectsOnUnmount()
{
  using namespace CommandSetTests;
  Fixture f;
  fixture = &f;
  NullPlatformContext context;
  NullWindow window(&context, props());
  WindowAdmissionTestApp app(window);
  app.operationLoop();
  verifyDispatch(window);
  NullMenuAttachment &attachment = window.scenePlatformController()->menuAttachment();
  const unsigned commandIds[] = {1, 2, 3, 4};
  window.teardownScene();
  LOKA_VERIFY(!attachment.connected());
  for (unsigned i = 0; i < Commands::count(); ++i)
    attachment.dispatch(commandIds[i]);
  LOKA_VERIFY(f.calls.size() == Commands::count());
}

void testCommandSetWithdrawsOnDetach()
{
  using namespace CommandSetTests;
  Fixture f;
  fixture = &f;
  f.external = true;
  NullPlatformContext context;
  NullWindow window(&context, props());
  WindowAdmissionTestApp app(window);
  app.operationLoop();
  f.commands.slot<TEST_FIRST_COMMAND>()->emit();
  LOKA_VERIFY(f.calls.size() == 1 && f.calls[0] == TEST_FIRST_COMMAND);
  window.teardownScene();
  for (unsigned i = 0; i < Commands::count(); ++i)
    f.commands.emitter(TestCommand(i))->emit();
  LOKA_VERIFY(f.calls.size() == 1);
}

void testCommandSetRuntimeBounds()
{
  using namespace CommandSetTests;
  Commands commands;
  LOKA_VERIFY(Commands::count() == TEST_COMMAND_COUNT);
  LOKA_VERIFY(commands.emitter(TEST_COMMAND_COUNT) == 0);
  LOKA_VERIFY(commands.emitter(TestCommand(-1)) == 0);
  LOKA_VERIFY(commands.emitter(TEST_FIRST_COMMAND) == commands.slot<TEST_FIRST_COMMAND>());
  LOKA_VERIFY(commands.emitter(TEST_LAST_COMMAND) == commands.slot<TEST_LAST_COMMAND>());
}

void testCommandSetCallbackFamilies()
{
  using namespace CommandSetTests;
  for (unsigned order = 0; order < 2; ++order)
  {
    Fixture f;
    fixture = &f;
    f.order = order == 0 ? ACTION_FIRST : COMMAND_FIRST;
    NullPlatformContext context;
    NullWindow window(&context, props());
    WindowAdmissionTestApp app(window);
    app.operationLoop();
    verifyDispatch(window);
    LOKA_VERIFY(f.actionCalls == 1);
  }
}

// Registration-only collision control: null member pointers of different
// signatures can have identical representations. Never dispatch these entries;
// real handlers above pin invocation, while these pin dedup before typed reads.
void testCommandSetCallbackFamilyCollision()
{
  using namespace CommandSetTests;
  for (unsigned order = 0; order < 2; ++order)
  {
    Fixture f;
    fixture = &f;
    f.order = order == 0 ? NULL_ACTION_FIRST : NULL_COMMAND_FIRST;
    NullPlatformContext context;
    NullWindow window(&context, props());
    WindowAdmissionTestApp app(window);
    app.operationLoop();
    window.teardownScene();
    LOKA_VERIFY(f.calls.empty() && f.actionCalls == 0);
  }
}
