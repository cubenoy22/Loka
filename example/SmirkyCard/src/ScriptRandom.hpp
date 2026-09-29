#ifndef SMIRKYCARD_SCRIPT_RANDOM_HPP
#define SMIRKYCARD_SCRIPT_RANDOM_HPP

namespace smirkycard
{
  /** Immutable seed for deterministic Math.random in each script generation.
      Date and performance stay wall-clock. Zero is a legal seed/state. */
  class ScriptRandom
  {
  public:
    static ScriptRandom Seeded(unsigned long seed)
    {
      return ScriptRandom(seed);
    }

    /** Pure masked-32-bit seed derivation; zero remains legal. */
    unsigned long seedForGeneration(unsigned long generation) const
    {
      // Add the golden-ratio word modulo 2^32 per committed generation.
      return (this->seed_ + generation * 0x9E3779B9UL) & 0xFFFFFFFFUL;
    }

  private:
    explicit ScriptRandom(unsigned long seed)
        : seed_(seed & 0xFFFFFFFFUL)
    {
    }
    const unsigned long seed_;
    ScriptRandom &operator=(const ScriptRandom &);
  };
} // namespace smirkycard
#endif
