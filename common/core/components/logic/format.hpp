#ifndef DECLARA_COMPONENTS_LOGIC_FORMAT_HPP
#define DECLARA_COMPONENTS_LOGIC_FORMAT_HPP

#include <string>
#include "core/State.hpp"

// StrFormatState: State<int>やState<T>を指定フォーマットでState<std::string>に変換するノード
// C++98対応・UIレス
// ※本テンプレートはint/double等の数値型専用。Tが未対応型の場合はformatValue未定義でコンパイルエラー。
// bindDeferを使うことで複数State更新時の効率化も可能。

template <typename T>
class StrFormatState : public State<std::string>
{
public:
  StrFormatState(State<T> *in, const std::string &fmt)
      : State<std::string>(), in_(in), fmt_(fmt)
  {
    updateValue();
    if (in_)
      in_->bindDefer(&StrFormatState::onInputChanged, this);
  }
  ~StrFormatState()
  {
    if (in_)
      in_->unbind(&StrFormatState::onInputChanged, this);
  }
  static void onInputChanged(void *userData)
  {
    StrFormatState *self = static_cast<StrFormatState *>(userData);
    if (self)
      self->updateValue();
  }
  void updateValue()
  {
    // 数値型(int/double等)の文字列化用途であれば64バイトで十分安全。
    // 例: 64bit整数や指数表記doubleでも64文字を超えることはまずない。
    // フォーマットや型が将来的に拡張される場合は動的バッファ化も検討可。
    char buf[64];
    formatValue(buf, sizeof(buf), in_ ? in_->get() : T());
    this->setValue(std::string(buf));
  }

protected:
  void formatValue(char *buf, size_t bufsize, int v)
  {
    snprintf(buf, bufsize, fmt_.c_str(), v);
  }
  void formatValue(char *buf, size_t bufsize, double v)
  {
    snprintf(buf, bufsize, fmt_.c_str(), v);
  }
  // 他の型は必要に応じて特殊化
private:
  State<T> *in_;
  std::string fmt_;
};

#endif // DECLARA_COMPONENTS_LOGIC_FORMAT_HPP
