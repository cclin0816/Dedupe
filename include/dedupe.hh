#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace dedupe {

inline namespace detail_v1 {

/**
 * @brief detects duplicate files using file size and hash,
 * collisions are possible, and hard links are included.
 * 
 * @param search_dir directories to search
 * @param max_thread maximum number of threads to use
 * @return vector[vector[path]] list of duplicates
 */
std::vector<std::vector<std::filesystem::path>> dedupe(
    const std::vector<std::filesystem::path> &search_dir,
    const uint32_t max_thread = 4);

}  // namespace detail_v1

}  // namespace dedupe