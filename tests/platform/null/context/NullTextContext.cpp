#include "platform/null/context/NullTextContext.hpp"

#include <climits>

#include "app/nodes/Text.hpp"
#include "app/scene/projection/PlatformNodeHandler.hpp"
#include "core/StringBuffer.hpp"

namespace
{
  const int kFixedAdvance = 4;
  const int kDefaultLineHeight = 10;

  struct LineGeometry
  {
    LineGeometry()
        : lineCount(1),
          maxColumns(0)
    {
    }

    int lineCount;
    int maxColumns;
  };

  bool IsLineBreak(unsigned int value)
  {
    return value == '\n' || value == '\r';
  }

  bool IsWordSpace(unsigned int value)
  {
    return value == ' ' || value == '\t';
  }

  void FinishLine(int columns, LineGeometry &geometry)
  {
    if (columns > geometry.maxColumns)
    {
      geometry.maxColumns = columns;
    }
  }

  void SkipLineFeedAfterCarriageReturn(const loka::core::StringBuffer &text, std::size_t &index)
  {
    if (text.characterAt(index) == '\r' && index + 1 < text.length() && text.characterAt(index + 1) == '\n')
    {
      ++index;
    }
  }

  LineGeometry MeasureUnwrapped(const loka::core::StringBuffer &text)
  {
    LineGeometry geometry;
    int columns = 0;
    for (std::size_t i = 0; i < text.length(); ++i)
    {
      const unsigned int value = text.characterAt(i);
      if (IsLineBreak(value))
      {
        FinishLine(columns, geometry);
        ++geometry.lineCount;
        columns = 0;
        SkipLineFeedAfterCarriageReturn(text, i);
      }
      else
      {
        ++columns;
      }
    }
    FinishLine(columns, geometry);
    return geometry;
  }

  LineGeometry MeasureCharacterWrapped(const loka::core::StringBuffer &text, int capacity)
  {
    if (capacity <= 0)
    {
      return MeasureUnwrapped(text);
    }

    LineGeometry geometry;
    int columns = 0;
    for (std::size_t i = 0; i < text.length(); ++i)
    {
      const unsigned int value = text.characterAt(i);
      if (IsLineBreak(value))
      {
        FinishLine(columns, geometry);
        ++geometry.lineCount;
        columns = 0;
        SkipLineFeedAfterCarriageReturn(text, i);
        continue;
      }
      if (columns == capacity)
      {
        FinishLine(columns, geometry);
        ++geometry.lineCount;
        columns = 0;
      }
      ++columns;
    }
    FinishLine(columns, geometry);
    return geometry;
  }

  void PlaceWord(int wordLength, int pendingSpaces, int capacity, int &columns, LineGeometry &geometry)
  {
    if (columns > 0 && columns + pendingSpaces + wordLength <= capacity)
    {
      columns += pendingSpaces + wordLength;
      return;
    }
    if (columns > 0)
    {
      FinishLine(columns, geometry);
      ++geometry.lineCount;
      columns = 0;
    }
    while (wordLength > capacity)
    {
      FinishLine(capacity, geometry);
      ++geometry.lineCount;
      wordLength -= capacity;
    }
    columns = wordLength;
  }

  LineGeometry MeasureWordWrapped(const loka::core::StringBuffer &text, int capacity)
  {
    if (capacity <= 0)
    {
      return MeasureUnwrapped(text);
    }

    LineGeometry geometry;
    int columns = 0;
    int pendingSpaces = 0;
    std::size_t index = 0;
    while (index < text.length())
    {
      const unsigned int value = text.characterAt(index);
      if (IsLineBreak(value))
      {
        FinishLine(columns, geometry);
        ++geometry.lineCount;
        columns = 0;
        pendingSpaces = 0;
        SkipLineFeedAfterCarriageReturn(text, index);
        ++index;
        continue;
      }
      if (IsWordSpace(value))
      {
        if (columns > 0)
        {
          ++pendingSpaces;
        }
        ++index;
        continue;
      }

      int wordLength = 0;
      while (index < text.length())
      {
        const unsigned int wordValue = text.characterAt(index);
        if (IsLineBreak(wordValue) || IsWordSpace(wordValue))
        {
          break;
        }
        ++wordLength;
        ++index;
      }
      PlaceWord(wordLength, pendingSpaces, capacity, columns, geometry);
      pendingSpaces = 0;
    }
    FinishLine(columns, geometry);
    return geometry;
  }

  short ClampToShort(int value)
  {
    if (value <= 0)
    {
      return 0;
    }
    if (value > SHRT_MAX)
    {
      return SHRT_MAX;
    }
    return static_cast<short>(value);
  }

  loka::app::TextWrap ResolveWrap(const loka::app::TextNode *node)
  {
    if (!node || !node->props.hasAttr_ || !node->props.attr_.hasWrapValue_)
    {
      return loka::app::TEXT_WRAP_NONE;
    }
    return node->props.attr_.wrapValue_;
  }

  loka::app::TextTruncation ResolveTruncation(const loka::app::TextNode *node)
  {
    if (!node || !node->props.hasAttr_ || !node->props.attr_.hasTruncationValue_)
    {
      return loka::app::TEXT_TRUNCATION_NONE;
    }
    return node->props.attr_.truncationValue_;
  }

  NullTextMeasurement MeasureText(const loka::app::TextNode *node, const loka::app::scene::LayoutState &state)
  {
    const int lineHeight = state.lineHeight > 0 ? state.lineHeight : kDefaultLineHeight;
    if (!node || !node->props.text_)
    {
      return NullTextMeasurement(0, ClampToShort(lineHeight), 1);
    }

    const loka::core::StringBuffer text = node->props.text_->get().bufferWithEncoding(loka::core::StringEncodingUtf32);
    const int capacity = state.width > 0 ? state.width / kFixedAdvance : 0;
    const loka::app::TextWrap wrap = ResolveWrap(node);
    LineGeometry lines;
    switch (wrap)
    {
    case loka::app::TEXT_WRAP_NONE:
      lines = MeasureUnwrapped(text);
      break;
    case loka::app::TEXT_WRAP_WORD:
      lines = MeasureWordWrapped(text, capacity);
      break;
    case loka::app::TEXT_WRAP_CHAR:
      lines = MeasureCharacterWrapped(text, capacity);
      break;
    }

    int measuredWidth = lines.maxColumns * kFixedAdvance;
    if (wrap == loka::app::TEXT_WRAP_NONE && state.width > 0 && measuredWidth > state.width)
    {
      const loka::app::TextTruncation truncation = ResolveTruncation(node);
      if (truncation == loka::app::TEXT_TRUNCATION_CLIP)
      {
        measuredWidth = state.width;
      }
      else if (truncation == loka::app::TEXT_TRUNCATION_ELLIPSIS)
      {
        measuredWidth = capacity * kFixedAdvance;
      }
    }
    const int measuredHeight = lines.lineCount * lineHeight;
    return NullTextMeasurement(
        ClampToShort(measuredWidth), ClampToShort(measuredHeight), ClampToShort(lines.lineCount));
  }

  class NullTextNodeHandler : public loka::app::scene::IPlatformNodeHandler
  {
  public:
    virtual const void *nodeTypeKey() const
    {
      return loka::app::scene::NodeTypeToken<loka::app::TextNode>();
    }

    virtual loka::app::scene::NodeContext *ensureContext(loka::app::scene::Node *node,
                                                         loka::app::scene::IPlatformController *controller,
                                                         const loka::app::scene::LayoutState &state)
    {
      (void)state;
      loka::app::TextNode *text = node ? node->asTextNode() : 0;
      NullScenePlatformController *nullPlatform = static_cast<NullScenePlatformController *>(controller);
      if (!text || !nullPlatform)
      {
        return 0;
      }
      NullTextContext *context = static_cast<NullTextContext *>(text->getContext());
      if (!context)
      {
        context = new NullTextContext(text);
        if (!context)
        {
          return 0;
        }
        text->setContext(context);
      }
      return context;
    }
  };

  NullTextNodeHandler gNullTextNodeHandler;
} // namespace

NullTextMeasurement::NullTextMeasurement()
    : width_(0),
      height_(0),
      lineCount_(0)
{
}

NullTextMeasurement::NullTextMeasurement(short width, short height, short lineCount)
    : width_(width),
      height_(height),
      lineCount_(lineCount)
{
}

short NullTextMeasurement::width() const
{
  return this->width_;
}

short NullTextMeasurement::height() const
{
  return this->height_;
}

short NullTextMeasurement::lineCount() const
{
  return this->lineCount_;
}

NullTextContext::NullTextContext(loka::app::TextNode *node)
    : loka::app::scene::NativeNodeContext(),
      node_(node),
      measurement_()
{
}

NullTextContext::~NullTextContext()
{
  this->node_ = 0;
}

short NullTextContext::layout(loka::app::scene::IPlatformController *, loka::app::scene::LayoutState &state)
{
  this->measurement_ = MeasureText(this->node_, state);
  state.height = this->measurement_.height();
  return ClampToShort(state.y + state.height + state.spacing);
}

const NullTextMeasurement &NullTextContext::measurement() const
{
  return this->measurement_;
}

void RegisterNullTextNodeHandler(NullScenePlatformController &controller)
{
  controller.registerNodeHandler(&gNullTextNodeHandler);
}
