#include "Page.hpp"
#include "Button.hpp"
#include "Renderer.hpp"
#include "App.hpp"
#include "Property.hpp"
#include <string>

class FormPage : public Page
{
public:
  FormPage() : Page(), name(&transaction_, ""), isValid(&transaction_, &name, &FormPage::evaluateLength) {}

  static bool evaluateLength(const std::string &s)
  {
    return s.length() >= 3;
  }
  static void onSendClick()
  {
    // 実行時のボタンクリック用（引数不要）
  }
  void build(PageBuilder &b)
  {
    b.Text("名前を入力してください");
    b.TextInput(&name);
    b.Button(
        ButtonOptions()
            .setLabel("送信")
            .setEnabled(&isValid)
            .setOnClick(&FormPage::onSendClick));
  }
  State<std::string> name;
  DerivedProp<bool, std::string> isValid;
};

class MyRenderer : public Renderer
{
  // ...仮実装...
};

int main()
{
  MyRenderer renderer;
  Window window(&renderer);
  FormPage page;
  window.setPage(&page);
  App app(&window);
  app.run();
  return 0;
}
