#define LAZYLIST_CAPACITY 300
// Isolate the differently configured example types from the default TU (ODR).
#define lazylist lazylist_capacity_test
#include "../example/LazyList/src/MainNode.hpp"
#undef lazylist
#include "LazyListTests.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "support/TestVerify.hpp"
#include "testing/scene/SceneTestFlow.hpp"

namespace lazylist_capacity_test
{
  namespace testing
  {
    void cardConstructed(short)
    {
      LOKA_VERIFY(false);
    }
  } // namespace testing
} // namespace lazylist_capacity_test

void testLazyListCapacityRefusal300()
{
  lazylist_capacity_test::LazyListModel model;
  LOKA_VERIFY(model.cards.size() == 300);
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(
      loka::app::scene::Boundary<lazylist_capacity_test::MainNode>(lazylist_capacity_test::MainProps(&model)));
  scene.mount(&platform);
  loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  scene.flushInvalidation();
  loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  // Main -> Box -> Column -> (bar, list seat, status).
  loka::app::scene::Node *column = root->asNestable()->childrenHead()->asNestable()->childrenHead();
  loka::app::scene::Node *seat = column->asNestable()->childrenHead()->nextInComposition;
  loka::app::LazyFlexNode<lazylist_capacity_test::CardProps> *flex =
      static_cast<loka::app::LazyFlexNode<lazylist_capacity_test::CardProps> *>(seat->asNestable()->childrenHead());
  LOKA_VERIFY(flex->status() == loka::app::LAZY_FLEX_CAPACITY_REFUSED);
  LOKA_VERIFY(flex->childrenHead() == 0);
  {
    const bool fact = platform.ledger().size() == 6;
    LOKA_VERIFY(fact);
  }
  loka::dsl::testing::SceneTestAccess::unmount(scene);
  scene.flushInvalidation();
  platform.drainNativeRetirements();
  {
    const bool fact = platform.ledger().size() == 0;
    LOKA_VERIFY(fact);
  }
}
