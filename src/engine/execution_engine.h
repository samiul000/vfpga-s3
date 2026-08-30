#pragma once

#include <cstdint>

class ExecutionEngine {
public:
    enum class Mode { SCALAR, BITPARALLEL, SIMD, DUAL_CORE };
    void set_mode(Mode mode);
    Mode get_mode() const;

private:
    Mode mode_ = Mode::SCALAR;
};
