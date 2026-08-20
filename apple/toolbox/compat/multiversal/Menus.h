#ifndef LOKA_TOOLBOX_MULTIVERSAL_MENUS_H
#define LOKA_TOOLBOX_MULTIVERSAL_MENUS_H

#include <Multiverse.h>

// Universal Interfaces keeps the classic spelling and constants available
// alongside its newer menu vocabulary. Multiversal currently exposes only
// the classic routine and omits the hierarchical-menu aliases.
#define CountMenuItems CountMItems

enum {
  hMenuCmd = 27,
  kInsertHierarchicalMenu = -1
};

#endif
