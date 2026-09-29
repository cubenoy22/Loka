#include "app/scene/projection/CollectPaintAnswers.hpp"
using namespace loka::app::scene;
struct Source
{
  enum { kMergesExactDamage = 0 };
  bool queryPaintAnswer(Node *, NodeContext *, const PaintQuery &, PaintAnswer &) { return false; }
};
void collect(BoundaryNode &root, const PaintQuery &query)
{
  Source source;
  PaintAnswerBuffer<> buffer;
  CollectPaintAnswers(root, query, buffer, source);
}
