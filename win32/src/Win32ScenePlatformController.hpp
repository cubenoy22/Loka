#ifndef DECLARA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
#define DECLARA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP

#include <windows.h>
#include <map>
#include <string>
#include <vector>
#include "core/State.hpp"
#include "core2/scene/PlatformController.hpp"

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

  virtual void materialize(declara::core::scene::Node *rootNode);
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

  class NodeContext
  {
  public:
    virtual ~NodeContext() {}
    virtual void destroy() = 0;
  };

  class ButtonContext : public NodeContext
  {
  public:
    ButtonContext(HWND parent, int x, int y, int width, declara::app::ButtonNode *node);
    virtual ~ButtonContext();

    virtual void destroy();
    HWND hwnd() const { return hwnd_; }
    bool handleCommand(WPARAM wParam, LPARAM lParam);

  private:
    void bindText();
    void unbindText();
    void applyText();
    static void TextChangedThunk(void *userData);

    declara::app::ButtonNode *node_;
    HWND hwnd_;
    State<std::string> *textState_;
  };

  class TextContext : public NodeContext
  {
  public:
    TextContext(HWND parent, int x, int y, int width, declara::app::TextNode *node);
    virtual ~TextContext();

    virtual void destroy();

  private:
    void bindText();
    void unbindText();
    void applyText();
    static void TextChangedThunk(void *userData);

    declara::app::TextNode *node_;
    HWND hwnd_;
    State<std::string> *textState_;
  };

  class EditTextContext : public NodeContext
  {
  public:
    EditTextContext(HWND parent, int x, int y, int width, declara::app::EditTextNode *node);
    virtual ~EditTextContext();

    virtual void destroy();
    HWND hwnd() const { return hwnd_; }
    bool handleCommand(WPARAM wParam, LPARAM lParam);

  private:
    void bindText();
    void unbindText();
    void applyText();
    void syncStateFromControl();
    static void TextChangedThunk(void *userData);

    declara::app::EditTextNode *node_;
    HWND hwnd_;
    State<std::string> *textState_;
    bool applyingFromState_;
    bool updatingFromControl_;
  };

  int layoutNode(declara::core::scene::Node *node, const LayoutState &state);
  void performLayout(int clientWidth, int clientHeight);
  void clearContexts();
  int measureClientWidth(int requestedWidth) const;

  HWND rootHwnd_;
  declara::core::scene::Node *rootNode_;
  int clientWidth_;
  int clientHeight_;
  std::vector<NodeContext *> contexts_;
  std::map<HWND, ButtonContext *> buttonMap_;
  std::map<HWND, EditTextContext *> editMap_;
};

#endif // DECLARA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
