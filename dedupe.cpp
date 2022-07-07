#include <xxhash.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <compare>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <span>
#include <thread>
#include <vector>

#define BOOST_ASIO_HAS_STD_INVOKE_RESULT
#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>

#if __cpp_lib_syncbuf >= 201803L
#include <syncstream>
namespace dedupe {
using oss = std::osyncstream;
}  // namespace dedupe
#else
namespace dedupe {
class oss {
 private:
  inline static std::mutex mtx;
  std::ostream &os;

 public:
  oss() = delete;
  oss(std::ostream &os) : os(os) { mtx.lock(); }
  ~oss() { mtx.unlock(); }
  oss(const oss &) = delete;
  oss(oss &&) = delete;
  oss &operator=(const oss &) = delete;
  oss &operator=(oss &&) = delete;
  template <typename Tp>
  oss &operator<<(const Tp &val) {
    os << val;
    return *this;
  }
};
}  // namespace dedupe
#endif

namespace dedupe {

namespace fs = std::filesystem;
namespace ba = boost::asio;
namespace cn = std::chrono;

constexpr auto blk_sz = 4096UL;

class time_log_t {
  cn::steady_clock::time_point prev_time;

 public:
  time_log_t() : prev_time(cn::steady_clock::now()) {}
  void clock() {
    auto cur_time = cn::steady_clock::now();
    std::cerr << "[log] elapsed time: "
              << std::chrono::duration_cast<cn::milliseconds>(cur_time -
                                                              prev_time)
                     .count()
              << " ms" << std::endl;
    prev_time = cur_time;
  }
};

class file_entry_t {
  fs::path _path;
  std::size_t _size = 0;

 public:
  file_entry_t(auto &&path, const std::size_t size) noexcept(
      noexcept(fs::path(std::forward<decltype(path)>(path))))
      : _path(std::forward<decltype(path)>(path)), _size(size) {}
  file_entry_t(const file_entry_t &rhs) = default;
  file_entry_t(file_entry_t &&rhs) = default;
  file_entry_t &operator=(const file_entry_t &rhs) = default;
  file_entry_t &operator=(file_entry_t &&rhs) = default;
  fs::path &path() noexcept { return _path; }
  std::size_t size() const noexcept { return _size; }
};

class hasher_t {
  XXH3_state_t *state;

 public:
  hasher_t() {
    state = XXH3_createState();
    if (state == nullptr) {
      throw std::runtime_error("XXH3_createState failed");
    }
    if (XXH3_128bits_reset(state) == XXH_ERROR) {
      throw std::runtime_error("XXH3_128bits_reset failed");
    }
  }
  hasher_t(const hasher_t &rhs) = delete;
  hasher_t(hasher_t &&rhs) = delete;
  hasher_t &operator=(const hasher_t &rhs) = delete;
  hasher_t &operator=(hasher_t &&rhs) = delete;
  ~hasher_t() {
    if (state != nullptr) {
      XXH3_freeState(state);
    }
  }
  void update(const char *data, const std::size_t size) {
    if (XXH3_128bits_update(state, data, size) == XXH_ERROR) {
      throw std::runtime_error("XXH3_128bits_update failed");
    }
  }
  XXH128_hash_t digest() { return XXH3_128bits_digest(state); }
};

class file_cmp_t {
  std::vector<XXH128_hash_t> file_hashes;
  fs::path _path;
  std::size_t _size = 0;
  std::size_t remain = 0;
  uint32_t max_hash = 0;

  void lazy_hash(const uint32_t idx) {
    if (idx < file_hashes.size()) {
      return;
    }
    std::vector<char> buf(blk_sz);
    hasher_t hasher;
    std::ifstream ifs(_path, std::ios::binary);
    const auto round = idx == 0U ? 1U : 1U << (idx - 1U);
    const auto offset = idx == 0U ? 0U : round * blk_sz;
    ifs.seekg((int64_t)offset);
    for (auto i = 0U; i < round; ++i) {
      const auto read_sz = std::min(blk_sz, remain);
      const auto read_len = ifs.read(buf.data(), (int64_t)read_sz).gcount();
      if (read_sz != (uint64_t)read_len) {
        oss(std::cerr) << "[err] read error: " << _path << '\n';
        file_hashes.resize(max_hash);
        remain = 0;
        return;
      }
      hasher.update(buf.data(), read_sz);
      remain -= read_sz;
    }
    file_hashes.emplace_back(hasher.digest());
  }

 public:
  file_cmp_t() = delete;
  file_cmp_t(auto &&path, const std::size_t size) noexcept(
      noexcept(fs::path(std::forward<decltype(path)>(path))))
      : _path(std::forward<decltype(path)>(path)), _size(size), remain(size) {
    auto blk_cnt = (_size + blk_sz - 1) / blk_sz;
    auto next_pow2 = std::bit_ceil(blk_cnt);
    max_hash = (uint32_t)(std::countr_zero(next_pow2) + 1);
    file_hashes.reserve(max_hash);
  }
  auto operator<=>(file_cmp_t &rhs) noexcept {
    for (auto i = 0U; i < max_hash; ++i) {
      lazy_hash(i);
      rhs.lazy_hash(i);
      auto cmp_high = file_hashes[i].high64 <=> rhs.file_hashes[i].high64;
      if (cmp_high != std::strong_ordering::equal) {
        return cmp_high;
      }
      auto cmp_low = file_hashes[i].low64 <=> rhs.file_hashes[i].low64;
      if (cmp_low != std::strong_ordering::equal) {
        return cmp_low;
      }
    }
    return std::strong_ordering::equal;
  }
  bool operator==(file_cmp_t &rhs) noexcept {
    return (*this <=> rhs) == std::strong_ordering::equal;
  }
  fs::path &path() noexcept { return _path; }
  std::size_t size() const noexcept { return _size; }
};

void list_dir_recusive(const fs::path dir, std::vector<file_entry_t> &file_list,
                       std::mutex &mtx, ba::thread_pool &pool) {
  std::vector<file_entry_t> file_list_tmp;
  try {
    for (const auto &dir_entry : fs::directory_iterator(dir)) {
      // symlink, skip
      if (dir_entry.is_symlink()) {
        oss(std::cerr) << "[warn] skip symlink: " << dir_entry.path() << '\n';

      }
      // directory, recursive call
      else if (dir_entry.is_directory()) {
        ba::post(pool,
                 std::bind(list_dir_recusive, dir_entry.path(),
                           std::ref(file_list), std::ref(mtx), std::ref(pool)));
      }
      // regular file, add to list
      else if (dir_entry.is_regular_file()) {
        std::error_code ec;
        auto file_size = dir_entry.file_size(ec);
        if (ec) {
          oss(std::cerr) << "[warn] skip file: " << dir_entry.path() << " - "
                         << ec.message() << '\n';
        } else if (file_size != 0) {
          file_list_tmp.emplace_back(dir_entry.path(), file_size);
        }
      }
      // other file type, skip
      else {
        oss(std::cerr) << "[warn] skip unsupport file: " << dir_entry.path()
                       << '\n';
      }
    }
  } catch (fs::filesystem_error &e) {
    oss(std::cerr) << "[warn] skip directory: " << dir << " - "
                   << e.code().message() << '\n';
  }

  if (!file_list_tmp.empty()) {
    std::lock_guard lk(mtx);
    file_list.insert(file_list.end(),
                     std::make_move_iterator(file_list_tmp.begin()),
                     std::make_move_iterator(file_list_tmp.end()));
  }
}

void dedupe_same_sz(std::span<file_entry_t> file_list,
                    std::vector<std::vector<fs::path>> &dupe_list,
                    std::mutex &mtx) {
  std::vector<file_cmp_t> file_cmp_list;
  file_cmp_list.reserve(file_list.size());
  for (auto &file : file_list) {
    file_cmp_list.emplace_back(std::move(file.path()), file.size());
  }

  std::sort(file_cmp_list.begin(), file_cmp_list.end());

  std::vector<std::vector<fs::path>> dupe_list_tmp;
  {
    auto file_start = file_cmp_list.begin();
    auto file_itr = file_start + 1;
    while (true) {
      if (file_itr == file_cmp_list.end() || *file_itr != *file_start) {
        auto dist = std::distance(file_start, file_itr);
        if (dist > 1) {
          auto &group = dupe_list_tmp.emplace_back();
          group.reserve((uint64_t)dist);
          oss _oss(std::cout);
          for (; file_start != file_itr; ++file_start) {
            _oss << file_start->path() << " - " << file_start->size() << '\n';
            group.emplace_back(std::move(file_start->path()));
          }
        }
        file_start = file_itr;
        if (file_itr == file_cmp_list.end()) {
          break;
        }
      }
      ++file_itr;
    }
  }
  if (!dupe_list_tmp.empty()) {
    std::lock_guard lk(mtx);
    dupe_list.insert(dupe_list.end(),
                     std::make_move_iterator(dupe_list_tmp.begin()),
                     std::make_move_iterator(dupe_list_tmp.end()));
  }
}

std::vector<std::vector<fs::path>> dedupe(
    const std::vector<fs::path> &search_dir, const uint8_t max_thread = 4) {
  time_log_t time_log;
  std::vector<file_entry_t> file_list;
  std::cerr << "[log] list files..." << std::endl;
  {
    ba::thread_pool pool(max_thread);
    std::mutex mtx;
    for (const auto &dir : search_dir) {
      ba::post(pool, std::bind(list_dir_recusive, dir, std::ref(file_list),
                               std::ref(mtx), std::ref(pool)));
    }
    pool.join();
  }
  time_log.clock();
  std::cerr << "[log] file count: " << file_list.size() << std::endl;

  std::cerr << "[log] sort files..." << std::endl;
  std::sort(
      file_list.begin(), file_list.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.size() < rhs.size(); });
  time_log.clock();

  std::vector<std::vector<fs::path>> dupe_list;
  std::cerr << "[log] detect duplicates..." << std::endl;
  if (file_list.size() > 1) {
    auto sz_start = file_list.begin();
    auto itr = sz_start + 1;
    ba::thread_pool pool(max_thread);
    std::mutex mtx;
    while (true) {
      if (itr == file_list.end() || itr->size() != sz_start->size()) {
        if (std::distance(sz_start, itr) > 1) {
          // dispatch to detect duplicates
          // &(*) is workaround for libc++ bug
          ba::post(pool,
                   std::bind(dedupe_same_sz, std::span(&(*sz_start), &(*itr)),
                             std::ref(dupe_list), std::ref(mtx)));
        }
        sz_start = itr;
        if (itr == file_list.end()) {
          break;
        }
      }
      ++itr;
    }
    pool.join();
  }
  time_log.clock();
  std::cerr << "[log] duplicate group count: " << dupe_list.size() << std::endl;

  return dupe_list;
}

}  // namespace dedupe

int main() {
  std::vector<std::filesystem::path> search_dir{"/mnt/d"};
  auto dupe_list = dedupe::dedupe(search_dir, 8);
  // for (const auto &group : dupe_list) {
  //   for (const auto &file : group) {
  //     std::cout << file << '\n';
  //   }
  // }
}
