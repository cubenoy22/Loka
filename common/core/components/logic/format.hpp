#ifndef DECLARA_COMPONENTS_LOGIC_FORMAT_HPP
#define DECLARA_COMPONENTS_LOGIC_FORMAT_HPP

#include <string>
#include "core/State.hpp"

// 数値型専用のフォーマッタ関数テンプレート定義
template <typename T>
std::string formatValue(T val, const std::string &fmt);

// StrFormatState: State<int>やState<T>を指定フォーマットでState<std::string>に変換するノード
// C++98対応・UIレス
// ※本テンプレートはint/double等の数値型専用。Tが未対応型の場合はformatValue未定義でコンパイルエラー。
// deferBindを使うことで複数State更新時の効率化も可能。

template <typename T>
class StrFormatState : public State<std::string>
{
public:
  StrFormatState(State<T> *in, const std::string &fmt)
      : State<std::string>(), in_(in), fmt_(fmt)
  {
    updateValue();
    if (in_)
      in_->deferBind(&StrFormatState::onInputChanged, this);
  }
  ~StrFormatState()
  {
    if (in_)
      in_->deferUnbind(&StrFormatState::onInputChanged, this);
  }
  static void onInputChanged(void *userData)
  {
    StrFormatState *self = static_cast<StrFormatState *>(userData);
    if (self)
      self->updateValue();
  }

private:
  void updateValue()
  {
    if (in_)
      State<std::string>::set(formatValue(in_->get(), fmt_));
  }
  State<T> *in_;
  std::string fmt_;
};

// 数値型専用のフォーマッタ（特殊化して対応型を増やせる）
template <>
inline std::string formatValue(int val, const std::string &fmt)
{
  char buf[256];
  snprintf(buf, sizeof(buf), fmt.c_str(), val);
  return std::string(buf);
}

template <>
inline std::string formatValue(float val, const std::string &fmt)
{
  char buf[256];
  snprintf(buf, sizeof(buf), fmt.c_str(), val);
  return std::string(buf);
}

template <>
inline std::string formatValue(double val, const std::string &fmt)
{
  char buf[256];
  snprintf(buf, sizeof(buf), fmt.c_str(), val);
  return std::string(buf);
}

#endif // DECLARA_COMPONENTS_LOGIC_FORMAT_HPP
