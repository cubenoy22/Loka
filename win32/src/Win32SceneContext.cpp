#include "Win32SceneContext.hpp"
#include <windows.h>

// 明示的なデフォルトコンストラクタ・デストラクタ実装（リンカエラー回避用）
Win32SceneContext::Win32SceneContext(Scene *scene)
    : scene_(scene) {}
Win32SceneContext::~Win32SceneContext() {}
