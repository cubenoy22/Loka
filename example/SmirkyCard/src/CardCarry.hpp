#ifndef SMIRKYCARD_CARD_CARRY_HPP
#define SMIRKYCARD_CARD_CARRY_HPP

#include "core/Managed.hpp"
#include "quickjs.h"

namespace smirkycard
{
  /** #1032 frozen v1: inclusive encoded-byte, container-depth and copied-entry limits.
      The outermost container is depth 1; entries are properties/elements, excluding
      the root and array length. Encoded bytes include punctuation and escaping. */
  const unsigned kCarryByteBudget = 16 * 1024;
  const unsigned kCarryDepthBudget = 32;
  const unsigned kCarryEntryBudget = 1024;

  /** Immutable, engine-independent JSON bytes. Empty means absent carry.
      Copies share only bytes; every destination parses its own mutable JS value. */
  class CardCarry
  {
  public:
    static bool encode(JSContext *ctx, JSValueConst value, CardCarry &result);
    JSValue decode(JSContext *ctx) const;
    bool operator<(const CardCarry &other) const;

  private:
    loka::core::Managed<char> bytes_;
    static void release(char *bytes, void *);
  };
} // namespace smirkycard
#endif
