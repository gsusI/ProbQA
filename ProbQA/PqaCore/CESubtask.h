#pragma once

namespace ProbQA {

template <typename taNumber> class CETask;

template<typename taNumber> class CESubtask {
public: // types
  enum class Kind : uint8_t {
    None = 0
  };

private: // variables
  CETask<taNumber> *_pTask;

public:
  virtual ~CESubtask() { }
  virtual Kind GetKind() = 0;
  CETask<taNumber>* GetTask() const { return _pTask; }
};

} // namespace ProbQA