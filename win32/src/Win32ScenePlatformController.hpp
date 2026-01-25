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

  virtual void onChange(loka::app::scene::Node *rootNode, loka::app::scene::NodeDirtyFlags flags);
  virtual void synchronize();
  virtual void destroy();

  bool handleCommand(WPARAM wParam, LPARAM lParam);
  void relayout(int clientWidth, int clientHeight);

private:
  struct LayoutState
  {
    int x;
    int y;
    int width;
    int height;
  };

  int layoutNode(loka::app::scene::Node *node, const LayoutState &state);
  void performLayout(int clientWidth, int clientHeight);
  void clearContexts();
  void clearNodeContexts(loka::app::scene::Node *node);
  int measureClientWidth(int requestedWidth) const;

  HWND rootHwnd_;
  loka::app::scene::Node *rootNode_;
  int clientWidth_;
  int clientHeight_;
  std::map<HWND, Win32ButtonContext *> buttonMap_;
  std::map<HWND, Win32EditTextContext *> editMap_;
  std::map<HWND, Win32PopupMenuContext *> popupMap_;
};

#endif // LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
