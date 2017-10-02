// Probabilistic Question-Answering system
// @2017 Sarge Rogatch
// This software is distributed under GNU AGPLv3 license. See file LICENSE in repository root for details.

#pragma once

#include "../PqaCore/CEHeapifyPriorsTask.h"

namespace ProbQA {

template<typename taNumber> class CEHeapifyPriorsSubtaskMake : public SRPlat::SRStandardSubtask {
  struct Context;

public: // types
  typedef CEHeapifyPriorsTask<taNumber> TTask;

public: // methods
  using SRPlat::SRStandardSubtask::SRStandardSubtask;
  virtual void Run() override final;
};

} // namespace ProbQA
