#include "execution_engine.h"

void ExecutionEngine::set_mode(Mode mode) { mode_ = mode; }
ExecutionEngine::Mode ExecutionEngine::get_mode() const { return mode_; }
