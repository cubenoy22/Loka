#include "RectSurfaceSpriteTests.hpp"

#include "support/TestVerify.hpp"

#include "app/RectSurface.hpp"
#include "core/resource/Image.hpp"

namespace
{
  void IgnoreNativeImage(void *, void *) {}

  loka::core::resource::Image TestImage(void *identity, int width, int height)
  {
    return loka::core::resource::Image::FromNative(identity, width, height, &IgnoreNativeImage, 0);
  }

  loka::app::RectSurfaceModel::DirtyRect SpriteBounds(const loka::app::RectSurfaceModel &model)
  {
    if (model.spriteCount <= 0)
    {
      return loka::app::RectSurfaceModel::DirtyRect();
    }
    short left = model.sprites[0].x;
    short top = model.sprites[0].y;
    short right = static_cast<short>(left + model.sprites[0].width);
    short bottom = static_cast<short>(top + model.sprites[0].height);
    for (short i = 1; i < model.spriteCount; ++i)
    {
      const short candidateLeft = model.sprites[i].x;
      const short candidateTop = model.sprites[i].y;
      const short candidateRight = static_cast<short>(candidateLeft + model.sprites[i].width);
      const short candidateBottom = static_cast<short>(candidateTop + model.sprites[i].height);
      if (candidateLeft < left)
      {
        left = candidateLeft;
      }
      if (candidateTop < top)
      {
        top = candidateTop;
      }
      if (candidateRight > right)
      {
        right = candidateRight;
      }
      if (candidateBottom > bottom)
      {
        bottom = candidateBottom;
      }
    }
    return loka::app::RectSurfaceModel::DirtyRect(
        left, top, static_cast<short>(right - left), static_cast<short>(bottom - top));
  }
} // namespace

void testRectSurfaceModelKeepsMixedSpriteOrderAndBounds()
{
  int identity = 0;
  const loka::core::resource::Image image = TestImage(&identity, 7, 9);
  loka::app::RectSurfaceModel mixed;
  LOKA_VERIFY(mixed.add(loka::app::RectSprite(2, 3, 5, 6)));
  LOKA_VERIFY(mixed.add(loka::app::ImageSprite(11, 13, image)));
  LOKA_VERIFY(mixed.add(loka::app::RectSprite(1, 17, 19, 2)));

  LOKA_VERIFY(mixed.spriteCount == 3);
  LOKA_VERIFY(mixed.sprites[0].kind() == loka::app::RectSurfaceSprite::KIND_RECT);
  LOKA_VERIFY(mixed.sprites[1].kind() == loka::app::RectSurfaceSprite::KIND_IMAGE);
  LOKA_VERIFY(mixed.sprites[2].kind() == loka::app::RectSurfaceSprite::KIND_RECT);

  loka::app::RectSurfaceModel rects;
  LOKA_VERIFY(rects.add(loka::app::RectSprite(2, 3, 5, 6)));
  LOKA_VERIFY(rects.add(loka::app::RectSprite(11, 13, 7, 9)));
  LOKA_VERIFY(rects.add(loka::app::RectSprite(1, 17, 19, 2)));
  LOKA_VERIFY(SpriteBounds(mixed) == SpriteBounds(rects));
}

void testRectSurfaceModelCapacityCountsEverySpriteKind()
{
  int identity = 0;
  const loka::core::resource::Image image = TestImage(&identity, 2, 3);
  loka::app::RectSurfaceModel rectRefusal;
  for (short i = 0; i < 15; ++i)
  {
    LOKA_VERIFY(rectRefusal.add(loka::app::RectSprite(i, i, 1, 1)));
  }
  LOKA_VERIFY(rectRefusal.add(loka::app::ImageSprite(15, 15, image)));
  LOKA_VERIFY(rectRefusal.spriteCount == loka::app::RectSurfaceModel::kMaxSprites);
  LOKA_VERIFY(!rectRefusal.add(loka::app::RectSprite(16, 16, 1, 1)));
  LOKA_VERIFY(rectRefusal.spriteCount == loka::app::RectSurfaceModel::kMaxSprites);

  loka::app::RectSurfaceModel imageRefusal;
  for (short i = 0; i < loka::app::RectSurfaceModel::kMaxSprites; ++i)
  {
    LOKA_VERIFY(imageRefusal.add(loka::app::RectSprite(i, i, 1, 1)));
  }
  LOKA_VERIFY(!imageRefusal.add(loka::app::ImageSprite(16, 16, image)));
  LOKA_VERIFY(imageRefusal.spriteCount == loka::app::RectSurfaceModel::kMaxSprites);
}

void testImageSpriteKeepsImageIdentityAndIntrinsicSize()
{
  int identity = 0;
  const loka::core::resource::Image image = TestImage(&identity, 23, 29);
  loka::app::RectSurfaceModel model;
  LOKA_VERIFY(model.add(loka::app::ImageSprite(5, 7, image)));

  LOKA_VERIFY(model.sprites[0].kind() == loka::app::RectSurfaceSprite::KIND_IMAGE);
  LOKA_VERIFY(model.sprites[0].x == 5);
  LOKA_VERIFY(model.sprites[0].y == 7);
  LOKA_VERIFY(model.sprites[0].width == 23);
  LOKA_VERIFY(model.sprites[0].height == 29);
  loka::core::resource::Image carried;
  LOKA_VERIFY(model.sprites[0].queryImage(carried));
  LOKA_VERIFY(carried == image);
  LOKA_VERIFY(carried.nativeHandle() == &identity);
}

void testImageSpriteHandleChangeRequiresRepaintAtSameGeometry()
{
  int firstIdentity = 0;
  int secondIdentity = 0;
  const loka::core::resource::Image first = TestImage(&firstIdentity, 23, 29);
  const loka::core::resource::Image second = TestImage(&secondIdentity, 23, 29);
  const loka::app::RectSurfaceSprite previous(loka::app::ImageSprite(5, 7, first));
  const loka::app::RectSurfaceSprite current(loka::app::ImageSprite(5, 7, second));

  LOKA_VERIFY(previous.x == current.x && previous.y == current.y);
  LOKA_VERIFY(previous.width == current.width && previous.height == current.height);
  LOKA_VERIFY(loka::app::RectSurfaceSpriteRequiresRepaint(current, previous));
  LOKA_VERIFY(!loka::app::RectSurfaceSpriteRequiresRepaint(previous, previous));
}

void testRectSurfacePaintListKeepsMixedSpriteOrder()
{
  int identity = 0;
  const loka::core::resource::Image image = TestImage(&identity, 7, 9);
  loka::app::RectSurfaceModel model;
  LOKA_VERIFY(model.add(loka::app::RectSprite(1, 2, 3, 4)));
  LOKA_VERIFY(model.add(loka::app::ImageSprite(5, 6, image)));
  LOKA_VERIFY(model.add(loka::app::RectSprite(7, 8, 9, 10)));

  const loka::app::RectSurfacePaintList paintList(model);
  LOKA_VERIFY(paintList.count() == 3);
  LOKA_VERIFY(paintList.querySprite(0)->kind() == loka::app::RectSurfaceSprite::KIND_RECT);
  LOKA_VERIFY(paintList.querySprite(1)->kind() == loka::app::RectSurfaceSprite::KIND_IMAGE);
  LOKA_VERIFY(paintList.querySprite(2)->kind() == loka::app::RectSurfaceSprite::KIND_RECT);
  LOKA_VERIFY(paintList.querySprite(3) == 0);
}
