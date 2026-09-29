#ifndef LOKA_TOOLBOX_PAINT_SUPPRESS_SCOPE_HPP
#define LOKA_TOOLBOX_PAINT_SUPPRESS_SCOPE_HPP

#include <Quickdraw.h>

/** Temporarily empties the current port's clip using borrowed scratch storage.
    The caller keeps the port unchanged and uses the scratch region exclusively
    for this scope. A refused scratch allocation preserves ordinary painting. */
class ToolboxPaintSuppressScope
{
public:
  explicit ToolboxPaintSuppressScope(RgnHandle scratch)
      : savedClip_(scratch)
  {
    if (this->savedClip_)
    {
      GetClip(this->savedClip_);
      const Rect empty = {0, 0, 0, 0};
      ClipRect(&empty);
    }
  }

  ~ToolboxPaintSuppressScope()
  {
    if (this->savedClip_)
      SetClip(this->savedClip_);
  }

private:
  ToolboxPaintSuppressScope(const ToolboxPaintSuppressScope &);
  ToolboxPaintSuppressScope &operator=(const ToolboxPaintSuppressScope &);
  RgnHandle const savedClip_;
};

#endif // LOKA_TOOLBOX_PAINT_SUPPRESS_SCOPE_HPP
