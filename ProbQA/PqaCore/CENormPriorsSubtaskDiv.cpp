// Probabilistic Question-Answering system
// @2017 Sarge Rogatch
// This software is distributed under GNU AGPLv3 license. See file LICENSE in repository root for details.

#include "stdafx.h"
#include "../PqaCore/CENormPriorsSubtaskDiv.h"
#include "../PqaCore/CENormPriorsTask.h"
#include "../PqaCore/CEQuiz.h"

using namespace SRPlat;

namespace ProbQA {

template<typename taNumber> CENormPriorsSubtaskDiv<taNumber>::CENormPriorsSubtaskDiv(TTask *pTask)
  : SRStandardSubtask(pTask) { }

template<> void CENormPriorsSubtaskDiv<SRDoubleNumber>::Run() {
  auto const &PTR_RESTRICT task = static_cast<const CENormPriorsTask<SRDoubleNumber>&>(*GetTask());
  __m256d* pMants = SRCast::Ptr<__m256d>(task.GetQuiz().GetTlhMants());
  for (TPqaId i = _iFirst; i < _iLimit; i++) {
    const __m256d original = SRSimd::Load<false>(pMants + i);
    const __m256d normalized = _mm256_div_pd(original, task._sumPriors._comps);
    SRSimd::Store<false>(pMants + i, original);
  }
}

template class CENormPriorsSubtaskDiv<SRPlat::SRDoubleNumber>;

} // namespace ProbQA