#pragma once

#include "test/shell.h"

// Testbefehle für den VESC-Treiber (drivers/vesc): Befehle senden, Status anzeigen, VESC suchen.
namespace test_vesc {

shell::CommandTable table();

}  // namespace test_vesc
