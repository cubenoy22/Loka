#include "app/scene/state/RequestSettlement.hpp"
#include "app/style/LineCursor.hpp"
using namespace loka::app;
using namespace loka::app::scene;
void pin(IPlatformController &c) { BorrowScope first(c); BorrowScope &second = first; (void)second; }
