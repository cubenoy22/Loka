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
              lifetimeHint_(NATIVE_HINT_DEFAULT)
        {
        }

        explicit NativeNodeContext(ResourcePriority priority)
            : NodeContext(),
              priority_(priority),
              memoryCostBytes_(0),
              lifetimeHint_(NATIVE_HINT_DEFAULT)
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

        /** The native side's observed copy of the owner Node's lifetime
            hint, refreshed on the normal observation paths (layout/update).
            A snapshot of the one wish axis — never an imperative command. */
        void observeLifetimeHint(NativeLifetimeHint hint)
        {
          lifetimeHint_ = hint;
        }
        NativeLifetimeHint lifetimeHint() const
        {
          return lifetimeHint_;
        }

      private:
        ResourcePriority priority_;
        std::size_t memoryCostBytes_;
        NativeLifetimeHint lifetimeHint_;
      };
    } // namespace scene
  } // namespace app
} // namespace loka

#endif // LOKA_CORE2_SCENE_PROJECTION_NATIVE_NODE_CONTEXT_HPP
