#include <Events.h>
#include <Lists.h>
#include <Controls.h>
#include <TextEdit.h>
#include <Menus.h>
#include <Quickdraw.h>
#include <Dialogs.h>

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
