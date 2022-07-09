#pragma once

#include <filesystem>
#include <mutex>
#include <regex>
#include <vector>

#include "file_entry.hh"

#ifndef BOOST_ASIO_HAS_STD_INVOKE_RESULT
#define BOOST_ASIO_HAS_STD_INVOKE_RESULT
#endif

#include <boost/asio/thread_pool.hpp>

namespace dedupe {

inline namespace detail_v1 {

inline bool is_exclude(const std::filesystem::path &path,
                       const std::vector<std::regex> &exclude_regex) {
  for (const auto &regex : exclude_regex) {
    if (std::regex_match(path.native(), regex)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief list directory recursively
 *
 * @param dir directory path
 * @param[out] file_list file list
 * @param mtx mutex for protecting file_list
 * @param pool thread pool for recursive calls
 * @param exclude_regex regular expression to exclude files or directories
 */
void ls_dir_rec(const std::filesystem::path dir,
                std::vector<file_entry_t> &file_list, std::mutex &mtx,
                boost::asio::thread_pool &pool,
                const std::vector<std::regex> &exclude_regex);

}  // namespace detail_v1

}  // namespace dedupe