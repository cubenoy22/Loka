#ifndef LOKA_PLATFORMCONTEXT_HPP
#define LOKA_PLATFORMCONTEXT_HPP
#if defined(_WIN32) || defined(WIN32)
#include <windows.h>
#else
// Provide a placeholder type for non-Windows builds
typedef void *HINSTANCE;
#endif

class AppConfigurable;
class PlatformContext;
class App;
class Window;
struct WindowProps;

namespace loka
{
  namespace file
  {
    struct File;
  }

  namespace platform
  {
    namespace file
    {
      struct FileHandle;
    }
  }

  namespace app
  {
    namespace scene
    {
      class Scene;
      class Node;
      struct NodeContext;
    }
  }
}

class PlatformContext
{
public:
  virtual ~PlatformContext() {}

  // Appインスタンス取得用の純粋仮想関数を追加
  virtual App *createApp(AppConfigurable *config, HINSTANCE hInstance, int nCmdShow) const = 0;

  // Window生成ファクトリーメソッド（サブクラス実装必須）
  // app_がnullの場合はassertで即座に失敗させる設計にする
  virtual Window *createWindow(const WindowProps &props) = 0;

  virtual loka::app::scene::NodeContext *createNodeContext(loka::app::scene::Node *node) const = 0;
  virtual bool openFile(const loka::file::File &item, loka::platform::file::FileHandle &out) const = 0;
};

#endif // LOKA_PLATFORMCONTEXT_HPP
