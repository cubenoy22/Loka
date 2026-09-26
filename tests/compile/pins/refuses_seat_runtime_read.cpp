#include "app/scene/boundary/detail/BoundaryBranchSeatState.hpp"

using namespace loka::app::scene;

void readSeatRuntime(const BoundaryBranchSeatState &seats,
                     const BoundaryParkedBranchKey &key,
                     BoundaryBranchSeatRuntimeEntry &row)
{
  (void)row;
  (void)seats.findRuntime(key);
}
