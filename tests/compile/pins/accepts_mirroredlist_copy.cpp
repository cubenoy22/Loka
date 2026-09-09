#include "core/MirroredList.hpp"
void pin()
{
  loka::core::PushStateTracker tracker;
  loka::core::ObservableList<int> model;
  model.attach(&tracker, 2);
  loka::core::MirroredList<int> source(model);
  (void)source.size();
}
