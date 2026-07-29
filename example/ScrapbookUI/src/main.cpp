#include "MyAppConfig.hpp"
#include "app/bootstrap/RunApp.hpp"

#if !defined(LOKA_RETRO68)
#error ScrapbookUI is a Retro68-only example
#endif

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::platform::RunApp<ScrapbookAppConfig>();
}
