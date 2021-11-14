#pragma once
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace dedupe {

class file_entry_t {
 public:
  std::string path;
  std::size_t size = 0;
  file_entry_t(auto &&_path, const std::size_t _size) noexcept
      : path(std::forward<decltype(_path)>(_path)), size(_size) {}
  file_entry_t(const file_entry_t &rhs) = default;
  file_entry_t(file_entry_t &&rhs) noexcept {
    path = std::move(rhs.path);
    size = std::exchange(rhs.size, 0);
  }
  file_entry_t &operator=(const file_entry_t &rhs) = default;
  file_entry_t &operator=(file_entry_t &&rhs) noexcept {
    path = std::move(rhs.path);
    size = std::exchange(rhs.size, 0);
    return *this;
  }
};

using file_entry_vec = std::vector<file_entry_t>;

}  // namespace dedupe