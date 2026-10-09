#include <cstdio>
#include <cstring>
#include <Dialogs.h>
#include <Events.h>
#include <Fonts.h>
#include <Gestalt.h>
#include <Menus.h>
#include <Palettes.h>
#include <Quickdraw.h>
#include <TextEdit.h>
#include <Windows.h>

#include "core/io/File.hpp"
#include "platform/file/AppLocation.hpp"
#include "platform/file/FileIO.hpp"

namespace
{
  void Label(short x, short y, const char *text)
  {
    Str255 label;
    const std::size_t length = std::strlen(text);
    label[0] = static_cast<unsigned char>(length);
    std::memcpy(label + 1, text, length);
    MoveTo(x, y);
    DrawString(label);
  }

  void Swatch(short top, const char *label)
  {
    const Rect rect = {top, 10, static_cast<short>(top + 14), 30};
    PaintRect(&rect);
    Label(38, top + 11, label);
  }

  void PlanarRows()
  {
    const long colors[] = {blackColor, whiteColor, redColor, greenColor,
                           blueColor, cyanColor, magentaColor, yellowColor};
    const char *const names[] = {"blackColor", "whiteColor", "redColor", "greenColor",
                                 "blueColor", "cyanColor", "magentaColor", "yellowColor"};
    TextFont(3); // Geneva, compact enough for the RGB labels.
    TextSize(9);
    TextFace(normal);
    TextMode(srcOr);
    for (short row = 0; row < 8; ++row)
    {
      ForeColor(colors[row]);
      Swatch(10 + row * 20, names[row]);
    }
    ForeColor(blackColor);
  }

  void DrawBWWindow(WindowPtr window)
  {
    SetPort(window);
    ForeColor(blackColor);
    BackColor(whiteColor);
    const Rect content = {0, 0, 290, 240};
    EraseRect(&content);
    PlanarRows();

    const Rect back = {170, 10, 184, 110};
    BackColor(cyanColor);
    EraseRect(&back);
    Label(12, 181, "BackColor cyan");
    BackColor(whiteColor);

    const Rect light = {190, 10, 204, 30};
    FillRect(&light, &qd.ltGray);
    Label(38, 201, "qd.ltGray");
    const Rect gray = {210, 10, 224, 30};
    FillRect(&gray, &qd.gray);
    Label(38, 221, "qd.gray");
    ForeColor(blackColor);
  }

  void DrawColorWindow(WindowPtr window, bool colorQD)
  {
    // This guard also covers every RGB trap reached by update events.
    if (!colorQD || !window)
      return;
    SetPort(window);
    const RGBColor black = {0, 0, 0};
    const RGBColor white = {0xFFFF, 0xFFFF, 0xFFFF};
    const RGBColor red = {0xFFFF, 0, 0};
    const RGBColor gray = {0x8000, 0x8000, 0x8000};
    const RGBColor light = {0xEB85, 0xEB85, 0xEB85};
    RGBForeColor(&black);
    RGBBackColor(&white);
    const Rect content = {0, 0, 290, 240};
    EraseRect(&content);
    PlanarRows();
    RGBForeColor(&red);
    Swatch(170, "RGB red (FFFF,0,0)");
    RGBForeColor(&gray);
    Swatch(190, "RGB gray 0x8000");
    RGBForeColor(&light);
    Swatch(210, "RGB gray 0.92 (EB85)");

    const Rect back = {230, 10, 244, 110};
    RGBBackColor(&light);
    EraseRect(&back);
    RGBForeColor(&black);
    Label(12, 241, "Back 0.92");
    RGBBackColor(&white);
  }

  int RunProbe()
  {
    loka::platform::file::FileHandle file;
    if (!loka::platform::file::ResolveApplicationSidecar(
            loka::file::File::Application() << loka::file::File("LOG.TXT"), file))
      return 1;
    std::FILE *log = loka::platform::file::OpenWriteTruncate(file);
    if (!log)
      return 1;

    long version = 0;
    const OSErr gestaltError = Gestalt(gestaltQuickdrawVersion, &version);
    const bool colorQD = gestaltError == noErr && version >= gestalt8BitQD;
    std::fprintf(log, "qd_version=0x%04lX\rcolor_qd=%s\r", version, colorQD ? "yes" : "no");
    if (colorQD)
      std::fprintf(log, "depth=%d\r", (**(**GetMainDevice()).gdPMap).pixelSize);
    else
      std::fprintf(log, "depth=1 (no Color QuickDraw)\r");
    if (gestaltError != noErr)
      std::fprintf(log, "ERROR Gestalt=%d\r", gestaltError);
#if LOKA_COLOR_PROBE_DEPTH > 0
    // Optional second run: switch the main screen to a color depth first (the
    // device type bit asks for color rather than gray), so the same drawing
    // shows how each port renders on a color screen.
    if (colorQD)
    {
      GDHandle device = GetMainDevice();
      if (HasDepth(device, LOKA_COLOR_PROBE_DEPTH, 1 << gdDevType, 1 << gdDevType))
        std::fprintf(log, "set_depth=%d err=%d\r", LOKA_COLOR_PROBE_DEPTH,
                     SetDepth(device, LOKA_COLOR_PROBE_DEPTH, 1 << gdDevType, 1 << gdDevType));
      else
        std::fprintf(log, "set_depth=%d unsupported\r", LOKA_COLOR_PROBE_DEPTH);
      std::fprintf(log, "depth_now=%d\r", (**(**GetMainDevice()).gdPMap).pixelSize);
    }
#endif

    // A null storage argument lets each constructor allocate its own record type.
    const Rect left = {50, 10, 340, 250};
    WindowPtr bwWindow = NewWindow(0, &left, "\pB&W port", true, documentProc,
                                   reinterpret_cast<WindowPtr>(-1L), false, 0);
    WindowPtr colorWindow = 0;
    int result = gestaltError == noErr ? 0 : 1;
    if (bwWindow)
    {
      DrawBWWindow(bwWindow);
      std::fprintf(log, "bw_window=drawn\r");
    }
    else
    {
      std::fprintf(log, "ERROR NewWindow failed\r");
      result = 1;
    }
    if (colorQD)
    {
      const Rect right = {50, 260, 340, 500};
      colorWindow = NewCWindow(0, &right, "\pColor port", true, documentProc,
                               reinterpret_cast<WindowPtr>(-1L), false, 0);
      if (colorWindow)
      {
        DrawColorWindow(colorWindow, colorQD);
        std::fprintf(log, "color_window=drawn\r");
      }
      else
      {
        std::fprintf(log, "ERROR NewCWindow failed\r");
        result = 1;
      }
    }
    else
      std::fprintf(log, "color_window=skipped\r");
    std::fprintf(log, "done\r");
    if (std::ferror(log))
      result = 1;
    if (!loka::platform::file::FlushWrite(log, file))
      result = 1;
    if (std::fclose(log) != 0)
      result = 1;

    bool quit = !bwWindow && !colorWindow;
    while (!quit)
    {
      SystemTask();
      EventRecord event;
      if (!GetNextEvent(everyEvent, &event))
        continue;
      if (event.what == updateEvt)
      {
        WindowPtr window = reinterpret_cast<WindowPtr>(event.message);
        BeginUpdate(window);
        if (window == bwWindow)
          DrawBWWindow(window);
        else if (window == colorWindow)
          DrawColorWindow(window, colorQD);
        EndUpdate(window);
      }
      else if (event.what == mouseDown)
      {
        WindowPtr window = 0;
        if (FindWindow(event.where, &window) == inContent && (window == bwWindow || window == colorWindow))
          quit = true;
      }
      else if (event.what == keyDown && (event.modifiers & cmdKey))
      {
        const char key = static_cast<char>(event.message & charCodeMask);
        quit = key == 'q' || key == 'Q';
      }
    }
    if (colorWindow)
      DisposeWindow(colorWindow);
    if (bwWindow)
      DisposeWindow(bwWindow);
    return result;
  }
} // namespace

int main()
{
  InitGraf(&qd.thePort);
  InitFonts();
  InitWindows();
  InitMenus();
  TEInit();
  InitDialogs(0);
  InitCursor();
  return RunProbe();
}
