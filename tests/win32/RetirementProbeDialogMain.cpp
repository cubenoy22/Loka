#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <cwchar>

#include "RetirementProbeSampler.hpp"
#include "RetirementProbeBitmap.hpp"
#include "platform/file/FileIO.hpp"
#include "platform/StringUTF8.hpp"
#include "testing/app/NativeResourceRetirementTestAccess.hpp"
#include "MainNode.hpp"
#include "ScenarioWindow.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "core/util/ScopedPtr.hpp"

// The existing TEST_BUILD friend is defined only in this executable.
namespace simpleviewer { namespace testing {
  class MainAccess
  {
  public:
    static void dimensions(const MainNode &node, int &width, int &height)
    {
      const loka::core::resource::Image &image = node.image_.state()->getRef();
      width = image.width();
      height = image.height();
    }
    static loka::core::String message(const MainNode &node) { return node.chooserMessage_.get(); }
  };
} }

namespace
{
  using loka::core::resource::Blob;
  using loka::core::resource::Image;

  typedef loka::app::testing::NativeResourceRetirementTestAccess Ledger;

  /** Completed fixture paths only; never retains Blob or Image storage. */
  class ReplacementFiles
  {
  public:
    explicit ReplacementFiles(RetirementProbeLog &log)
    {
      if (!write(L"probe-a.bmp", 1024, 768, 0x40, this->a_)
          || !write(L"probe-b.bmp", 800, 600, 0xc0, this->b_))
        log.error("fixture-write-failed");
      else
      {
        log.note("fixture=probe-a.bmp img=1024x768 pixel_bytes=3145728 file_bytes=3145782");
        log.note("fixture=probe-b.bmp img=800x600 pixel_bytes=1920000 file_bytes=1920054");
      }
    }
    const wchar_t *path(bool first) const { return (first ? this->a_ : this->b_).c_str(); }
  private:
    static bool write(const wchar_t *name, int width, int height, unsigned char fill, std::wstring &path)
    {
      loka::platform::file::FileHandle file;
      if (!ResolveRetirementProbeFile(name, file, path)) return false;
      const Blob blob = retirement_probe::Bitmap(width, height, fill);
      if (!blob.size()) return false;
      std::FILE *stream = loka::platform::file::OpenWriteTruncate(file);
      if (!stream) return false;
      const bool written = std::fwrite(blob.data(), 1, blob.size(), stream) == blob.size();
      const bool flushed = loka::platform::file::FlushWrite(stream, file);
      const bool closed = std::fclose(stream) == 0;
      return written && flushed && closed;
    }
    std::wstring a_, b_;
  };

  /** Stack-only descendant search, preferring the Explorer cmb13 owner. */
  struct FilenameBox
  {
    explicit FilenameBox(HWND root) : dialog(root), edit(0), ownerId(0), match(0) {}
    static BOOL CALLBACK visit(HWND window, LPARAM data)
    {
      FilenameBox &result = *reinterpret_cast<FilenameBox *>(data);
      wchar_t name[32];
      if (!GetClassNameW(window, name, 32) || std::wcscmp(name, L"Edit") != 0) return TRUE;
      for (HWND parent = GetParent(window); parent && parent != result.dialog; parent = GetParent(parent))
      {
        if (!GetClassNameW(parent, name, 32)) continue;
        const bool combo = std::wcscmp(name, L"ComboBox") == 0;
        const bool extended = std::wcscmp(name, L"ComboBoxEx32") == 0;
        if (!combo && !extended) continue;
        const int id = GetDlgCtrlID(parent);
        if (!result.edit || id == 1148)
        {
          result.edit = window;
          result.ownerId = id;
          result.match = id == 1148 ? "cmb13" : (extended ? "ComboBoxEx32" : "ComboBox");
        }
        if (id == 1148) return FALSE;
      }
      return TRUE;
    }
    HWND dialog, edit;
    int ownerId;
    const char *match;
  };

  /** App-lifetime owner of the timer and sole decoded Image. Timer callbacks
      borrow this owner on the UI thread, including the chooser's modal loop. */
  class DialogProbe : public AppConfigurable
  {
  public:
    explicit DialogProbe(PlatformContext *context)
        : AppConfigurable(context), log_(L"retirement-probe-dialog.log"), app_(0), main_(0),
          open_(), image_(), phase_(WAIT), timer_(0), since_(0), ticks_(0),
          files_(this->log_), replaceBaseHeld_(0)
    {
      const Blob blob = retirement_probe::Bitmap(1024, 768, 0x80);
      if (!context->createImageFromBlob(blob, 0, blob.size(), this->image_))
        this->log_.error("decode-failed");
      else if (this->image_.width() != 1024 || this->image_.height() != 768)
        this->log_.error("unexpected-image-dimensions");
    }
    ~DialogProbe() { this->disarm(); }
    bool ready() const { return this->log_.valid(); }
    int exitCode() const { return this->phase_ == DONE && this->log_.valid() ? 0 : 1; }
    void setApp(App *app) { this->app_ = app; }
    virtual void compose(AppComposition &composition)
    {
      composition << loka::scenario_tests::MakeScenarioWindow<simpleviewer::MainProps, simpleviewer::MainNode>(
          simpleviewer::MainProps().platformContext(this->getPlatformContext()).openDialogEvent(&this->open_),
          &this->main_, 480, 280, "Dialog retirement probe", loka::app::IdlePolicy::interval(0.05),
          &OnIdle, this);
    }

  private:
    enum Phase { WAIT, SEEK, DWELL, DROPPED, DISMISSING, SETTLE,
                 BEGIN_REPLACE, SEEK_A, FILL_A, DISMISS_A, LOAD_A,
                 SEEK_B, FILL_B, DISMISS_B, REPLACING, REPLACE_SETTLE, DONE };
    static DialogProbe *timerOwner_;
    static void OnIdle(Window *, double, void *data) { static_cast<DialogProbe *>(data)->idle(); }
    static void CALLBACK OnTimer(HWND, UINT, UINT_PTR id, DWORD)
    {
      if (timerOwner_ && timerOwner_->timer_ == id) timerOwner_->timerTick();
    }
    static BOOL CALLBACK FindDialog(HWND window, LPARAM result)
    {
      wchar_t name[32];
      if (IsWindowVisible(window) && GetClassNameW(window, name, 32) && std::wcscmp(name, L"#32770") == 0)
      {
        *reinterpret_cast<HWND *>(result) = window;
        return FALSE;
      }
      return TRUE;
    }
    void disarm()
    {
      if (this->timer_) KillTimer(0, this->timer_);
      this->timer_ = 0;
      if (timerOwner_ == this) timerOwner_ = 0;
    }
    void fail(const char *reason)
    {
      this->log_.error(reason);
      this->disarm();
      this->app_->quit();
    }
    void sample(const char *point)
    {
      // Dimensions describe the one fixture even after its last copy drops.
      this->log_.sample(*this->getPlatformContext(), this->ticks_, point, 0, 1024, 768);
    }
    void idle()
    {
      if (!this->log_.valid()) { this->fail("log-failed"); return; }
      if (this->phase_ >= BEGIN_REPLACE) { this->replacementIdle(); return; }
      if (this->phase_ == WAIT)
      {
        if (!this->main_)
        {
          if (++this->ticks_ >= 100) this->fail("mount-timeout");
          return;
        }
        this->ticks_ = 0;
        this->log_.begin("dialog");
        this->sample("pre");
        this->phase_ = SEEK;
        this->since_ = GetTickCount();
        timerOwner_ = this;
        this->timer_ = SetTimer(0, 0, 100, &OnTimer);
        if (!this->timer_) { this->fail("timer-failed"); return; }
        // Arm before emitting: even a synchronous presentation has a timer.
        this->open_.emit();
      }
      else if (this->phase_ == DISMISSING)
      {
        HWND dialog = 0;
        EnumThreadWindows(GetCurrentThreadId(), &FindDialog, reinterpret_cast<LPARAM>(&dialog));
        if (dialog) return;
        this->phase_ = SETTLE;
        this->ticks_ = 0;
      }
      if (this->phase_ == SETTLE)
      {
        this->sample("after-dismiss");
        if (++this->ticks_ == 5)
        {
          this->log_.summary();
          this->phase_ = BEGIN_REPLACE;
        }
      }
    }
    void timerTick()
    {
      if (this->phase_ >= BEGIN_REPLACE) { this->replacementTimer(); return; }
      HWND dialog = 0;
      EnumThreadWindows(GetCurrentThreadId(), &FindDialog, reinterpret_cast<LPARAM>(&dialog));
      const DWORD now = GetTickCount();
      if (this->phase_ == SEEK)
      {
        if (!dialog)
        {
          if (now - this->since_ >= 5000) this->fail("dialog-not-found");
          return;
        }
        this->sample("open");
        this->phase_ = DWELL;
        this->since_ = now;
      }
      else if (!dialog)
        this->fail("dialog-disappeared-early");
      else if (this->phase_ == DWELL && now - this->since_ >= 1000)
      {
        this->sample("dwell-before-drop");
        this->image_ = Image();
        this->sample("dwell-after-drop");
        this->phase_ = DROPPED;
        this->since_ = now;
      }
      else if (this->phase_ == DROPPED && now - this->since_ >= 1000)
      {
        this->sample("dwell-late");
        if (!PostMessageW(dialog, WM_COMMAND, IDCANCEL, 0))
        {
          this->fail("dismiss-post-failed");
          return;
        }
        this->phase_ = DISMISSING;
        this->disarm();
      }
    }
    void replacementSample(const char *point)
    {
      int width = 0, height = 0;
      simpleviewer::testing::MainAccess::dimensions(*this->main_, width, height);
      this->log_.sample(*this->getPlatformContext(), this->ticks_, point, 0, width, height);
    }
    bool loaded(bool first) const
    {
      int width = 0, height = 0;
      simpleviewer::testing::MainAccess::dimensions(*this->main_, width, height);
      return width == (first ? 1024 : 800) && height == (first ? 768 : 600);
    }
    bool loadRefused()
    {
      std::string message;
      if (!loka::platform::CollectUtf8(simpleviewer::testing::MainAccess::message(*this->main_), message))
      {
        this->fail("chooser-message-unreadable");
        return true;
      }
      // The prior canceled result can persist until this chooser's result is applied.
      if (message == "(none)" || message == "Canceled" || message.find("Loka file: ") == 0) return false;
      // Error vocabulary is owned by ImageLoadSessionFlow.hpp, not a new getter.
      this->fail((std::string("load-refused message=\"") + message + "\"").c_str());
      return true;
    }
    void openReplacement(bool first)
    {
      this->phase_ = first ? SEEK_A : SEEK_B;
      this->since_ = GetTickCount();
      timerOwner_ = this;
      this->timer_ = SetTimer(0, 0, 100, &OnTimer);
      if (!this->timer_) { this->fail("timer-failed"); return; }
      this->open_.emit();
    }
    void replacementIdle()
    {
      if (this->phase_ == BEGIN_REPLACE)
      {
        this->ticks_ = 0;
        this->log_.begin("replace");
        this->replaceBaseHeld_ = Ledger::held(*this->getPlatformContext());
        this->replacementSample("pre");
        this->openReplacement(true);
        return;
      }
      if (this->phase_ == DISMISS_A || this->phase_ == DISMISS_B)
      {
        HWND dialog = 0;
        EnumThreadWindows(GetCurrentThreadId(), &FindDialog, reinterpret_cast<LPARAM>(&dialog));
        if (dialog) return;
        this->phase_ = this->phase_ == DISMISS_A ? LOAD_A : REPLACING;
        this->ticks_ = 0;
      }
      if (this->phase_ == LOAD_A)
      {
        this->replacementSample("load-a");
        if (this->loadRefused()) return;
        const std::size_t held = Ledger::held(*this->getPlatformContext());
        if (this->loaded(true) && (held > this->replaceBaseHeld_ || this->replaceBaseHeld_ == 0))
        {
          // PR 1's inline HBITMAPs never enter the ledger. Dimensions also
          // positively identify the actual loaded image on that baseline.
          this->log_.note(held > this->replaceBaseHeld_
              ? "loaded=a evidence=dimensions-and-ledger" : "loaded=a evidence=dimensions ledger=unchanged");
          this->openReplacement(false);
        }
        else if (++this->ticks_ == 40) this->fail("load-a-timeout");
      }
      else if (this->phase_ == REPLACING)
      {
        this->replacementSample("replace");
        if (this->loadRefused()) return;
        if (++this->ticks_ == 10)
        {
          if (!this->loaded(false)) { this->fail("load-b-timeout"); return; }
          this->phase_ = REPLACE_SETTLE;
          this->ticks_ = 0;
        }
      }
      else if (this->phase_ == REPLACE_SETTLE)
      {
        this->replacementSample("settle");
        if (++this->ticks_ == 5)
        {
          this->log_.summary();
          this->phase_ = DONE;
          this->app_->quit();
        }
      }
    }
    void replacementTimer()
    {
      HWND dialog = 0;
      EnumThreadWindows(GetCurrentThreadId(), &FindDialog, reinterpret_cast<LPARAM>(&dialog));
      const DWORD now = GetTickCount();
      const bool first = this->phase_ == SEEK_A || this->phase_ == FILL_A;
      if (this->phase_ == SEEK_A || this->phase_ == SEEK_B)
      {
        if (!dialog)
        {
          if (now - this->since_ >= 5000) this->fail("dialog-not-found");
          return;
        }
        this->replacementSample(first ? "open-a" : "open-b");
        this->phase_ = first ? FILL_A : FILL_B;
        this->since_ = now;
      }
      if (!dialog) { this->fail("dialog-disappeared-early"); return; }
      FilenameBox box(dialog);
      EnumChildWindows(dialog, &FilenameBox::visit, reinterpret_cast<LPARAM>(&box));
      if (!box.edit)
      {
        if (now - this->since_ >= 2000) this->fail("filename-box-not-found");
        return;
      }
      char match[128];
      std::sprintf(match, "chooser=%s filename_box=%s owner_id=%d", first ? "a" : "b", box.match, box.ownerId);
      this->log_.note(match);
      if (!SendMessageW(box.edit, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(this->files_.path(first))))
      { this->fail("filename-set-failed"); return; }
      HWND ok = GetDlgItem(dialog, IDOK);
      if (!ok || !PostMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), reinterpret_cast<LPARAM>(ok)))
      { this->fail("accept-post-failed"); return; }
      this->phase_ = first ? DISMISS_A : DISMISS_B;
      this->disarm();
    }
    RetirementProbeLog log_;
    App *app_;
    simpleviewer::MainNode *main_;
    loka::core::EmitterState open_;
    Image image_;
    Phase phase_;
    UINT_PTR timer_;
    DWORD since_;
    int ticks_;
    const ReplacementFiles files_;
    std::size_t replaceBaseHeld_;
  };
  DialogProbe *DialogProbe::timerOwner_ = 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
  loka::platform::InitPlatformRuntime();
  loka::core::ScopedPtr<PlatformContext> context(loka::platform::CreatePlatformContext());
  if (!context.get()) return 1;
  DialogProbe config(context.get());
  if (!config.ready()) return 1;
  loka::core::ScopedPtr<App> app(context->createApp(&config, instance, show));
  if (!app.get()) return 1;
  config.setApp(app.get());
  app->run();
  return config.exitCode();
}
