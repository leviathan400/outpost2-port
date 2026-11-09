#pragma once

namespace op2 {

// --harness: a scenario played headless, written in the op2-rs harness's formats
// (src/harness_mode.cpp has the options and outputs). Returns the process exit code.
int runHarness(int argc, char** argv);

}  // namespace op2
