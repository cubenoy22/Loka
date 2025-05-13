#ifndef DECLARA_APPCONFIGURABLE_HPP
#define DECLARA_APPCONFIGURABLE_HPP

// 前方宣言のみに変更して循環参照を解消
class AppBuilder;
class PlatformContext;

class AppConfigurable
{
protected:
  PlatformContext *ctx_;

public:
  AppConfigurable(PlatformContext *ctx) : ctx_(ctx) {}
  virtual void configure(AppBuilder &builder) = 0;
  virtual PlatformContext *getPlatformContext() const { return ctx_; }
  virtual ~AppConfigurable() {}
};

#endif // DECLARA_APPCONFIGURABLE_HPP
