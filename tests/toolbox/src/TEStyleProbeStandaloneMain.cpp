#include <cstdio>
#include <string>
#include <Processes.h>
#include <TextEdit.h>

#include "StandaloneFlowRunner.hpp"
#include "ToolboxProbeTiming.hpp"
#include "ToolboxWindow.hpp"
#include "app/core/AppComposition.hpp"
#include "app/core/AppConfigurable.hpp"
#include "core/String.hpp"
#include "core/io/File.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileIO.hpp"

#if !defined(LOKA_RETRO68)
#error TextEdit style probe requires a Toolbox build
#endif

namespace
{
  using namespace loka::app;
  using loka::core::String;
  using loka_toolbox_probe::Sample;
  using loka_toolbox_probe::Timer;
  const int kLines = 200;
  const int kLineChars = 40;
  const int kLineStride = kLineChars + 1;
  const int kEditLine = 100; // Zero-based logical line, independent of TE wrapping.
  const int kIterations = 20;
  const int kCancels = 10;
  const int kRuns = 10;

  /** Owns one TE and restores the borrowed port, including on early refusal. */
  class ProbeText
  {
  public:
    ProbeText(WindowPtr window, const Rect &view, bool styled)
        : te_(0)
    {
      GetPort(&this->previousPort_);
      SetPort(window);
      TextFont(4); // Monaco; fixed metrics keep the 40-column ASCII fixture legible.
      TextSize(12);
      this->te_ = styled ? TEStyleNew(&view, &view) : TENew(&view, &view);
      if (this->te_)
        TEAutoView(true, this->te_);
    }
    ~ProbeText()
    {
      this->dispose();
      SetPort(this->previousPort_);
    }
    TEHandle get() const
    {
      return this->te_;
    }
    void dispose()
    {
      if (this->te_)
        TEDispose(this->te_);
      this->te_ = 0;
    }

  private:
    ProbeText(const ProbeText &);
    ProbeText &operator=(const ProbeText &);
    GrafPtr previousPort_;
    TEHandle te_;
  };

  std::string BuildDocument()
  {
    std::string document;
    document.reserve(kLines * kLineStride);
    for (int line = 0; line < kLines; ++line)
    {
      char text[kLineChars + 1];
      std::sprintf(text, "var v%03d = %03d + foo * 02; //ok", line, line);
      std::string padded(text);
      padded.resize(kLineChars, ' ');
      document += padded;
      document += '\r';
    }
    return document;
  }

  // Same ten token styles as AttributedStringProbe::BuildLine. Padding belongs
  // to the final comment token; CR keeps the default style and is not a run.
  void StyleLine(TEHandle te, int line)
  {
    const short lengths[kRuns] = {4, 4, 3, 3, 3, 3, 3, 2, 2, 13};
    const unsigned char faces[kRuns] = {bold, italic, normal, normal, normal, italic, normal, normal, normal, bold};
    long offset = line * kLineStride;
    for (int run = 0; run < kRuns; ++run)
    {
      ::TextStyle style = {};
      style.tsFace = faces[run];
      style.tsSize = 12;
      TESetSelect(offset, offset + lengths[run], te);
      // Both interface sets require an explicit redraw argument. Defer drawing
      // until the caller's single TEUpdate, but retain TE's layout work.
      TESetStyle(doFace | doSize, &style, false, te);
      offset += lengths[run];
    }
  }

  void StyleAll(TEHandle te, const Rect &view)
  {
    for (int line = 0; line < kLines; ++line)
      StyleLine(te, line);
    TEUpdate(&view, te);
  }

  ::TextStyle StyleAt(TEHandle te, short offset)
  {
    ::TextStyle style = {};
    short height = 0;
    short ascent = 0;
    TEGetStyle(offset, &style, &height, &ascent, te);
    return style;
  }

  bool SameStyle(const ::TextStyle &a, const ::TextStyle &b)
  {
    return a.tsFont == b.tsFont && a.tsFace == b.tsFace && a.tsSize == b.tsSize && a.tsColor.red == b.tsColor.red
           && a.tsColor.green == b.tsColor.green && a.tsColor.blue == b.tsColor.blue;
  }

  /** Measures only local fixture rows; no scene or framework state is mutated. */
  bool MeasureText(std::FILE *log, TEHandle te, const Rect &view, bool styled)
  {
    const std::string document = BuildDocument();
    const Timer setText;
    TESetText(document.data(), static_cast<long>(document.size()), te);
    const Sample p1(setText);
    p1.write(log, styled ? "P1-settext" : "P1-settext-plain");
    std::fprintf(
        log, " us_settext=%lu lines=%d bytes=%lu\r", p1.us, kLines, static_cast<unsigned long>(document.size()));
    if ((**te).teLength != static_cast<long>(document.size()))
      return false;
    TEUpdate(&view, te); // Visible native layout/drawing before P2 starts.

    if (styled)
    {
      const Timer all;
      StyleAll(te, view);
      const Sample p2(all);
      p2.write(log, "P2-style-all");
      std::fprintf(log, " us_total=%lu us_per_line=%lu runs=%d\r", p2.us, p2.us / kLines, kLines * kRuns);
      // Positive control: a plain style at the later sentinel would make the
      // before/after preservation query meaningless (e.g. failed style allocation).
      if (StyleAt(te, kEditLine * kLineStride + 4).tsFace != italic)
        return false;
    }

    // Inside the plain " * " token, not its italic/plain boundary. The P5
    // italic sentinel differs from both this insertion style and the first run.
    const long caret = kEditLine * kLineStride + kLineChars / 2 + 1;
    TESetSelect(caret, caret, te);
    TEActivate(te);
    unsigned long fullUs = 0;
    unsigned long sliceUs = 0;
    unsigned long bytesRead = 0;
    const Timer keys;
    for (int iteration = 0; iteration < kIterations; ++iteration)
    {
      const Timer fullRead;
      TEKey('x', te);
      // Deliberate copy of updateStateFromEdit's read-back shape: TEGetText,
      // lock, std::string of ALL teLength bytes, unlock, then core::String.
      CharsHandle text = TEGetText(te);
      const long length = (**te).teLength; // Signed 16-bit in both TERec headers.
      if (!text || length != static_cast<long>(document.size()) + iteration + 1)
        return false;
      std::string copy;
      HLock(reinterpret_cast<Handle>(text));
      copy.assign(reinterpret_cast<const char *>(*text), static_cast<std::size_t>(length));
      HUnlock(reinterpret_cast<Handle>(text));
      const String full(copy);
      fullUs += fullRead.elapsed();
      bytesRead += length;

      const Timer slice;
      std::size_t begin = 0;
      for (int line = 0; line < kEditLine; ++line)
      {
        const std::size_t cr = copy.find('\r', begin);
        if (cr == std::string::npos)
          return false;
        begin = cr + 1;
      }
      const std::size_t end = copy.find('\r', begin);
      if (end == std::string::npos)
        return false;
      const String oneLine(copy.substr(begin, end - begin));
      sliceUs += slice.elapsed();
      if (full.empty() || oneLine.empty() || end - begin != static_cast<std::size_t>(kLineChars + iteration + 1))
        return false;
    }
    const Sample p3(keys);
    p3.write(log, styled ? "P3-keystroke" : "P3-keystroke-plain");
    std::fprintf(log,
                 " us_per_keystroke_fullread=%lu us_per_keystroke_slice=%lu bytes_read=%lu iterations=%d\r",
                 fullUs / kIterations,
                 sliceUs / kIterations,
                 bytesRead,
                 kIterations);
    TESetSelect(caret, caret + kIterations, te);
    TEDelete(te);
    if ((**te).teLength != static_cast<long>(document.size()))
      return false;

    if (styled)
    {
      const Timer restyle;
      for (int iteration = 0; iteration < kIterations; ++iteration)
      {
        ::TextStyle plain = {};
        plain.tsFace = normal;
        plain.tsSize = 12;
        TESetSelect(kEditLine * kLineStride, kEditLine * kLineStride + kLineChars, te);
        TESetStyle(doFace | doSize, &plain, false, te);
        StyleLine(te, kEditLine);
        TEUpdate(&view, te);
      }
      const Sample p4(restyle);
      p4.write(log, "P4-restyle-line");
      std::fprintf(log, " us_per_line_restyle=%lu iterations=%d\r", p4.us / kIterations, kIterations);
    }

    TESetSelect(caret, caret, te);
    unsigned long cancelSetUs = 0;
    unsigned long cancelStyleUs = 0;
    bool stylesSurvive = true;
    const Timer cancel;
    for (int iteration = 0; iteration < kCancels; ++iteration)
    {
      const short start = (**te).selStart;
      const short end = (**te).selEnd;
      // Offset 4 is italic, unlike the document's first (bold) run; checking
      // the first run alone could mistake collapsed styles for preservation.
      const short sentinel = kEditLine * kLineStride + 4;
      ::TextStyle before = {};
      if (styled)
        before = StyleAt(te, sentinel);
      if (styled && before.tsFace != italic)
        return false;
      const Timer replace;
      TESetText(document.data(), static_cast<long>(document.size()), te);
      TESetSelect(start, end, te);
      cancelSetUs += replace.elapsed();
      if ((**te).teLength != static_cast<long>(document.size()))
        return false;
      if (styled)
      {
        const bool survived = SameStyle(before, StyleAt(te, sentinel));
        stylesSurvive = stylesSurvive && survived;
        if (!survived)
        {
          const Timer restore;
          StyleAll(te, view);
          // Styling changes the selection; restore it after projection as well.
          TESetSelect(start, end, te);
          cancelStyleUs += restore.elapsed();
        }
      }
    }
    const Sample p5(cancel);
    p5.write(log, styled ? "P5-cancel" : "P5-cancel-plain");
    if (styled)
    {
      std::fprintf(log,
                   " us_per_cancel_settext=%lu us_per_cancel_restyle=%lu styles_survive_settext=%d iterations=%d\r",
                   cancelSetUs / kCancels,
                   cancelStyleUs / kCancels,
                   stylesSurvive ? 1 : 0,
                   kCancels);
    }
    else
      std::fprintf(log, " us_per_cancel_settext=%lu\r", cancelSetUs / kCancels);
    TEDeactivate(te);
    return true;
  }

  /** Mirrors the attributed-string probe's app-owned log and one idle pass. */
  class ProbeConfig : public AppConfigurable
  {
  public:
    explicit ProbeConfig(PlatformContext *context)
        : AppConfigurable(context),
          app_(0),
          log_(0),
          result_(0)
    {
      if (loka::platform::file::ResolveApplicationSidecar(
              loka::file::File::Application() << loka::file::File("LOG.TXT"), this->file_))
        this->log_ = loka::platform::file::OpenWriteTruncate(this->file_);
      if (this->log_)
        std::setvbuf(this->log_, 0, _IONBF, 0);
      else
        this->result_ = 1;
    }
    virtual ~ProbeConfig()
    {
      if (this->log_)
        std::fclose(this->log_);
    }
    void setApp(App *app)
    {
      this->app_ = app;
    }
    int exitCode() const
    {
      return this->result_;
    }
    virtual void compose(AppComposition &composition)
    {
      composition << WindowDefinition<WindowProps>(WindowProps()
                                                       .title("TextEdit style probe")
                                                       .size(500, 300)
                                                       .visible(true)
                                                       .idlePolicy(IdlePolicy::everyTick())
                                                       .onIdle(&ProbeConfig::OnIdle, this));
    }

  private:
    App *app_;
    loka::platform::file::FileHandle file_;
    std::FILE *log_;
    int result_;

    bool measure(Window *window, bool styled)
    {
      const Sample p0((Timer()));
      p0.write(this->log_, styled ? "P0-baseline" : "P0-baseline-plain");
      std::fprintf(this->log_, "\r");
      ToolboxWindow *native = static_cast<ToolboxWindow *>(window);
      if (!native || !native->window())
        return false;
      const Rect view = {10, 10, 280, 490};
      ProbeText text(native->window(), view, styled);
      if (!text.get())
        return false;
      const bool measured = MeasureText(this->log_, text.get(), view, styled);
      const Timer teardown;
      text.dispose();
      const Sample p6(teardown);
      p6.write(this->log_, styled ? "P6-teardown" : "P6-teardown-plain");
      std::fprintf(this->log_, " delta_from_P0=%ld\r", p0.freeBytes - p6.freeBytes);
      return measured;
    }

    static void OnIdle(Window *window, double, void *data)
    {
      ProbeConfig *self = static_cast<ProbeConfig *>(data);
      if (!self->measure(window, true) || !self->measure(window, false))
      {
        self->result_ = 1;
        std::fprintf(self->log_, "ERROR TextEdit probe failed\r");
      }
      std::fprintf(self->log_, "done\r");
      if (std::ferror(self->log_) || !loka::platform::file::FlushWrite(self->log_, self->file_))
        self->result_ = 1;
      self->app_->quit();
    }
  };
} // namespace

int main(int, char **)
{
  const int result = loka::standalone_tests::RunStandaloneFlowWithConfig<ProbeConfig>(0, 0);
  ExitToShell();
  return result;
}
