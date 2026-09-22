#include <cstdio>
#include <Processes.h>

#include "app/bootstrap/RunApp.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "app/Menu.hpp"
#include "app/nodes/Text.hpp"
#include "app/nodes/controls/TextEditor.hpp"
#include "app/nodes/nestable/Box.hpp"
#include "app/nodes/nestable/RowColumn.hpp"
#include "core/util/StateTrackerGuard.hpp"

#if !defined(LOKA_RETRO68)
#error Text editor application requires a Toolbox build
#endif

namespace
{
  using namespace loka::app;
  using namespace loka::core;

  /** RunApp destroys every borrowing window before this document owner. */
  class TextEditorConfig : public AppConfigurable
  {
  public:
    explicit TextEditorConfig(PlatformContext *context)
        : AppConfigurable(context),
          cursor_(&this->cursorValue_, &this->tracker_)
    {
      this->tracker_.addState(&this->cursorValue_);
      this->tracker_.addState(&this->status_);
      if (this->lines_.attach(&this->tracker_, TextEditorProps::kMaxLines) != ATTACH_OK
          || this->lines_.insert(0, String::Literal("first")) != EDIT_OK
          || this->lines_.insert(1, String::Literal("second")) != EDIT_OK
          || this->lines_.insert(2, String::Literal("third")) != EDIT_OK)
      {
        this->lines_.detach();
        return;
      }
      this->cursor_.set(LineCursor(this->lines_.at(0).id, 0));
      this->refreshStatus();
      this->lines_.revision().deferBind(&TextEditorConfig::RefreshStatus, this);
      this->cursor_.state()->deferBind(&TextEditorConfig::RefreshStatus, this);
    }

    virtual ~TextEditorConfig()
    {
      this->lines_.revision().deferUnbind(&TextEditorConfig::RefreshStatus, this);
      this->cursor_.state()->deferUnbind(&TextEditorConfig::RefreshStatus, this);
      this->lines_.detach();
      this->tracker_.removeState(&this->status_);
      this->tracker_.removeState(&this->cursorValue_);
    }

    virtual void compose(AppComposition &composition)
    {
      Column content = Column() << Text("Type in the box; File > Quit to leave.");
      if (this->lines_.size())
        content << (Box().size(560, 300) << TextEditor(this->lines_, this->cursor_))
                << Text(&this->status_);
      else
        content << Text("Editor unavailable: document allocation failed");
      composition << WindowDef(WindowProps()
                                   .frame(20, 40, 600, 400)
                                   .title("Loka TextEditor")
                                   .visible(true)
                                   .scene(content));
    }

    virtual void composeMenu(MenuComposition &composition)
    {
      composition.declare(Menu("File") << MenuItem("Quit").actionType(MENU_ACTION_QUIT_APP));
    }

  private:
    PushStateTracker tracker_;
    ObservableList<String> lines_;
    MutableState<LineCursor> cursorValue_;
    scene::NodeState<LineCursor> cursor_;
    MutableState<String> status_;

    void refreshStatus()
    {
      std::size_t bytes = this->lines_.size() ? this->lines_.size() - 1 : 0;
      for (unsigned short i = 0; i < this->lines_.size(); ++i)
      {
        std::size_t units = 0;
        if (!this->lines_.at(i).value.requiredUnits(StringEncodingUtf8, units))
        {
          StateTrackerGuard transaction(&this->tracker_);
          this->status_.set(String::Literal("Document byte count unavailable"));
          return;
        }
        bytes += units;
      }
      const LineCursor cursor = this->cursor_.get();
      char text[96];
      std::sprintf(text, "Lines: %u  Cursor: %d:%d  Bytes: %lu",
                   static_cast<unsigned>(this->lines_.size()),
                   this->lines_.find(cursor.line), cursor.column,
                   static_cast<unsigned long>(bytes));
      StateTrackerGuard transaction(&this->tracker_);
      this->status_.set(String::Literal(text));
    }

    static void RefreshStatus(void *data)
    {
      static_cast<TextEditorConfig *>(data)->refreshStatus();
    }
  };
} // namespace

int main(int, char **)
{
  const int result = loka::platform::RunApp<TextEditorConfig>();
  ExitToShell();
  return result;
}
