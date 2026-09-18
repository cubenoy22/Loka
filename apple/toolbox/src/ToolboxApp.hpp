#ifndef LOKA_TOOLBOX_APP_HPP
#define LOKA_TOOLBOX_APP_HPP

#include "app/core/App.hpp"
#include <vector>
#include <Menus.h>
#include <Quickdraw.h>
#include "ToolboxActivationPhase.hpp"
class ToolboxApp;
class ToolboxSceneDebugStats;

/** The app-wide cursor writer. Scopes borrow it only for non-yielding work. */
class CursorOwner
{
public:
  enum HoverCursor { HOVER_ARROW, HOVER_IBEAM };
  enum AppliedCursor { UNKNOWN, ARROW, IBEAM, WATCH };

  explicit CursorOwner(ToolboxApp &app);
  /** Load optional resources once, after Toolbox initialization. */
  void initialize();
  void setHover(HoverCursor cursor, bool force = false);
  void apply(bool force);
  /** Re-sample the front window after a native writer may have changed cursors. */
  void reconcile();
  void assertIdle() const;
#ifdef TEST_BUILD
  void copyDiagnostics(ToolboxSceneDebugStats &snapshot) const;
#endif

private:
  friend class BusyScope;
  /** Startup copy survives resource purging without update-cycle allocation. */
  class CachedCursor
  {
  public:
    CachedCursor() : value_(), available_(false) {}
    void load(short resourceId);
    const Cursor *get() const { return this->available_ ? &this->value_ : 0; }
  private:
    Cursor value_;
    bool available_;
  };
  CursorOwner(const CursorOwner &);
  CursorOwner &operator=(const CursorOwner &);
  void enterBusy();
  void exitBusy();
  ToolboxApp &app_;
  HoverCursor hoverCursor_;
  short busyDepth_;
  AppliedCursor lastApplied_;
  unsigned long nativeApplies_;
  unsigned long outerEntries_;
  unsigned long outerExits_;
  CachedCursor iBeam_;
  CachedCursor watch_;
};

/** Synchronous, non-copyable borrow; nested scopes issue no native writes. */
class BusyScope
{
public:
  explicit BusyScope(CursorOwner &owner) : owner_(&owner) { this->owner_->enterBusy(); }
  /** A null owner makes a conditional borrow inert, without allocation. */
  explicit BusyScope(CursorOwner *owner) : owner_(owner)
  {
    if (this->owner_)
      this->owner_->enterBusy();
  }
  ~BusyScope()
  {
    if (this->owner_)
      this->owner_->exitBusy();
  }
private:
  BusyScope(const BusyScope &);
  BusyScope &operator=(const BusyScope &);
  CursorOwner *owner_;
};

class ToolboxApp : public App
{
protected:
  explicit ToolboxApp(AppConfigurable *config);
  virtual ~ToolboxApp();
  friend class ToolboxPlatformContext;

public:
  CursorOwner &cursorOwner() { return this->cursorOwner_; }
  virtual void run();
  virtual void quit();
  void handleMenuSelection(short menuId, short item);
  static void MenuEnabledChangedThunk(void *userData);
  static void MenuCheckedChangedThunk(void *userData);

public:
  virtual void applyMenuBar(Window *activeWindow);

  struct MenuCommand
  {
    short menuId;
    short itemIndex;
    loka::app::MenuActionType action;
    loka::core::EmitterState *emitter;
  };

  struct MenuBinding
  {
    ToolboxApp *app;
    MenuHandle menu;
    short itemIndex;
    loka::core::State<bool> *enabledState;
    bool invertEnabled;
    loka::core::State<bool> *checkedState;
  };

  /** Called by a live menu binding when it has updated the app-owned menu
      data. Foreground: redraws the menu bar immediately. Background: the
      shared menu bar is the foreground application's surface, so the single
      redraw is deferred until resume. */
  void noteMenuBarChangedFromBinding();

private:
  friend class CursorOwner;
  void sampleHover(bool force);
  struct MenuEntry
  {
    MenuHandle menu;
    short menuId;
    bool isAppMenu;
    loka::core::String title;
  };

  void clearMenuBindings();
  void clearMenuBindingsFor(MenuHandle menuHandle, short menuId);
  void resetMenuState();
  void disposeMenuEntries();
  void disposeHierarchicalMenus();
  /** Applies recorded scene changes and, while foreground, paints each window
      once at the run-loop tick's presentation boundary. */
  void present(ActivationPhase phase);
  /** The run loop owns this; every step branches on it rather than taking
      per-step booleans (see ToolboxActivationPhase.hpp). */
  ActivationPhase activationPhase_;
  CursorOwner cursorOwner_;
  /** A background binding changed the menu data; one DrawMenuBar is owed at
      resume. */
  bool menuBarDrawDeferred_;
  short nextMenuId_;
  std::vector<MenuCommand> commands_;
  std::vector<MenuBinding *> bindings_;
  std::vector<MenuEntry> menuEntries_;
  std::vector<MenuHandle> hierarchicalMenus_;
  bool running_;
};

#ifdef TEST_BUILD
#include "debug/ToolboxSceneDebugStats.hpp"
inline void CursorOwner::copyDiagnostics(ToolboxSceneDebugStats &snapshot) const
{
  snapshot.cursorNativeApplies = this->nativeApplies_;
  snapshot.cursorOuterEntries = this->outerEntries_;
  snapshot.cursorOuterExits = this->outerExits_;
  snapshot.cursorDepth = this->busyDepth_;
  snapshot.cursorLastApplied = this->lastApplied_;
}
#endif

#endif // LOKA_TOOLBOX_APP_HPP
