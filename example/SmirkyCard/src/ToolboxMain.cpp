#include "app/bootstrap/RunApp.hpp"
#include "MyAppConfig.hpp"

int main(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  return loka::platform::RunApp<SmirkyCardAppConfig>();
}
