#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <cwchar>

#include "RetirementProbeSampler.hpp"
#include "MainNode.hpp"
#include "ScenarioWindow.hpp"
#include "app/bootstrap/PlatformBootstrap.hpp"
#include "app/core/App.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "core/resource/Blob.hpp"
#include "core/resource/Image.hpp"
#include "core/util/ScopedPtr.hpp"

namespace
{
  using loka::core::resource::Blob;
  using loka::core::resource::Image;

  void Put32(unsigned char *bytes, unsigned long value)
  {
    for (int i = 0; i != 4; ++i) bytes[i] = static_cast<unsigned char>(value >> (8 * i));
  }

  // Original BMP fixture: the retirement pin's header shape, expanded to a
  // 1024x768 32bpp uncompressed image (no padding required).
  Blob BitmapBlob()
  {
    const unsigned long pixelBytes = 1024UL * 768UL * 4UL;
    Blob blob = Blob::Create();
    if (!blob.tryResize(54 + pixelBytes)) return Blob();
    unsigned char *bytes = blob.mutableData();
    std::memset(bytes, 0, 54 + pixelBytes);
    bytes[0] = 'B'; bytes[1] = 'M';
    Put32(bytes + 2, 54 + pixelBytes);
    Put32(bytes + 10, 54);
    Put32(bytes + 14, 40);
    Put32(bytes + 18, 1024);
    Put32(bytes + 22, 768);
    bytes[26] = 1; bytes[28] = 32;
    Put32(bytes + 34, pixelBytes);
    std::memset(bytes + 54, 0x80, pixelBytes);
    return blob;
  }

  /** App-lifetime owner of the timer and sole decoded Image. Timer callbacks
      borrow this owner on the UI thread, including the chooser's modal loop. */
  class DialogProbe : public AppConfigurable
  {
  public:
    explicit DialogProbe(PlatformContext *context)
        : AppConfigurable(context), log_(L"retirement-probe-dialog.log"), app_(0), main_(0),
          open_(), image_(), phase_(WAIT), timer_(0), since_(0), ticks_(0)
    {
      const Blob blob = BitmapBlob();
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
    enum Phase { WAIT, SEEK, DWELL, DROPPED, DISMISSING, SETTLE, DONE };
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
          this->phase_ = DONE;
          this->app_->quit();
        }
      }
    }
    void timerTick()
    {
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
    RetirementProbeLog log_;
    App *app_;
    simpleviewer::MainNode *main_;
    loka::core::EmitterState open_;
    Image image_;
    Phase phase_;
    UINT_PTR timer_;
    DWORD since_;
    int ticks_;
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
