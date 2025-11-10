#ifndef DECLARA_STATEUTIL_HPP
#define DECLARA_STATEUTIL_HPP
#include <vector>
#include <cstdarg>

// StateBaseの前方宣言
class StateBase;

// 可変長引数でStateBase*のvectorを生成（C++98互換）
static std::vector<StateBase *> makeStateVector(StateBase *first, ...)
{
  std::vector<StateBase *> v;
  va_list args;
  va_start(args, first);
  for (StateBase *s = first; s != 0; s = va_arg(args, StateBase *))
  {
    if (s) // 念のためNULLチェック（終端判定だけでなく、NULLポインタ自体を除外する）
      v.push_back(s);
  }
  va_end(args);
  return v;
}

#endif // DECLARA_STATEUTIL_HPP
