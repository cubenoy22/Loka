#ifndef DECLARA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
#define DECLARA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP

#include <windows.h>
#include <map>
#include "core2/scene/PlatformController.hpp"
#include "context/Win32ButtonContext.hpp"
#include "context/Win32TextContext.hpp"
#include "context/Win32EditTextContext.hpp"

namespace declara
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

class Win32ScenePlatformController : public declara::core::scene::IPlatformController
{
public:
  explicit Win32ScenePlatformController(HWND rootHwnd);
  virtual ~Win32ScenePlatformController();

  virtual void onChange(declara::core::scene::Node *rootNode, declara::core::scene::NodeDirtyFlags flags);
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

  int layoutNode(declara::core::scene::Node *node, const LayoutState &state);
  void performLayout(int clientWidth, int clientHeight);
  void clearContexts();
  void clearNodeContexts(declara::core::scene::Node *node);
  int measureClientWidth(int requestedWidth) const;

  HWND rootHwnd_;
  declara::core::scene::Node *rootNode_;
  int clientWidth_;
  int clientHeight_;
  std::map<HWND, Win32ButtonContext *> buttonMap_;
  std::map<HWND, Win32EditTextContext *> editMap_;
};

#endif // DECLARA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
