#ifndef LOKA_WIN32_DISPLAY_FONT_HPP
#define LOKA_WIN32_DISPLAY_FONT_HPP

#include <windows.h>
#include "Win32DisplayScale.hpp"

namespace loka
{
  namespace win32
  {
    /** Owns the message font selected for one Win32 display scale. */
    class Win32DisplayFont
    {
    public:
      Win32DisplayFont();
      ~Win32DisplayFont();

      bool create(const Win32DisplayScale &scale);
      void swap(Win32DisplayFont &other);
      HFONT get() const
      {
        return this->font_;
      }

    private:
      HFONT font_;

      Win32DisplayFont(const Win32DisplayFont &);
      Win32DisplayFont &operator=(const Win32DisplayFont &);
    };
  } // namespace win32
} // namespace loka

#endif // LOKA_WIN32_DISPLAY_FONT_HPP
