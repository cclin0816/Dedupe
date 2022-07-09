#include "dedupe.hh"

#include <chrono>
#include <functional>
#include <iostream>
#include <mutex>

#include "dedupe_same_sz.hh"
#include "file_entry.hh"
#include "ls_dir_rec.hh"
#include "oss.hh"

#ifndef BOOST_ASIO_HAS_STD_INVOKE_RESULT
#define BOOST_ASIO_HAS_STD_INVOKE_RESULT
#endif

#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>

namespace dedupe {

inline namespace detail_v1 {

class timer_t {
  std::chrono::steady_clock::time_point _prev_time;

 public:
  timer_t() noexcept : _prev_time(std::chrono::steady_clock::now()) {}
  std::chrono::milliseconds time() noexcept {
    auto cur_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        cur_time - _prev_time);
    _prev_time = cur_time;
    return duration;
  }
};

std::vector<std::vector<std::filesystem::path>> dedupe(
    const std::vector<std::filesystem::path> &search_dir,
    const std::vector<std::regex> &exclude_regex, const uint32_t max_thread) {
  // generate file list
  timer_t timer;
  std::vector<file_entry_t> file_list;
  std::cerr << "[log] list files..." << std::endl;
  {
    boost::asio::thread_pool pool(max_thread);
    std::mutex mtx;
    for (const auto &dir : search_dir) {
      if (is_exclude(dir, exclude_regex)) {
        oss(std::cerr) << "[log] exclude: " << dir << '\n';
        continue;
      }
      boost::asio::post(
          pool, std::bind(ls_dir_rec, dir, std::ref(file_list), std::ref(mtx),
                          std::ref(pool), std::cref(exclude_regex)));
    }
    pool.join();
  }
  std::cerr << "[log] elapsed: " << timer.time().count() << "ms" << std::endl;
  std::cerr << "[log] file count: " << file_list.size() << std::endl;

  // sort files by size
  std::cerr << "[log] sort files..." << std::endl;
  std::sort(
      file_list.begin(), file_list.end(),
      [](const auto &lhs, const auto &rhs) { return lhs.size() < rhs.size(); });
  std::cerr << "[log] elapsed: " << timer.time().count() << "ms" << std::endl;

  // detect duplicates
  std::vector<std::vector<std::filesystem::path>> dupe_list;
  uint64_t job_count = 0;
  std::cerr << "[log] detect duplicates..." << std::endl;
  if (file_list.size() > 1) {
    // finding union of same file size
    auto union_st = file_list.begin();
    auto union_ed = union_st + 1;
    boost::asio::thread_pool pool(max_thread);
    std::mutex mtx;
    while (true) {
      if (union_ed == file_list.end() || union_ed->size() != union_st->size()) {
        // end of union
        auto union_sz = std::distance(union_st, union_ed);
        if (union_sz > 1) {
          // dispatch to detect duplicates for same file size
          // &(*) is workaround for libc++ bug
          boost::asio::post(
              pool,
              std::bind(dedupe_same_sz, std::span(&(*union_st), &(*union_ed)),
                        std::ref(dupe_list), std::ref(mtx)));
          ++job_count;
        }
        if (union_ed == file_list.end()) {
          break;
        }
        union_st = union_ed;
      }
      ++union_ed;
    }
    oss(std::cerr) << "[log] job count: " << job_count << std::endl;
    pool.join();
  }
  std::cerr << "[log] elapsed: " << timer.time().count() << "ms" << std::endl;
  std::cerr << "[log] duplicate group count: " << dupe_list.size() << std::endl;

  return dupe_list;
}

}  // namespace detail_v1

}  // namespace dedupe