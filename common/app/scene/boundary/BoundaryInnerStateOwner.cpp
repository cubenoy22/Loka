#include "BoundaryInnerStateOwner.hpp"
#include "app/scene/boundary/Boundary.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {
      void BoundaryInnerStateOwner::reserveHeldArena(size_t totalSize)
      {
        BoundaryNode *boundary = this->enclosingBoundary();
        assert(boundary &&
               "Boundary-inner Held storage requires an enclosing Boundary");
        if (boundary)
        {
          boundary->reserveHeldArena(totalSize);
        }
      }

      void *BoundaryInnerStateOwner::allocateHeldMemory(size_t size,
                                                        size_t align)
      {
        BoundaryNode *boundary = this->enclosingBoundary();
        assert(boundary &&
               "Boundary-inner Held storage requires an enclosing Boundary");
        return boundary ? boundary->allocateHeldMemory(size, align) : 0;
      }

      void BoundaryInnerStateOwner::registerHeldMemory(
          loka::core::detail::HeldBlockBase *block)
      {
        BoundaryNode *boundary = this->enclosingBoundary();
        assert(boundary &&
               "Boundary-inner Held storage requires an enclosing Boundary");
        if (boundary)
        {
          boundary->registerHeldMemory(block);
        }
      }

      void BoundaryInnerStateOwner::retireHeldBlock(
          loka::core::detail::HeldBlockBase *block)
      {
        BoundaryNode *boundary = this->enclosingBoundary();
        assert(boundary &&
               "Boundary-inner Held release requires an enclosing Boundary");
        if (boundary)
        {
          boundary->retireHeldBlock(block);
        }
      }
    } // namespace scene
  } // namespace app
} // namespace loka
