#ifndef LOKA_APP_SCENE_ABILITY_CAPTURABLE_BITMAP_HPP
#define LOKA_APP_SCENE_ABILITY_CAPTURABLE_BITMAP_HPP

namespace loka
{
  namespace core
  {
    namespace resource
    {
      class Image;
    }
  }

  namespace app
  {
    namespace scene
    {
      struct ICapturableBitmap
      {
        virtual ~ICapturableBitmap() {}
        virtual bool captureBitmap(::loka::core::resource::Image &out) const = 0;
      };
    }
  }
}

#endif // LOKA_APP_SCENE_ABILITY_CAPTURABLE_BITMAP_HPP
