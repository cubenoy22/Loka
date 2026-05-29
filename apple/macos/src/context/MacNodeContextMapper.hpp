#ifndef LOKA_MAC_NODE_CONTEXT_MAPPER_HPP
#define LOKA_MAC_NODE_CONTEXT_MAPPER_HPP

namespace loka
{
  namespace app
  {
    class ButtonNode;
    class CellNode;
    class EditTextNode;
    class ImageViewNode;
    class OpenFileDialogNode;
    class PopupMenuNode;
    class TextNode;
  } // namespace app
} // namespace loka

class MacButtonContext;
class MacCellContext;
class MacEditTextContext;
class MacImageViewContext;
class MacOpenFileDialogContext;
class MacPopupMenuContext;
class MacTextContext;

class MacNodeContextMapper
{
public:
  explicit MacNodeContextMapper(void *rootView)
      : rootView_(rootView)
  {
  }

  MacButtonContext *ensureButtonContext(loka::app::ButtonNode *node, int x, int y, int width, int height) const;
  MacCellContext *ensureCellContext(loka::app::CellNode *node, int x, int y, int width, int height) const;
  MacEditTextContext *ensureEditTextContext(loka::app::EditTextNode *node, int x, int y, int width, int height) const;
  MacTextContext *ensureTextContext(loka::app::TextNode *node, int x, int y, int width, int height) const;
  MacImageViewContext *
  ensureImageViewContext(loka::app::ImageViewNode *node, int x, int y, int width, int height) const;
  MacOpenFileDialogContext *ensureOpenFileDialogContext(loka::app::OpenFileDialogNode *node) const;
  MacPopupMenuContext *
  ensurePopupMenuContext(loka::app::PopupMenuNode *node, int x, int y, int width, int height) const;

private:
  void *rootView_;
};

#endif // LOKA_MAC_NODE_CONTEXT_MAPPER_HPP
