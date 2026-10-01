#ifndef SMIRKYCARD_EXECUTION_SERIAL_HPP
#define SMIRKYCARD_EXECUTION_SERIAL_HPP
#include <stdint.h>
namespace smirkycard
{
  namespace testing
  {
    class ExecutionSerialAccess;
  }

  /** Runtime-wide issuer. Zero refuses admission; exhaustion never wraps. */
  class ExecutionSerial
  {
  public:
    explicit ExecutionSerial(uint32_t seed = 0)
        : last_(seed)
    {
    }
    uint32_t issue()
    {
      if (this->last_ == static_cast<uint32_t>(0xFFFFFFFFu))
        return 0;
      return ++this->last_;
    }

  private:
    friend class testing::ExecutionSerialAccess;
    uint32_t last_;
    ExecutionSerial(const ExecutionSerial &);
    ExecutionSerial &operator=(const ExecutionSerial &);
  };
} // namespace smirkycard
#endif
