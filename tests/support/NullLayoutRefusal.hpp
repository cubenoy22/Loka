#ifndef LOKA_TESTS_SUPPORT_NULL_LAYOUT_REFUSAL_HPP
#define LOKA_TESTS_SUPPORT_NULL_LAYOUT_REFUSAL_HPP
namespace loka { namespace app { namespace scene { class Node; } } }
namespace loka { namespace testing {
void failNullTextMeasurements(unsigned count, app::scene::Node *markDuringRefusal = 0);
bool declineNullTextMeasurement();
void failNullProjectedLayoutRestores(unsigned count);
bool declineNullProjectedLayoutRestore();
} }
#endif
