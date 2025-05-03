#include "Page.cpp"
#include "Button.hpp"
#include "Renderer.hpp"
#include "App.hpp"
#include "Property.hpp"
#include <string>

class FormPage : public Page
{
public:
  FormPage()
      : name(),
        isEnabled(&name, &FormPage::evaluateLength)
  {
  }

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
    isEnabled = DerivedProp<bool, std::string>(&name, &FormPage::evaluateLength);
    b.Button(
        ButtonOptions()
            .setLabel("送信")
            .setEnabled(&isEnabled)
            .setOnClick(&FormPage::onSendClick));
  }

private:
  State<std::string> name;
  DerivedProp<bool, std::string> isEnabled;
};

class MyRenderer : public Renderer
{
  // ...仮実装...
};

int main()
{
  App app;
  app.setRenderer(new MyRenderer());
  app.setPage(new FormPage());
  app.run();
  return 0;
}
