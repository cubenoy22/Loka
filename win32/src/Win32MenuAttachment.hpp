#ifndef LOKA_WIN32_MENU_ATTACHMENT_HPP
#define LOKA_WIN32_MENU_ATTACHMENT_HPP

#include "app/Menu.hpp"
#include "core/util/OwnedDef.hpp"
#include <windows.h>
#include <vector>

class Win32Window;
namespace loka { namespace app { namespace scene { class Scene; } } }

/** Owns one Window's native menu and endpoint borrows. Scene detach makes
    the installed menu inert until replacement or native window destruction. */
class Win32MenuAttachment
{
public:
  explicit Win32MenuAttachment(Win32Window &window);
  ~Win32MenuAttachment();
  /** Equal offers return false. Snapshot refusal clears the baseline while
      keeping the installed projection; native build refusal preserves it. */
  /** project() answers three ways so the caller can tell a refused native swap
      (nothing changed, keep the pending diff) from an equal offer or a swap. */
  enum ProjectResult
  {
    PROJECT_APPLIED,
    PROJECT_UNCHANGED,
    PROJECT_REFUSED
  };
  ProjectResult project(const loka::app::MenuBarDefinition *bar,
               const loka::app::scene::Scene *source);
  void disconnect();
  void releaseFrom(const loka::app::scene::Scene *source);
  bool dispatch(int commandId);

private:
  friend class Win32Window;
  Win32MenuAttachment(const Win32MenuAttachment &);
  Win32MenuAttachment &operator=(const Win32MenuAttachment &);
  enum DetachMode
  {
    DETACH_PRESERVING_CONTENT_FRAME,
    DETACH_FOR_TEARDOWN
  };
  bool reset(DetachMode mode);
  bool resetForTeardown();
  struct MenuCommand
  {
    int commandId;
    loka::app::MenuActionType action;
    loka::core::EmitterState *emitter;
  };

  struct MenuBinding
  {
    HMENU menu;
    // Win32 addresses a menu item either by command id or by position: a
    // leaf carries its command id with MF_BYCOMMAND, a popup title has no
    // command and carries its appended index with MF_BYPOSITION.
    UINT item;
    UINT byFlags;
    HWND hwnd;
    loka::core::State<bool> *enabledState;
    bool invertEnabled;
    loka::core::State<bool> *checkedState;
  };

  void clearMenuBindings();
  static void MenuEnabledChangedThunk(void *userData);
  static void MenuCheckedChangedThunk(void *userData);
  void bindMenuItemStates(HMENU menu,
                          UINT item,
                          UINT byFlags,
                          const loka::app::MenuItemDefinition *itemDef,
                          HWND hwnd);
  bool buildMenuItem(HMENU menu, const loka::app::MenuItemDefinition *itemDef, HWND hwnd);
  bool buildMenuItems(HMENU menu, const loka::app::MenuItemDefinition *itemsHead, HWND hwnd);

  Win32Window &window_;
  HMENU menu_;
  int nextCommandId_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
  loka::core::OwnedDef<loka::app::MenuBarDefinition> applied_;
  const loka::app::scene::Scene *source_;
};
#endif
