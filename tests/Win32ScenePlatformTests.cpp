#include "Win32ScenePlatformTests.hpp"

#if defined(_WIN32) || defined(WIN32)

#include <cassert>

#include <windows.h>

#include "app/Button.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "core/resource/Image.hpp"
#include "loka/core/State.hpp"
#include "loka/dsl/testing/SceneTestFlow.hpp"
#include "Win32ScenePlatformController.hpp"

namespace
{
  ::loka::app::scene::Node *findNodeByTestId(::loka::app::scene::Node *node, const std::string &testId)
  {
    if (!node)
    {
      return 0;
    }
    if (node->testId() == testId)
    {
      return node;
    }
    ::loka::app::scene::INestable *nestable = node->asNestable();
    if (!nestable)
    {
      return 0;
    }
    ::loka::app::scene::Node *child = nestable->childrenHead();
    while (child)
    {
      ::loka::app::scene::Node *found = findNodeByTestId(child, testId);
      if (found)
      {
        return found;
      }
      child = child->nextInComposition;
    }
    return 0;
  }

  loka::core::MutableState<loka::core::String> *g_staticEditTextState = 0;
  loka::core::MutableState<int> *g_staticSelectedIndexState = 0;
  loka::core::MutableState<loka::core::resource::Image> *g_staticImageState = 0;

  class StaticWin32ControlNode;
  typedef loka::app::scene::StaticCompositionPropsFor<StaticWin32ControlNode> StaticWin32ControlProps;

  class StaticWin32ControlNode : public loka::app::scene::StaticCompositionNodeFor<StaticWin32ControlNode>
  {
  public:
    StaticWin32ControlNode(const StaticWin32ControlProps &p)
        : loka::app::scene::StaticCompositionNodeFor<StaticWin32ControlNode>(StaticWin32ControlProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      static const char *kPopupItems[] = {"One", "Two", "Three"};
      c.declare(
          loka::app::VStack()
          << loka::app::Button("Run").testId("ReuseButton")
          << loka::app::EditText(g_staticEditTextState).controlTag(201).testId("ReuseEdit")
          << loka::app::PopupMenu(kPopupItems, 3).selectedIndex(g_staticSelectedIndexState).testId("ReusePopup")
          << loka::app::ImageView().image(g_staticImageState).size(64, 64).testId("ReuseImage"));
    }
  };

  loka::app::scene::BoundaryDefinition<StaticWin32ControlProps, StaticWin32ControlNode> StaticWin32ControlBoundary()
  {
    return loka::app::scene::StaticCompositionBoundary<StaticWin32ControlNode>();
  }
}

void testWin32ScenePlatformRelayoutReusesControlContexts()
{
  loka::core::MutableState<loka::core::String> editTextState(loka::core::String::Literal("Hello"));
  loka::core::MutableState<int> selectedIndexState(1);
  loka::core::resource::Image initialImage;
  loka::core::MutableState<loka::core::resource::Image> imageState(initialImage);
  g_staticEditTextState = &editTextState;
  g_staticSelectedIndexState = &selectedIndexState;
  g_staticImageState = &imageState;

  HWND rootHwnd = CreateWindowExA(0, "STATIC", "LokaWin32TestRoot", WS_OVERLAPPEDWINDOW,
                                  0, 0, 400, 320, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(rootHwnd != NULL);

  Win32ScenePlatformController controller(rootHwnd);
  loka::app::scene::Scene scene(StaticWin32ControlBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);

  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "ReuseButton");
  ::loka::app::scene::Node *editNode = findNodeByTestId(root, "ReuseEdit");
  ::loka::app::scene::Node *popupNode = findNodeByTestId(root, "ReusePopup");
  ::loka::app::scene::Node *imageNode = findNodeByTestId(root, "ReuseImage");
  assert(buttonNode != 0);
  assert(editNode != 0);
  assert(popupNode != 0);
  assert(imageNode != 0);

  ::loka::app::scene::NodeContext *buttonContext = buttonNode->getContext();
  ::loka::app::scene::NodeContext *editContext = editNode->getContext();
  ::loka::app::scene::NodeContext *popupContext = popupNode->getContext();
  ::loka::app::scene::NodeContext *imageContext = imageNode->getContext();
  assert(buttonContext != 0);
  assert(editContext != 0);
  assert(popupContext != 0);
  assert(imageContext != 0);

  controller.relayout(520, 360);

  assert(buttonNode->getContext() == buttonContext);
  assert(editNode->getContext() == editContext);
  assert(popupNode->getContext() == popupContext);
  assert(imageNode->getContext() == imageContext);

  scene.unmount();
  DestroyWindow(rootHwnd);
  g_staticEditTextState = 0;
  g_staticSelectedIndexState = 0;
  g_staticImageState = 0;
}

#endif
