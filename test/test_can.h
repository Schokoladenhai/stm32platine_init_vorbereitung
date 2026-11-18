#pragma once

#include "test/shell.h"

// Testbefehle für den CAN-Bus (bus/can_bus): senden, mithören, Zähler anzeigen.
namespace test_can {

shell::CommandTable table();

}  // namespace test_can
