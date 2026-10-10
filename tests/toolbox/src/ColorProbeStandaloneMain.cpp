#include <cstdio>
#include <cstring>
#include <Appearance.h>
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

  // Appearance 1.0 (Mac OS 8.0/8.1, the last 68k systems) has no getter for a
  // theme color: GetThemeBrushAsColor / GetThemeTextColor arrived in 1.1 and
  // are PPC-only. The 1.0 setters apply the theme to the current port, so the
  // probe applies each one to its own color port and reads the port back.
  void LogThemeColors(std::FILE *log, WindowPtr window, short depth)
  {
    GrafPtr saved;
    GetPort(&saved);
    SetPort(window);
    RGBColor foreground;
    RGBColor background;
    GetForeColor(&foreground);
    GetBackColor(&background);
    const struct BrushSample
    {
      ThemeBrush brush;
      const char *name;
    } brushes[] = {
      {kThemeBrushDialogBackgroundActive, "kThemeBrushDialogBackgroundActive"},
      {kThemeBrushDocumentWindowBackground, "kThemeBrushDocumentWindowBackground"},
      {kThemeBrushModelessDialogBackgroundActive, "kThemeBrushModelessDialogBackgroundActive"},
      {kThemeBrushUtilityWindowBackgroundActive, "kThemeBrushUtilityWindowBackgroundActive"},
      {kThemeBrushListViewBackground, "kThemeBrushListViewBackground"}
    };
    for (std::size_t i = 0; i < sizeof(brushes) / sizeof(brushes[0]); ++i)
    {
      RGBBackColor(&background);
      const OSStatus error = SetThemeBackground(brushes[i].brush, depth, true);
      RGBColor rgb;
      GetBackColor(&rgb);
      std::fprintf(log, "brush %s set_err=%ld port_back=%04X,%04X,%04X\r", brushes[i].name,
                   static_cast<long>(error), static_cast<unsigned int>(rgb.red),
                   static_cast<unsigned int>(rgb.green), static_cast<unsigned int>(rgb.blue));
    }
    const struct TextSample
    {
      ThemeTextColor color;
      const char *name;
    } texts[] = {
      {kThemeTextColorDialogActive, "kThemeTextColorDialogActive"},
      {kThemeTextColorDialogInactive, "kThemeTextColorDialogInactive"},
      {kThemeTextColorWindowHeaderActive, "kThemeTextColorWindowHeaderActive"},
      {kThemeTextColorPushButtonActive, "kThemeTextColorPushButtonActive"},
      {kThemeTextColorListView, "kThemeTextColorListView"},
      {kThemeTextColorDocumentWindowTitleActive, "kThemeTextColorDocumentWindowTitleActive"}
    };
    for (std::size_t i = 0; i < sizeof(texts) / sizeof(texts[0]); ++i)
    {
      RGBForeColor(&foreground);
      const OSStatus error = SetThemeTextColor(texts[i].color, depth, true);
      RGBColor rgb;
      GetForeColor(&rgb);
      std::fprintf(log, "text %s set_err=%ld port_fore=%04X,%04X,%04X\r", texts[i].name,
                   static_cast<long>(error), static_cast<unsigned int>(rgb.red),
                   static_cast<unsigned int>(rgb.green), static_cast<unsigned int>(rgb.blue));
    }
    RGBForeColor(&foreground);
    RGBBackColor(&background);
    SetPort(saved);
  }

  void DrawThemeWindow(WindowPtr window, bool colorQD, bool appearance)
  {
    // Update events must obey both trap-availability guards too.
    if (!colorQD || !appearance || !window)
      return;
    SetPort(window);
    RGBColor foreground;
    RGBColor background;
    GetForeColor(&foreground);
    GetBackColor(&background);
    const short depth = (**(**GetMainDevice()).gdPMap).pixelSize;
    SetThemeWindowBackground(window, kThemeBrushDialogBackgroundActive, true);
    EraseRect(&window->portRect);
    TextFont(3);
    TextSize(9);
    TextFace(normal);
    TextMode(srcOr);
    SetThemeTextColor(kThemeTextColorDialogActive, depth, true);
    Label(10, 25, "kThemeTextColorDialogActive");
    SetThemeTextColor(kThemeTextColorListView, depth, true);
    Label(10, 50, "kThemeTextColorListView");
    RGBForeColor(&foreground);
    RGBBackColor(&background);
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
    long appearanceAttr = 0;
    const OSErr appearanceError = Gestalt(gestaltAppearanceAttr, &appearanceAttr);
    const bool appearance = appearanceError == noErr &&
                            (appearanceAttr & (1L << gestaltAppearanceExists)) != 0;
    std::fprintf(log, "appearance=%s\r", appearance ? "yes" : "no");
    if (appearance)
    {
      long appearanceVersion = 0;
      if (Gestalt(gestaltAppearanceVersion, &appearanceVersion) == noErr)
        std::fprintf(log, "appearance_version=0x%04lX\r", appearanceVersion);
      else
        std::fprintf(log, "appearance_version=absent\r");
    }
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

    if (appearance)
    {
      std::fprintf(log, "RegisterAppearanceClient err=%ld\r",
                   static_cast<long>(RegisterAppearanceClient()));
    }

    // A null storage argument lets each constructor allocate its own record type.
    const Rect left = {50, 10, 340, 250};
    WindowPtr bwWindow = NewWindow(0, &left, "\pB&W port", true, documentProc,
                                   reinterpret_cast<WindowPtr>(-1L), false, 0);
    WindowPtr colorWindow = 0;
    WindowPtr themeWindow = 0;
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
    if (appearance && colorQD)
    {
      // Leave room for the title bar below the existing ports, within 640x480.
      const Rect bottom = {365, 10, 465, 500};
      themeWindow = NewCWindow(0, &bottom, "\pTheme port", true, documentProc,
                               reinterpret_cast<WindowPtr>(-1L), false, 0);
      if (themeWindow)
      {
        LogThemeColors(log, themeWindow, (**(**GetMainDevice()).gdPMap).pixelSize);
        DrawThemeWindow(themeWindow, colorQD, appearance);
        std::fprintf(log, "theme_window=drawn\r");
      }
      else
      {
        std::fprintf(log, "ERROR NewCWindow Theme port failed\r");
        result = 1;
      }
    }
    else
      std::fprintf(log, "theme_window=skipped\r");
    std::fprintf(log, "done\r");
    if (std::ferror(log))
      result = 1;
    if (!loka::platform::file::FlushWrite(log, file))
      result = 1;
    if (std::fclose(log) != 0)
      result = 1;

    bool quit = !bwWindow && !colorWindow && !themeWindow;
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
        else if (window == themeWindow)
          DrawThemeWindow(window, colorQD, appearance);
        EndUpdate(window);
      }
      else if (event.what == mouseDown)
      {
        WindowPtr window = 0;
        if (FindWindow(event.where, &window) == inContent &&
            (window == bwWindow || window == colorWindow || window == themeWindow))
          quit = true;
      }
      else if (event.what == keyDown && (event.modifiers & cmdKey))
      {
        const char key = static_cast<char>(event.message & charCodeMask);
        quit = key == 'q' || key == 'Q';
      }
    }
    if (themeWindow)
      DisposeWindow(themeWindow);
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
