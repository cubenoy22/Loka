#ifndef DECLARA_TRANSACTION_HPP
#define DECLARA_TRANSACTION_HPP

#include <vector>
#include "Property.hpp"

// ==============================
// トランザクション評価モデル
// ==============================
class Transaction
{
public:
  static void markDirty(PropBase *prop)
  {
    dirtyProps.push_back(prop);
  }

  static void markDirtyDependents(void *statePtr)
  {
    for (int i = 0; i < (int)allProps.size(); ++i)
    {
      markDirty(allProps[i]);
    }
  }

  static bool commit()
  {
    bool changed = false;
    bool anyChanged = false;
    int safety = 100;
    do
    {
      changed = false;
      for (int i = 0; i < (int)dirtyProps.size(); ++i)
      {
        if (dirtyProps[i]->recompute())
        {
          changed = true;
          anyChanged = true;
        }
      }
    } while (changed && --safety > 0);

    dirtyProps.clear();
    return anyChanged;
  }

  static void registerProp(PropBase *prop)
  {
    allProps.push_back(prop);
  }

private:
  static std::vector<PropBase *> dirtyProps;
  static std::vector<PropBase *> allProps;
};

#endif // DECLARA_TRANSACTION_HPP
