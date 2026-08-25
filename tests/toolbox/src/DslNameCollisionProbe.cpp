// The classic headers a rig actually ships depend on which header set its
// Retro68 was built from: the multiversal CIncludes carry some of these, while
// others arrive only with Apple's Universal Interfaces, which cannot be
// redistributed and are therefore absent from the hosted CI image. Include
// whatever this rig has rather than assuming one set, so the probe guards the
// collisions that exist HERE instead of failing to build. A rig missing a
// header simply cannot host the collision that header introduces.
#if defined(__has_include)
#if __has_include(<Events.h>)
#include <Events.h>
#endif
#if __has_include(<Lists.h>)
#include <Lists.h>
#endif
#if __has_include(<Controls.h>)
#include <Controls.h>
#endif
#if __has_include(<TextEdit.h>)
#include <TextEdit.h>
#endif
#if __has_include(<Menus.h>)
#include <Menus.h>
#endif
#if __has_include(<Quickdraw.h>)
#include <Quickdraw.h>
#endif
#if __has_include(<Dialogs.h>)
#include <Dialogs.h>
#endif
#else
#error "The collision probe needs __has_include to adapt to the rig's header set"
#endif

#include "example/FloppyBird/src/MainNode.hpp"
#include "example/HelloWorld/src/MainNode.hpp"
#include "example/MineSweeper/src/MainNode.hpp"
#include "example/ScrapbookUI/src/MainNode.hpp"
#include "example/SimpleViewer/src/MainNode.hpp"
#include "example/Tutorial/src/DoItYourselfNode.hpp"
#include "example/Tutorial/src/Step1Node.hpp"
#include "example/Tutorial/src/Step2Node.hpp"
#include "example/Tutorial/src/Step3Node.hpp"
#include "example/Tutorial/src/Step4Node.hpp"

namespace helloworld
{
  inline loka::app::ButtonDefinition ProbeButtonName()
  {
    return Button("probe");
  }
} // namespace helloworld

namespace minesweeper
{
  inline loka::app::ButtonDefinition ProbeButtonName()
  {
    return Button("probe");
  }

  inline loka::app::CellDefinition ProbeCellName()
  {
    return Cell("probe");
  }
} // namespace minesweeper

namespace scrapbook
{
  inline loka::app::ButtonDefinition ProbeButtonName()
  {
    return Button("probe");
  }
} // namespace scrapbook

namespace simpleviewer
{
  inline loka::app::ButtonDefinition ProbeButtonName()
  {
    return Button("probe");
  }
} // namespace simpleviewer

namespace tutorial
{
  inline loka::app::ButtonDefinition ProbeButtonName()
  {
    return Button("probe");
  }
} // namespace tutorial
