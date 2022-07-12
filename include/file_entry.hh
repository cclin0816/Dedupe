#pragma once

#include <cstdint>
#include <filesystem>
#include <utility>

namespace dedupe {

inline namespace detail_v1_0_0 {

class file_entry_t {
  std::filesystem::path _path;
  uint64_t _size = 0;

 public:
  template <typename Tp>
  inline file_entry_t(Tp &&path, const uint64_t size) noexcept(
      noexcept(std::filesystem::path(std::forward<Tp>(path))))
      : _path(std::forward<Tp>(path)), _size(size) {}

  inline file_entry_t(const file_entry_t &rhs) = default;
  inline file_entry_t(file_entry_t &&rhs) = default;
  inline file_entry_t &operator=(const file_entry_t &rhs) = default;
  inline file_entry_t &operator=(file_entry_t &&rhs) = default;

  inline const std::filesystem::path &path() const noexcept { return _path; }
  inline uint64_t size() const noexcept { return _size; }
};

}  // namespace detail_v1_0_0

}  // namespace dedupe
