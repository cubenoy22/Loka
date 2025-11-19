#ifndef DECLARA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
#define DECLARA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP

#include <windows.h>
#include <map>
#include <string>
#include "core/State.hpp"
#include "core2/scene/PlatformController.hpp"
#include "core2/scene/NativeNodeContext.hpp"

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

  class ControlContext : public declara::core::scene::NativeNodeContext
  {
  public:
    ControlContext() : destroyed_(false) {}
    virtual ~ControlContext()
    {
      destroy();
    }

    virtual void destroy()
    {
      if (destroyed_)
      {
        return;
      }
      destroyed_ = true;
      onDestroy();
    }

  protected:
    virtual void onDestroy() = 0;

  private:
    bool destroyed_;
  };

  class ButtonContext : public ControlContext
  {
  public:
    ButtonContext(HWND parent, int x, int y, int width, declara::app::ButtonNode *node);
    virtual ~ButtonContext();

    HWND hwnd() const { return hwnd_; }
    bool handleCommand(WPARAM wParam, LPARAM lParam);

  private:
    void bindText();
    void unbindText();
    void applyText();
    static void TextChangedThunk(void *userData);
    virtual void onDestroy();

    declara::app::ButtonNode *node_;
    HWND hwnd_;
    State<std::string> *textState_;
  };

  class TextContext : public ControlContext
  {
  public:
    TextContext(HWND parent, int x, int y, int width, declara::app::TextNode *node);
    virtual ~TextContext();

  private:
    void bindText();
    void unbindText();
    void applyText();
    static void TextChangedThunk(void *userData);
    virtual void onDestroy();

    declara::app::TextNode *node_;
    HWND hwnd_;
    State<std::string> *textState_;
  };

  class EditTextContext : public ControlContext
  {
  public:
    EditTextContext(HWND parent, int x, int y, int width, declara::app::EditTextNode *node);
    virtual ~EditTextContext();

    HWND hwnd() const { return hwnd_; }
    bool handleCommand(WPARAM wParam, LPARAM lParam);

  private:
    void bindText();
    void unbindText();
    void applyText();
    void syncStateFromControl();
    static void TextChangedThunk(void *userData);
    virtual void onDestroy();

    declara::app::EditTextNode *node_;
    HWND hwnd_;
    State<std::string> *textState_;
    bool applyingFromState_;
    bool updatingFromControl_;
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
  std::map<HWND, ButtonContext *> buttonMap_;
  std::map<HWND, EditTextContext *> editMap_;
};

#endif // DECLARA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
