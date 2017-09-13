// Probabilistic Question-Answering system
// @2017 Sarge Rogatch
// This software is distributed under GNU AGPLv3 license. See file LICENSE in repository root for details.

#pragma once

#include "../SRPlatform/Interface/SRStandardSubtask.h"
#include "../SRPlatform/Interface/SRThreadPool.h"

namespace SRPlat {

class SRPoolRunner {
public: // types
  template<typename taSubtask> class Keeper {
    friend class SRPoolRunner;

    taSubtask *const _pSubtasks;
    typename taSubtask::TTask *_pTask;
    SRThreadCount _nSubtasks = 0;

    //NOTE: this method doesn't set variables in a way to prevent another ReleaseInternal() operation.
    void ReleaseInternal() {
      if (_pTask) {
        _pTask->WaitComplete();
      }
      for (SRThreadCount i = 0; i < _nSubtasks; i++) {
        _pSubtasks[i].~taSubtask();
      }
    }

  public:
    explicit Keeper(void *pSubtasksMem, typename taSubtask::TTask& task)
      : _pSubtasks(SRCast::Ptr<taSubtask>(pSubtasksMem)), _pTask(&task) { }

    ~Keeper() {
      ReleaseInternal();
    }

    Keeper(const Keeper&) = delete;
    Keeper& operator=(const Keeper&) = delete;

    Keeper(Keeper&& source) : _pSubtasks(source._pSubtasks), _pTask(source._pTask), _nSubtasks(source._nSubtasks)
    {
      source._pTask = nullptr;
      source._nSubtasks = 0;
    }

    Keeper& operator=(Keeper&& source) {
      if (this != &source) {
        ReleaseInternal();
        _pSubtasks = source._pSubtasks;
        _pTask = source._pTask;
        _nSubtasks = source._nSubtasks;
        source._pTask = nullptr;
        source._nSubtasks = 0;
      }
      return *this;
    }
  };

private: // variables
  SRThreadPool *_pTp;
  void *_pSubtasksMem;

public:
  explicit SRPoolRunner(SRThreadPool& tp, void *pSubtasksMem) : _pTp(&tp), _pSubtasksMem(pSubtasksMem) { }

  SRThreadPool& GetThreadPool() const { return *_pTp; }

  template<typename taSubtask, typename taCallback> inline Keeper<taSubtask> SplitAndRunSubtasks(
    typename taSubtask::TTask& task, const size_t nItems, const SRThreadCount nWorkers, const taCallback &subtaskInit);

  template<typename taSubtask> inline Keeper<taSubtask> SplitAndRunSubtasks(typename taSubtask::TTask& task,
    const size_t nItems, const SRThreadCount nWorkers)
  {
    return SplitAndRunSubtasks<taSubtask>(task, nItems, nWorkers,
      [&](void *pStMem, SRThreadCount iWorker, int64_t iFirst, int64_t iLimit) {
        taSubtask *pSt = new(pStMem) taSubtask(&task);
        pSt->SetStandardParams(iWorker, iFirst, iLimit);
      }
    );
  }

  template<typename taSubtask> inline Keeper<taSubtask> SplitAndRunSubtasks(typename taSubtask::TTask& task,
    const size_t nItems)
  {
    return SplitAndRunSubtasks<taSubtask>(task, nItems, _pTp->GetWorkerCount());
  }

  template<typename taSubtask, typename taCallback> inline Keeper<taSubtask> RunPerWorkerSubtasks(
    typename taSubtask::TTask& task, const SRThreadCount nWorkers, const taCallback &subtaskInit);

  template<typename taSubtask> inline Keeper<taSubtask> RunPerWorkerSubtasks(typename taSubtask::TTask& task,
    const SRThreadCount nWorkers)
  {
    return RunPerWorkerSubtasks<taSubtask>(task, nWorkers, [&](void *pStMem, SRThreadCount iWorker) {
      taSubtask *pSt = new(pStMem) taSubtask(&task);
      pSt->SetStandardParams(iWorker, iWorker, iWorker + 1);
    });
  }
};

template<typename taSubtask, typename taCallback> inline
  SRPoolRunner::Keeper<taSubtask> SRPoolRunner::SplitAndRunSubtasks(
  typename taSubtask::TTask& task, const size_t nItems, const SRThreadCount nWorkers, const taCallback &subtaskInit)
{
  Keeper<taSubtask> kp(_pSubtasksMem, task);

  size_t nextStart = 0;
  const lldiv_t perWorker = div((long long)nItems, (long long)nWorkers);

  while (kp._nSubtasks < nWorkers && nextStart < nItems) {
    const size_t curStart = nextStart;
    nextStart += perWorker.quot + (((long long)kp._nSubtasks < perWorker.rem) ? 1 : 0);
    assert(nextStart <= nItems);
    subtaskInit(kp._pSubtasks + kp._nSubtasks, kp._nSubtasks, curStart, nextStart);
    // For finalization, it's important to increment subtask counter right after another subtask has been
    //   constructed.
    kp._nSubtasks++;
  }
  _pTp->EnqueueAdjacent(kp._pSubtasks, kp._nSubtasks, task);

  kp._pTask = nullptr; // Don't call again SRBaseTask::WaitComplete() if it throws here.
  task.WaitComplete();
  return std::move(kp);
}

template<typename taSubtask, typename taCallback> inline
  SRPoolRunner::Keeper<taSubtask> SRPoolRunner::RunPerWorkerSubtasks(
  typename taSubtask::TTask& task, const SRThreadCount nWorkers, const taCallback &subtaskInit)
{
  Keeper<taSubtask> kp(_pSubtasksMem, task);

  while (kp._nSubtasks < nWorkers) {
    subtaskInit(kp._pSubtasks + kp._nSubtasks, kp._nSubtasks);
    // For finalization, it's important to increment subtask counter right after another subtask has been
    //   constructed.
    kp._nSubtasks++;
  }
  _pTp->EnqueueAdjacent(kp._pSubtasks, kp._nSubtasks, task);

  kp._pTask = nullptr; // Don't call again SRBaseTask::WaitComplete() if it throws here.
  task.WaitComplete();
  return std::move(kp);
}

} // namespace SRPlat
