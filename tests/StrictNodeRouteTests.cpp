#include "StrictNodeRouteTests.hpp"
#include "support/TestVerify.hpp"
#include "../example/MineSweeper/src/MainNode.hpp"
#include "app/scene/Scene.hpp"
#include "platform/null/NullScenePlatformController.hpp"
#include "testing/scene/SceneTestFlow.hpp"
#include "core/SmallObjectPool.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <vector>

using namespace loka::app;
using namespace loka::app::scene;
namespace
{
  struct Source
  {
    enum { kAlignment = 16 };
    static bool denied;
    static void *acquire(size_t bytes) { return denied ? 0 : std::malloc(bytes); }
    static void release(void *storage) { std::free(storage); }
  };
  bool Source::denied = false;
  loka::core::SmallObjectPool<Source> nodePool;
  unsigned boots = 0;
  bool nodeSite(const loka::core::LokaAllocationSite &site)
  { return std::strcmp(site.ownerTag, "NodeDefinition") == 0 || std::strcmp(site.ownerTag, "NodePartition") == 0; }
  void *allocate(size_t bytes, const loka::core::LokaAllocationSite &site)
  {
    if (std::strcmp(site.ownerTag, "NodePartition") == 0) ++boots;
    return nodeSite(site) ? nodePool.allocate(bytes) : std::malloc(bytes);
  }
  void release(void *storage, const loka::core::LokaAllocationSite &site)
  { if (nodeSite(site)) nodePool.release(storage); else std::free(storage); }
  void collect(Node *node, std::vector<Node *> &slots, unsigned &cells)
  {
    if (!node) return;
    if (node->isPartitionAllocated()) slots.push_back(node);
    if (node->asCellNode()) ++cells;
    INestable *nestable = node->asNestable();
    for (Node *child = nestable ? nestable->childrenHead() : 0; child; child = child->nextInComposition)
      collect(child, slots, cells);
  }
  void newGame(Scene &scene)
  {
    Scene *input = &scene, *output = 0;
    loka::dsl::FlowError error;
    const loka::dsl::StepRunStatus clicked =
        loka::dsl::testing::ClickButton("MineSweeper.NewGameButton").run(input, output, error);
    LOKA_VERIFY(clicked == loka::dsl::FLOW_STEP_SUCCEEDED);
    for (int i = 0; i < 4; ++i)
      loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
  }
}
void testStrictNodeRouteMineSweeperReuse()
{
  boots = 0;
  loka::core::LokaAllocSetBackend(allocate, release);
  {
    NullScenePlatformController platform;
    Scene scene((Boundary<minesweeper::MainNode>(minesweeper::MainProps(12345))));
    scene.mount(&platform);
    loka::dsl::testing::SceneTestAccess::updateAttached(scene, true);
    std::vector<Node *> cold;
    unsigned cells = 0;
    collect(loka::dsl::testing::SceneTestAccess::rootNode(scene), cold, cells);
    LOKA_VERIFY(boots == 1);
    LOKA_VERIFY(cells == 64);
    LOKA_VERIFY(cold.size() == 195);
    std::sort(cold.begin(), cold.end(), std::less<Node *>());
    newGame(scene);
    const loka::core::UpstreamGauge before = nodePool.snapshot();
    Source::denied = true;
    newGame(scene);
    const loka::core::UpstreamGauge after = nodePool.snapshot();
    Source::denied = false;
    std::vector<Node *> reused;
    cells = 0;
    collect(loka::dsl::testing::SceneTestAccess::rootNode(scene), reused, cells);
    std::sort(reused.begin(), reused.end(), std::less<Node *>());
    std::fprintf(stderr, "strict New Game: node upstream attempts=%lu; slots=%lu; cells=%u; boots=%u\n",
                 after.attempts - before.attempts, static_cast<unsigned long>(reused.size()), cells, boots);
    LOKA_VERIFY(after.attempts == before.attempts);
    LOKA_VERIFY(cells == 64);
    LOKA_VERIFY(cold == reused);
    LOKA_VERIFY(boots == 1);
  }
  loka::core::LokaAllocSetBackend(0, 0);
}
