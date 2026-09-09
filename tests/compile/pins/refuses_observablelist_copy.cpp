#include "core/MirroredList.hpp"
void pin()
{
  loka::core::PushStateTracker tracker;
  loka::core::ObservableList<int> model;
  model.attach(&tracker, 2);
  loka::core::ObservableList<int> source;
  loka::core::ObservableList<int> other(source);
}
