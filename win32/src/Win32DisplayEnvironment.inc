// Shared with the native host fixture: real scale assignment and font-swap ordering.
namespace
{
  struct DisplayFontReplacement
  {
    const loka::win32::Win32DisplayFont &previous;
    const loka::win32::Win32DisplayFont &next;
  };

  BOOL CALLBACK ApplyDisplayFont(HWND hwnd, LPARAM fontValue)
  {
    const DisplayFontReplacement &fonts = *reinterpret_cast<const DisplayFontReplacement *>(fontValue);
    const HFONT previous = reinterpret_cast<HFONT>(SendMessageW(hwnd, WM_GETFONT, 0, 0));
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(fonts.previous.replacementFor(previous, fonts.next)), TRUE);
    return TRUE;
  }

} // namespace

void Win32ScenePlatformController::updateDisplayScale(const loka::win32::Win32DisplayScale &displayScale)
{
  this->displayScale_ = loka::win32::Win32DisplayScale(displayScale.dpi(), this->railMetrics_);
  this->textEnvironment_.changed();
  this->ensureDisplayFont();
}

void Win32ScenePlatformController::ensureDisplayFont()
{
  if (this->displayFont_.matches(this->displayScale_))
  {
    return;
  }
  loka::win32::Win32DisplayFont replacement;
  if (replacement.create(this->displayScale_))
  {
    this->textEnvironment_.changed();
    this->applyDisplayFontToNativeSubtree(replacement);
    this->displayFont_.swap(replacement);
  }
}

void Win32ScenePlatformController::applyDisplayFontToNativeSubtree(const loka::win32::Win32DisplayFont &replacement)
{
  if (!this->rootHwnd_ || !replacement.get())
  {
    return;
  }
  const DisplayFontReplacement fonts = {this->displayFont_, replacement};
  EnumChildWindows(this->rootHwnd_, &ApplyDisplayFont, reinterpret_cast<LPARAM>(&fonts));
}
