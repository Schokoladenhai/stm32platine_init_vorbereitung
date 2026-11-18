#pragma once

#include "test/shell.h"

// Testbefehle für den Lenkservo (drivers/servo) an SERVO1 (TIM3 CH1, PA6).
namespace test_servo {

shell::CommandTable table();

}  // namespace test_servo
