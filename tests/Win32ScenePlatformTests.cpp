#include "Win32ScenePlatformTests.hpp"

#if defined(_WIN32) || defined(WIN32)

#include <cassert>

#include <windows.h>

#include "app/Button.hpp"
#include "app/Cell.hpp"
#include "app/EditText.hpp"
#include "app/ImageView.hpp"
#include "app/PopupMenu.hpp"
#include "app/RowColumn.hpp"
#include "app/Text.hpp"
#include "app/scene/Scene.hpp"
#include "app/scene/node/DynamicComposition.hpp"
#include "app/scene/node/StaticComposition.hpp"
#include "core/resource/Image.hpp"
#include "loka/core/State.hpp"
#include "loka/dsl/testing/SceneTestFlow.hpp"
#include "Win32ScenePlatformController.hpp"
#include "testing/Win32ScenePlatformTestAccess.hpp"

namespace
{
  int countChildWindows(HWND parent)
  {
    int count = 0;
    for (HWND child = GetWindow(parent, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
      ++count;
    }
    return count;
  }

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

  loka::core::MutableState<loka::core::String> *g_textState = 0;
  loka::core::MutableState<bool> *g_enabledState = 0;
  loka::core::MutableState<loka::core::String> *g_staticEditTextState = 0;
  loka::core::MutableState<int> *g_staticSelectedIndexState = 0;
  loka::core::MutableState<loka::core::resource::Image> *g_staticImageState = 0;
  loka::core::MutableState<bool> *g_swapChildState = 0;

  class DynamicWin32TestNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicWin32TestNode> DynamicWin32TestProps;

  class DynamicWin32TestNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicWin32TestNode>
  {
  public:
    DynamicWin32TestNode(const DynamicWin32TestProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicWin32TestNode>(DynamicWin32TestProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      loka::app::ColumnDefinition stack = loka::app::VStack();
      if (g_textState)
      {
        stack << loka::app::Text(g_textState)
                     .attr(loka::app::TextAttr().wrap(loka::app::TEXT_WRAP_WORD))
                     .testId("MainText");
      }
      if (g_enabledState)
      {
        stack << loka::app::Button("Run").enabled(g_enabledState).testId("MainButton");
      }
      c.declare(stack);
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicWin32TestProps, DynamicWin32TestNode> DynamicWin32TestBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicWin32TestNode>();
  }

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

  class DynamicWin32SwapNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicWin32SwapNode> DynamicWin32SwapProps;

  class DynamicWin32SwapNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicWin32SwapNode>
  {
  public:
    DynamicWin32SwapNode(const DynamicWin32SwapProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicWin32SwapNode>(DynamicWin32SwapProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      if (g_swapChildState && g_swapChildState->get())
      {
        c.declare(loka::app::EditText(loka::core::StaticState<loka::core::String>(loka::core::String::Literal("Edit")))
                      .controlTag(401)
                      .testId("SwapEdit"));
      }
      else
      {
        c.declare(loka::app::Button("Run").testId("SwapButton"));
      }
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_swapChildState)
      {
        registrar.observe(g_swapChildState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicWin32SwapProps, DynamicWin32SwapNode> DynamicWin32SwapBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicWin32SwapNode>();
  }

  class DynamicWin32SwapHostNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicWin32SwapHostNode> DynamicWin32SwapHostProps;

  class DynamicWin32SwapHostNode : public loka::app::scene::BoundaryNodeFor<DynamicWin32SwapHostNode>
  {
  public:
    DynamicWin32SwapHostNode(const DynamicWin32SwapHostProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicWin32SwapHostNode>(DynamicWin32SwapHostProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(DynamicWin32SwapBoundary());
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicWin32SwapHostProps, DynamicWin32SwapHostNode> DynamicWin32SwapHostBoundary()
  {
    return loka::app::scene::Boundary<DynamicWin32SwapHostNode>();
  }

  loka::core::MutableState<loka::core::String> *g_staticTextState = 0;
  loka::core::MutableState<loka::core::String> *g_staticCellState = 0;
  loka::core::MutableState<bool> *g_foreignSwapChildState = 0;
  loka::core::MutableState<loka::core::String> *g_foreignPersistEditState = 0;
  loka::core::MutableState<int> *g_foreignPersistSelectedIndexState = 0;
  loka::core::MutableState<bool> *g_foreignReorderState = 0;

  class StaticWin32CellTextNode;
  typedef loka::app::scene::StaticCompositionPropsFor<StaticWin32CellTextNode> StaticWin32CellTextProps;

  class StaticWin32CellTextNode : public loka::app::scene::StaticCompositionNodeFor<StaticWin32CellTextNode>
  {
  public:
    StaticWin32CellTextNode(const StaticWin32CellTextProps &p)
        : loka::app::scene::StaticCompositionNodeFor<StaticWin32CellTextNode>(StaticWin32CellTextProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(
          loka::app::VStack()
          << loka::app::Text(g_staticTextState).testId("ReuseText")
          << loka::app::Cell(g_staticCellState).testId("ReuseCell"));
    }
  };

  loka::app::scene::BoundaryDefinition<StaticWin32CellTextProps, StaticWin32CellTextNode> StaticWin32CellTextBoundary()
  {
    return loka::app::scene::StaticCompositionBoundary<StaticWin32CellTextNode>();
  }

  class DynamicWin32ForeignSwapNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicWin32ForeignSwapNode> DynamicWin32ForeignSwapProps;

  class DynamicWin32ForeignSwapNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicWin32ForeignSwapNode>
  {
  public:
    DynamicWin32ForeignSwapNode(const DynamicWin32ForeignSwapProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicWin32ForeignSwapNode>(DynamicWin32ForeignSwapProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      if (g_foreignSwapChildState && g_foreignSwapChildState->get())
      {
        c.declare(loka::app::Text("Dynamic rebuild branch").testId("ForeignSwapText"));
      }
      else
      {
        c.declare(loka::app::Button("Dynamic hidden branch").testId("ForeignSwapButton"));
      }
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_foreignSwapChildState)
      {
        registrar.observe(g_foreignSwapChildState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicWin32ForeignSwapProps, DynamicWin32ForeignSwapNode> DynamicWin32ForeignSwapBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicWin32ForeignSwapNode>();
  }

  class DynamicWin32ForeignSwapHostNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicWin32ForeignSwapHostNode> DynamicWin32ForeignSwapHostProps;

  class DynamicWin32ForeignSwapHostNode : public loka::app::scene::BoundaryNodeFor<DynamicWin32ForeignSwapHostNode>
  {
  public:
    DynamicWin32ForeignSwapHostNode(const DynamicWin32ForeignSwapHostProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicWin32ForeignSwapHostNode>(DynamicWin32ForeignSwapHostProps(p)), observed_() {}

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      c.declareStates().state(this->observed_, false);
      g_foreignSwapChildState = this->observed_.mutableState();
    }

    virtual void detachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      g_foreignSwapChildState = 0;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(loka::app::HStack()
                << loka::app::Text("StaticMarker").testId("ForeignStaticMarker")
                << DynamicWin32ForeignSwapBoundary());
    }

  private:
    loka::app::scene::BoundState<bool> observed_;
  };

  loka::app::scene::BoundaryDefinition<DynamicWin32ForeignSwapHostProps, DynamicWin32ForeignSwapHostNode> DynamicWin32ForeignSwapHostBoundary()
  {
    return loka::app::scene::Boundary<DynamicWin32ForeignSwapHostNode>();
  }

  class DynamicWin32ForeignPersistNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicWin32ForeignPersistNode> DynamicWin32ForeignPersistProps;

  class DynamicWin32ForeignPersistNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicWin32ForeignPersistNode>
  {
  public:
    DynamicWin32ForeignPersistNode(const DynamicWin32ForeignPersistProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicWin32ForeignPersistNode>(DynamicWin32ForeignPersistProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      static const char *kItems[] = {"One", "Two", "Three"};
      loka::app::ColumnDefinition column = loka::app::VStack();
      if (g_foreignSwapChildState && g_foreignSwapChildState->get())
      {
        column << loka::app::Text("Dynamic rebuild branch").testId("ForeignPersistText").tag(20);
      }
      else
      {
        column << loka::app::Button("Dynamic hidden branch").testId("ForeignPersistButton").tag(20);
      }
      column << loka::app::EditText(g_foreignPersistEditState).controlTag(451).testId("ForeignPersistEdit").tag(10)
             << loka::app::PopupMenu(kItems, 3).selectedIndex(g_foreignPersistSelectedIndexState).testId("ForeignPersistPopup").tag(11);
      c.declare(column);
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_foreignSwapChildState)
      {
        registrar.observe(g_foreignSwapChildState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicWin32ForeignPersistProps, DynamicWin32ForeignPersistNode> DynamicWin32ForeignPersistBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicWin32ForeignPersistNode>();
  }

  class DynamicWin32ForeignPersistHostNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicWin32ForeignPersistHostNode> DynamicWin32ForeignPersistHostProps;

  class DynamicWin32ForeignPersistHostNode : public loka::app::scene::BoundaryNodeFor<DynamicWin32ForeignPersistHostNode>
  {
  public:
    DynamicWin32ForeignPersistHostNode(const DynamicWin32ForeignPersistHostProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicWin32ForeignPersistHostNode>(DynamicWin32ForeignPersistHostProps(p)), observed_() {}

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      c.declareStates().state(this->observed_, false);
      g_foreignSwapChildState = this->observed_.mutableState();
    }

    virtual void detachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      g_foreignSwapChildState = 0;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(DynamicWin32ForeignPersistBoundary());
    }

  private:
    loka::app::scene::BoundState<bool> observed_;
  };

  loka::app::scene::BoundaryDefinition<DynamicWin32ForeignPersistHostProps, DynamicWin32ForeignPersistHostNode> DynamicWin32ForeignPersistHostBoundary()
  {
    return loka::app::scene::Boundary<DynamicWin32ForeignPersistHostNode>();
  }

  class DynamicWin32ForeignReorderNode;
  typedef loka::app::scene::DynamicCompositionPropsFor<DynamicWin32ForeignReorderNode> DynamicWin32ForeignReorderProps;

  class DynamicWin32ForeignReorderNode : public loka::app::scene::DynamicCompositionNodeFor<DynamicWin32ForeignReorderNode>
  {
  public:
    DynamicWin32ForeignReorderNode(const DynamicWin32ForeignReorderProps &p)
        : loka::app::scene::DynamicCompositionNodeFor<DynamicWin32ForeignReorderNode>(DynamicWin32ForeignReorderProps(p)) {}

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      static const char *kItems[] = {"One", "Two", "Three"};
      loka::app::ColumnDefinition column = loka::app::VStack();
      if (g_foreignReorderState && g_foreignReorderState->get())
      {
        column << loka::app::PopupMenu(kItems, 3).selectedIndex(g_foreignPersistSelectedIndexState).testId("ForeignReorderPopup").tag(11)
               << loka::app::EditText(g_foreignPersistEditState).controlTag(461).testId("ForeignReorderEdit").tag(10);
      }
      else
      {
        column << loka::app::EditText(g_foreignPersistEditState).controlTag(461).testId("ForeignReorderEdit").tag(10)
               << loka::app::PopupMenu(kItems, 3).selectedIndex(g_foreignPersistSelectedIndexState).testId("ForeignReorderPopup").tag(11);
      }
      c.declare(column);
    }

    virtual void declareObservedStates(loka::app::scene::ObservedStateRegistrar &registrar)
    {
      if (g_foreignReorderState)
      {
        registrar.observe(g_foreignReorderState, loka::app::scene::NODE_DIRTY_CHILD);
      }
    }
  };

  loka::app::scene::BoundaryDefinition<DynamicWin32ForeignReorderProps, DynamicWin32ForeignReorderNode> DynamicWin32ForeignReorderBoundary()
  {
    return loka::app::scene::DynamicCompositionBoundary<DynamicWin32ForeignReorderNode>();
  }

  class DynamicWin32ForeignReorderHostNode;
  typedef loka::app::scene::BoundaryPropsFor<DynamicWin32ForeignReorderHostNode> DynamicWin32ForeignReorderHostProps;

  class DynamicWin32ForeignReorderHostNode : public loka::app::scene::BoundaryNodeFor<DynamicWin32ForeignReorderHostNode>
  {
  public:
    DynamicWin32ForeignReorderHostNode(const DynamicWin32ForeignReorderHostProps &p)
        : loka::app::scene::BoundaryNodeFor<DynamicWin32ForeignReorderHostNode>(DynamicWin32ForeignReorderHostProps(p)), observed_() {}

    virtual void attachNode(loka::app::scene::NodeComposition &c)
    {
      c.declareStates().state(this->observed_, false);
      g_foreignReorderState = this->observed_.mutableState();
    }

    virtual void detachNode(loka::app::scene::NodeComposition &c)
    {
      (void)c;
      g_foreignReorderState = 0;
    }

    virtual void composeNode(loka::app::scene::NodeComposition &c)
    {
      c.declare(DynamicWin32ForeignReorderBoundary());
    }

  private:
    loka::app::scene::BoundState<bool> observed_;
  };

  loka::app::scene::BoundaryDefinition<DynamicWin32ForeignReorderHostProps, DynamicWin32ForeignReorderHostNode> DynamicWin32ForeignReorderHostBoundary()
  {
    return loka::app::scene::Boundary<DynamicWin32ForeignReorderHostNode>();
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

void testWin32ScenePlatformRelayoutReusesCellAndTextContexts()
{
  loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Label"));
  loka::core::MutableState<loka::core::String> cellState(loka::core::String::Literal("42"));
  g_staticTextState = &textState;
  g_staticCellState = &cellState;

  HWND rootHwnd = CreateWindowExA(0, "STATIC", "LokaWin32TestRoot", WS_OVERLAPPEDWINDOW,
                                  0, 0, 400, 320, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(rootHwnd != NULL);

  Win32ScenePlatformController controller(rootHwnd);
  loka::app::scene::Scene scene(StaticWin32CellTextBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);

  ::loka::app::scene::Node *textNode = findNodeByTestId(root, "ReuseText");
  ::loka::app::scene::Node *cellNode = findNodeByTestId(root, "ReuseCell");
  assert(textNode != 0);
  assert(cellNode != 0);

  ::loka::app::scene::NodeContext *textContext = textNode->getContext();
  ::loka::app::scene::NodeContext *cellContext = cellNode->getContext();
  assert(textContext != 0);
  assert(cellContext != 0);

  controller.relayout(520, 360);

  assert(textNode->getContext() == textContext);
  assert(cellNode->getContext() == cellContext);

  scene.unmount();
  DestroyWindow(rootHwnd);
  g_staticTextState = 0;
  g_staticCellState = 0;
}

void testWin32ScenePlatformDynamicPropsAndLayoutReuseContexts()
{
  loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Short"));
  loka::core::MutableState<bool> enabledState(false);
  g_textState = &textState;
  g_enabledState = &enabledState;

  HWND rootHwnd = CreateWindowExA(0, "STATIC", "LokaWin32TestRoot", WS_OVERLAPPEDWINDOW,
                                  0, 0, 400, 320, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(rootHwnd != NULL);

  Win32ScenePlatformController controller(rootHwnd);
  loka::app::scene::Scene scene(DynamicWin32TestBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  ::loka::app::scene::Node *textNode = findNodeByTestId(root, "MainText");
  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "MainButton");
  assert(textNode != 0);
  assert(buttonNode != 0);
  ::loka::app::scene::NodeContext *textContext = textNode->getContext();
  ::loka::app::scene::NodeContext *buttonContext = buttonNode->getContext();
  assert(textContext != 0);
  assert(buttonContext != 0);
  const int initialChildWindowCount = countChildWindows(rootHwnd);

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    enabledState.set(true);
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  textNode = findNodeByTestId(root, "MainText");
  buttonNode = findNodeByTestId(root, "MainButton");
  assert(textNode != 0);
  assert(buttonNode != 0);
  assert(textNode->getContext() == textContext);
  assert(buttonNode->getContext() == buttonContext);
  assert(countChildWindows(rootHwnd) == initialChildWindowCount);

  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    textState.set(loka::core::String::Literal("A much longer line that should relayout without rebuilding controls"));
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  textNode = findNodeByTestId(root, "MainText");
  buttonNode = findNodeByTestId(root, "MainButton");
  assert(textNode != 0);
  assert(buttonNode != 0);
  assert(textNode->getContext() == textContext);
  assert(buttonNode->getContext() == buttonContext);
  assert(countChildWindows(rootHwnd) == initialChildWindowCount);

  scene.unmount();
  DestroyWindow(rootHwnd);
  g_textState = 0;
  g_enabledState = 0;
}

void testWin32ScenePlatformFullRebuildFlagControlsContextReuse()
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
  assert(buttonNode != 0);
  ::loka::app::scene::NodeContext *buttonContext = buttonNode->getContext();
  assert(buttonContext != 0);

  controller.onChange(root, loka::app::scene::NODE_DIRTY_LAYOUT, false);
  assert(buttonNode->getContext() == buttonContext);

  controller.onChange(root, loka::app::scene::NODE_DIRTY_CHILD, true);
  assert(buttonNode->getContext() != 0);
  assert(buttonNode->getContext() != buttonContext);

  scene.unmount();
  DestroyWindow(rootHwnd);
  g_staticEditTextState = 0;
  g_staticSelectedIndexState = 0;
  g_staticImageState = 0;
}

void testWin32ScenePlatformChildRebuildCleansUpOldContexts()
{
  loka::core::MutableState<bool> swapState(false);
  g_swapChildState = &swapState;

  HWND rootHwnd = CreateWindowExA(0, "STATIC", "LokaWin32TestRoot", WS_OVERLAPPEDWINDOW,
                                  0, 0, 400, 320, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(rootHwnd != NULL);

  Win32ScenePlatformController controller(rootHwnd);
  loka::app::scene::Scene scene(DynamicWin32SwapHostBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "SwapButton");
  assert(buttonNode != 0);
  assert(buttonNode->getContext() != 0);
  assert(countChildWindows(rootHwnd) == 1);

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  boundary = boundary ? boundary->childrenHead() ? boundary->childrenHead()->asBoundary() : 0 : 0;
  assert(boundary != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    swapState.set(true);
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(findNodeByTestId(root, "SwapButton") == 0);
  ::loka::app::scene::Node *editNode = findNodeByTestId(root, "SwapEdit");
  assert(editNode != 0);
  assert(editNode->getContext() != 0);
  assert(countChildWindows(rootHwnd) == 1);

  scene.unmount();
  DestroyWindow(rootHwnd);
  g_swapChildState = 0;
}

void testWin32ScenePlatformForeignObservedChildRebuildSwapsContexts()
{
  HWND rootHwnd = CreateWindowExA(0, "STATIC", "LokaWin32TestRoot", WS_OVERLAPPEDWINDOW,
                                  0, 0, 400, 320, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(rootHwnd != NULL);

  Win32ScenePlatformController controller(rootHwnd);
  loka::app::scene::Scene scene(DynamicWin32ForeignSwapHostBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "ForeignSwapButton");
  assert(buttonNode != 0);
  assert(buttonNode->getContext() != 0);
  assert(findNodeByTestId(root, "ForeignSwapText") == 0);
  const int initialChildWindowCount = countChildWindows(rootHwnd);

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  assert(g_foreignSwapChildState != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    g_foreignSwapChildState->set(true);
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(findNodeByTestId(root, "ForeignSwapButton") == 0);
  ::loka::app::scene::Node *textNode = findNodeByTestId(root, "ForeignSwapText");
  assert(textNode != 0);
  assert(textNode->getContext() != 0);
  assert(countChildWindows(rootHwnd) == initialChildWindowCount);

  scene.unmount();
  DestroyWindow(rootHwnd);
  g_foreignSwapChildState = 0;
}

void testWin32ScenePlatformForeignObservedChildRebuildPreservesSiblingContexts()
{
  loka::core::MutableState<loka::core::String> editTextState(loka::core::String::Literal("Hello"));
  loka::core::MutableState<int> selectedIndexState(1);
  g_foreignPersistEditState = &editTextState;
  g_foreignPersistSelectedIndexState = &selectedIndexState;

  HWND rootHwnd = CreateWindowExA(0, "STATIC", "LokaWin32TestRoot", WS_OVERLAPPEDWINDOW,
                                  0, 0, 400, 320, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(rootHwnd != NULL);

  Win32ScenePlatformController controller(rootHwnd);
  loka::app::scene::Scene scene(DynamicWin32ForeignPersistHostBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  ::loka::app::scene::Node *buttonNode = findNodeByTestId(root, "ForeignPersistButton");
  ::loka::app::scene::Node *editNode = findNodeByTestId(root, "ForeignPersistEdit");
  ::loka::app::scene::Node *popupNode = findNodeByTestId(root, "ForeignPersistPopup");
  assert(buttonNode != 0);
  assert(editNode != 0);
  assert(popupNode != 0);
  ::loka::app::scene::NodeContext *editContext = editNode->getContext();
  ::loka::app::scene::NodeContext *popupContext = popupNode->getContext();
  assert(editContext != 0);
  assert(popupContext != 0);
  const int initialChildWindowCount = countChildWindows(rootHwnd);

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  assert(g_foreignSwapChildState != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    g_foreignSwapChildState->set(true);
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(findNodeByTestId(root, "ForeignPersistButton") == 0);
  assert(findNodeByTestId(root, "ForeignPersistText") != 0);
  editNode = findNodeByTestId(root, "ForeignPersistEdit");
  popupNode = findNodeByTestId(root, "ForeignPersistPopup");
  assert(editNode != 0);
  assert(popupNode != 0);
  assert(editNode->getContext() == editContext);
  assert(popupNode->getContext() == popupContext);
  assert(countChildWindows(rootHwnd) == initialChildWindowCount);

  scene.unmount();
  DestroyWindow(rootHwnd);
  g_foreignPersistEditState = 0;
  g_foreignPersistSelectedIndexState = 0;
  g_foreignSwapChildState = 0;
}

void testWin32ScenePlatformForeignObservedChildReorderPreservesSiblingContexts()
{
  loka::core::MutableState<loka::core::String> editTextState(loka::core::String::Literal("Hello"));
  loka::core::MutableState<int> selectedIndexState(1);
  g_foreignPersistEditState = &editTextState;
  g_foreignPersistSelectedIndexState = &selectedIndexState;

  HWND rootHwnd = CreateWindowExA(0, "STATIC", "LokaWin32TestRoot", WS_OVERLAPPEDWINDOW,
                                  0, 0, 400, 320, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(rootHwnd != NULL);

  Win32ScenePlatformController controller(rootHwnd);
  loka::app::scene::Scene scene(DynamicWin32ForeignReorderHostBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::Node *root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(root != 0);
  ::loka::app::scene::Node *editNode = findNodeByTestId(root, "ForeignReorderEdit");
  ::loka::app::scene::Node *popupNode = findNodeByTestId(root, "ForeignReorderPopup");
  assert(editNode != 0);
  assert(popupNode != 0);
  ::loka::app::scene::NodeContext *editContext = editNode->getContext();
  ::loka::app::scene::NodeContext *popupContext = popupNode->getContext();
  assert(editContext != 0);
  assert(popupContext != 0);
  const int initialChildWindowCount = countChildWindows(rootHwnd);

  loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);
  assert(g_foreignReorderState != 0);
  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    g_foreignReorderState->set(true);
  }
  scene.flushInvalidation();

  root = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  editNode = findNodeByTestId(root, "ForeignReorderEdit");
  popupNode = findNodeByTestId(root, "ForeignReorderPopup");
  assert(editNode != 0);
  assert(popupNode != 0);
  assert(editNode->getContext() == editContext);
  assert(popupNode->getContext() == popupContext);
  assert(countChildWindows(rootHwnd) == initialChildWindowCount);

  scene.unmount();
  DestroyWindow(rootHwnd);
  g_foreignPersistEditState = 0;
  g_foreignPersistSelectedIndexState = 0;
  g_foreignReorderState = 0;
}

void testWin32ScenePlatformBoundaryLocalPaintQueuesDirtyRect()
{
  HWND rootHwnd = CreateWindowExA(0, "STATIC", "LokaWin32TestRoot", WS_OVERLAPPEDWINDOW,
                                  0, 0, 400, 320, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(rootHwnd != NULL);

  Win32ScenePlatformController controller(rootHwnd);
  loka::app::scene::Scene scene(StaticWin32ControlBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::BoundaryNode *rootBoundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  ::loka::app::scene::Node *rootNode = loka::dsl::testing::SceneTestAccess::rootNode(scene);
  assert(rootBoundary != 0);
  assert(rootNode != 0);

  rootBoundary->setLayoutBounds(0, 0, 64, 24);
  rootBoundary->noteOpaquePaintCoverage(true);

  loka::app::scene::PlatformApplyPlan plan;
  plan.paintKind = loka::app::scene::PlatformApplyPlan::PAINT_LOCAL_OPAQUE;
  plan.paintRoot = rootBoundary;
  plan.layoutRoot = 0;
  plan.structureRoot = 0;

  const loka::app::scene::BoundaryLocalApplyInfo info = rootBoundary->localApplyInfo(plan);
  assert(info.hasPaintBoundsHint());
  assert(!info.hasLayoutBoundsHint());
  controller.onBoundaryApply(rootNode, rootBoundary, info, plan);
  assert(controller.hasPendingSync());
  assert(loka::dsl::testing::Win32ScenePlatformTestAccess::queuedRectInvalidates(controller) == 1);
  assert(loka::dsl::testing::Win32ScenePlatformTestAccess::queuedPaintBoundsInvalidates(controller) == 1);
  assert(loka::dsl::testing::Win32ScenePlatformTestAccess::queuedLayoutBoundsInvalidates(controller) == 0);
  assert(loka::dsl::testing::Win32ScenePlatformTestAccess::queuedMissingBoundsInvalidates(controller) == 0);
  controller.synchronize();
  assert(!controller.hasPendingSync());

  scene.unmount();
  DestroyWindow(rootHwnd);
}

void testWin32ScenePlatformPaintOnlyStateChangeUsesLocalApplyPath()
{
  loka::core::MutableState<loka::core::String> textState(loka::core::String::Literal("Short"));
  loka::core::MutableState<bool> enabledState(false);
  g_textState = &textState;
  g_enabledState = &enabledState;

  HWND rootHwnd = CreateWindowExA(0, "STATIC", "LokaWin32TestRoot", WS_OVERLAPPEDWINDOW,
                                  0, 0, 400, 320, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(rootHwnd != NULL);

  Win32ScenePlatformController controller(rootHwnd);
  loka::app::scene::Scene scene(DynamicWin32TestBoundary());
  scene.mount(&controller);
  scene.updateAttached(true);

  ::loka::app::scene::BoundaryNode *boundary = loka::dsl::testing::SceneTestAccess::rootBoundary(scene);
  assert(boundary != 0);

  loka::dsl::testing::Win32ScenePlatformTestAccess::resetRedrawStats(controller);

  {
    loka::core::StateTrackerGuard guard(boundary->tracker());
    enabledState.set(true);
  }
  scene.flushInvalidation();

  assert(loka::dsl::testing::Win32ScenePlatformTestAccess::onChangeCalls(controller) == 1);
  assert(loka::dsl::testing::Win32ScenePlatformTestAccess::onBoundaryApplyCalls(controller) == 1);

  scene.unmount();
  DestroyWindow(rootHwnd);
  g_textState = 0;
  g_enabledState = 0;
}

void testWin32ScenePlatformPropsOnlyOnChangeSkipsLayout()
{
  HWND rootHwnd = CreateWindowExA(0, "STATIC", "LokaWin32TestRoot", WS_OVERLAPPEDWINDOW,
                                  0, 0, 400, 320, NULL, NULL, GetModuleHandle(NULL), NULL);
  assert(rootHwnd != NULL);

  Win32ScenePlatformController controller(rootHwnd);
  controller.onChange(0, ::loka::app::scene::NODE_DIRTY_PROPS, false);
  assert(loka::dsl::testing::Win32ScenePlatformTestAccess::onChangeCalls(controller) == 1);
  assert(loka::dsl::testing::Win32ScenePlatformTestAccess::lastOnChangeFlags(controller) == ::loka::app::scene::NODE_DIRTY_PROPS);
  assert(!loka::dsl::testing::Win32ScenePlatformTestAccess::lastOnChangeRequiredLayout(controller));
  assert(!loka::dsl::testing::Win32ScenePlatformTestAccess::lastOnChangeFullRebuild(controller));
  assert(loka::dsl::testing::Win32ScenePlatformTestAccess::clientWidth(controller) == 0);
  assert(loka::dsl::testing::Win32ScenePlatformTestAccess::clientHeight(controller) == 0);

  DestroyWindow(rootHwnd);
}

#endif
