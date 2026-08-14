#include <Foundation/Foundation.h>

#include "StandaloneFlowApplication.hpp"

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
  const int result = loka::standalone_tests::RunStandaloneFlowApplication();
  (void)pool;
  return result;
}
