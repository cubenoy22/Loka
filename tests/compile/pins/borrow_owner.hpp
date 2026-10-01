#include "app/scene/state/RequestSettlement.hpp"
#include "app/style/LineCursor.hpp"
using namespace loka::app;
using namespace loka::app::scene;
class PinOwner : public SettleOwner<LineCursor>
{
public:
  PinOwner(IPlatformController &controller, Node *node, const NodeContext *identity)
#if defined(LOKA_PIN_WITHOUT_CONTROLLER)
      : SettleOwner<LineCursor>(node, identity)
#elif defined(LOKA_PIN_DEFAULT_OWNER)
      : SettleOwner<LineCursor>()
#else
      : SettleOwner<LineCursor>(controller, node, identity)
#endif
  {
    (void)controller;
    (void)node;
    (void)identity;
  }
#ifdef TEST_BUILD
  PinOwner(BorrowPhase &phase, Node *node, const NodeContext *identity)
      : SettleOwner<LineCursor>(phase, node, identity) {}
  virtual LineCursor fact(Node &) const { return LineCursor(); }
#endif
  virtual FollowUpResult finishSettle(Node &, const FollowUps &) { return FOLLOW_UP_NONE; }
};
