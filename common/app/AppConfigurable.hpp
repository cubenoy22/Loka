#ifndef LOKA_APPCONFIGURABLE_HPP
#define LOKA_APPCONFIGURABLE_HPP

class AppComposition;
class PlatformContext;

namespace loka
{
  namespace app
  {
    class MenuComposition;

    enum IdleMode
    {
      IDLE_MODE_NONE = 0,
      IDLE_MODE_EVERY_TICK = 1,
      IDLE_MODE_INTERVAL = 2
    };

    struct IdlePolicy
    {
      IdleMode mode;
      double intervalSeconds;

      IdlePolicy()
          : mode(IDLE_MODE_NONE),
            intervalSeconds(0.0)
      {
      }

      static IdlePolicy none()
      {
        return IdlePolicy();
      }

      static IdlePolicy everyTick()
      {
        IdlePolicy policy;
        policy.mode = IDLE_MODE_EVERY_TICK;
        return policy;
      }

      static IdlePolicy interval(double seconds)
      {
        IdlePolicy policy;
        policy.mode = IDLE_MODE_INTERVAL;
        policy.intervalSeconds = seconds;
        return policy;
      }
    };
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
  virtual loka::app::IdlePolicy idlePolicy() const { return loka::app::IdlePolicy::none(); }
  virtual void onIdle(double elapsedSeconds) { (void)elapsedSeconds; }
  virtual bool handleKeyPress(char key)
  {
    (void)key;
    return false;
  }
  virtual PlatformContext *getPlatformContext() const { return ctx_; }
  virtual ~AppConfigurable() {}
};

#endif // LOKA_APPCONFIGURABLE_HPP
