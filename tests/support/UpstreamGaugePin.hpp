#ifndef LOKA_TESTS_SUPPORT_UPSTREAM_GAUGE_PIN_HPP
#define LOKA_TESTS_SUPPORT_UPSTREAM_GAUGE_PIN_HPP
#include "core/SmallObjectPool.hpp"
loka::core::UpstreamGauge upstreamPinSnapshot();
void upstreamPinCheck(const char *name, const loka::core::UpstreamGauge &before,
                      const loka::core::UpstreamGauge &after,
                      unsigned long attempts, unsigned long bytes);
#endif
