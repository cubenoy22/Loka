#include "app/scene/boundary/detail/BoundaryBranchSeatState.hpp"

using loka::app::scene::BoundaryBranchSeatRuntimeRegistrationPlan;

void registrationPlanOwnership()
{
  BoundaryBranchSeatRuntimeRegistrationPlan plan;
  BoundaryBranchSeatRuntimeRegistrationPlan copied(plan);
}
