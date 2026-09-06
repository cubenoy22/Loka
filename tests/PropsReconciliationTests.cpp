#include "PropsReconciliationTests.hpp"
#include "support/PropsReconciliation.hpp"
#include "app/nodes/controls/Button.hpp"
#include "platform/null/NullScenePlatformController.hpp"

namespace
{
  using namespace loka::app;
  using namespace loka::app::scene;
  using namespace loka::core;
  using namespace PropsReconciliationSupport;

  class CountingContext : public NodeContext
  {
  public:
    CountingContext()
        : calls_(0)
    {
    }
    virtual void onPropsApplied()
    {
      ++this->calls_;
    }
    unsigned calls() const
    {
      return this->calls_;
    }

  private:
    unsigned calls_;
  };

  class ButtonHandler : public IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return NodeTypeToken<ButtonNode>();
    }
    virtual NodeContext *ensureContext(Node *node, IPlatformController *, const LayoutState &)
    {
      if (!node->getContext())
        node->setContext(new CountingContext());
      return node->getContext();
    }
  };
} // namespace

void testRetainedPropsApplicationNotifiesContextExactlyOnce()
{
  MutableState<String> a(String::Literal("A")), b(String::Literal("B"));
  Button declaration(&a);
  ButtonHandler handler;
  NullScenePlatformController platform;
  LOKA_VERIFY(platform.registerNodeHandler(&handler));
  Scene scene((Boundary<Tree<Button> >(Props<Button>(&declaration))));
  scene.mount(&platform);
  scene.updateAttached(true);
  settle(scene);
  ButtonNode *button = root(scene)->childrenHead()->asButtonNode();
  assert(button);
  CountingContext *context = static_cast<CountingContext *>(button->getContext());
  assert(context && context->calls() == 0);
  recompose(scene); // Equivalent declaration repoints without applying props.
  assert(root(scene)->childrenHead() == button && button->getContext() == context);
  assert(context->calls() == 0);
  declaration = Button(&b);
  recompose(scene);
  assert(root(scene)->childrenHead() == button && button->getContext() == context);
  assert(button->props.text_ == &b);
  assert(context->calls() == 1);
  recompose(scene);
  assert(context->calls() == 1);
  declaration = Button(&a);
  recompose(scene);
  assert(root(scene)->childrenHead() == button && button->getContext() == context);
  assert(context->calls() == 2);
  scene.unmount();
}
