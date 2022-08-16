#include "flopoco/Tools/ScaledMPZ.hpp"
#include <cassert>

namespace flopoco {
void ScaledMPZ::rescale(int64_t new_scale) {
    assert(new_scale <= lowbit_weight);
    value <<= (lowbit_weight - new_scale);
    lowbit_weight = new_scale;
}
}