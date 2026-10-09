#include "app/CommandSet.hpp"

enum TestCommand { TEST_FIRST_COMMAND, TEST_SECOND_COMMAND, TEST_THIRD_COMMAND,
                   TEST_LAST_COMMAND, TEST_COMMAND_COUNT };

void commandSetSlotPin()
{
  loka::app::CommandSet<TestCommand, TEST_COMMAND_COUNT> commands;
  (void)commands.slot<TEST_COMMAND_COUNT>();
}
