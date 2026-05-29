#ifndef LOKA_FRAME_HPP
#define LOKA_FRAME_HPP

namespace loka
{
  namespace core
  {
    struct Frame
    {
      int x;
      int y;
      int width;
      int height;

      Frame()
          : x(-1),
            y(-1),
            width(-1),
            height(-1)
      {
      }
      Frame(int xValue, int yValue, int widthValue, int heightValue)
          : x(xValue),
            y(yValue),
            width(widthValue),
            height(heightValue)
      {
      }

      bool hasPosition() const
      {
        return x >= 0 && y >= 0;
      }
      bool hasSize() const
      {
        return width > 0 && height > 0;
      }
    };

    inline bool operator==(const Frame &lhs, const Frame &rhs)
    {
      return lhs.x == rhs.x && lhs.y == rhs.y && lhs.width == rhs.width && lhs.height == rhs.height;
    }

    inline bool operator!=(const Frame &lhs, const Frame &rhs)
    {
      return !(lhs == rhs);
    }
  } // namespace core
} // namespace loka

#endif // LOKA_FRAME_HPP
