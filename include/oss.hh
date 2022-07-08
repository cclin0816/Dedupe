#pragma once

#include <version>

#if __cpp_lib_syncbuf >= 201803L

#include <syncstream>

namespace dedupe {

inline namespace detail_v1 {

// osyncstream is provided
using oss = std::osyncstream;

}  // namespace detail_v1

}  // namespace dedupe

#else

#include <iostream>
#include <thread>

namespace dedupe {

inline namespace detail_v1 {

// self-implemented osyncstream
class oss {
 private:
  inline static std::mutex _mtx;
  std::ostream &_os;

 public:
  oss() = delete;
  inline oss(std::ostream &os) : _os(os) { _mtx.lock(); }
  inline ~oss() { _mtx.unlock(); }

  oss(const oss &) = delete;
  oss(oss &&) = delete;
  oss &operator=(const oss &) = delete;
  oss &operator=(oss &&) = delete;
  
  template <typename Tp>
  inline oss &operator<<(const Tp &val) {
    _os << val;
    return *this;
  }
  inline operator std::ostream &() noexcept { return _os; }
};

}  // namespace detail_v1

}  // namespace dedupe

#endif