#pragma once

#include "../PqaCore/DoubleNumber.h"

namespace ProbQA {

// Number-specific data for CETrainTask
template <typename taNumber> class CETrainTaskNumberSpec;

template<> class CETrainTaskNumberSpec<DoubleNumber> {
public: // variables
  __m256d _fullAddend; // non-colliding (4 at once)
  __m256d _collAddend; // colliding (3 at once)
};

} // namespace ProbQA
