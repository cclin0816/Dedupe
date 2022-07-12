#pragma once

#include <cstdint>
#include <filesystem>
#include <regex>
#include <vector>

namespace dedupe {

inline namespace detail_v1_0_0 {

/**
 * @brief detects duplicate files using file size and hash,
 * collisions are possible, and hard links are included.
 *
 * @param search_dir directories to search
 * @param exclude_regex regular expression to exclude files or directories
 * @param max_thread maximum number of threads to use
 * @return vector[vector[path]] list of duplicates
 */
std::vector<std::vector<std::filesystem::path>> dedupe(
    const std::vector<std::filesystem::path> &search_dir,
    const std::vector<std::regex> &exclude_regex,
    const uint32_t max_thread = 4);

/**
 * @brief remove files
 *
 * @param rm_list files to remove
 */
void remove(const std::vector<std::filesystem::path> &rm_list);

}  // namespace detail_v1_0_0

}  // namespace dedupe