#if !defined(LOKA_STANDALONE_FLOW_CONFIG_HEADER)
#error StandaloneFlowMain requires LOKA_STANDALONE_FLOW_CONFIG_HEADER
#endif

#if !defined(LOKA_STANDALONE_FLOW_CONFIG_TYPE)
#error StandaloneFlowMain requires LOKA_STANDALONE_FLOW_CONFIG_TYPE
#endif

#include LOKA_STANDALONE_FLOW_CONFIG_HEADER
#include "StandaloneFlowRunner.hpp"

#include <Foundation/Foundation.h>

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  const int result =
      loka::standalone_tests::RunStandaloneFlowWithConfig<LOKA_STANDALONE_FLOW_CONFIG_TYPE>(0, 0);
  (void)pool;
  return result;
}
