#define LOKA_LAZYFLEX_MAX_ITEMS 1000
#include "LazyFlexTests.hpp"
#include "support/TestVerify.hpp"
#include "app/nodes/nestable/LazyFlex.hpp"
#include "app/nodes/controls/Button.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/OwnershipDump.hpp"
namespace
{
  class LargeCard;
  struct LargeCardProps : loka::app::scene::NodePropsBase<LargeCardProps>
  {
    typedef LargeCardProps TypeTag;
    typedef LargeCard NodeType;
    bool operator<(const loka::app::scene::PropsBase &) const
    {
      return false;
    }
    bool operator!=(const LargeCardProps &) const
    {
      return false;
    }
  };
  class LargeCard : public loka::app::scene::ComponentNodeWithProps<LargeCardProps>
  {
  public:
    explicit LargeCard(const LargeCardProps &p)
        : ComponentNodeWithProps<LargeCardProps>(p)
    {
    }
    virtual void composeChildren(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::Button("large"));
    }
  };
} // namespace
void testLazyFlexOverriddenCapacityAccepts300()
{
  loka::core::PushStateTracker tracker;
  loka::core::ObservableList<LargeCardProps> list;
  LOKA_VERIFY(list.attach(&tracker, 300) == loka::core::ATTACH_OK);
  for (unsigned i = 0; i < 8; ++i)
    LOKA_VERIFY(list.insert(i, LargeCardProps()) == loka::core::EDIT_OK);
  loka::core::MutableState<loka::core::Frame> viewport(loka::core::Frame(0, 0, 200, 160));
  tracker.addState(&viewport);
  NullScenePlatformController platform;
  loka::app::scene::Scene scene(loka::app::LazyColumn(list).cells(200, 20).viewport(viewport));
  scene.mount(&platform);
  scene.updateAttached(true);
  loka::app::LazyFlexNode<LargeCardProps> *node =
      static_cast<loka::app::LazyFlexNode<LargeCardProps> *>(loka::dsl::testing::SceneTestAccess::rootBoundary(scene));
  LOKA_VERIFY(node->status() == loka::app::LAZY_FLEX_READY);
  {
    const bool ledgerFact = platform.ledger().size() == 8;
    LOKA_VERIFY(ledgerFact);
  }
  const std::string dump = loka::dsl::testing::OwnershipDump::dump(scene);
  LOKA_VERIFY(dump.find("states: 9 (arena 0, heap 9)") != std::string::npos);
}
