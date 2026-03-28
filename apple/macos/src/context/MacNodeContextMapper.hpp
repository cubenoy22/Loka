#ifndef LOKA_MAC_NODE_CONTEXT_MAPPER_HPP
#define LOKA_MAC_NODE_CONTEXT_MAPPER_HPP

namespace loka
{
  namespace app
  {
    class ButtonNode;
    class ImageViewNode;
    class TextNode;
  }
}

class MacButtonContext;
class MacImageViewContext;
class MacTextContext;

class MacNodeContextMapper
{
public:
  explicit MacNodeContextMapper(void *rootView)
      : rootView_(rootView) {}

  MacButtonContext *ensureButtonContext(loka::app::ButtonNode *node,
                                        int x,
                                        int y,
                                        int width,
                                        int height) const;
  MacTextContext *ensureTextContext(loka::app::TextNode *node,
                                    int x,
                                    int y,
                                    int width,
                                    int height) const;
  MacImageViewContext *ensureImageViewContext(loka::app::ImageViewNode *node,
                                              int x,
                                              int y,
                                              int width,
                                              int height) const;

private:
  void *rootView_;
};

#endif // LOKA_MAC_NODE_CONTEXT_MAPPER_HPP
