#ifndef LOKA_APP_SHOW_HPP
#define LOKA_APP_SHOW_HPP

#include "app/Empty.hpp"
#include "app/scene/node/Conditional.hpp"

namespace loka
{
  namespace app
  {
    class ShowBuilder
    {
    public:
      explicit ShowBuilder(loka::core::State<bool> *condition) : condition_(condition) {}

      template <typename T>
      scene::ConditionalDefinition operator<<(T &trueDef) const
      {
        Empty falseDef;
        return scene::ConditionalDefinition(scene::ConditionalProps(condition_, &trueDef, &falseDef));
      }

      template <typename T>
      scene::ConditionalDefinition operator<<(const T &trueDef) const
      {
        Empty falseDef;
        return scene::ConditionalDefinition(scene::ConditionalProps(condition_, const_cast<T *>(&trueDef), &falseDef));
      }

    private:
      loka::core::State<bool> *condition_;
    };

    inline ShowBuilder Show(loka::core::State<bool> &condition)
    {
      return ShowBuilder(&condition);
    }

    inline ShowBuilder Show(const loka::core::State<bool> &condition)
    {
      return ShowBuilder(const_cast<loka::core::State<bool> *>(&condition));
    }
  } // namespace app
} // namespace loka

#endif // LOKA_APP_SHOW_HPP
