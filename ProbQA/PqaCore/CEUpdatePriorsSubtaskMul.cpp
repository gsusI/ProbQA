// Probabilistic Question-Answering system
// @2017 Sarge Rogatch
// This software is distributed under GNU AGPLv3 license. See file LICENSE in repository root for details.

#include "stdafx.h"
#include "../PqaCore/CEUpdatePriorsSubtaskMul.h"
#include "../PqaCore/CEUpdatePriorsTask.h"
#include "../PqaCore/CEQuiz.h"

using namespace SRPlat;

namespace ProbQA {

template class CEUpdatePriorsSubtaskMul<SRDoubleNumber>;

template<> template<bool taCache> void CEUpdatePriorsSubtaskMul<SRDoubleNumber>::RunInternal(const TTask& task) const {
  auto& engine = static_cast<const CpuEngine<SRDoubleNumber>&>(task.GetBaseEngine());
  const CEQuiz<SRDoubleNumber> &quiz = *task._pQuiz;

  static_assert(std::is_same<int64_t, CEQuiz<SRDoubleNumber>::TExponent>::value, "The code below assumes TExponent is"
    " 64-bit integer.");

  auto *PTR_RESTRICT pExps = SRCast::Ptr<__m256i>(quiz.GetTlhExps());
  auto *PTR_RESTRICT pMants = SRCast::Ptr<__m256d>(quiz.GetPriorMants());
  auto *PTR_RESTRICT pvB = SRCast::CPtr<__m256d>( &(engine.GetB(0)) );

  //TODO: consider replacing this with an assert(), because CpuEngine checks for nAnswered==0 and resorts to StartQuiz()
  //  in that case.
  if (task._nAnswered == 0) {
    for (TPqaId i = _iFirst; i < _iLimit; i++) {
      SRSimd::Store<false>(pMants + i, SRSimd::Load<false>(pvB + i));
      SRSimd::Store<false>(pExps + i, _mm256_setzero_si256());
    }
    _mm_sfence();
    return;
  }

  assert(_iLimit > _iFirst);
  const size_t nVectsInBlock = (taCache ? (task._nVectsInCache >> 1) : (_iLimit - _iFirst));
  size_t iBlockStart = _iFirst;
  for(;;) {
    const size_t iBlockLim = std::min(SRCast::ToSizeT(_iLimit), iBlockStart + nVectsInBlock);
    { // separate step for i==0
      const AnsweredQuestion& aq = task._pAQs[0];
      const __m256d *PTR_RESTRICT pAdjMuls = SRCast::CPtr<__m256d>(&(engine.GetA(aq._iQuestion, aq._iAnswer, 0)));
      const __m256d *PTR_RESTRICT pAdjDivs = SRCast::CPtr<__m256d>(&(engine.GetD(aq._iQuestion, 0)));
      for (size_t j = iBlockStart; j < iBlockLim; j++) {
        const __m256d adjMuls = SRSimd::Load<false>(pAdjMuls + j);
        const __m256d adjDivs = SRSimd::Load<false>(pAdjDivs + j);
        // P(answer(aq._iQuestion)==aq._iAnswer GIVEN target==(j0,j1,j2,j3))
        const __m256d P_qa_given_t = _mm256_div_pd(adjMuls, adjDivs);

        const __m256d oldMants = SRSimd::Load<false>(pvB);
        const __m256d product = _mm256_mul_pd(oldMants, P_qa_given_t);
        
        const __m256d newMants = SRSimd::MakeExponent0(product);
        SRSimd::Store<taCache>(pMants + j, newMants);

        const __m256i prodExps = SRSimd::ExtractExponents64<false>(product);
        SRSimd::Store<taCache>(pExps + j, prodExps);
      }
    }
    for (size_t i = 1; i < SRCast::ToSizeT(task._nAnswered); i++) {
      const AnsweredQuestion& aq = task._pAQs[i];
      const __m256d *PTR_RESTRICT pAdjMuls = SRCast::CPtr<__m256d>(&engine.GetA(aq._iQuestion, aq._iAnswer, 0));
      const __m256d *PTR_RESTRICT pAdjDivs = SRCast::CPtr<__m256d>(&engine.GetD(aq._iQuestion, 0));
      for (size_t j = iBlockStart; j < iBlockLim; j++) {
        const __m256d adjMuls = SRSimd::Load<false>(pAdjMuls + j);
        const __m256d adjDivs = SRSimd::Load<false>(pAdjDivs + j);
        // P(answer(aq._iQuestion)==aq._iAnswer GIVEN target==(j0,j1,j2,j3))
        const __m256d P_qa_given_t = _mm256_div_pd(adjMuls, adjDivs);

        //TODO: verify that taCache based branchings are compile-time
        const __m256d oldMants = SRSimd::Load<taCache>(pMants + j);
        const __m256d product = _mm256_mul_pd(oldMants, P_qa_given_t);
        //TODO: move separate summation of exponent to a common function (available to other subtasks etc.)?
        const __m256d newMants = SRSimd::MakeExponent0(product);
        SRSimd::Store<taCache>(pMants + j, newMants);

        const __m256i prodExps = SRSimd::ExtractExponents64<false>(product);
        const __m256i oldExps = SRSimd::Load<taCache>(pExps+j);
        const __m256i newExps = _mm256_add_epi64(prodExps, oldExps);
        SRSimd::Store<taCache>(pExps + j, newExps);
      }
    }
    if (taCache) {
      const size_t nBytes = (iBlockLim - iBlockStart) << SRSimd::_cLogNBytes;
      // The bounds may be used in the next block or by another thread.
      if (iBlockStart > SRCast::ToSizeT(_iFirst)) {
        // Can flush left because it's for the current thread only and has been processed.
        SRUtils::FlushCache<true, false>(pMants + iBlockStart, nBytes);
        SRUtils::FlushCache<true, false>(pExps + iBlockStart, nBytes);
      } else {
        // Can't flush left because another thread may be using it
        SRUtils::FlushCache<false, false>(pMants + iBlockStart, nBytes);
        SRUtils::FlushCache<false, false>(pExps + iBlockStart, nBytes);
      }
      _mm_sfence();
    }
    if (iBlockLim >= SRCast::ToSizeT(_iLimit)) {
      break;
    }
    iBlockStart = iBlockLim;
  }
  if (!taCache) {
    _mm_sfence();
  }
}

template<> void CEUpdatePriorsSubtaskMul<SRDoubleNumber>::Run() {
  auto& task = static_cast<const TTask&>(*GetTask());
  // This should be a tail call
  (task._nVectsInCache < 2) ? RunInternal<false>(task) : RunInternal<true>(task);
}

} // namespace ProbQA
