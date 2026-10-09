#ifndef LOKA_TESTS_MAC_RETIREMENT_PROBE_HPP
#define LOKA_TESTS_MAC_RETIREMENT_PROBE_HPP

/** Test-only association released by native deallocation, independent of retain
    counts. The counter must outlive the native object and every enclosing pool. */
void observeMacNativeDeallocation(void *object, int &count);

#endif
