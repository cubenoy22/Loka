#ifndef LOKA_TEST_WIN32_TEXT_HOST_HPP
#define LOKA_TEST_WIN32_TEXT_HOST_HPP
// Replace only the platform neighbors; compile the actual production table.
#define LOKA_WIN32_SCENE_PLATFORM_CONTROLLER_HPP
#include "windows.h"
#include "app/style/Style.hpp"
class Win32ScenePlatformController
{
public:
  Win32ScenePlatformController()
      : small_(),
        large_()
  {
    this->small_.ascent = 10;
    this->small_.descent = 2;
    this->small_.leading = 1;
    this->small_.advance = 4;
    this->large_.ascent = 20;
    this->large_.descent = 5;
    this->large_.leading = 2;
    this->large_.advance = 8;
  }
  HFONT textFont(const loka::app::TextStyle &style) const
  {
    return style.hasItalic_ && style.italic_ ? &this->large_ : &this->small_;
  }

private:
  mutable HostFont small_, large_;
};
#endif
