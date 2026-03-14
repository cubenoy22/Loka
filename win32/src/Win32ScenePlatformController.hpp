#ifndef LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
#define LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP

#include <windows.h>
#include <map>
#include "app/scene/PlatformController.hpp"
#include "context/Win32ButtonContext.hpp"
#include "context/Win32TextContext.hpp"
#include "context/Win32EditTextContext.hpp"
#include "context/Win32OpenFileDialogContext.hpp"
#include "context/Win32PopupMenuContext.hpp"

namespace loka
{
  namespace core
  {
    namespace scene
    {
      class Node;
    }
  }

  namespace app
  {
    class BoxNode;
    class ButtonNode;
    class TextNode;
    class EditTextNode;
  }
}

class Win32ScenePlatformController : public loka::app::scene::IPlatformController
{
public:
  explicit Win32ScenePlatformController(HWND rootHwnd);
  virtual ~Win32ScenePlatformController();

  static void requestDirtyRect(HWND targetHwnd, const RECT *rect, BOOL eraseBackground);
  static void requestDirtySubtree(HWND targetHwnd, const RECT *rect, BOOL eraseBackground);
  static void redrawDirtySubtreeNow(HWND targetHwnd, const RECT *rect, BOOL eraseBackground);

  virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags);
  virtual void synchronize();
  virtual bool hasPendingSync() const;
  virtual void destroy();

  bool handleCommand(WPARAM wParam, LPARAM lParam);
  void relayout(int clientWidth, int clientHeight);

private:
  struct PendingInvalidate
  {
    PendingInvalidate() : hwnd(0), eraseBackground(TRUE), fullWindow(false), includeChildren(false)
    {
      rect.left = rect.top = rect.right = rect.bottom = 0;
    }
    HWND hwnd;
    RECT rect;
    BOOL eraseBackground;
    bool fullWindow;
    bool includeChildren;
  };

  struct LayoutState
  {
    int x;
    int y;
    int width;
    int height;
  };

  int layoutNode(loka::app::scene::Node *node, const LayoutState &state);
  void performLayout(int clientWidth, int clientHeight, bool rebuildContexts);
  void clearContexts();
  void clearNodeContexts(loka::app::scene::Node *node);
  int measureClientWidth(int requestedWidth) const;
  void queueDirtyRect(HWND targetHwnd, const RECT *rect, BOOL eraseBackground, bool includeChildren);

  HWND rootHwnd_;
  loka::app::scene::Node *rootNode_;
  int clientWidth_;
  int clientHeight_;
  std::map<HWND, Win32ButtonContext *> buttonMap_;
  std::map<HWND, Win32EditTextContext *> editMap_;
  std::map<HWND, Win32PopupMenuContext *> popupMap_;
  std::vector<PendingInvalidate> pendingInvalidations_;
};

#endif // LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
