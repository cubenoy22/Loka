#ifndef LOKA_APPCONFIGURABLE_HPP
#define LOKA_APPCONFIGURABLE_HPP

// 前方宣言のみに変更して循環参照を解消
class AppComposition;
class PlatformContext;

namespace loka
{
  namespace app
  {
    class MenuComposition;
  }
}

class AppConfigurable
{
protected:
  PlatformContext *ctx_;

public:
  AppConfigurable(PlatformContext *ctx) : ctx_(ctx) {}
  virtual void compose(AppComposition &c) = 0;
  virtual void composeMenu(loka::app::MenuComposition &c) {}
  virtual PlatformContext *getPlatformContext() const { return ctx_; }
  virtual ~AppConfigurable() {}
};

#endif // LOKA_APPCONFIGURABLE_HPP
