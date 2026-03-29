#ifndef LOKA_WIN32_NODE_CONTEXT_MAPPER_HPP
#define LOKA_WIN32_NODE_CONTEXT_MAPPER_HPP

#include <windows.h>

namespace loka
{
  namespace app
  {
    class ButtonNode;
    class EditTextNode;
    class ImageViewNode;
    class PopupMenuNode;
    class TextNode;
  }
}

class Win32ButtonContext;
class Win32EditTextContext;
class Win32ImageViewContext;
class Win32PopupMenuContext;
class Win32TextContext;

class Win32NodeContextMapper
{
public:
  explicit Win32NodeContextMapper(HWND rootHwnd)
      : rootHwnd_(rootHwnd) {}

  Win32ButtonContext *ensureButtonContext(loka::app::ButtonNode *node,
                                          int x,
                                          int y,
                                          int width,
                                          int height) const;
  Win32EditTextContext *ensureEditTextContext(loka::app::EditTextNode *node,
                                              int x,
                                              int y,
                                              int width,
                                              int height) const;
  Win32TextContext *ensureTextContext(loka::app::TextNode *node,
                                      int x,
                                      int y,
                                      int width,
                                      int height) const;
  Win32ImageViewContext *ensureImageViewContext(loka::app::ImageViewNode *node,
                                                int x,
                                                int y,
                                                int width,
                                                int height) const;
  Win32PopupMenuContext *ensurePopupMenuContext(loka::app::PopupMenuNode *node,
                                                int x,
                                                int y,
                                                int width,
                                                int height) const;

private:
  HWND rootHwnd_;
};

#endif // LOKA_WIN32_NODE_CONTEXT_MAPPER_HPP
