#pragma once

#include <xxhash.h>

#include <bit>
#include <compare>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "file_entry.hh"

namespace dedupe {

inline namespace detail_v1 {

// tools for calculating max_hash

inline constexpr auto log2_ceil(const auto x) noexcept {
  auto next_pow2 = std::bit_ceil(x);
  return std::countr_zero(next_pow2);
}

inline constexpr auto div_ceil(const auto x, const auto y) noexcept {
  return (x + y - 1) / y;
}

class file_cmp_t {
  // using mutable to allow lazy hashing during comparison

  file_entry_t _file_entry;
  mutable std::vector<XXH128_hash_t> _file_hashes;
  mutable uint64_t _remain_sz;
  mutable std::ifstream _file_stream;
  uint64_t _hard_link_cnt;
  uint32_t _max_hash;

  /**
   * @brief lazy hash file content
   *
   * @param idx hash block index
   */
  void lazy_hash(uint32_t idx) const;
  void open_file() const noexcept;
  void close_file() const noexcept;

 public:
  file_cmp_t() = delete;
  template <typename Tp>
  inline file_cmp_t(Tp &&file_entry, uint32_t max_hash)
      : _file_entry(std::forward<Tp>(file_entry)),
        _remain_sz(_file_entry.size()),
        _max_hash(max_hash) {
    _file_hashes.reserve(_max_hash);
    _hard_link_cnt = std::filesystem::hard_link_count(_file_entry.path());
  }

  file_cmp_t(const file_cmp_t &) = delete;
  file_cmp_t(file_cmp_t &&) = default;
  file_cmp_t &operator=(const file_cmp_t &) = delete;
  file_cmp_t &operator=(file_cmp_t &&) = default;

  std::strong_ordering operator<=>(const file_cmp_t &rhs) const;
  inline bool operator==(const file_cmp_t &rhs) const {
    return (*this <=> rhs) == std::strong_ordering::equal;
  }

  inline std::filesystem::path path() const noexcept {
    return _file_entry.path();
  }
  inline uint64_t size() const noexcept { return _file_entry.size(); }
};

}  // namespace detail_v1

}  // namespace dedupe