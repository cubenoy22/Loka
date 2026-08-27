#include "MacMenuProjection.hpp"

#include <AppKit/AppKit.h>
#include <cassert>

MacMenuProjection::MacMenuProjection(void *ownedTarget)
    : target_(ownedTarget),
      menu_(0)
{
  assert(this->target_);
}

MacMenuProjection::~MacMenuProjection()
{
  this->reset();
  [(id)this->target_ release];
  this->target_ = 0;
}

void *MacMenuProjection::target() const
{
  return this->target_;
}

void MacMenuProjection::install(void *menu)
{
  assert(menu);
  this->reset();
  this->menu_ = menu;
  [(id)this->menu_ retain];
  [NSApp setMainMenu:(NSMenu *)this->menu_];
}

void MacMenuProjection::reset()
{
  if (!this->menu_)
  {
    return;
  }
  if ([NSApp mainMenu] == (NSMenu *)this->menu_)
  {
    [NSApp setMainMenu:nil];
  }
  [(id)this->menu_ release];
  this->menu_ = 0;
}
