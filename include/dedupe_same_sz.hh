#pragma once

#include <filesystem>
#include <mutex>
#include <span>
#include <vector>

#include "file_entry.hh"

namespace dedupe {

inline namespace detail_v1_0_0 {

/**
 * @brief detects duplicate files of the same size using hash,
 * collisions are possible, and hard links are included.
 *
 * @param file_list files to search
 * @param[out] dupe_list list of duplicates
 * @param mtx mutex for protecting dupe_list
 */
void dedupe_same_sz(std::span<file_entry_t> file_list,
                    std::vector<std::vector<std::filesystem::path>> &dupe_list,
                    std::mutex &mtx);

}  // namespace detail_v1_0_0

}  // namespace dedupe