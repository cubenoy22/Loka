#ifndef DECLARA_MAC_APP_HPP
#define DECLARA_MAC_APP_HPP

#include "core/App.hpp"

class MacApp : public App
{
public:
  explicit MacApp(AppConfigurable *config);
  virtual ~MacApp();

  virtual void run();
  virtual void quit();
};

#endif // DECLARA_MAC_APP_HPP
