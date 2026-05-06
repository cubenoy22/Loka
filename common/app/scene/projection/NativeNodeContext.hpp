#ifndef LOKA_CORE2_SCENE_PROJECTION_NATIVE_NODE_CONTEXT_HPP
#define LOKA_CORE2_SCENE_PROJECTION_NATIVE_NODE_CONTEXT_HPP

#include <cstddef>
#include "app/scene/Node.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      struct NativeNodeContext : public NodeContext
      {
      public:
        enum ResourcePriority
        {
          PRIORITY_CRITICAL = 100,
          PRIORITY_HIGH = 75,
          PRIORITY_NORMAL = 50,
          PRIORITY_LOW = 25,
          PRIORITY_BACKGROUND = 0
        };

        NativeNodeContext()
            : NodeContext(),
              priority_(PRIORITY_NORMAL),
              memoryCostBytes_(0),
              persistent_(false),
              releaseRequested_(false)
        {
        }

        explicit NativeNodeContext(ResourcePriority priority)
            : NodeContext(),
              priority_(priority),
              memoryCostBytes_(0),
              persistent_(false),
              releaseRequested_(false)
        {
        }

        virtual ~NativeNodeContext() {}

        virtual ICapturableBitmap *asCapturableBitmap()
        {
          return 0;
        }
        virtual const ICapturableBitmap *asCapturableBitmap() const
        {
          return 0;
        }

        void setPriority(ResourcePriority priority)
        {
          priority_ = priority;
        }
        ResourcePriority priority() const
        {
          return priority_;
        }

        void setMemoryCostBytes(std::size_t bytes)
        {
          memoryCostBytes_ = bytes;
        }
        std::size_t memoryCostBytes() const
        {
          return memoryCostBytes_;
        }

        void setPersistent(bool persistent)
        {
          persistent_ = persistent;
        }
        bool isPersistent() const
        {
          return persistent_;
        }

        void requestRelease()
        {
          releaseRequested_ = true;
        }
        bool releaseRequested() const
        {
          return releaseRequested_;
        }

      private:
        ResourcePriority priority_;
        std::size_t memoryCostBytes_;
        bool persistent_;
        bool releaseRequested_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_PROJECTION_NATIVE_NODE_CONTEXT_HPP
