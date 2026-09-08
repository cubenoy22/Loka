#ifndef LOKA_BRANCH_SEAT_DECLARATION_HPP
#define LOKA_BRANCH_SEAT_DECLARATION_HPP

#include "app/scene/composition/NodeComposition.hpp"
#include "app/scene/boundary/detail/BoundaryBranchSeatState.hpp"

namespace loka
{
  namespace app
  {
    namespace scene
    {

      /** Owns the definitions and nested seat plans of one completed declaration.
          A replacement is built separately and published only after materialization.
          The enclosing seat owns this scope; it does not own application State. */
      class BranchSeatDeclaration
      {
      public:
        virtual ~BranchSeatDeclaration() {}
        virtual bool matchesCurrentKey() const = 0;

        /** Complete the window with a distinct runtime branch root. */
        bool completeWindow()
        {
          if (!this->composition.encloseRoot())
            return false;
          this->composition.assignCompositionSeatSlots();
          return true;
        }

        NodeComposition composition;
        BoundaryBranchSeatState seats;

      protected:
        BranchSeatDeclaration() {}

      private:
        BranchSeatDeclaration(const BranchSeatDeclaration &);
        BranchSeatDeclaration &operator=(const BranchSeatDeclaration &);
      };

    } // namespace scene
  } // namespace app
} // namespace loka
#endif
