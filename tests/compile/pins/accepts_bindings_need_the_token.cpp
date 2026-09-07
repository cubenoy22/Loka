/**
 * UI bindings can be declared only through the kernel-issued BindingToken.
 * This accepts twin pins that contract together with
 * refuses_bindings_need_the_token.cpp in the same header environment.
 */
#include "app/scene/node/ComponentNode.hpp"

using namespace loka::app::scene;
class Probe : public ComponentNode
{
public:
  virtual void declareBindings(BindingToken &t)
  {
    t.action(emitter, this, &Probe::m);
  }
  virtual void composeChildren(NodeComposition &c) { (void)c; }
private:
  void m() {}
  loka::core::EmitterState emitter;
};
